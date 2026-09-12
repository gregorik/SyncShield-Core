// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSyncShieldToolbar.h"

#include "SyncShieldModule.h"
#include "SyncShieldSettings.h"

#include "Async/Async.h"
#include "Editor/UnrealEdEngine.h"
#include "FileHelpers.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlWindows.h"
#include "SyncShieldStyle.h"
#include "SyncShieldProcess.h"
#include "SyncShieldGitProbe.h"
#include "SyncShieldPlasticProbe.h"
#include "SyncShieldStatusPresenter.h"
#include "SyncShieldAsyncRunner.h"
#include "SyncShieldCommands.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Styling/AppStyle.h"
#include "UnrealEdGlobals.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "SyncShieldToolbar"

void SSyncShieldToolbar::Construct(const FArguments& InArgs)
{
	bDisableLiveUpdates = InArgs._DisableLiveUpdates;
	bHasUnsavedAssets = false;
	UnsavedAssetCount = 0;
	SampleUnsavedPackage.Reset();
	SourceControlStatus = FSyncShieldSourceControlStatus();
	bStatusUpdateInFlight = false;
	LastAutoFetchSeconds = FPlatformTime::Seconds();
	LastStatusLabel.Reset();
	LastStatusToastSeconds = 0.0;
	bHasSeenStatusLabel = false;

	ChildSlot
	[
		SNew(SComboButton)
			.OnGetMenuContent(this, &SSyncShieldToolbar::BuildMenu)
			.ContentPadding(FMargin(6.0f, 2.0f))
			.ToolTipText(this, &SSyncShieldToolbar::GetTooltip)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SImage)
						.Image(this, &SSyncShieldToolbar::GetIcon)
						.ColorAndOpacity(this, &SSyncShieldToolbar::GetColor)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(this, &SSyncShieldToolbar::GetLabel)
						.Font(FAppStyle::GetFontStyle("BoldFont"))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
			]
	];

	if (!bDisableLiveUpdates)
	{
		RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SSyncShieldToolbar::UpdateState));
		UpdateUnsavedState();
		RequestSourceControlStatusUpdate();
	}
}

EActiveTimerReturnType SSyncShieldToolbar::UpdateState(double InCurrentTime, float InDeltaTime)
{
	if (GEditor && (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != nullptr))
	{
		return EActiveTimerReturnType::Continue;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	const USyncShieldSettings* Settings = GetDefault<USyncShieldSettings>();
	const double DirtyInterval = Settings ? FMath::Max(0.1, (double)Settings->DirtyCheckIntervalSeconds) : 1.0;
	const double GitInterval = Settings ? FMath::Max(1.0, (double)Settings->GitCheckIntervalSeconds) : 5.0;

	if (NowSeconds - LastDirtyCheckSeconds >= DirtyInterval)
	{
		UpdateUnsavedState();
		LastDirtyCheckSeconds = NowSeconds;
	}

	if (NowSeconds - LastSourceControlCheckSeconds >= GitInterval)
	{
		RequestSourceControlStatusUpdate();
		LastSourceControlCheckSeconds = NowSeconds;
	}

	if (Settings && Settings->bAutoFetch && IsGitProvider())
	{
		const double AutoFetchInterval = FMath::Max(10.0, (double)Settings->AutoFetchIntervalSeconds);
		const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
		const bool bCanAutoFetch = Status.bClientAvailable && Status.bRepo && !Status.bStatusError && !bStatusUpdateInFlight.Load();

		if (bCanAutoFetch && (NowSeconds - LastAutoFetchSeconds >= AutoFetchInterval))
		{
			RunGitCommandAsync(&FSyncShieldGitCommands::Fetch, LOCTEXT("AutoFetchSuccess", "Auto fetch completed."), LOCTEXT("AutoFetchFail", "Auto fetch failed."), true, true);
			LastAutoFetchSeconds = NowSeconds;
		}
	}

	return EActiveTimerReturnType::Continue;
}

void SSyncShieldToolbar::UpdateUnsavedState()
{
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyPackages(DirtyPackages);

	bHasUnsavedAssets = DirtyPackages.Num() > 0;
	UnsavedAssetCount = DirtyPackages.Num();
	SampleUnsavedPackage = bHasUnsavedAssets ? DirtyPackages[0]->GetName() : FString();

	LockedByOtherCount = 0;
	NeedsCheckoutCount = 0;

	if (bHasUnsavedAssets && ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
		for (UPackage* Package : DirtyPackages)
		{
			FSourceControlStatePtr State = Provider.GetState(Package, EStateCacheUsage::Use);
			if (State.IsValid())
			{
				if (State->IsCheckedOutOther())
				{
					LockedByOtherCount++;
				}
				else if (!State->IsCheckedOut() && !State->IsAdded() && State->CanCheckout())
				{
					NeedsCheckoutCount++;
				}
			}
		}
	}

	MaybeNotifyStatusChange();
}

void SSyncShieldToolbar::RequestSourceControlStatusUpdate()
{
	if (bStatusUpdateInFlight.Load())
	{
		return;
	}

	StartSourceControlStatusUpdate();
}

void SSyncShieldToolbar::StartSourceControlStatusUpdate()
{
	bStatusUpdateInFlight = true;

	// Resolve the provider here, on the game thread. ISourceControlModule::Get() loads
	// a module and reads editor state, neither of which is safe from the thread pool.
	const ESyncShieldProvider PreferredProvider = GetPreferredProvider();
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Only a weak pointer crosses the thread boundary. Pinning the widget on the pool
	// would let the final reference drop there, destroying an SWidget off the game
	// thread when the toolbar is rebuilt or the editor shuts down mid-command.
	// Capturing `this` in the continuation is safe: RunForOwner invokes it only
	// after confirming, on the game thread, that the owner is still alive.
	SyncShieldAsync::RunForOwner<FSyncShieldSourceControlStatus>(AsShared(),
		[PreferredProvider, ProjectDir]() -> FSyncShieldSourceControlStatus
	{
		FSyncShieldSourceControlStatus NewStatus;
		NewStatus.LastUpdateUtc = FDateTime::UtcNow();

		FString GitError;
		FString PlasticError;
		FSyncShieldSourceControlStatus GitStatus;
		FSyncShieldSourceControlStatus PlasticStatus;
		bool bGitRepoFound = false;
		bool bPlasticRepoFound = false;

		if (PreferredProvider == ESyncShieldProvider::Plastic)
		{
			bPlasticRepoFound = FSyncShieldPlasticProbe::Populate(ProjectDir, PlasticStatus, PlasticError);
			NewStatus = PlasticStatus;
		}
		else if (PreferredProvider == ESyncShieldProvider::Git)
		{
			bGitRepoFound = FSyncShieldGitProbe::Populate(ProjectDir, GitStatus, GitError);
			NewStatus = GitStatus;
		}
		else
		{
			bGitRepoFound = FSyncShieldGitProbe::Populate(ProjectDir, GitStatus, GitError);
			const bool bGitClientAvailable = GitStatus.bClientAvailable;
			bool bPlasticClientAvailable = false;

			if (!bGitRepoFound)
			{
				bPlasticRepoFound = FSyncShieldPlasticProbe::Populate(ProjectDir, PlasticStatus, PlasticError);
				bPlasticClientAvailable = PlasticStatus.bClientAvailable;
			}

			if (bGitRepoFound)
			{
				NewStatus = GitStatus;
			}
			else if (bPlasticRepoFound)
			{
				NewStatus = PlasticStatus;
			}
			else
			{
				NewStatus = FSyncShieldSourceControlStatus();
				NewStatus.Provider = ESyncShieldProvider::None;
				NewStatus.bClientAvailable = bGitClientAvailable || bPlasticClientAvailable;
				NewStatus.bRepo = false;

				TArray<FString> Errors;
				if (!GitError.IsEmpty())
				{
					Errors.Add(FString::Printf(TEXT("Git: %s"), *GitError));
				}
				if (!PlasticError.IsEmpty())
				{
					Errors.Add(FString::Printf(TEXT("Plastic SCM: %s"), *PlasticError));
				}
				NewStatus.LastError = Errors.Num() > 0 ? FString::Join(Errors, TEXT("\n")) : FString();
			}
		}

		return NewStatus;
	},
		[this](const FSyncShieldSourceControlStatus& NewStatus)
	{
		SourceControlStatus = NewStatus;
		bStatusUpdateInFlight = false;
		MaybeNotifyStatusChange();
	});
}

FSyncShieldSourceControlStatus SSyncShieldToolbar::GetStatusSnapshot() const
{
	return SourceControlStatus;
}

ESyncShieldProvider SSyncShieldToolbar::GetPreferredProvider() const
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		const FString ProviderName = ISourceControlModule::Get().GetProvider().GetName().ToString().ToLower();
		if (ProviderName.Contains(TEXT("plastic")) || ProviderName.Contains(TEXT("unity")))
		{
			return ESyncShieldProvider::Plastic;
		}
		if (ProviderName.Contains(TEXT("git")))
		{
			return ESyncShieldProvider::Git;
		}
	}

	return ESyncShieldProvider::None;
}

TSharedRef<SWidget> SSyncShieldToolbar::BuildMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("SyncShieldActions", LOCTEXT("SyncShieldActions", "SyncShield"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveAll", "Save All"),
		LOCTEXT("SaveAllTooltip", "Save all dirty assets and maps."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
		FUIAction(FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteSaveAll))
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveBlueprints", "Save Blueprints"),
		LOCTEXT("SaveBlueprintsTooltip", "Save only dirty Blueprint assets."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
		FUIAction(FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteSaveBlueprints))
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveCurrentLevel", "Save Current Level"),
		LOCTEXT("SaveCurrentLevelTooltip", "Save the current level and its dirty external packages."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
		FUIAction(FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteSaveCurrentLevel))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CheckIn", "Submit Content"),
		LOCTEXT("CheckInTooltip", "Open the source control submit window to check in changes."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteCheckIn),
			FCanExecuteAction::CreateSP(this, &SSyncShieldToolbar::CanExecuteCheckIn)
		)
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Refresh", "Refresh Status"),
		LOCTEXT("RefreshTooltip", "Re-scan dirty assets and refresh source control status."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
		FUIAction(FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteRefresh))
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowStatus", "Show Status Details"),
		LOCTEXT("ShowStatusTooltip", "Show a detailed SyncShield status summary."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"),
		FUIAction(FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteShowStatus))
	);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("SourceControlActions", GetProviderLabel());
	if (IsGitProvider())
	{

		MenuBuilder.AddMenuEntry(
			LOCTEXT("GitFetch", "Fetch"),
			LOCTEXT("GitFetchTooltip", "Fetch latest refs from remote without changing local files."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteGitFetch),
				FCanExecuteAction::CreateSP(this, &SSyncShieldToolbar::CanExecuteGitCommand)
			)
		);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutoFetchToggle", "Auto Fetch"),
			LOCTEXT("AutoFetchToggleTooltip", "Automatically fetch from remote at a configurable interval."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ToggleAutoFetch),
				FCanExecuteAction::CreateSP(this, &SSyncShieldToolbar::CanExecuteGitCommand),
				FIsActionChecked::CreateSP(this, &SSyncShieldToolbar::IsAutoFetchEnabled)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(16.0f, 4.0f))
			[
				SNew(STextBlock)
					.Text(this, &SSyncShieldToolbar::GetAutoFetchIntervalLabel)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
			],
			FText::GetEmpty(),
			true
		);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("GitPull", "Pull (Rebase)"),
			LOCTEXT("GitPullTooltip", "Pull incoming commits from upstream using rebase. Requires no tracked changes, unresolved conflicts, or unsaved assets."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteGitPullRebase),
				FCanExecuteAction::CreateSP(this, &SSyncShieldToolbar::CanExecuteGitPull)
			)
		);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("GitPush", "Push"),
			LOCTEXT("GitPushTooltip", "Push local commits to upstream. Only enabled when the working tree is clean."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecuteGitPush),
				FCanExecuteAction::CreateSP(this, &SSyncShieldToolbar::CanExecuteGitPush)
			)
		);

	}
	else if (IsPlasticProvider())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PlasticUpdate", "Update Workspace"),
			LOCTEXT("PlasticUpdateTooltip", "Update workspace to the latest changeset. Only enabled when the working tree is clean."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSyncShieldToolbar::ExecutePlasticUpdate),
				FCanExecuteAction::CreateSP(this, &SSyncShieldToolbar::CanExecutePlasticUpdate)
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSyncShieldToolbar::ExecuteSaveAll()
{
	if (!FSyncShieldModule::IsAvailable())
	{
		return;
	}

	FString Summary;
	const bool bSuccess = FSyncShieldModule::Get().SaveAllDirtyPackages(Summary);
	Notify(FText::FromString(Summary), bSuccess);
	UpdateUnsavedState();
}

void SSyncShieldToolbar::ExecuteSaveBlueprints()
{
	if (!FSyncShieldModule::IsAvailable())
	{
		return;
	}

	FString Summary;
	const bool bSuccess = FSyncShieldModule::Get().SaveDirtyBlueprintPackages(Summary);
	Notify(FText::FromString(Summary), bSuccess);
	UpdateUnsavedState();
}

void SSyncShieldToolbar::ExecuteSaveCurrentLevel()
{
	if (!FSyncShieldModule::IsAvailable())
	{
		return;
	}

	FString Summary;
	const bool bSuccess = FSyncShieldModule::Get().SaveCurrentLevelPackages(Summary);
	Notify(FText::FromString(Summary), bSuccess);
	UpdateUnsavedState();
}

void SSyncShieldToolbar::ExecuteRefresh()
{
	UpdateUnsavedState();
	RequestSourceControlStatusUpdate();
}

void SSyncShieldToolbar::ExecuteGitFetch()
{
	RunGitCommandAsync(&FSyncShieldGitCommands::Fetch, LOCTEXT("FetchSuccess", "Fetch completed."), LOCTEXT("FetchFail", "Fetch failed."), true);
}

void SSyncShieldToolbar::ExecuteGitPullRebase()
{
	if (!CanExecuteGitPull())
	{
		Notify(LOCTEXT("PullDisabled", "Pull requires an upstream with incoming commits and no tracked changes, unresolved conflicts, or unsaved assets."), false);
		return;
	}

	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("ConfirmPull", "Pull from upstream with rebase? This will update your working tree.")
	);

	if (Result == EAppReturnType::Yes)
	{
		RunGitCommandAsync(&FSyncShieldGitCommands::PullRebase, LOCTEXT("PullSuccess", "Pull completed."), LOCTEXT("PullFail", "Pull failed."), true);
	}
}

void SSyncShieldToolbar::ExecuteGitPush()
{
	if (!CanExecuteGitPush())
	{
		Notify(LOCTEXT("PushDisabled", "Push is disabled until the working tree is clean, ahead, and upstream is set."), false);
		return;
	}

	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("ConfirmPush", "Push local commits to upstream?")
	);

	if (Result != EAppReturnType::Yes)
	{
		return;
	}

	RunGitCommandAsync(&FSyncShieldGitCommands::Push, LOCTEXT("PushSuccess", "Push completed."), LOCTEXT("PushFail", "Push failed."), true);
}

void SSyncShieldToolbar::ExecutePlasticUpdate()
{
	if (!CanExecutePlasticUpdate())
	{
		Notify(LOCTEXT("PlasticUpdateDisabled", "Update is disabled until the workspace is clean, logged in, and there are no unsaved assets."), false);
		return;
	}

	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("ConfirmPlasticUpdate", "Update workspace to the latest changeset?")
	);

	if (Result == EAppReturnType::Yes)
	{
		RunPlasticCommandAsync(&FSyncShieldPlasticCommands::UpdateWorkspace, LOCTEXT("PlasticUpdateSuccess", "Update completed."), LOCTEXT("PlasticUpdateFail", "Update failed."), true);
	}
}

void SSyncShieldToolbar::ExecuteShowStatus()
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	const FString Summary = BuildStatusSummary(Status);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Summary));
}

void SSyncShieldToolbar::ToggleAutoFetch()
{
	USyncShieldSettings* Settings = GetMutableDefault<USyncShieldSettings>();
	if (!Settings)
	{
		return;
	}

	Settings->bAutoFetch = !Settings->bAutoFetch;
	Settings->SaveConfig();
	LastAutoFetchSeconds = FPlatformTime::Seconds();
}

bool SSyncShieldToolbar::CanExecuteGitCommand() const
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	return IsGitProvider() && Status.bClientAvailable && Status.bRepo && !Status.bStatusError;
}

bool SSyncShieldToolbar::CanExecuteGitPull() const
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	// Untracked files alone do not block the action. Git rejects a pull if it
	// would overwrite an untracked file.
	const bool bCleanTree = (Status.Staged + Status.Unstaged) == 0;
	return IsGitProvider() && Status.bClientAvailable && Status.bRepo && !Status.bStatusError && !Status.bHasConflicts && Status.bHasUpstream && Status.Behind > 0 && bCleanTree && !bHasUnsavedAssets;
}

bool SSyncShieldToolbar::CanExecuteGitPush() const
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	const bool bCleanTree = (Status.Staged + Status.Unstaged) == 0;
	return IsGitProvider() && Status.bClientAvailable && Status.bRepo && !Status.bStatusError && !Status.bHasConflicts && Status.bHasUpstream && Status.Ahead > 0 && Status.Behind == 0 && bCleanTree && !bHasUnsavedAssets;
}

bool SSyncShieldToolbar::CanExecutePlasticUpdate() const
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	const bool bCleanTree = (Status.Staged + Status.Unstaged) == 0;
	return IsPlasticProvider() && Status.bClientAvailable && Status.bRepo && !Status.bAuthRequired && !Status.bStatusError && bCleanTree && !bHasUnsavedAssets;
}

void SSyncShieldToolbar::ExecuteCheckIn()
{
	if (!CanExecuteCheckIn()) return;
	TWeakPtr<SSyncShieldToolbar> SelfWeak = StaticCastSharedRef<SSyncShieldToolbar>(AsShared());
	FSourceControlWindows::ChoosePackagesToCheckIn(
		FSourceControlWindowsOnCheckInComplete::CreateLambda(
			[SelfWeak](const FCheckinResultInfo& ResultInfo)
			{
				if (TSharedPtr<SSyncShieldToolbar> Pinned = SelfWeak.Pin())
				{
					Pinned->RequestSourceControlStatusUpdate();
				}
			}));
}

bool SSyncShieldToolbar::CanExecuteCheckIn() const
{
	return ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable();
}

bool SSyncShieldToolbar::IsAutoFetchEnabled() const
{
	const USyncShieldSettings* Settings = GetDefault<USyncShieldSettings>();
	return Settings && Settings->bAutoFetch;
}

bool SSyncShieldToolbar::IsGitProvider() const
{
	return GetStatusSnapshot().Provider == ESyncShieldProvider::Git;
}

bool SSyncShieldToolbar::IsPlasticProvider() const
{
	return GetStatusSnapshot().Provider == ESyncShieldProvider::Plastic;
}

FText SSyncShieldToolbar::GetAutoFetchIntervalLabel() const
{
	const USyncShieldSettings* Settings = GetDefault<USyncShieldSettings>();
	const int32 IntervalSeconds = Settings ? FMath::Max(10, (int32)Settings->AutoFetchIntervalSeconds) : 120;
	const bool bEnabled = Settings && Settings->bAutoFetch;

	if (bEnabled)
	{
		return FText::FromString(FString::Printf(TEXT("Auto fetch interval: %ds"), IntervalSeconds));
	}

	return FText::FromString(FString::Printf(TEXT("Auto fetch interval: %ds (disabled)"), IntervalSeconds));
}

void SSyncShieldToolbar::MaybeNotifyStatusChange()
{
	const USyncShieldSettings* Settings = GetDefault<USyncShieldSettings>();
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	const FString CurrentKey = GetStatusChangeKey();

	if (!Settings || !Settings->bToastOnStatusChange)
	{
		LastStatusLabel = CurrentKey;
		bHasSeenStatusLabel = true;
		return;
	}

	if (!bHasSeenStatusLabel)
	{
		LastStatusLabel = CurrentKey;
		bHasSeenStatusLabel = true;
		return;
	}

	if (CurrentKey != LastStatusLabel)
	{
		const double NowSeconds = FPlatformTime::Seconds();
		const double MinInterval = FMath::Max(0.5, (double)Settings->StatusToastMinIntervalSeconds);

		if (NowSeconds - LastStatusToastSeconds >= MinInterval)
		{
			Notify(FText::Format(LOCTEXT("StatusChanged", "SyncShield: {0}"), GetLabel()), !IsStatusDegraded(Status));
			LastStatusToastSeconds = NowSeconds;
		}

		LastStatusLabel = CurrentKey;
	}
}

void SSyncShieldToolbar::RunGitCommandAsync(FSyncShieldGitOperation Operation, const FText& SuccessMessage, const FText& FailureMessage, bool bRefreshAfter, bool bSilentSuccess)
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	if (!IsGitProvider() || !Status.bClientAvailable || !Status.bRepo || Status.bStatusError)
	{
		Notify(LOCTEXT("GitUnavailable", "Git is not available for this project."), false);
		return;
	}

	const FString WorkingDir = Status.RepoRoot.IsEmpty() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) : Status.RepoRoot;
	// Capturing `this` in the continuation is safe: RunForOwner invokes it only
	// after confirming, on the game thread, that the owner is still alive.
	SyncShieldAsync::RunForOwner<FSyncShieldCommandResult>(AsShared(),
		[WorkingDir, Operation = MoveTemp(Operation)]() -> FSyncShieldCommandResult
	{
		return Operation(WorkingDir);
	},
		[this, SuccessMessage, FailureMessage, bRefreshAfter, bSilentSuccess](const FSyncShieldCommandResult& Outcome)
	{
		const bool bSuccess = Outcome.bSuccess;
		const FString& ErrorText = Outcome.ErrorText;

		if (!(bSuccess && bSilentSuccess))
		{
			Notify(bSuccess ? SuccessMessage : FailureMessage, bSuccess);
		}
		if (!bSuccess && !ErrorText.IsEmpty())
		{
			Notify(FText::FromString(ErrorText.Left(200)), false);
		}

		if (bRefreshAfter)
		{
			RequestSourceControlStatusUpdate();
		}
	});
}

void SSyncShieldToolbar::RunPlasticCommandAsync(FSyncShieldGitOperation Operation, const FText& SuccessMessage, const FText& FailureMessage, bool bRefreshAfter, bool bSilentSuccess)
{
	const FSyncShieldSourceControlStatus Status = GetStatusSnapshot();
	if (!IsPlasticProvider() || !Status.bClientAvailable || !Status.bRepo || Status.bAuthRequired || Status.bStatusError)
	{
		Notify(LOCTEXT("PlasticUnavailable", "Plastic SCM is not available for this project."), false);
		return;
	}

	const FString WorkingDir = Status.RepoRoot.IsEmpty() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) : Status.RepoRoot;
	// Capturing `this` in the continuation is safe: RunForOwner invokes it only
	// after confirming, on the game thread, that the owner is still alive.
	SyncShieldAsync::RunForOwner<FSyncShieldCommandResult>(AsShared(),
		[WorkingDir, Operation = MoveTemp(Operation)]() -> FSyncShieldCommandResult
	{
		return Operation(WorkingDir);
	},
		[this, SuccessMessage, FailureMessage, bRefreshAfter, bSilentSuccess](const FSyncShieldCommandResult& Outcome)
	{
		const bool bSuccess = Outcome.bSuccess;
		const FString& ErrorText = Outcome.ErrorText;

		if (!(bSuccess && bSilentSuccess))
		{
			Notify(bSuccess ? SuccessMessage : FailureMessage, bSuccess);
		}
		if (!bSuccess && !ErrorText.IsEmpty())
		{
			Notify(FText::FromString(ErrorText.Left(200)), false);
		}

		if (bRefreshAfter)
		{
			RequestSourceControlStatusUpdate();
		}
	});
}

void SSyncShieldToolbar::Notify(const FText& Message, bool bSuccess) const
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 4.0f;
	Info.bUseLargeFont = false;
	Info.bFireAndForget = true;
	Info.Image = FAppStyle::GetBrush(bSuccess ? "Icons.Info" : "Icons.WarningWithColor");

	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

FSyncShieldEditorState SSyncShieldToolbar::GetEditorState() const
{
	FSyncShieldEditorState Editor;
	Editor.bHasUnsavedAssets = bHasUnsavedAssets;
	Editor.UnsavedAssetCount = UnsavedAssetCount;
	Editor.LockedByOtherCount = LockedByOtherCount;
	Editor.NeedsCheckoutCount = NeedsCheckoutCount;
	Editor.SampleUnsavedPackage = SampleUnsavedPackage;
	return Editor;
}

FString SSyncShieldToolbar::GetSubsystemSummary()
{
	return FSyncShieldModule::IsAvailable() ? FSyncShieldModule::Get().BuildSubsystemSummary() : FString();
}

const FSlateBrush* SSyncShieldToolbar::GetIcon() const
{
	return FSyncShieldStatusPresenter::GetIcon(GetStatusSnapshot(), GetEditorState());
}

FText SSyncShieldToolbar::GetLabel() const
{
	return FSyncShieldStatusPresenter::GetLabel(GetStatusSnapshot(), GetEditorState());
}

FSlateColor SSyncShieldToolbar::GetColor() const
{
	return FSyncShieldStatusPresenter::GetColor(GetStatusSnapshot(), GetEditorState());
}

FText SSyncShieldToolbar::GetTooltip() const
{
	return FSyncShieldStatusPresenter::GetTooltip(GetStatusSnapshot(), GetEditorState(), GetSubsystemSummary());
}

FText SSyncShieldToolbar::GetProviderLabel() const
{
	return FSyncShieldStatusPresenter::GetProviderLabel(GetStatusSnapshot());
}

FString SSyncShieldToolbar::GetContextLabel(const FSyncShieldSourceControlStatus& Status) const
{
	return FSyncShieldStatusPresenter::GetContextLabel(Status);
}

bool SSyncShieldToolbar::IsStatusDegraded(const FSyncShieldSourceControlStatus& Status) const
{
	return FSyncShieldStatusPresenter::IsStatusDegraded(Status, GetEditorState());
}

FString SSyncShieldToolbar::GetStatusChangeKey() const
{
	return FSyncShieldStatusPresenter::GetStatusChangeKey(GetStatusSnapshot(), GetEditorState());
}

FString SSyncShieldToolbar::BuildStatusSummary(const FSyncShieldSourceControlStatus& Status) const
{
	return FSyncShieldStatusPresenter::BuildStatusSummary(Status, GetEditorState(), GetSubsystemSummary());
}

#undef LOCTEXT_NAMESPACE
