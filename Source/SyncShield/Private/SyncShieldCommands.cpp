// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "SyncShieldCommands.h"

#include "SyncShieldProcess.h"

// Named rather than anonymous: this module builds as a unity blob.
namespace SyncShieldCommandsPrivate
{
	FString TrimCopy(const FString& InText)
	{
		FString OutText = InText;
		OutText.TrimStartAndEndInline();
		return OutText;
	}
}

FSyncShieldCommandResult FSyncShieldGitCommands::Run(const FString& WorkingDir, const FString& Args)
{
	FString StdOut;
	FString StdErr;
	int32 ExitCode = 0;
	const bool bLaunched = FSyncShieldProcess::RunGit(Args, WorkingDir, StdOut, StdErr, ExitCode);

	FSyncShieldCommandResult Result;
	Result.bSuccess = bLaunched && ExitCode == 0;
	Result.ErrorText = SyncShieldCommandsPrivate::TrimCopy(StdErr);
	return Result;
}

FSyncShieldCommandResult FSyncShieldGitCommands::Fetch(const FString& WorkingDir)
{
	return Run(WorkingDir, TEXT("fetch --prune"));
}

FSyncShieldCommandResult FSyncShieldGitCommands::PullRebase(const FString& WorkingDir)
{
	return Run(WorkingDir, TEXT("pull --rebase"));
}

FSyncShieldCommandResult FSyncShieldGitCommands::Push(const FString& WorkingDir)
{
	return Run(WorkingDir, TEXT("push"));
}

FSyncShieldCommandResult FSyncShieldPlasticCommands::Run(const FString& WorkingDir, const FString& Args)
{
	FString StdOut;
	FString StdErr;
	int32 ExitCode = 0;
	const bool bLaunched = FSyncShieldProcess::RunPlastic(Args, WorkingDir, StdOut, StdErr, ExitCode);

	FSyncShieldCommandResult Result;
	Result.bSuccess = bLaunched && ExitCode == 0;
	Result.ErrorText = SyncShieldCommandsPrivate::TrimCopy(StdErr);
	return Result;
}

FSyncShieldCommandResult FSyncShieldPlasticCommands::UpdateWorkspace(const FString& WorkingDir)
{
	return Run(WorkingDir, TEXT("update"));
}
