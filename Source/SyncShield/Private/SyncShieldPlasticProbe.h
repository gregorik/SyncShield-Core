// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SyncShieldSourceControlStatus.h"

/**
 * Probes a Plastic SCM workspace and parses cm's output. Runs on the thread pool,
 * so nothing here may touch widget or editor state.
 */
struct FSyncShieldPlasticProbe
{
	static bool Populate(const FString& ProjectDir, FSyncShieldSourceControlStatus& OutStatus, FString& OutError);
	static void ParseStatusOutput(const FString& Output, FSyncShieldSourceControlStatus& Status);
};
