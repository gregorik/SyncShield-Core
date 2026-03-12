// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SyncShieldSettings.h"

USyncShieldSettings::USyncShieldSettings()
{
	DirtyCheckIntervalSeconds = 5.0f;
	GitCheckIntervalSeconds = 15.0f;
	bAutoFetch = false;
	AutoFetchIntervalSeconds = 120.0f;
	bToastOnStatusChange = true;
	StatusToastMinIntervalSeconds = 4.0f;
}
