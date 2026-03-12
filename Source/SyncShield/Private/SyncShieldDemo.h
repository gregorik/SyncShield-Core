// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

class FSyncShieldDemo
{
public:
	static void RegisterCommands();
	static void UnregisterCommands();

private:
	// Generates 1,000 actors in a spiral and dirties them to test UI performance
	static void GenerateStressScene();
};

