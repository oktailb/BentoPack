#ifndef CLIPARSER_H
#define CLIPARSER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

namespace SpriteStudioCli {

/**
 * @brief POSIX-compliant exit codes for script automation and CI/CD pipelines.
 */
enum ExitCode {
    ExitSuccess             = 0, ///< Successful execution
    ExitSyntaxError         = 1, ///< Invalid arguments, unknown options, or missing parameters
    ExitFileNotFound        = 2, ///< Input files, directories, or assets not found
    ExitConstraintFailed    = 3, ///< Packing constraints unsatisfied (e.g. sprites exceed max-size)
    ExitIoError             = 4  ///< Disk read/write permissions or file creation error
};

/**
 * @brief Invocation flavor for CLI command compatibility.
 */
enum class CliFlavor {
    Auto,          ///< Automatically detect based on argv[0] or arguments
    TexturePacker, ///< 100% TexturePacker CLI compatibility mode
    Aseprite,      ///< Aseprite batch CLI compatibility mode
    Godot,         ///< Dedicated Godot 4 asset pipeline mode
    Native         ///< Native SpriteStudio subcommands (pack, slice, filter, ssp)
};

/**
 * @brief Structured result of a CLI operation.
 */
struct CliResult {
    int exitCode = ExitSuccess;
    QString message;
    QJsonObject json;

    static CliResult success(const QString &msg = QString(), const QJsonObject &obj = QJsonObject()) {
        CliResult res;
        res.exitCode = ExitSuccess;
        res.message = msg;
        res.json = obj;
        if (!res.json.contains(QStringLiteral("status"))) {
            res.json[QStringLiteral("status")] = QStringLiteral("success");
        }
        return res;
    }

    static CliResult error(int code, const QString &msg, const QJsonObject &obj = QJsonObject()) {
        CliResult res;
        res.exitCode = code;
        res.message = msg;
        res.json = obj;
        res.json[QStringLiteral("status")] = QStringLiteral("error");
        res.json[QStringLiteral("error_code")] = code;
        res.json[QStringLiteral("error_message")] = msg;
        return res;
    }
};

/**
 * @brief Multi-flavor CLI argument parser and command dispatcher.
 */
class CliParser
{
public:
    CliParser() = default;

    /**
     * @brief Parses CLI arguments and executes the requested command.
     */
    CliResult parseAndExecute(const QStringList &args);

    bool isJsonOutput() const { return m_jsonOutput; }
    bool isQuiet() const { return m_quiet; }
    bool isVerbose() const { return m_verbose; }
    CliFlavor flavor() const { return m_flavor; }

    static QString helpText();
    static QString versionText();

private:
    CliFlavor detectFlavor(const QString &programName, const QStringList &args) const;

    bool m_jsonOutput = false;
    bool m_quiet = false;
    bool m_verbose = false;
    CliFlavor m_flavor = CliFlavor::Auto;
};

} // namespace SpriteStudioCli

#endif // CLIPARSER_H
