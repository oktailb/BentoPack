#include "cli/cliparser.h"
#include "cli/clipackpipeline.h"
#include "cli/native_commands.h"
#include "cli/watch_daemon.h"
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <csignal>

#if defined(Q_OS_WIN)
#include <windows.h>
static BOOL WINAPI consoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        QCoreApplication::quit();
        return TRUE;
    }
    return FALSE;
}
#endif

namespace SpriteStudioCli {

QString CliParser::versionText()
{
    return QStringLiteral("SpriteStudio CLI v1.0.0 (Qt 6 - C++17 Headless Engine)");
}

QString CliParser::helpText()
{
    return QStringLiteral(
        "SpriteStudio CLI - Headless Texture Packing & CI/CD Asset Pipeline\n\n"
        "Usage:\n"
        "  spritestudio-cli [command] [options] <files...>\n"
        "  spritestudio-cli --sheet <file> --data <file> [options] <files...>\n"
        "  TexturePacker [options] <files...>  (Drop-in replacement mode)\n"
        "  aseprite -b [options] <files...>     (Aseprite batch mode)\n\n"
        "Global Options:\n"
        "  -h, --help                  Show this help message\n"
        "  -v, --version               Show application version\n"
        "  --json                      Emit structured JSON output to stdout\n"
        "  --quiet                     Suppress progress and status messages\n"
        "  --verbose                   Display verbose debug details\n"
        "  --flavor <tp|aseprite|godot|native>  Force CLI syntax flavor\n\n"
        "Watch & Daemon Options:\n"
        "  --watch                     Watch input files/folders and automatically re-pack on change\n"
        "  --daemon                    Run watch mode as a background service with isolated lock\n"
        "  --debounce <ms>             Debounce delay in milliseconds before re-packing (default: 300)\n"
        "  --stop-watch <dir>          Terminate active watch daemon guarding the specified directory\n\n"
        "Commands:\n"
        "  pack                        Pack individual images, folders or projects into sprite sheet\n"
        "  slice                       Auto-slice raw sheet with connected components & background removal\n"
        "  filter                      Apply headless image filters (despill, outline, color-swap, etc.)\n"
        "  ssp                         Inspect or export native .ssp projects\n\n"
        "TexturePacker Compatibility Options:\n"
        "  --sheet <file.png>          Target sprite sheet texture file\n"
        "  --data <file.json|tres>     Target metadata file (JSON or Godot 4 .tres)\n"
        "  --format <fmt>              Data format (json-array, json-hash, godot, godot4)\n"
        "  --algorithm <algo>          Packing algorithm (MaxRects [default], Basic, Grid)\n"
        "  --maxrects-heuristics <h>   MaxRects heuristic (BestShortSideFit, BestAreaFit, BestLongSideFit)\n"
        "  --size-constraints <c>      Size constraint (POT, AnySize)\n"
        "  --max-size <w> <h>          Maximum texture dimensions (e.g. 2048 2048)\n"
        "  --padding <px>              Spacing between sprites (default: 2)\n"
        "  --border-padding <px>       Spacing around sheet border (default: 0)\n"
        "  --extrude <px>              Extrude border pixels anti-bleeding (0, 1, 2)\n"
        "  --trim-mode <mode>          Trimming mode (Trim, Crop, None)\n"
        "  --trim-threshold <0-255>    Alpha threshold for trim (default: 1)\n"
        "  --enable-auto-alias         Enable visual deduplication of identical sprites\n"
        "  --disable-auto-alias        Disable visual deduplication\n"
        "  --pivot-point <x> <y>       Normalized sprite anchor point [0.0 - 1.0]\n"
        "  --godot-uid <uid>           Explicit Godot 4 UID (or preserve existing in .tres)\n"
        "  --godot-scene <file.tscn>   Generate instantiable Godot 4 AnimatedSprite2D scene\n\n"
        "Aseprite Batch Options:\n"
        "  -b, --batch                 Headless batch execution\n"
        "  --sheet-type <type>         Sheet arrangement (packed, horizontal, vertical, matrix)\n"
        "  --list-tags                 Include animation frameTags in JSON metadata\n"
        "  --ignore-empty              Skip completely transparent frames\n"
    );
}

CliFlavor CliParser::detectFlavor(const QString &programName, const QStringList &args) const
{
    QString baseProg = QFileInfo(programName).completeBaseName().toLower();
    if (baseProg.contains(QStringLiteral("texturepacker"))) {
        return CliFlavor::TexturePacker;
    }
    if (baseProg.contains(QStringLiteral("aseprite"))) {
        return CliFlavor::Aseprite;
    }

    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("--flavor") && i + 1 < args.size()) {
            QString fl = args[i + 1].toLower();
            if (fl == QStringLiteral("tp") || fl == QStringLiteral("texturepacker")) return CliFlavor::TexturePacker;
            if (fl == QStringLiteral("aseprite")) return CliFlavor::Aseprite;
            if (fl == QStringLiteral("godot")) return CliFlavor::Godot;
            if (fl == QStringLiteral("native")) return CliFlavor::Native;
        } else if (args[i].startsWith(QStringLiteral("--flavor="))) {
            QString fl = args[i].mid(9).toLower();
            if (fl == QStringLiteral("tp") || fl == QStringLiteral("texturepacker")) return CliFlavor::TexturePacker;
            if (fl == QStringLiteral("aseprite")) return CliFlavor::Aseprite;
            if (fl == QStringLiteral("godot")) return CliFlavor::Godot;
            if (fl == QStringLiteral("native")) return CliFlavor::Native;
        }
    }

    if (args.contains(QStringLiteral("-b")) || args.contains(QStringLiteral("--batch"))) {
        return CliFlavor::Aseprite;
    }

    return CliFlavor::Auto;
}

CliResult CliParser::dispatchCommand(const QStringList &cleanArgs)
{
    if (m_flavor == CliFlavor::TexturePacker) {
        return CliPackPipeline::execute(cleanArgs);
    }
    if (m_flavor == CliFlavor::Aseprite) {
        QStringList aspArgs = cleanArgs;
        if (!aspArgs.contains(QStringLiteral("--format"))) {
            aspArgs.append(QStringLiteral("--format"));
            aspArgs.append(QStringLiteral("json-array"));
        }
        return CliPackPipeline::execute(aspArgs);
    }
    if (m_flavor == CliFlavor::Godot) {
        QStringList godotArgs = cleanArgs;
        if (!godotArgs.contains(QStringLiteral("--format"))) {
            godotArgs.append(QStringLiteral("--format"));
            godotArgs.append(QStringLiteral("godot4"));
        }
        return CliPackPipeline::execute(godotArgs);
    }

    // Native subcommands or auto fallback
    if (!cleanArgs.isEmpty()) {
        const QString &subCmd = cleanArgs.first();
        if (subCmd == QStringLiteral("pack")) {
            return NativeCommands::executePack(cleanArgs.mid(1));
        }
        if (subCmd == QStringLiteral("slice")) {
            return NativeCommands::executeSlice(cleanArgs.mid(1));
        }
        if (subCmd == QStringLiteral("filter")) {
            return NativeCommands::executeFilter(cleanArgs.mid(1));
        }
        if (subCmd == QStringLiteral("ssp")) {
            return NativeCommands::executeSsp(cleanArgs.mid(1));
        }

        // If options contain --sheet or --data, fallback to generic pack pipeline
        if (cleanArgs.contains(QStringLiteral("--sheet")) || cleanArgs.contains(QStringLiteral("--data"))) {
            return CliPackPipeline::execute(cleanArgs);
        }

        // If unknown option or command
        return CliResult::error(ExitSyntaxError, QStringLiteral("Unknown command or invalid option: ") + subCmd);
    }

    return CliResult::success(helpText());
}

CliResult CliParser::parseAndExecute(const QStringList &args)
{
    if (args.size() <= 1) {
        return CliResult::success(helpText());
    }

    QString progName = args.first();
    QStringList rawArgs = args.mid(1);

    // Global flags detection
    QStringList cleanArgs;
    for (int i = 0; i < rawArgs.size(); ++i) {
        const QString &arg = rawArgs[i];
        if (arg == QStringLiteral("-h") || arg == QStringLiteral("--help")) {
            return CliResult::success(helpText());
        }
        if (arg == QStringLiteral("-v") || arg == QStringLiteral("--version")) {
            return CliResult::success(versionText());
        }
        if (arg == QStringLiteral("--json")) {
            m_jsonOutput = true;
            continue;
        }
        if (arg == QStringLiteral("--quiet")) {
            m_quiet = true;
            continue;
        }
        if (arg == QStringLiteral("--verbose")) {
            m_verbose = true;
            continue;
        }
        if (arg == QStringLiteral("--flavor") && i + 1 < rawArgs.size()) {
            ++i; // skip flavor value
            continue;
        }
        if (arg.startsWith(QStringLiteral("--flavor="))) {
            continue;
        }
        if (arg == QStringLiteral("--watch")) {
            m_watchMode = true;
            continue;
        }
        if (arg == QStringLiteral("--daemon")) {
            m_daemonMode = true;
            m_watchMode = true;
            continue;
        }
        if (arg == QStringLiteral("--debounce") && i + 1 < rawArgs.size()) {
            m_debounceMs = rawArgs[i + 1].toInt();
            ++i;
            continue;
        }
        if (arg == QStringLiteral("--stop-watch") && i + 1 < rawArgs.size()) {
            m_stopWatchDir = rawArgs[i + 1];
            ++i;
            return WatchDaemon::stopWatch(m_stopWatchDir);
        }

        // Detect target output files
        if (arg == QStringLiteral("--sheet") && i + 1 < rawArgs.size()) {
            m_watchSheetPath = rawArgs[i + 1];
        }
        if (arg == QStringLiteral("--data") && i + 1 < rawArgs.size()) {
            m_watchDataPath = rawArgs[i + 1];
        }

        // If not a flag, collect as potential watch inputs
        if (!arg.startsWith(QStringLiteral("-")) && arg != QStringLiteral("pack") &&
            arg != QStringLiteral("slice") && arg != QStringLiteral("filter") && arg != QStringLiteral("ssp")) {
            m_watchInputPaths.append(arg);
        }

        cleanArgs.append(arg);
    }

    m_flavor = detectFlavor(progName, rawArgs);

    if (m_watchMode) {
        // Install console interrupt handlers for clean unlocking
#if defined(Q_OS_WIN)
        SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#else
        std::signal(SIGINT, [](int){ QCoreApplication::quit(); });
        std::signal(SIGTERM, [](int){ QCoreApplication::quit(); });
#endif

        if (m_watchInputPaths.isEmpty()) {
            m_watchInputPaths.append(QDir::currentPath());
        }

        auto daemon = new WatchDaemon(qApp);
        auto repackFn = [this, cleanArgs]() -> CliResult {
            return this->dispatchCommand(cleanArgs);
        };

        CliResult startRes = daemon->startWatching(m_watchInputPaths, m_watchSheetPath, m_watchDataPath,
                                                  repackFn, m_debounceMs, m_daemonMode);
        if (startRes.exitCode != ExitSuccess) {
            return startRes;
        }

        int ret = QCoreApplication::exec();
        return CliResult::success(QStringLiteral("Watch daemon terminated cleanly."),
                                 QJsonObject{{QStringLiteral("exit_code"), ret}});
    }

    return dispatchCommand(cleanArgs);
}

} // namespace SpriteStudioCli
