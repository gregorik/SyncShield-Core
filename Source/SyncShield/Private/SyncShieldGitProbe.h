// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SyncShieldSourceControlStatus.h"

/**
 * Probes a git working copy and parses git's output. Runs on the thread pool, so
 * nothing here may touch widget or editor state.
 */
struct FSyncShieldGitProbe
{
	static bool Populate(const FString& ProjectDir, FSyncShieldSourceControlStatus& OutStatus, FString& OutError);
	static void ParseStatusOutput(const FString& Output, FSyncShieldSourceControlStatus& Status);
	static void ParseLfsLocksJson(const FString& JsonText, TArray<FString>& OutLockedFiles);
	static bool DetectLfsFromGitattributes(const FString& GitattributesContent);
};
