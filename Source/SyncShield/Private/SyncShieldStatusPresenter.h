// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "SyncShieldSourceControlStatus.h"

/** Editor-side counts the presenter needs but cannot derive from source control status. */
struct FSyncShieldEditorState
{
	bool bHasUnsavedAssets = false;
	int32 UnsavedAssetCount = 0;
	int32 LockedByOtherCount = 0;
	int32 NeedsCheckoutCount = 0;
	FString SampleUnsavedPackage;
};

/**
 * Turns status into what the toolbar shows. Pure: every input is a parameter, so
 * these are testable without constructing a Slate widget. SubsystemSummary is
 * passed in rather than read from the module, which keeps that dependency on the
 * caller side.
 */
struct FSyncShieldStatusPresenter
{
	static const FSlateBrush* GetIcon(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor);
	static FText GetLabel(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor);
	static FSlateColor GetColor(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor);
	static FText GetTooltip(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor, const FString& SubsystemSummary);
	static FString BuildStatusSummary(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor, const FString& SubsystemSummary);
	static FString GetStatusChangeKey(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor);
	static FString GetContextLabel(const FSyncShieldSourceControlStatus& Status);
	static FText GetProviderLabel(const FSyncShieldSourceControlStatus& Status);
	static bool IsStatusDegraded(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor);
};
