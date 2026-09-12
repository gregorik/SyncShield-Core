// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "SyncShieldProcess.h"

#include "SyncShieldSettings.h"

#include "HAL/PlatformProcess.h"
#include "Misc/MonitoredProcess.h"

// Named rather than anonymous: this module builds as a unity blob, so an
// anonymous-namespace helper duplicated across two .cpp files is a redefinition.
namespace SyncShieldProcessPrivate
{
	FString TrimCopy(const FString& InText)
	{
		FString OutText = InText;
		OutText.TrimStartAndEndInline();
		return OutText;
	}
}

bool FSyncShieldProcess::Run(const FString& Command, const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode, float TimeoutSeconds)
{
	// FMonitoredProcess merges stdout and stderr into a single stream.
	// We route the combined output to OutStdOut, and only populate OutStdErr on failure.
	//
	// SharedOutput is declared before MonitoredProcess so that it outlives it. The
	// monitoring thread can still deliver output while ~FMonitoredProcess joins it,
	// so a buffer declared after the process would be written to after destruction.
	struct FSharedOutput
	{
		FCriticalSection Guard;
		FString Text;
	};
	FSharedOutput SharedOutput;

	FMonitoredProcess MonitoredProcess(Command, Args, WorkingDir, true);
	MonitoredProcess.OnOutput().BindLambda([&SharedOutput](const FString& InOutput)
	{
		FScopeLock Lock(&SharedOutput.Guard);
		SharedOutput.Text += InOutput;
		SharedOutput.Text += TEXT("\n");
	});

	if (!MonitoredProcess.Launch())
	{
		OutStdOut.Reset();
		OutStdErr = FString::Printf(
			TEXT("[SyncShield] Could not launch '%s'. Check that it is installed and reachable on PATH, or set an explicit path in Editor Preferences > Plugins > SyncShield."),
			*Command);
		OutExitCode = -1;
		return false;
	}

	const double StartTime = FPlatformTime::Seconds();
	bool bTimedOut = false;

	while (MonitoredProcess.Update())
	{
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			MonitoredProcess.Cancel(true);
			bTimedOut = true;
			break;
		}
		FPlatformProcess::Sleep(0.01f);
	}

	// Cancel() only raises a flag; the monitoring thread is what terminates the child
	// and clears the running state. GetReturnCode() asserts unless the process has
	// actually stopped, so wait for that rather than racing the check.
	if (bTimedOut)
	{
		const double CancelDeadline = FPlatformTime::Seconds() + 5.0;
		while (MonitoredProcess.Update() && FPlatformTime::Seconds() < CancelDeadline)
		{
			FPlatformProcess::Sleep(0.01f);
		}

		if (MonitoredProcess.Update())
		{
			// Still running: report the timeout without reading the return code.
			FScopeLock Lock(&SharedOutput.Guard);
			OutStdOut = SharedOutput.Text;
			OutStdErr = SharedOutput.Text + TEXT("\n[SyncShield] Process timed out and could not be terminated.");
			OutExitCode = -1;
			return false;
		}
	}

	OutExitCode = MonitoredProcess.GetReturnCode();

	{
		FScopeLock Lock(&SharedOutput.Guard);
		// FMonitoredProcess only forwards complete lines to the output delegate. Any
		// trailing text without a newline stays in its internal buffer, so collect it
		// here or the last line of output is silently dropped.
		const FString& Remainder = MonitoredProcess.GetFullOutputWithoutDelegate();
		if (!Remainder.IsEmpty())
		{
			SharedOutput.Text += Remainder;
		}
		OutStdOut = SharedOutput.Text;
	}

	OutStdErr = (OutExitCode != 0) ? OutStdOut : FString();

	if (bTimedOut)
	{
		OutStdErr += TEXT("\n[SyncShield] Process timed out.");
		OutExitCode = -1;
		return false;
	}

	return true;
}

bool FSyncShieldProcess::RunGit(const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode)
{
	const FString GitExe = GetGitExecutable();
	const FString HeadlessArgs = TEXT("--no-optional-locks -c core.terminalPrompt=false -c core.sshCommand=\"ssh -o BatchMode=yes\" ") + Args;
	return Run(GitExe, HeadlessArgs, WorkingDir, OutStdOut, OutStdErr, OutExitCode, 60.0f);
}

FString FSyncShieldProcess::GetGitExecutable()
{
	// An explicit path matters on macOS and Linux, where a GUI-launched editor does
	// not inherit the login shell PATH and so cannot see Homebrew or /usr/local.
	if (const USyncShieldSettings* Settings = GetDefault<USyncShieldSettings>())
	{
		const FString Configured = SyncShieldProcessPrivate::TrimCopy(Settings->GitExecutablePath);
		if (!Configured.IsEmpty())
		{
			return Configured;
		}
	}

#if PLATFORM_WINDOWS
	return TEXT("git.exe");
#else
	return TEXT("git");
#endif
}

bool FSyncShieldProcess::RunPlastic(const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode)
{
	const FString PlasticExe = GetPlasticExecutable();
	return Run(PlasticExe, Args, WorkingDir, OutStdOut, OutStdErr, OutExitCode, 60.0f);
}

FString FSyncShieldProcess::GetPlasticExecutable()
{
	if (const USyncShieldSettings* Settings = GetDefault<USyncShieldSettings>())
	{
		const FString Configured = SyncShieldProcessPrivate::TrimCopy(Settings->PlasticExecutablePath);
		if (!Configured.IsEmpty())
		{
			return Configured;
		}
	}

#if PLATFORM_WINDOWS
	return TEXT("cm.exe");
#else
	return TEXT("cm");
#endif
}
