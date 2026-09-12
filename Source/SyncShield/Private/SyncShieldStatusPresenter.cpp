// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "SyncShieldStatusPresenter.h"

#include "SyncShieldStyle.h"

#include "Styling/AppStyle.h"

// Unchanged from the widget so every localisation key keeps its identity.
#define LOCTEXT_NAMESPACE "SyncShieldToolbar"

// Named rather than anonymous: this module builds as a unity blob.
namespace SyncShieldStatusPresenterPrivate
{
	void AppendUnsavedDetails(FString& InOutText, const FSyncShieldEditorState& Editor);
}

FString FSyncShieldStatusPresenter::GetContextLabel(const FSyncShieldSourceControlStatus& Status)
{
	FString Context = Status.Branch;
	if (Context.IsEmpty())
	{
		Context = Status.WorkspaceName;
	}
	if (Context.IsEmpty() && Status.bAuthRequired && Status.Provider == ESyncShieldProvider::Plastic)
	{
		Context = TEXT("Plastic");
	}
	if (Context.IsEmpty())
	{
		switch (Status.Provider)
		{
		case ESyncShieldProvider::Git:
			Context = TEXT("unknown");
			break;
		case ESyncShieldProvider::Plastic:
			Context = TEXT("Plastic");
			break;
		default:
			Context = TEXT("Source Control");
			break;
		}
	}
	if (Status.Provider == ESyncShieldProvider::Git && Context.Contains(TEXT("detached")))
	{
		Context = TEXT("detached");
	}
	return Context;
}

void SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(FString& InOutText, const FSyncShieldEditorState& Editor)
{
	if (!Editor.bHasUnsavedAssets)
	{
		return;
	}

	if (!InOutText.IsEmpty() && !InOutText.EndsWith(TEXT("\n")))
	{
		InOutText += TEXT("\n");
	}

	InOutText += FString::Printf(TEXT("Unsaved assets: %d"), Editor.UnsavedAssetCount);
	if (!Editor.SampleUnsavedPackage.IsEmpty())
	{
		InOutText += FString::Printf(TEXT("\nExample: %s"), *Editor.SampleUnsavedPackage);
	}
}

bool FSyncShieldStatusPresenter::IsStatusDegraded(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor)
{
	return Editor.bHasUnsavedAssets
		|| !Status.bClientAvailable
		|| !Status.bRepo
		|| Status.bAuthRequired
		|| Status.bStatusError
		|| Status.bHasConflicts
		|| (Status.Ahead > 0 && Status.Behind > 0);
}

const FSlateBrush* FSyncShieldStatusPresenter::GetIcon(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor)
{
	if (Status.bStatusError)
	{
		return FAppStyle::GetBrush("Icons.WarningWithColor");
	}

	if (Status.bAuthRequired || (Editor.bHasUnsavedAssets && (!Status.bClientAvailable || !Status.bRepo)))
	{
		return FAppStyle::GetBrush("Icons.WarningWithColor");
	}

	if (!Status.bClientAvailable || !Status.bRepo)
	{
		return FAppStyle::GetBrush("Icons.Warning");
	}

	if (Status.bHasConflicts || (Status.Ahead > 0 && Status.Behind > 0) || Editor.LockedByOtherCount > 0)
	{
		return FAppStyle::GetBrush("Icons.WarningWithColor");
	}

	if (Editor.bHasUnsavedAssets || Editor.NeedsCheckoutCount > 0)
	{
		return FAppStyle::GetBrush("Icons.Save");
	}

	if (Status.Behind > 0)
	{
		return FAppStyle::GetBrush("Icons.Refresh");
	}

	if (Status.Ahead > 0)
	{
		return FAppStyle::GetBrush("Icons.Save");
	}

	if (Status.Staged + Status.Unstaged + Status.Untracked > 0)
	{
		return FAppStyle::GetBrush("Icons.Save");
	}

	if (const FSlateBrush* StatusBrush = FSyncShieldStyle::GetOptionalBrush("SyncShield.StatusIcon"))
	{
		return StatusBrush;
	}

	return FAppStyle::GetBrush("Icons.Info");
}

FText FSyncShieldStatusPresenter::GetLabel(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor)
{
	if (!Status.bClientAvailable)
	{
		if (Editor.bHasUnsavedAssets)
		{
			return FText::FromString(FString::Printf(TEXT("SCM Missing | Unsaved %d"), Editor.UnsavedAssetCount));
		}
		return LOCTEXT("SCMMissing", "SCM Missing");
	}

	if (Status.bAuthRequired)
	{
		if (Editor.bHasUnsavedAssets)
		{
			return FText::FromString(FString::Printf(TEXT("Login Required | Unsaved %d"), Editor.UnsavedAssetCount));
		}
		return LOCTEXT("SCMLoginRequired", "Login Required");
	}

	if (!Status.bRepo)
	{
		if (Editor.bHasUnsavedAssets)
		{
			return FText::FromString(FString::Printf(TEXT("No SCM Repo | Unsaved %d"), Editor.UnsavedAssetCount));
		}
		return LOCTEXT("NoRepo", "No SCM Repo");
	}

	const FString Branch = GetContextLabel(Status);

	FString StateText;
	if (Status.bStatusError)
	{
		StateText = TEXT("Status Error");
	}
	else if (Status.bHasConflicts)
	{
		StateText = TEXT("Conflicts");
	}
	else if (Editor.LockedByOtherCount > 0)
	{
		StateText = FString::Printf(TEXT("Locked (%d)"), Editor.LockedByOtherCount);
	}
	else if (Editor.NeedsCheckoutCount > 0)
	{
		StateText = FString::Printf(TEXT("Checkout Req (%d)"), Editor.NeedsCheckoutCount);
	}
	else if (Editor.bHasUnsavedAssets)
	{
		StateText = FString::Printf(TEXT("Unsaved %d"), Editor.UnsavedAssetCount);
	}
	else if (Status.Ahead > 0 && Status.Behind > 0)
	{
		StateText = TEXT("Diverged");
	}
	else if (Status.Behind > 0)
	{
		StateText = FString::Printf(TEXT("Behind %d"), Status.Behind);
	}
	else if (Status.Staged + Status.Unstaged + Status.Untracked > 0)
	{
		StateText = TEXT("Changes");
	}
	else if (Status.Ahead > 0)
	{
		StateText = FString::Printf(TEXT("Ahead %d"), Status.Ahead);
	}
	else
	{
		StateText = TEXT("Clean");
	}

	return FText::FromString(FString::Printf(TEXT("%s | %s"), *Branch, *StateText));
}

FSlateColor FSyncShieldStatusPresenter::GetColor(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor)
{
	if (Status.bStatusError)
	{
		return FLinearColor(1.0f, 0.2f, 0.2f);
	}

	if (Status.bAuthRequired)
	{
		return FLinearColor(1.0f, 0.65f, 0.0f);
	}

	if (Editor.bHasUnsavedAssets && (!Status.bClientAvailable || !Status.bRepo))
	{
		return FLinearColor(1.0f, 0.5f, 0.0f);
	}

	if (!Status.bClientAvailable || !Status.bRepo)
	{
		return FLinearColor::Gray;
	}

	if (Status.bHasConflicts || (Status.Ahead > 0 && Status.Behind > 0) || Editor.LockedByOtherCount > 0)
	{
		return FLinearColor(1.0f, 0.2f, 0.2f);
	}

	if (Editor.bHasUnsavedAssets || Editor.NeedsCheckoutCount > 0)
	{
		return FLinearColor(1.0f, 0.5f, 0.0f);
	}

	if (Status.Behind > 0)
	{
		return FLinearColor(0.0f, 0.45f, 1.0f);
	}

	if (Status.Staged + Status.Unstaged + Status.Untracked > 0)
	{
		return FLinearColor(1.0f, 0.5f, 0.0f);
	}

	return FLinearColor(0.2f, 0.85f, 0.2f);
}

FText FSyncShieldStatusPresenter::GetTooltip(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor, const FString& SubsystemSummary)
{
	FString Tooltip;

	if (!Status.bClientAvailable)
	{
		Tooltip = TEXT("Git or Plastic SCM CLI not found. Install Git or Unity Version Control (Plastic SCM) CLI and restart the editor.");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Tooltip, Editor);
		if (!Status.LastError.IsEmpty())
		{
			Tooltip += TEXT("\n");
			Tooltip += Status.LastError;
		}
		return FText::FromString(Tooltip);
	}

	if (Status.bAuthRequired)
	{
		Tooltip = TEXT("Plastic SCM login required. Sign in via Source Control to continue.");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Tooltip, Editor);
		if (!Status.LastError.IsEmpty())
		{
			Tooltip += TEXT("\n");
			Tooltip += Status.LastError;
		}
		return FText::FromString(Tooltip);
	}

	if (!Status.bRepo)
	{
		Tooltip = TEXT("Project is not inside a Git repository or Plastic SCM workspace.");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Tooltip, Editor);
		if (!Status.LastError.IsEmpty())
		{
			Tooltip += TEXT("\n");
			Tooltip += Status.LastError;
		}
		return FText::FromString(Tooltip);
	}

	Tooltip += FString::Printf(TEXT("Provider: %s\n"), *GetProviderLabel(Status).ToString());
	if (Status.Provider == ESyncShieldProvider::Plastic && !Status.WorkspaceName.IsEmpty())
	{
		Tooltip += FString::Printf(TEXT("Workspace: %s\n"), *Status.WorkspaceName);
	}
	Tooltip += FString::Printf(TEXT("Root: %s\n"), *Status.RepoRoot);

	const FString BranchLabel = GetContextLabel(Status);
	if (!BranchLabel.IsEmpty())
	{
		Tooltip += FString::Printf(TEXT("Branch: %s\n"), *BranchLabel);
	}

	if (Status.bStatusError)
	{
		Tooltip += TEXT("Status: error\n");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Tooltip, Editor);
		if (!Status.LastError.IsEmpty())
		{
			if (!Tooltip.EndsWith(TEXT("\n")))
			{
				Tooltip += TEXT("\n");
			}
			Tooltip += TEXT("Details:\n");
			Tooltip += Status.LastError;
		}
		if (Status.LastUpdateUtc != FDateTime())
		{
			if (!Tooltip.IsEmpty() && !Tooltip.EndsWith(TEXT("\n")))
			{
				Tooltip += TEXT("\n");
			}
			const FTimespan Age = FDateTime::UtcNow() - Status.LastUpdateUtc;
			Tooltip += FString::Printf(TEXT("Updated: %ds ago"), (int32)Age.GetTotalSeconds());
		}
		return FText::FromString(Tooltip);
	}

	if (Status.Provider == ESyncShieldProvider::Git)
	{
		if (Status.bHasUpstream)
		{
			Tooltip += FString::Printf(TEXT("Ahead: %d  Behind: %d\n"), Status.Ahead, Status.Behind);
		}
		else
		{
			Tooltip += TEXT("Upstream: not set\n");
		}

		Tooltip += FString::Printf(TEXT("Staged: %d  Unstaged: %d  Untracked: %d\n"), Status.Staged, Status.Unstaged, Status.Untracked);
		if (Status.bLfsDetected)
		{
			Tooltip += FString::Printf(TEXT("Git LFS: active, Locks: %d\n"), Status.LfsLockCount);
		}
		if (!Status.bGitattributesConfigured)
		{
			Tooltip += TEXT("WARNING: .gitattributes missing *.uasset / *.umap entries\n");
		}
	}
	else if (Status.Provider == ESyncShieldProvider::Plastic)
	{
		if (Status.Behind > 0)
		{
			Tooltip += FString::Printf(TEXT("Updates available: %d\n"), Status.Behind);
		}
		Tooltip += FString::Printf(TEXT("Pending changes: %d\n"), Status.Unstaged + Status.Untracked);
	}

	if (Editor.bHasUnsavedAssets)
	{
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Tooltip, Editor);
	}

	if (Editor.LockedByOtherCount > 0)
	{
		if (!Tooltip.EndsWith(TEXT("\n"))) Tooltip += TEXT("\n");
		Tooltip += FString::Printf(TEXT("Locked by Others: %d\n"), Editor.LockedByOtherCount);
	}
	if (Editor.NeedsCheckoutCount > 0)
	{
		if (!Tooltip.EndsWith(TEXT("\n"))) Tooltip += TEXT("\n");
		Tooltip += FString::Printf(TEXT("Checkout Required: %d\n"), Editor.NeedsCheckoutCount);
	}

	if (Status.LastUpdateUtc != FDateTime())
	{
		if (!Tooltip.IsEmpty() && !Tooltip.EndsWith(TEXT("\n")))
		{
			Tooltip += TEXT("\n");
		}
		const FTimespan Age = FDateTime::UtcNow() - Status.LastUpdateUtc;
		Tooltip += FString::Printf(TEXT("Updated: %ds ago"), (int32)Age.GetTotalSeconds());
	}

	{
		const FString& ExtraSummary = SubsystemSummary;
		if (!ExtraSummary.IsEmpty())
		{
			if (!Tooltip.IsEmpty() && !Tooltip.EndsWith(TEXT("\n")))
			{
				Tooltip += TEXT("\n");
			}
			Tooltip += TEXT("\n");
			Tooltip += ExtraSummary;
		}
	}

	return FText::FromString(Tooltip);
}

FText FSyncShieldStatusPresenter::GetProviderLabel(const FSyncShieldSourceControlStatus& Status)
{
	switch (Status.Provider)
	{
	case ESyncShieldProvider::Git:
		return LOCTEXT("ProviderGit", "Git");
	case ESyncShieldProvider::Plastic:
		return LOCTEXT("ProviderPlastic", "Plastic SCM");
	default:
		return LOCTEXT("ProviderSourceControl", "Source Control");
	}
}

FString FSyncShieldStatusPresenter::GetStatusChangeKey(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor)
{
	// Deliberately excludes counts. The visible label embeds the unsaved-asset count,
	// so keying the toast on it fired a notification on every single edit. Only real
	// state transitions should interrupt the user.
	return FString::Printf(
		TEXT("%s|%d%d%d%d%d|%d%d%d%d"),
		*GetContextLabel(Status),
		Status.bClientAvailable ? 1 : 0,
		Status.bRepo ? 1 : 0,
		Status.bAuthRequired ? 1 : 0,
		Status.bStatusError ? 1 : 0,
		Status.bHasConflicts ? 1 : 0,
		Status.Ahead > 0 ? 1 : 0,
		Status.Behind > 0 ? 1 : 0,
		Editor.bHasUnsavedAssets ? 1 : 0,
		Editor.LockedByOtherCount > 0 ? 1 : 0);
}

FString FSyncShieldStatusPresenter::BuildStatusSummary(const FSyncShieldSourceControlStatus& Status, const FSyncShieldEditorState& Editor, const FString& SubsystemSummary)
{
	FString Summary;

	if (!Status.bClientAvailable)
	{
		Summary = TEXT("Git or Plastic SCM CLI not found. Install Git or Unity Version Control (Plastic SCM) CLI and restart the editor.");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Summary, Editor);
		if (!Status.LastError.IsEmpty())
		{
			Summary += TEXT("\nDetails:\n");
			Summary += Status.LastError;
		}
		return Summary;
	}

	if (Status.bAuthRequired)
	{
		Summary = TEXT("Plastic SCM login required. Sign in via Source Control to continue.");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Summary, Editor);
		if (!Status.LastError.IsEmpty())
		{
			Summary += TEXT("\nDetails:\n");
			Summary += Status.LastError;
		}
		return Summary;
	}

	if (!Status.bRepo)
	{
		Summary = TEXT("Project is not inside a Git repository or Plastic SCM workspace.");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Summary, Editor);
		if (!Status.LastError.IsEmpty())
		{
			Summary += TEXT("\nDetails:\n");
			Summary += Status.LastError;
		}
		return Summary;
	}

	Summary += FString::Printf(TEXT("Provider: %s\n"), *GetProviderLabel(Status).ToString());
	if (Status.Provider == ESyncShieldProvider::Plastic && !Status.WorkspaceName.IsEmpty())
	{
		Summary += FString::Printf(TEXT("Workspace: %s\n"), *Status.WorkspaceName);
	}
	Summary += FString::Printf(TEXT("Root: %s\n"), *Status.RepoRoot);

	const FString BranchLabel = GetContextLabel(Status);
	if (!BranchLabel.IsEmpty())
	{
		Summary += FString::Printf(TEXT("Branch: %s\n"), *BranchLabel);
	}

	if (Status.bStatusError)
	{
		Summary += TEXT("Status: error\n");
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Summary, Editor);
		if (!Status.LastError.IsEmpty())
		{
			if (!Summary.EndsWith(TEXT("\n")))
			{
				Summary += TEXT("\n");
			}
			Summary += TEXT("Details:\n");
			Summary += Status.LastError;
		}
		return Summary;
	}

	if (Status.Provider == ESyncShieldProvider::Git)
	{
		if (Status.bHasUpstream)
		{
			Summary += FString::Printf(TEXT("Ahead: %d  Behind: %d\n"), Status.Ahead, Status.Behind);
		}
		else
		{
			Summary += TEXT("Upstream: not set\n");
		}

		Summary += FString::Printf(TEXT("Staged: %d  Unstaged: %d  Untracked: %d\n"), Status.Staged, Status.Unstaged, Status.Untracked);
		if (Status.bLfsDetected)
		{
			Summary += FString::Printf(TEXT("Git LFS: active, Locks: %d\n"), Status.LfsLockCount);
		}
		if (!Status.bGitattributesConfigured)
		{
			Summary += TEXT("WARNING: .gitattributes is missing entries for *.uasset and/or *.umap.\n");
			Summary += TEXT("Binary UE assets should be marked as binary or LFS-tracked to prevent corruption.\n");
		}
	}
	else if (Status.Provider == ESyncShieldProvider::Plastic)
	{
		if (Status.Behind > 0)
		{
			Summary += FString::Printf(TEXT("Updates available: %d\n"), Status.Behind);
		}
		Summary += FString::Printf(TEXT("Pending changes: %d\n"), Status.Unstaged + Status.Untracked);
	}

	if (Editor.bHasUnsavedAssets)
	{
		SyncShieldStatusPresenterPrivate::AppendUnsavedDetails(Summary, Editor);
	}

	{
		const FString& ExtraSummary = SubsystemSummary;
		if (!ExtraSummary.IsEmpty())
		{
			if (!Summary.IsEmpty() && !Summary.EndsWith(TEXT("\n")))
			{
				Summary += TEXT("\n");
			}
			Summary += TEXT("\n");
			Summary += ExtraSummary;
		}
	}

	return Summary;
}

#undef LOCTEXT_NAMESPACE
