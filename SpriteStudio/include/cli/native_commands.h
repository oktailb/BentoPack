#ifndef NATIVE_COMMANDS_H
#define NATIVE_COMMANDS_H

#include <QStringList>
#include "cli/cliparser.h"

namespace SpriteStudioCli {

class NativeCommands
{
public:
    static CliResult executePack(const QStringList &args);
    static CliResult executeSlice(const QStringList &args);
    static CliResult executeFilter(const QStringList &args);
    static CliResult executeSsp(const QStringList &args);
};

} // namespace SpriteStudioCli

#endif // NATIVE_COMMANDS_H
