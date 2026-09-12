// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "SyncShieldGitProbe.h"

#include "SyncShieldProcess.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Named rather than anonymous: this module builds as a unity blob, so an
// anonymous-namespace helper duplicated across two .cpp files is a redefinition.
namespace SyncShieldGitProbePrivate
{
	FString TrimCopy(const FString& InText)
	{
		FString OutText = InText;
		OutText.TrimStartAndEndInline();
		return OutText;
	}
}

bool FSyncShieldGitProbe::Populate(const FString& ProjectDir, FSyncShieldSourceControlStatus& OutStatus, FString& OutError)
{
	OutStatus = FSyncShieldSourceControlStatus();
	OutStatus.Provider = ESyncShieldProvider::Git;

	FString StdOut;
	FString StdErr;
	int32 ExitCode = 0;

	const bool bGitLaunched = FSyncShieldProcess::RunGit(TEXT("rev-parse --show-toplevel"), ProjectDir, StdOut, StdErr, ExitCode);
	if (!bGitLaunched)
	{
		OutStatus.bClientAvailable = false;
		OutError = TEXT("Git executable not found.");
		OutStatus.LastError = OutError;
		return false;
	}

	OutStatus.bClientAvailable = true;

	if (ExitCode != 0)
	{
		OutStatus.bRepo = false;
		OutError = SyncShieldGitProbePrivate::TrimCopy(StdErr);
		OutStatus.LastError = OutError;
		return false;
	}

	OutStatus.bRepo = true;
	OutStatus.RepoRoot = SyncShieldGitProbePrivate::TrimCopy(StdOut);

	StdOut.Reset();
	StdErr.Reset();
	ExitCode = 0;

	const bool bStatusOk = FSyncShieldProcess::RunGit(TEXT("status --porcelain=v2 -b"), OutStatus.RepoRoot, StdOut, StdErr, ExitCode);
	if (bStatusOk && ExitCode == 0)
	{
		ParseStatusOutput(StdOut, OutStatus);

		// Read .gitattributes once: it tells us both whether Unreal binaries are
		// declared and whether this repository actually routes them through LFS.
		const FString GitattributesPath = FPaths::Combine(OutStatus.RepoRoot, TEXT(".gitattributes"));
		FString GitattributesContent;
		if (FFileHelper::LoadFileToString(GitattributesContent, *GitattributesPath))
		{
			OutStatus.bGitattributesConfigured = GitattributesContent.Contains(TEXT("*.uasset")) && GitattributesContent.Contains(TEXT("*.umap"));
			OutStatus.bLfsDetected = DetectLfsFromGitattributes(GitattributesContent);
		}
		else
		{
			OutStatus.bGitattributesConfigured = false;
			OutStatus.bLfsDetected = false;
		}

		if (OutStatus.bLfsDetected)
		{
			StdOut.Reset();
			StdErr.Reset();
			ExitCode = 0;
			if (FSyncShieldProcess::RunGit(TEXT("lfs locks --local --json"), OutStatus.RepoRoot, StdOut, StdErr, ExitCode) && ExitCode == 0)
			{
				ParseLfsLocksJson(StdOut, OutStatus.LfsLockedFiles);
				OutStatus.LfsLockCount = OutStatus.LfsLockedFiles.Num();
			}
		}
	}
	else
	{
		const FString CommandError = SyncShieldGitProbePrivate::TrimCopy(StdErr + TEXT("\n") + StdOut);
		OutStatus.bStatusError = true;
		const FString StatusError = CommandError.IsEmpty()
			? FString(TEXT("git status --porcelain=v2 -b failed."))
			: FString::Printf(TEXT("git status --porcelain=v2 -b failed: %s"), *CommandError);
		OutStatus.LastError = StatusError;
		OutError = OutStatus.LastError;
	}

	return true;
}

void FSyncShieldGitProbe::ParseStatusOutput(const FString& Output, FSyncShieldSourceControlStatus& Status)
{
	TArray<FString> Lines;
	Output.ParseIntoArrayLines(Lines, true);

	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("# branch.head ")))
		{
			Status.Branch = SyncShieldGitProbePrivate::TrimCopy(Line.RightChop(14));
		}
		else if (Line.StartsWith(TEXT("# branch.upstream ")))
		{
			Status.bHasUpstream = true;
		}
		else if (Line.StartsWith(TEXT("# branch.ab ")))
		{
			const FString Ab = SyncShieldGitProbePrivate::TrimCopy(Line.RightChop(12));
			TArray<FString> Parts;
			Ab.ParseIntoArray(Parts, TEXT(" "), true);

			for (const FString& Part : Parts)
			{
				if (Part.StartsWith(TEXT("+")))
				{
					Status.Ahead = FCString::Atoi(*Part.RightChop(1));
				}
				else if (Part.StartsWith(TEXT("-")))
				{
					Status.Behind = FCString::Atoi(*Part.RightChop(1));
				}
			}
		}
		else if (Line.StartsWith(TEXT("1 ")) || Line.StartsWith(TEXT("2 ")))
		{
			if (Line.Len() > 3)
			{
				const TCHAR X = Line[2];
				const TCHAR Y = Line[3];

				if (X != TEXT('.'))
				{
					Status.Staged++;
				}
				if (Y != TEXT('.'))
				{
					Status.Unstaged++;
				}
				if (X == TEXT('U') || Y == TEXT('U'))
				{
					Status.bHasConflicts = true;
				}
			}
		}
		else if (Line.StartsWith(TEXT("u ")))
		{
			Status.bHasConflicts = true;
		}
		else if (Line.StartsWith(TEXT("? ")))
		{
			Status.Untracked++;
		}
	}
}

void FSyncShieldGitProbe::ParseLfsLocksJson(const FString& JsonText, TArray<FString>& OutLockedFiles)
{
	OutLockedFiles.Reset();

	const FString Trimmed = SyncShieldGitProbePrivate::TrimCopy(JsonText);
	if (Trimmed.IsEmpty())
	{
		return;
	}

	// `git lfs locks --json` is the only stable interface here. The human-readable
	// listing is space-aligned, so splitting it on tabs mangles any path containing
	// a space and produces unusable unlock arguments.
	TArray<TSharedPtr<FJsonValue>> Entries;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
	if (!FJsonSerializer::Deserialize(Reader, Entries))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Entry : Entries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* LockObject = nullptr;
		if (!Entry->TryGetObject(LockObject) || LockObject == nullptr || !LockObject->IsValid())
		{
			continue;
		}

		FString LockedPath;
		if ((*LockObject)->TryGetStringField(TEXT("path"), LockedPath))
		{
			const FString CleanPath = SyncShieldGitProbePrivate::TrimCopy(LockedPath);
			if (!CleanPath.IsEmpty())
			{
				OutLockedFiles.Add(CleanPath);
			}
		}
	}
}

bool FSyncShieldGitProbe::DetectLfsFromGitattributes(const FString& GitattributesContent)
{
	// `git lfs env` succeeds in any repository whenever git-lfs is merely installed,
	// so it describes the machine rather than the repository. The .gitattributes
	// filter is what actually tells us this repo routes assets through LFS.
	return GitattributesContent.Contains(TEXT("filter=lfs"));
}
