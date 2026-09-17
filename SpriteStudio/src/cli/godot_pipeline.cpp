#include "cli/godot_pipeline.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRegularExpression>

namespace SpriteStudioCli {

QString GodotPipeline::extractExistingUid(const QString &tresPath)
{
    QFile file(tresPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QString content = QString::fromUtf8(file.read(1024));
    file.close();

    QRegularExpression uidRegex(QStringLiteral("uid=\"([^\"]+)\""));
    QRegularExpressionMatch match = uidRegex.match(content);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

QString GodotPipeline::generateDeterministicUid(const QString &seed)
{
    QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256).toHex();
    // Godot 4 UIDs are typically 12-16 lowercase alphanumeric characters
    QString shortHash = QString::fromLatin1(hash.left(12));
    return QStringLiteral("uid://") + shortHash;
}

bool GodotPipeline::generateScene(const QString &scenePath,
                                  const QString &tresPath,
                                  const QString &defaultAnimName,
                                  QString *outError)
{
    QFileInfo sceneInfo(scenePath);
    QDir().mkpath(sceneInfo.dir().absolutePath());

    QFile file(scenePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) *outError = QStringLiteral("Cannot write Godot scene file: ") + scenePath;
        return false;
    }

    QFileInfo tresInfo(tresPath);
    QString relTresPath = tresInfo.fileName();
    QString sceneUid = generateDeterministicUid(scenePath);

    QTextStream out(&file);
    out << "[gd_scene load_steps=2 format=3 uid=\"" << sceneUid << "\"]\n\n";
    out << "[ext_resource type=\"SpriteFrames\" path=\"res://" << relTresPath << "\" id=\"1_frames\"]\n\n";
    out << "[node name=\"Player\" type=\"AnimatedSprite2D\"]\n";
    out << "sprite_frames = ExtResource(\"1_frames\")\n";
    QString anim = defaultAnimName.isEmpty() ? QStringLiteral("default") : defaultAnimName;
    out << "animation = &\"" << anim << "\"\n";
    out << "autoplay = &\"" << anim << "\"\n";

    file.close();
    return true;
}

CliResult GodotPipeline::exportGodot(const ExportArgs &args)
{
    if (args.frameRects.isEmpty() && args.frames.isEmpty()) {
        return CliResult::error(ExitConstraintFailed, QStringLiteral("No frames to export to Godot."));
    }

    // 1. Ensure directories exist
    QFileInfo sheetInfo(args.sheetPath);
    QFileInfo tresInfo(args.tresPath);
    QDir().mkpath(sheetInfo.dir().absolutePath());
    QDir().mkpath(tresInfo.dir().absolutePath());

    // 2. Save atlas texture
    if (!args.atlas.save(args.sheetPath, "PNG")) {
        return CliResult::error(ExitIoError, QStringLiteral("Failed to write atlas image: ") + args.sheetPath);
    }

    // 3. Resolve UID
    QString uid = args.explicitUid;
    if (uid.isEmpty() && args.preserveExistingUid && QFile::exists(args.tresPath)) {
        uid = extractExistingUid(args.tresPath);
    }
    if (uid.isEmpty()) {
        uid = generateDeterministicUid(tresInfo.fileName());
    }

    // 4. Compute animation envelopes for margins
    QMap<QString, QRect> animEnvelopes;
    for (auto it = args.animations.begin(); it != args.animations.end(); ++it) {
        QRect env;
        for (int fIdx : it.value().frameIndices) {
            if (fIdx >= 0 && fIdx < args.frameRects.size()) {
                const QRect &r = args.frameRects[fIdx];
                QPoint piv = (fIdx < args.pivots.size()) ? args.pivots[fIdx] : QPoint(r.width() / 2, r.height());
                QRect localRect(-piv.x(), -piv.y(), r.width(), r.height());
                env = env.united(localRect);
            }
        }
        animEnvelopes.insert(it.key(), env);
    }

    // 5. Write .tres file
    QFile file(args.tresPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return CliResult::error(ExitIoError, QStringLiteral("Cannot write Godot resource file: ") + args.tresPath);
    }

    QTextStream out(&file);
    int loadSteps = args.frameRects.size() + 2;
    out << "[gd_resource type=\"SpriteFrames\" load_steps=" << loadSteps << " format=3";
    if (!uid.isEmpty()) {
        out << " uid=\"" << uid << "\"";
    }
    out << "]\n\n";

    QString relSheetFilename = sheetInfo.fileName();
    out << "[ext_resource type=\"Texture2D\" path=\"res://" << relSheetFilename << "\" id=\"1_atlas\"]\n\n";

    for (int i = 0; i < args.frameRects.size(); ++i) {
        const QRect &r = args.frameRects[i];
        QString subResId = QStringLiteral("AtlasTexture_%1").arg(i);
        out << "[sub_resource type=\"AtlasTexture\" id=\"" << subResId << "\"]\n";
        out << "atlas = ExtResource(\"1_atlas\")\n";
        out << "region = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n";

        // Find which animation contains frame i
        QString animName;
        for (auto it = args.animations.begin(); it != args.animations.end(); ++it) {
            if (it.value().frameIndices.contains(i)) {
                animName = it.key();
                break;
            }
        }

        QRect env = animEnvelopes.value(animName);
        QPoint piv = (i < args.pivots.size()) ? args.pivots[i] : QPoint(r.width() / 2, r.height());
        if (!env.isEmpty()) {
            int ox = env.x() - (-piv.x());
            int oy = env.y() - (-piv.y());
            if (env.width() > r.width() || env.height() > r.height() || ox != 0 || oy != 0) {
                out << "margin = Rect2(" << ox << ", " << oy << ", " << env.width() << ", " << env.height() << ")\n";
            }
        }
        out << "filter = 0\n\n";
    }

    out << "[resource]\n";
    out << "animations = [{\n";

    if (args.animations.isEmpty()) {
        out << "\"frames\": [";
        for (int i = 0; i < args.frameRects.size(); ++i) {
            if (i > 0) out << ", ";
            out << "SubResource(\"AtlasTexture_" << i << "\")";
        }
        out << "],\n";
        out << "\"loop\": true,\n";
        out << "\"name\": &\"default\",\n";
        out << "\"speed\": 10.0\n";
        out << "}]\n";
    } else {
        bool firstAnim = true;
        for (auto it = args.animations.begin(); it != args.animations.end(); ++it) {
            if (!firstAnim) out << "}, {\n";
            firstAnim = false;

            out << "\"frames\": [";
            const QList<int> &fIndices = it.value().frameIndices;
            for (int j = 0; j < fIndices.size(); ++j) {
                if (j > 0) out << ", ";
                out << "SubResource(\"AtlasTexture_" << fIndices[j] << "\")";
            }
            out << "],\n";
            out << "\"loop\": " << (it.value().loop ? "true" : "false") << ",\n";
            out << "\"name\": &\"" << it.key() << "\",\n";
            out << "\"speed\": " << static_cast<double>(it.value().fps) << ".0\n";
        }
        out << "}]\n";
    }

    file.close();

    // 6. Generate optional .tscn scene
    if (!args.scenePath.isEmpty()) {
        QString err;
        QString defaultAnim = args.animations.isEmpty() ? QStringLiteral("default") : args.animations.firstKey();
        if (!generateScene(args.scenePath, args.tresPath, defaultAnim, &err)) {
            return CliResult::error(ExitIoError, err);
        }
    }

    QJsonObject json;
    json[QStringLiteral("atlas")] = args.sheetPath;
    json[QStringLiteral("data")] = args.tresPath;
    json[QStringLiteral("format")] = QStringLiteral("godot4");
    json[QStringLiteral("uid")] = uid;
    json[QStringLiteral("frames_count")] = args.frameRects.size();
    if (!args.scenePath.isEmpty()) {
        json[QStringLiteral("scene")] = args.scenePath;
    }

    return CliResult::success(QStringLiteral("Godot 4 SpriteFrames exported successfully."), json);
}

} // namespace SpriteStudioCli
