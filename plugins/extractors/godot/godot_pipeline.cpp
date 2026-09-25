// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "godot_pipeline.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRegularExpression>

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
