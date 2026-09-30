#pragma once

#include "CoreMinimal.h"

/**
 * Connects the Unreal Engine Editor to the external bentopack-cli and BentoPack Studio app.
 */
class BENTOPACKEDITOR_API FBentoCliBridge
{
public:
	static const FString ReleasesUrl;

	static FString FindCliPath();
	static FString FindGuiPath();

	static bool OpenInBentoPack(const FString& TargetFilePath);
	static bool RunCliCommand(const FString& Arguments, FString& OutStdOutput, FString& OutStdError);

	static bool StartWatchDaemon(const FString& Arguments, int32& OutProcessId, FString& OutError);
	static void StopWatchDaemon();
	static bool IsWatchActive();
	static int32 GetWatchPid();

private:
	static FProcHandle WatchProcessHandle;
	static int32 WatchProcessId;
};
