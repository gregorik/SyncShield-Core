// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/** Which source control system SyncShield detected for the current project. */
enum class ESyncShieldProvider : uint8
{
	None,
	Git,
	Plastic
};

/**
 * A snapshot of source control state. Produced on a worker thread by the probes and
 * consumed on the game thread, so it holds only owned values and no references.
 */
struct FSyncShieldSourceControlStatus
{
	ESyncShieldProvider Provider = ESyncShieldProvider::None;
	bool bClientAvailable = false;
	bool bRepo = false;
	bool bAuthRequired = false;
	bool bStatusError = false;
	bool bHasUpstream = false;
	bool bHasConflicts = false;
	int32 Ahead = 0;
	int32 Behind = 0;
	int32 Staged = 0;
	int32 Unstaged = 0;
	int32 Untracked = 0;
	FString Branch;
	FString RepoRoot;
	FString WorkspaceName;
	FString LastError;
	FDateTime LastUpdateUtc;
	bool bLfsDetected = false;
	int32 LfsLockCount = 0;
	TArray<FString> LfsLockedFiles;
	bool bGitattributesConfigured = true;
};
