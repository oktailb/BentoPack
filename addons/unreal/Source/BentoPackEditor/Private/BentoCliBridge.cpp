#include "BentoCliBridge.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

const FString FBentoCliBridge::ReleasesUrl = TEXT("https://github.com/oktailb/BentoPack/releases");
FProcHandle FBentoCliBridge::WatchProcessHandle;
int32 FBentoCliBridge::WatchProcessId = -1;

FString FBentoCliBridge::FindCliPath()
{
	// 1. Check environment variable override
	FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("BENTOPACK_CLI"));
	if (!EnvPath.IsEmpty() && IFileManager::Get().FileExists(*EnvPath))
	{
		return EnvPath;
	}

	// 2. Search common development and build paths
	TArray<FString> Candidates = {
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/BentoPack/bentopack-cli")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("../build/bin/bentopack-cli")),
		FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/BentoPack/bentopack-cli")),
#if PLATFORM_WINDOWS
		TEXT("C:/Program Files/BentoPack/bentopack-cli.exe"),
		TEXT("C:/BentoPack/bin/bentopack-cli.exe")
#else
		TEXT("/usr/local/bin/bentopack-cli"),
		TEXT("/usr/bin/bentopack-cli")
#endif
	};

	for (const FString& Cand : Candidates)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Cand);
		if (IFileManager::Get().FileExists(*FullPath))
		{
			return FullPath;
		}
	}

	return TEXT("");
}

FString FBentoCliBridge::FindGuiPath()
{
	FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("BENTOPACK_GUI"));
	if (!EnvPath.IsEmpty() && IFileManager::Get().FileExists(*EnvPath))
	{
		return EnvPath;
	}

	TArray<FString> Candidates = {
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/BentoPack/bentopack")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("../build/bin/bentopack")),
#if PLATFORM_WINDOWS
		TEXT("C:/Program Files/BentoPack/bentopack.exe"),
		TEXT("C:/BentoPack/bin/bentopack.exe")
#else
		TEXT("/usr/local/bin/bentopack"),
		TEXT("/usr/bin/bentopack")
#endif
	};

	for (const FString& Cand : Candidates)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Cand);
		if (IFileManager::Get().FileExists(*FullPath))
		{
			return FullPath;
		}
	}

	return TEXT("");
}

bool FBentoCliBridge::OpenInBentoPack(const FString& TargetFilePath)
{
	FString GuiExe = FindGuiPath();
	if (GuiExe.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BentoPack] Desktop executable not found. Please ensure BENTOPACK_GUI is set."));
		return false;
	}

	FString Args = FString::Printf(TEXT("\"%s\""), *TargetFilePath);
	FProcHandle Proc = FPlatformProcess::CreateProc(*GuiExe, *Args, true, false, false, nullptr, 0, nullptr, nullptr);
	return Proc.IsValid();
}

bool FBentoCliBridge::RunCliCommand(const FString& Arguments, FString& OutStdOutput, FString& OutStdError)
{
	FString CliExe = FindCliPath();
	if (CliExe.IsEmpty())
	{
		OutStdError = TEXT("bentopack-cli executable not found.");
		return false;
	}

	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	FPlatformProcess::CreatePipe(PipeRead, PipeWrite);

	FProcHandle Proc = FPlatformProcess::CreateProc(*CliExe, *Arguments, false, true, true, nullptr, 0, nullptr, PipeWrite);
	if (!Proc.IsValid())
	{
		OutStdError = TEXT("Failed to launch bentopack-cli process.");
		FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
		return false;
	}

	FPlatformProcess::WaitForProc(Proc);

	OutStdOutput = FPlatformProcess::ReadPipe(PipeRead);
	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);

	int32 ExitCode = 0;
	FPlatformProcess::GetProcReturnCode(Proc, &ExitCode);
	return ExitCode == 0;
}

bool FBentoCliBridge::StartWatchDaemon(const FString& Arguments, int32& OutProcessId, FString& OutError)
{
	StopWatchDaemon();

	FString CliExe = FindCliPath();
	if (CliExe.IsEmpty())
	{
		OutError = TEXT("bentopack-cli executable not found.");
		return false;
	}

	uint32 ProcessID = 0;
	WatchProcessHandle = FPlatformProcess::CreateProc(*CliExe, *Arguments, true, false, false, &ProcessID, 0, nullptr, nullptr);
	if (WatchProcessHandle.IsValid())
	{
		WatchProcessId = (int32)ProcessID;
		OutProcessId = WatchProcessId;
		return true;
	}

	OutError = TEXT("Failed to start background daemon.");
	return false;
}

void FBentoCliBridge::StopWatchDaemon()
{
	if (WatchProcessHandle.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(WatchProcessHandle))
		{
			FPlatformProcess::TerminateProc(WatchProcessHandle);
		}
		FPlatformProcess::CloseProc(WatchProcessHandle);
		WatchProcessHandle.Reset();
		WatchProcessId = -1;
	}
}

bool FBentoCliBridge::IsWatchActive()
{
	return WatchProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(WatchProcessHandle);
}

int32 FBentoCliBridge::GetWatchPid()
{
	return IsWatchActive() ? WatchProcessId : -1;
}
