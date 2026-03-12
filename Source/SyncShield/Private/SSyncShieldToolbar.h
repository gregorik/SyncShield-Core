// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

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
	enum class ESourceControlProvider : uint8
	{
		None,
		Git,
		Plastic
	};

	struct FSourceControlStatus
	{
		ESourceControlProvider Provider = ESourceControlProvider::None;
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
	};

	EActiveTimerReturnType UpdateState(double InCurrentTime, float InDeltaTime);

	TSharedRef<SWidget> BuildMenu();
	const FSlateBrush* GetIcon() const;
	FText GetLabel() const;
	FSlateColor GetColor() const;
	FText GetTooltip() const;
	FText GetAutoFetchIntervalLabel() const;
	FText GetProviderLabel() const;
	FString GetContextLabel(const FSourceControlStatus& Status) const;
	void AppendUnsavedDetails(FString& InOutText) const;
	bool IsStatusDegraded(const FSourceControlStatus& Status) const;

	void ExecuteSaveAll();
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
	bool TryPopulateGitStatus(const FString& ProjectDir, FSourceControlStatus& OutStatus, FString& OutError) const;
	bool TryPopulatePlasticStatus(const FString& ProjectDir, FSourceControlStatus& OutStatus, FString& OutError) const;
	void ParseGitStatusOutput(const FString& Output, FSourceControlStatus& Status) const;
	void ParsePlasticStatusOutput(const FString& Output, FSourceControlStatus& Status) const;
	FString BuildStatusSummary(const FSourceControlStatus& Status) const;
	FSourceControlStatus GetStatusSnapshot() const;
	ESourceControlProvider GetPreferredProvider() const;
	void MaybeNotifyStatusChange();

	bool RunCommandWithTimeout(const FString& Command, const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode, float TimeoutSeconds) const;
	bool RunGit(const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode) const;
	FString GetGitExecutable() const;
	bool RunPlastic(const FString& Args, const FString& WorkingDir, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode) const;
	FString GetPlasticExecutable() const;
	void RunGitCommandAsync(const FString& Args, const FText& SuccessMessage, const FText& FailureMessage, bool bRefreshAfter, bool bSilentSuccess = false);
	void RunPlasticCommandAsync(const FString& Args, const FText& SuccessMessage, const FText& FailureMessage, bool bRefreshAfter, bool bSilentSuccess = false);

	void Notify(const FText& Message, bool bSuccess) const;

	FSourceControlStatus SourceControlStatus;
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

