// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * Launches external source control clients. Everything here is callable from any
 * thread and reads no widget or editor state, which is what lets the probes run
 * on the thread pool.
 */
struct FSyncShieldProcess
{
	static bool Run(const FString& Command, const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode, float TimeoutSeconds);
	static bool RunGit(const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode);
	static FString GetGitExecutable();
	static bool RunPlastic(const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode);
	static FString GetPlasticExecutable();
};
