// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/** The outcome of one source control operation, as seen from a worker thread. */
struct FSyncShieldCommandResult
{
	bool bSuccess = false;
	FString ErrorText;
};

/** One source control operation, bound to a working directory and ready to run on the pool. */
using FSyncShieldGitOperation = TFunction<FSyncShieldCommandResult(const FString& WorkingDir)>;

/**
 * Source control operations as pure functions: run a process, interpret the result,
 * return it. No dialogs, no notifications, no widget or editor state, so these are
 * callable from the thread pool and directly testable against a scratch repository.
 */
struct FSyncShieldGitCommands
{
	/** Runs an arbitrary git invocation. The other entry points are named cases of this. */
	static FSyncShieldCommandResult Run(const FString& WorkingDir, const FString& Args);

	static FSyncShieldCommandResult Fetch(const FString& WorkingDir);
	static FSyncShieldCommandResult PullRebase(const FString& WorkingDir);
	static FSyncShieldCommandResult Push(const FString& WorkingDir);

};

struct FSyncShieldPlasticCommands
{
	static FSyncShieldCommandResult Run(const FString& WorkingDir, const FString& Args);
	static FSyncShieldCommandResult UpdateWorkspace(const FString& WorkingDir);
};
