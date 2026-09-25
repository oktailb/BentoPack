/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#ifndef CLIPARSER_H
#define CLIPARSER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include "bentopackcore_export.h"

namespace BentoPackCli {

/**
 * @brief POSIX-compliant exit codes for script automation and CI/CD pipelines.
 */
enum ExitCode {
    ExitSuccess             = 0, ///< Successful execution
    ExitSyntaxError         = 1, ///< Invalid arguments, unknown options, or missing parameters
    ExitFileNotFound        = 2, ///< Input files, directories, or assets not found
    ExitConstraintFailed    = 3, ///< Packing constraints unsatisfied (e.g. sprites exceed max-size)
    ExitIoError             = 4, ///< Disk read/write permissions or file creation error
    ExitLockConflict        = 5, ///< Another watch daemon instance is already active for this directory
    ExitPluginNotFound      = 6  ///< Requested plugin or encoder format not found
};

/**
 * @brief Invocation flavor for CLI command compatibility.
 */
enum class CliFlavor {
    Auto,          ///< Automatically detect based on argv[0] or arguments
    TexturePacker, ///< 100% TexturePacker CLI compatibility mode
    Aseprite,      ///< Aseprite batch CLI compatibility mode
    Godot,         ///< Dedicated Godot 4 asset pipeline mode
    Native         ///< Native BentoPack subcommands (pack, slice, filter, ssp)
};

/**
 * @brief Structured result of a CLI operation.
 */
struct BENTOPACK_CORE_EXPORT CliResult {
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
class BENTOPACK_CORE_EXPORT CliParser
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

    bool isWatchMode() const { return m_watchMode; }
    bool isDaemonMode() const { return m_daemonMode; }
    int debounceMs() const { return m_debounceMs; }
    QString stopWatchDir() const { return m_stopWatchDir; }

    static QString helpText();
    static QString versionText();

    CliResult dispatchCommand(const QStringList &cleanArgs);

private:
    CliFlavor detectFlavor(const QString &programName, const QStringList &args) const;

    bool m_jsonOutput = false;
    bool m_quiet = false;
    bool m_verbose = false;
    CliFlavor m_flavor = CliFlavor::Auto;

    bool m_watchMode = false;
    bool m_daemonMode = false;
    int m_debounceMs = 300;
    QString m_stopWatchDir;

    QString m_watchSheetPath;
    QString m_watchDataPath;
    QStringList m_watchInputPaths;
};

} // namespace BentoPackCli

#endif // CLIPARSER_H
