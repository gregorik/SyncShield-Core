// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "SyncShieldSourceControlStatus.h"
#include "SyncShieldCommands.h"
#include "SyncShieldStatusPresenter.h"

class UPackage;

class SSyncShieldToolbar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSyncShieldToolbar)
		: _DisableLiveUpdates(false)
	{}
		SLATE_ARGUMENT(bool, DisableLiveUpdates)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	EActiveTimerReturnType UpdateState(double InCurrentTime, float InDeltaTime);

	TSharedRef<SWidget> BuildMenu();
	const FSlateBrush* GetIcon() const;
	FText GetLabel() const;
	FSlateColor GetColor() const;
	FText GetTooltip() const;
	FText GetAutoFetchIntervalLabel() const;
	FText GetProviderLabel() const;
	FString GetContextLabel(const FSyncShieldSourceControlStatus& Status) const;
	// Gathers the editor-side counts the presenter needs; the presenter itself
	// reads no widget state.
	FSyncShieldEditorState GetEditorState() const;
	static FString GetSubsystemSummary();
	bool IsStatusDegraded(const FSyncShieldSourceControlStatus& Status) const;

	void ExecuteSaveAll();
	void ExecuteSaveBlueprints();
	void ExecuteSaveCurrentLevel();
	void ExecuteRefresh();
	void ExecuteGitFetch();
	void ExecuteGitPullRebase();
	void ExecuteGitPush();
	void ExecutePlasticUpdate();
	void ExecuteShowStatus();
	void ExecuteCheckIn();
	void ToggleAutoFetch();

	bool CanExecuteGitCommand() const;
	bool CanExecuteGitPull() const;
	bool CanExecuteGitPush() const;
	bool CanExecutePlasticUpdate() const;
	bool CanExecuteCheckIn() const;
	bool IsAutoFetchEnabled() const;
	bool IsGitProvider() const;
	bool IsPlasticProvider() const;

	void UpdateUnsavedState();
	void RequestSourceControlStatusUpdate();
	void StartSourceControlStatusUpdate();
	// These run on worker threads and therefore must not touch widget state.
	FString BuildStatusSummary(const FSyncShieldSourceControlStatus& Status) const;
	FSyncShieldSourceControlStatus GetStatusSnapshot() const;
	ESyncShieldProvider GetPreferredProvider() const;
	void MaybeNotifyStatusChange();
	FString GetStatusChangeKey() const;

	// Take the operation rather than raw arguments: the widget owns notification
	// policy, FSyncShieldGitCommands owns the process invocation.
	void RunGitCommandAsync(FSyncShieldGitOperation Operation, const FText& SuccessMessage, const FText& FailureMessage, bool bRefreshAfter, bool bSilentSuccess = false);
	void RunPlasticCommandAsync(FSyncShieldGitOperation Operation, const FText& SuccessMessage, const FText& FailureMessage, bool bRefreshAfter, bool bSilentSuccess = false);

	void Notify(const FText& Message, bool bSuccess) const;

	FSyncShieldSourceControlStatus SourceControlStatus;
	bool bHasUnsavedAssets = false;
	int32 UnsavedAssetCount = 0;
	int32 LockedByOtherCount = 0;
	int32 NeedsCheckoutCount = 0;
	FString SampleUnsavedPackage;
	FString LastStatusLabel;
	bool bDisableLiveUpdates = false;

	double LastDirtyCheckSeconds = 0.0;
	double LastSourceControlCheckSeconds = 0.0;
	double LastAutoFetchSeconds = 0.0;
	double LastStatusToastSeconds = 0.0;

	TAtomic<bool> bStatusUpdateInFlight = false;
	bool bHasSeenStatusLabel = false;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FSyncShieldToolbarTestAccessor;
#endif
};
