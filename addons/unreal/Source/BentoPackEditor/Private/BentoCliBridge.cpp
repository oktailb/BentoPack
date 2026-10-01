#include "BentoCliBridge.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

const FString FBentoCliBridge::ReleasesUrl = TEXT("https://github.com/oktailb/BentoPack/releases");
FProcHandle FBentoCliBridge::WatchProcessHandle;
int32 FBentoCliBridge::WatchProcessId = -1;

FString FBentoCliBridge::FindCliPath()
{
	const FString ExeName = PLATFORM_WINDOWS ? TEXT("bentopack-cli.exe") : TEXT("bentopack-cli");

	// 1. Check environment variable override
	FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("BENTOPACK_CLI"));
	if (!EnvPath.IsEmpty() && IFileManager::Get().FileExists(*EnvPath))
	{
		return EnvPath;
	}

	// 2. Official System and User PATH
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	if (!PathEnv.IsEmpty())
	{
		TArray<FString> PathDirs;
#if PLATFORM_WINDOWS
		PathEnv.ParseIntoArray(PathDirs, TEXT(";"), true);
#else
		PathEnv.ParseIntoArray(PathDirs, TEXT(":"), true);
#endif
		for (const FString& Dir : PathDirs)
		{
			FString Cand = FPaths::Combine(Dir.TrimStartAndEnd(), ExeName);
			if (IFileManager::Get().FileExists(*Cand))
			{
				return Cand;
			}
		}
	}

	// 3. Official OS Installation Directories
	TArray<FString> StandardPaths;
#if PLATFORM_WINDOWS
	FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
	FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));

	if (!ProgramFiles.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack Studio/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack Studio"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack"), ExeName));
	}
	if (!ProgramFilesX86.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(ProgramFilesX86, TEXT("BentoPack Studio/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFilesX86, TEXT("BentoPack/bin"), ExeName));
	}
	if (!LocalAppData.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(LocalAppData, TEXT("Programs/BentoPack Studio/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(LocalAppData, TEXT("Programs/BentoPack/bin"), ExeName));
	}
#elif PLATFORM_MAC
	StandardPaths.Add(TEXT("/Applications/BentoPack Studio.app/Contents/MacOS/bentopack-cli"));
	StandardPaths.Add(TEXT("/Applications/BentoPack.app/Contents/MacOS/bentopack-cli"));
	StandardPaths.Add(TEXT("/usr/local/bin/bentopack-cli"));
	StandardPaths.Add(TEXT("/opt/homebrew/bin/bentopack-cli"));
#else // Linux
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	StandardPaths.Add(TEXT("/usr/bin/bentopack-cli"));
	StandardPaths.Add(TEXT("/usr/local/bin/bentopack-cli"));
	if (!Home.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(Home, TEXT(".local/bin/bentopack-cli")));
	}
	StandardPaths.Add(TEXT("/opt/bentopack/bin/bentopack-cli"));
#endif

	for (const FString& Cand : StandardPaths)
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
	const FString ExeName = PLATFORM_WINDOWS ? TEXT("bentopack.exe") : TEXT("bentopack");

	// 1. Check environment variable override
	FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("BENTOPACK_GUI"));
	if (!EnvPath.IsEmpty() && IFileManager::Get().FileExists(*EnvPath))
	{
		return EnvPath;
	}

	// 2. Official System and User PATH
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	if (!PathEnv.IsEmpty())
	{
		TArray<FString> PathDirs;
#if PLATFORM_WINDOWS
		PathEnv.ParseIntoArray(PathDirs, TEXT(";"), true);
#else
		PathEnv.ParseIntoArray(PathDirs, TEXT(":"), true);
#endif
		for (const FString& Dir : PathDirs)
		{
			FString Cand = FPaths::Combine(Dir.TrimStartAndEnd(), ExeName);
			if (IFileManager::Get().FileExists(*Cand))
			{
				return Cand;
			}
		}
	}

	// 3. Official OS Installation Directories
	TArray<FString> StandardPaths;
#if PLATFORM_WINDOWS
	FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
	FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));

	if (!ProgramFiles.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack Studio/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack Studio"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFiles, TEXT("BentoPack"), ExeName));
	}
	if (!ProgramFilesX86.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(ProgramFilesX86, TEXT("BentoPack Studio/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(ProgramFilesX86, TEXT("BentoPack/bin"), ExeName));
	}
	if (!LocalAppData.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(LocalAppData, TEXT("Programs/BentoPack Studio/bin"), ExeName));
		StandardPaths.Add(FPaths::Combine(LocalAppData, TEXT("Programs/BentoPack/bin"), ExeName));
	}
#elif PLATFORM_MAC
	StandardPaths.Add(TEXT("/Applications/BentoPack Studio.app/Contents/MacOS/bentopack"));
	StandardPaths.Add(TEXT("/Applications/BentoPack.app/Contents/MacOS/bentopack"));
	StandardPaths.Add(TEXT("/usr/local/bin/bentopack"));
	StandardPaths.Add(TEXT("/opt/homebrew/bin/bentopack"));
#else // Linux
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	StandardPaths.Add(TEXT("/usr/bin/bentopack"));
	StandardPaths.Add(TEXT("/usr/local/bin/bentopack"));
	if (!Home.IsEmpty())
	{
		StandardPaths.Add(FPaths::Combine(Home, TEXT(".local/bin/bentopack")));
	}
	StandardPaths.Add(TEXT("/opt/bentopack/bin/bentopack"));
#endif

	for (const FString& Cand : StandardPaths)
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
