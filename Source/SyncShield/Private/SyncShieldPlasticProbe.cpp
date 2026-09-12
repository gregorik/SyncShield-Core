// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "SyncShieldPlasticProbe.h"

#include "SyncShieldProcess.h"

#include "Misc/Paths.h"

// Named rather than anonymous: this module builds as a unity blob, so an
// anonymous-namespace helper duplicated across two .cpp files is a redefinition.
namespace SyncShieldPlasticProbePrivate
{
	FString TrimCopy(const FString& InText)
	{
		FString OutText = InText;
		OutText.TrimStartAndEndInline();
		return OutText;
	}

	const FString PlasticFieldSeparator = TEXT("|");
	const FString PlasticLineStart = TEXT("@@SAFE@@");
	const FString PlasticLineEnd = TEXT("##SAFE##");

	bool IsPlasticAuthError(const FString& Text)
	{
		const FString Lower = Text.ToLower();
		return Lower.Contains(TEXT("login"))
			|| Lower.Contains(TEXT("log in"))
			|| Lower.Contains(TEXT("authentication"))
			|| Lower.Contains(TEXT("credential"))
			|| Lower.Contains(TEXT("unauthorized"))
			|| Lower.Contains(TEXT("not authorized"))
			|| Lower.Contains(TEXT("access denied"))
			|| Lower.Contains(TEXT("token"))
			|| Lower.Contains(TEXT("expired"));
	}
}

bool FSyncShieldPlasticProbe::Populate(const FString& ProjectDir, FSyncShieldSourceControlStatus& OutStatus, FString& OutError)
{
	OutStatus = FSyncShieldSourceControlStatus();
	OutStatus.Provider = ESyncShieldProvider::Plastic;

	auto AppendStatusError = [&OutStatus, &OutError](const FString& ErrorText)
	{
		const FString CleanError = SyncShieldPlasticProbePrivate::TrimCopy(ErrorText);
		if (CleanError.IsEmpty())
		{
			return;
		}

		OutStatus.bStatusError = true;
		if (!OutStatus.LastError.IsEmpty())
		{
			OutStatus.LastError += TEXT("\n");
		}
		OutStatus.LastError += CleanError;
		OutError = OutStatus.LastError;
	};

	auto CheckAuthError = [&ProjectDir](const FString& Text) -> bool
	{
		if (SyncShieldPlasticProbePrivate::IsPlasticAuthError(Text)) return true;
		FString DummyOut, DummyErr;
		int32 DummyExit = 0;
		FSyncShieldProcess::RunPlastic(TEXT("whoami"), ProjectDir, DummyOut, DummyErr, DummyExit);
		return DummyExit != 0;
	};

	FString StdOut;
	FString StdErr;
	int32 ExitCode = 0;

	const FString WorkspaceArgs = FString::Printf(TEXT("getworkspacefrompath \"%s\" --format=\"{wkname}|{wkpath}\""), *ProjectDir);
	const bool bPlasticLaunched = FSyncShieldProcess::RunPlastic(WorkspaceArgs, ProjectDir, StdOut, StdErr, ExitCode);

	if (!bPlasticLaunched)
	{
		OutStatus.bClientAvailable = false;
		OutError = TEXT("Plastic SCM CLI not found.");
		OutStatus.LastError = OutError;
		return false;
	}

	OutStatus.bClientAvailable = true;

	if (ExitCode != 0 || StdOut.IsEmpty())
	{
		OutStatus.bRepo = false;
		const FString Combined = SyncShieldPlasticProbePrivate::TrimCopy(StdErr + TEXT("\n") + StdOut);
		if (CheckAuthError(Combined))
		{
			OutStatus.bAuthRequired = true;
			OutError = TEXT("Plastic SCM login required.");
			OutStatus.LastError = Combined;
		}
		else
		{
			OutError = SyncShieldPlasticProbePrivate::TrimCopy(StdErr);
			OutStatus.LastError = OutError;
		}
		return false;
	}

	FString WorkspaceName;
	FString WorkspaceRoot;
	{
		const FString Trimmed = SyncShieldPlasticProbePrivate::TrimCopy(StdOut);
		TArray<FString> Parts;
		Trimmed.ParseIntoArray(Parts, TEXT("|"), true);
		if (Parts.Num() >= 2)
		{
			WorkspaceName = SyncShieldPlasticProbePrivate::TrimCopy(Parts[0]);
			WorkspaceRoot = SyncShieldPlasticProbePrivate::TrimCopy(Parts[1]);
		}
	}

	if (WorkspaceRoot.IsEmpty())
	{
		OutStatus.bRepo = false;
		OutError = TEXT("Plastic SCM workspace root not found.");
		OutStatus.LastError = OutError;
		return false;
	}

	OutStatus.bRepo = true;
	OutStatus.RepoRoot = WorkspaceRoot;
	OutStatus.WorkspaceName = WorkspaceName;

	StdOut.Reset();
	StdErr.Reset();
	ExitCode = 0;

	const FString WorkspaceInfoArgs = FString::Printf(TEXT("workspaceinfo \"%s\""), *OutStatus.RepoRoot);
	const bool bInfoOk = FSyncShieldProcess::RunPlastic(WorkspaceInfoArgs, OutStatus.RepoRoot, StdOut, StdErr, ExitCode);
	if (bInfoOk && ExitCode == 0)
	{
		TArray<FString> InfoLines;
		StdOut.ParseIntoArrayLines(InfoLines, true);
		for (const FString& Line : InfoLines)
		{
			const FString Trimmed = SyncShieldPlasticProbePrivate::TrimCopy(Line);
			int32 SplitIndex = INDEX_NONE;
			if (Trimmed.StartsWith(TEXT("Branch")) && (Trimmed.FindChar(TEXT(':'), SplitIndex) || Trimmed.FindChar(TEXT('='), SplitIndex)))
			{
				OutStatus.Branch = SyncShieldPlasticProbePrivate::TrimCopy(Trimmed.Mid(SplitIndex + 1));
				break;
			}
		}
	}
	else
	{
		const FString Combined = SyncShieldPlasticProbePrivate::TrimCopy(StdErr + TEXT("\n") + StdOut);
		if (CheckAuthError(Combined))
		{
			OutStatus.bAuthRequired = true;
			OutStatus.LastError = Combined;
			OutError = TEXT("Plastic SCM login required.");
			return true;
		}
		else
		{
			const FString StatusError = Combined.IsEmpty()
				? FString(TEXT("Plastic workspaceinfo failed."))
				: FString::Printf(TEXT("Plastic workspaceinfo failed: %s"), *Combined);
			AppendStatusError(StatusError);
		}
	}

	StdOut.Reset();
	StdErr.Reset();
	ExitCode = 0;

	const bool bHeaderOk = FSyncShieldProcess::RunPlastic(TEXT("status --header --head"), OutStatus.RepoRoot, StdOut, StdErr, ExitCode);
	if (bHeaderOk && ExitCode == 0)
	{
		int32 CurrentChangeset = INDEX_NONE;
		int32 HeadChangeset = INDEX_NONE;

		const FRegexPattern CsPattern(TEXT("cs:(\\d+)"));
		const FRegexPattern HeadPattern(TEXT("head:(\\d+)"));

		TArray<FString> HeaderLines;
		StdOut.ParseIntoArrayLines(HeaderLines, true);

		for (const FString& Line : HeaderLines)
		{
			FRegexMatcher CsMatcher(CsPattern, Line);
			if (CsMatcher.FindNext())
			{
				CurrentChangeset = FCString::Atoi(*CsMatcher.GetCaptureGroup(1));
			}

			FRegexMatcher HeadMatcher(HeadPattern, Line);
			if (HeadMatcher.FindNext())
			{
				HeadChangeset = FCString::Atoi(*HeadMatcher.GetCaptureGroup(1));
			}

			if (OutStatus.Branch.IsEmpty())
			{
				FString Left = Line;
				int32 ParenIndex = INDEX_NONE;
				if (Left.FindChar(TEXT('('), ParenIndex))
				{
					Left = Left.Left(ParenIndex);
				}
				Left = SyncShieldPlasticProbePrivate::TrimCopy(Left);

				if (Left.StartsWith(TEXT("/")) || Left.StartsWith(TEXT("lb:"), ESearchCase::IgnoreCase))
				{
					int32 AtIndex = Left.Find(TEXT("@"));
					if (AtIndex != INDEX_NONE)
					{
						Left = Left.Left(AtIndex);
					}
					OutStatus.Branch = SyncShieldPlasticProbePrivate::TrimCopy(Left);
				}
			}
		}

		if (CurrentChangeset >= 0 && HeadChangeset >= 0)
		{
			OutStatus.bHasUpstream = true;
			OutStatus.Behind = FMath::Max(0, HeadChangeset - CurrentChangeset);
			OutStatus.Ahead = FMath::Max(0, CurrentChangeset - HeadChangeset);
		}
	}
	else
	{
		const FString Combined = SyncShieldPlasticProbePrivate::TrimCopy(StdErr + TEXT("\n") + StdOut);
		if (CheckAuthError(Combined))
		{
			OutStatus.bAuthRequired = true;
			OutStatus.LastError = Combined;
			OutError = TEXT("Plastic SCM login required.");
			return true;
		}
		else
		{
			const FString StatusError = Combined.IsEmpty()
				? FString(TEXT("Plastic status --header --head failed."))
				: FString::Printf(TEXT("Plastic status --header --head failed: %s"), *Combined);
			AppendStatusError(StatusError);
		}
	}

	StdOut.Reset();
	StdErr.Reset();
	ExitCode = 0;

	const FString StatusArgs = FString::Printf(
		TEXT("status --machinereadable --noheader --controlledchanged --private --fieldseparator=%s --startlineseparator=%s --endlineseparator=%s"),
		*SyncShieldPlasticProbePrivate::PlasticFieldSeparator,
		*SyncShieldPlasticProbePrivate::PlasticLineStart,
		*SyncShieldPlasticProbePrivate::PlasticLineEnd
	);

	const bool bStatusOk = FSyncShieldProcess::RunPlastic(StatusArgs, OutStatus.RepoRoot, StdOut, StdErr, ExitCode);
	if (bStatusOk && ExitCode == 0)
	{
		ParseStatusOutput(StdOut, OutStatus);
	}
	else
	{
		const FString Combined = SyncShieldPlasticProbePrivate::TrimCopy(StdErr + TEXT("\n") + StdOut);
		if (CheckAuthError(Combined))
		{
			OutStatus.bAuthRequired = true;
			OutStatus.LastError = Combined;
			OutError = TEXT("Plastic SCM login required.");
		}
		else
		{
			const FString StatusError = Combined.IsEmpty()
				? FString(TEXT("Plastic status --machinereadable failed."))
				: FString::Printf(TEXT("Plastic status --machinereadable failed: %s"), *Combined);
			AppendStatusError(StatusError);
		}
	}

	return true;
}

void FSyncShieldPlasticProbe::ParseStatusOutput(const FString& Output, FSyncShieldSourceControlStatus& Status)
{
	TArray<FString> Lines;
	Output.ParseIntoArrayLines(Lines, true);

	int32 ChangeCount = 0;
	int32 UntrackedCount = 0;
	bool bHasConflicts = false;

	for (const FString& Line : Lines)
	{
		FString CleanLine = Line;
		CleanLine.ReplaceInline(*SyncShieldPlasticProbePrivate::PlasticLineStart, TEXT(""));
		CleanLine.ReplaceInline(*SyncShieldPlasticProbePrivate::PlasticLineEnd, TEXT(""));
		CleanLine = SyncShieldPlasticProbePrivate::TrimCopy(CleanLine);

		if (CleanLine.IsEmpty())
		{
			continue;
		}

		TArray<FString> Fields;
		CleanLine.ParseIntoArray(Fields, *SyncShieldPlasticProbePrivate::PlasticFieldSeparator, false);
		if (Fields.Num() == 0)
		{
			continue;
		}

		const FString Code = SyncShieldPlasticProbePrivate::TrimCopy(Fields[0]);
		if (Code.Equals(TEXT("STATUS"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		ChangeCount++;

		TArray<FString> CodeParts;
		Code.ParseIntoArray(CodeParts, TEXT("+"), true);
		for (const FString& Part : CodeParts)
		{
			if (Part.Equals(TEXT("PR"), ESearchCase::IgnoreCase))
			{
				UntrackedCount++;
				break;
			}
		}

		for (const FString& Field : Fields)
		{
			const FString UpperField = Field.ToUpper();
			if (UpperField.Contains(TEXT("CONFLICT")))
			{
				bHasConflicts = true;
				break;
			}
			if (UpperField.Contains(TEXT("MERGE")) && !UpperField.Contains(TEXT("NO_MERGES")))
			{
				bHasConflicts = true;
				break;
			}
		}
	}

	Status.Untracked = UntrackedCount;
	Status.Unstaged = FMath::Max(0, ChangeCount - UntrackedCount);
	Status.bHasConflicts = bHasConflicts;
}
