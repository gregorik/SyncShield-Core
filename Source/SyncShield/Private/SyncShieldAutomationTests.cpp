// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/IConsoleManager.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SSyncShieldToolbar.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Widgets/SWidget.h"

class FSyncShieldToolbarTestAccessor
{
public:
	using FStatusSnapshot = SSyncShieldToolbar::FSourceControlStatus;

	enum class EProvider : uint8
	{
		None,
		Git,
		Plastic
	};

	static TSharedRef<SSyncShieldToolbar> CreateToolbar()
	{
		return SNew(SSyncShieldToolbar)
			.DisableLiveUpdates(true);
	}

	static void SetUnsavedState(const TSharedRef<SSyncShieldToolbar>& Toolbar, bool bHasUnsavedAssets, int32 UnsavedCount, const FString& SamplePackage)
	{
		Toolbar->bHasUnsavedAssets = bHasUnsavedAssets;
		Toolbar->UnsavedAssetCount = UnsavedCount;
		Toolbar->SampleUnsavedPackage = SamplePackage;
	}

	static void SetStatus(
		const TSharedRef<SSyncShieldToolbar>& Toolbar,
		EProvider Provider,
		bool bClientAvailable,
		bool bRepo,
		bool bAuthRequired,
		bool bStatusError,
		bool bHasUpstream,
		bool bHasConflicts,
		int32 Ahead,
		int32 Behind,
		int32 Staged,
		int32 Unstaged,
		int32 Untracked,
		const FString& Branch,
		const FString& RepoRoot,
		const FString& WorkspaceName,
		const FString& LastError)
	{
		SSyncShieldToolbar::FSourceControlStatus& Status = Toolbar->SourceControlStatus;
		Status = SSyncShieldToolbar::FSourceControlStatus();

		switch (Provider)
		{
		case EProvider::Git:
			Status.Provider = SSyncShieldToolbar::ESourceControlProvider::Git;
			break;
		case EProvider::Plastic:
			Status.Provider = SSyncShieldToolbar::ESourceControlProvider::Plastic;
			break;
		default:
			Status.Provider = SSyncShieldToolbar::ESourceControlProvider::None;
			break;
		}

		Status.bClientAvailable = bClientAvailable;
		Status.bRepo = bRepo;
		Status.bAuthRequired = bAuthRequired;
		Status.bStatusError = bStatusError;
		Status.bHasUpstream = bHasUpstream;
		Status.bHasConflicts = bHasConflicts;
		Status.Ahead = Ahead;
		Status.Behind = Behind;
		Status.Staged = Staged;
		Status.Unstaged = Unstaged;
		Status.Untracked = Untracked;
		Status.Branch = Branch;
		Status.RepoRoot = RepoRoot;
		Status.WorkspaceName = WorkspaceName;
		Status.LastError = LastError;
		Status.LastUpdateUtc = FDateTime::UtcNow();
	}

	static void ApplyStatusSnapshot(const TSharedRef<SSyncShieldToolbar>& Toolbar, const FStatusSnapshot& Status)
	{
		Toolbar->SourceControlStatus = Status;
	}

	static bool TryPopulateGitStatus(const TSharedRef<SSyncShieldToolbar>& Toolbar, const FString& ProjectDir, FStatusSnapshot& OutStatus, FString& OutError)
	{
		return Toolbar->TryPopulateGitStatus(ProjectDir, OutStatus, OutError);
	}

	static bool TryPopulatePlasticStatus(const TSharedRef<SSyncShieldToolbar>& Toolbar, const FString& ProjectDir, FStatusSnapshot& OutStatus, FString& OutError)
	{
		return Toolbar->TryPopulatePlasticStatus(ProjectDir, OutStatus, OutError);
	}

	static bool RunGit(const TSharedRef<SSyncShieldToolbar>& Toolbar, const FString& WorkingDir, const FString& Args, FString& OutStdOut, FString& OutStdErr, int32& OutExitCode)
	{
		return Toolbar->RunGit(Args, WorkingDir, OutStdOut, OutStdErr, OutExitCode);
	}

	static FString GetLabel(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->GetLabel().ToString();
	}

	static FString GetTooltip(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->GetTooltip().ToString();
	}

	static FString GetSummary(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->BuildStatusSummary(Toolbar->GetStatusSnapshot());
	}

	static FLinearColor GetColor(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->GetColor().GetSpecifiedColor();
	}

	static bool CanExecuteGitCommand(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->CanExecuteGitCommand();
	}

	static bool CanExecuteGitPull(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->CanExecuteGitPull();
	}

	static bool CanExecuteGitPush(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->CanExecuteGitPush();
	}

	static bool CanExecutePlasticUpdate(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->CanExecutePlasticUpdate();
	}

	static bool IsStatusDegraded(const TSharedRef<SSyncShieldToolbar>& Toolbar)
	{
		return Toolbar->IsStatusDegraded(Toolbar->GetStatusSnapshot());
	}
};

namespace
{
	class FScopedAutomationDirectory
	{
	public:
		explicit FScopedAutomationDirectory(FString InPath)
			: Path(MoveTemp(InPath))
		{
			IFileManager::Get().DeleteDirectory(*Path, false, true);
			IFileManager::Get().MakeDirectory(*Path, true);
		}

		~FScopedAutomationDirectory()
		{
			IFileManager::Get().DeleteDirectory(*Path, false, true);
		}

		const FString& GetPath() const
		{
			return Path;
		}

	private:
		FString Path;
	};

	FString MakeAutomationDirectoryPath(const TCHAR* ScenarioName)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("SyncShield"),
			ScenarioName,
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	bool WriteTextFileChecked(FAutomationTestBase& Test, const FString& FilePath, const FString& Contents, const FString& Description)
	{
		const bool bSaved = FFileHelper::SaveStringToFile(Contents, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test.TestTrue(*Description, bSaved);
		return bSaved;
	}

	bool RunGitChecked(
		FAutomationTestBase& Test,
		const TSharedRef<SSyncShieldToolbar>& Toolbar,
		const FString& WorkingDir,
		const FString& Args,
		const FString& Description)
	{
		FString StdOut;
		FString StdErr;
		int32 ExitCode = INDEX_NONE;

		const bool bLaunched = FSyncShieldToolbarTestAccessor::RunGit(Toolbar, WorkingDir, Args, StdOut, StdErr, ExitCode);
		Test.TestTrue(*FString::Printf(TEXT("%s launched"), *Description), bLaunched);
		Test.TestEqual(*FString::Printf(TEXT("%s exit code"), *Description), ExitCode, 0);

		if (!bLaunched || ExitCode != 0)
		{
			if (!StdOut.IsEmpty())
			{
				Test.AddError(FString::Printf(TEXT("%s stdout: %s"), *Description, *StdOut));
			}
			if (!StdErr.IsEmpty())
			{
				Test.AddError(FString::Printf(TEXT("%s stderr: %s"), *Description, *StdErr));
			}
			return false;
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldEditorIntegrationTest,
	"SyncShield.Editor.Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldEditorIntegrationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("SyncShield module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("SyncShield")));

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"));
	TestNotNull(TEXT("Settings module is loaded"), SettingsModule);
	if (!SettingsModule)
	{
		return false;
	}

	const TSharedPtr<ISettingsContainer> EditorContainer = SettingsModule->GetContainer(TEXT("Editor"));
	TestTrue(TEXT("Editor settings container exists"), EditorContainer.IsValid());
	if (!EditorContainer.IsValid())
	{
		return false;
	}

	const TSharedPtr<ISettingsCategory> PluginsCategory = EditorContainer->GetCategory(TEXT("Plugins"));
	TestTrue(TEXT("Plugins settings category exists"), PluginsCategory.IsValid());
	if (!PluginsCategory.IsValid())
	{
		return false;
	}

	const TSharedPtr<ISettingsSection> SyncShieldSection = PluginsCategory->GetSection(TEXT("SyncShield"), true);
	TestTrue(TEXT("SyncShield settings section exists"), SyncShieldSection.IsValid());

	UToolMenus* ToolMenus = UToolMenus::Get();
	TestNotNull(TEXT("ToolMenus subsystem is available"), ToolMenus);
	if (!ToolMenus)
	{
		return false;
	}

	UToolMenu* ToolbarMenu = ToolMenus->FindMenu(TEXT("LevelEditor.LevelEditorToolBar.User"));
	TestNotNull(TEXT("Level Editor toolbar menu exists"), ToolbarMenu);
	if (!ToolbarMenu)
	{
		return false;
	}

	FToolMenuSection* SyncShieldSectionMenu = ToolbarMenu->FindSection(TEXT("SyncShieldControls"));
	TestNotNull(TEXT("SyncShield toolbar section exists"), SyncShieldSectionMenu);
	TestTrue(TEXT("SyncShield toolbar entry exists"), ToolbarMenu->ContainsEntry(TEXT("SyncShieldStatusWidget")));

	IConsoleObject* StressTestCommand = IConsoleManager::Get().FindConsoleObject(TEXT("SyncShield.StressTest"));
	TestNotNull(TEXT("SyncShield stress-test command is registered"), StressTestCommand);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldToolbarStateTest,
	"SyncShield.Editor.ToolbarStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldToolbarStateTest::RunTest(const FString& Parameters)
{
	TSharedRef<SSyncShieldToolbar> Toolbar = FSyncShieldToolbarTestAccessor::CreateToolbar();
	FSyncShieldToolbarTestAccessor::SetUnsavedState(Toolbar, true, 3, TEXT("/Game/Maps/TestMap"));

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::None,
		false,
		false,
		false,
		false,
		false,
		false,
		0,
		0,
		0,
		0,
		0,
		FString(),
		FString(),
		FString(),
		TEXT("Git executable not found."));
	TestEqual(TEXT("Missing SCM label includes unsaved state"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("SCM Missing | Unsaved 3")));
	TestTrue(TEXT("Missing SCM tooltip shows unsaved assets"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Unsaved assets: 3")));
	TestEqual(TEXT("Missing SCM with unsaved assets is orange"), FSyncShieldToolbarTestAccessor::GetColor(Toolbar), FLinearColor(1.0f, 0.5f, 0.0f));

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::None,
		true,
		false,
		false,
		false,
		false,
		false,
		0,
		0,
		0,
		0,
		0,
		FString(),
		FString(),
		FString(),
		TEXT("Not inside a repository."));
	TestEqual(TEXT("No repo label includes unsaved state"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("No SCM Repo | Unsaved 3")));
	TestTrue(TEXT("No repo tooltip shows unsaved assets"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Unsaved assets: 3")));

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::Plastic,
		true,
		false,
		true,
		false,
		false,
		false,
		0,
		0,
		0,
		0,
		0,
		FString(),
		FString(),
		TEXT("WorkspaceAlpha"),
		TEXT("Plastic SCM login required."));
	TestEqual(TEXT("Login required label includes unsaved state"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("Login Required | Unsaved 3")));
	TestTrue(TEXT("Login required tooltip shows unsaved assets"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Unsaved assets: 3")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldToolbarCommandGatingTest,
	"SyncShield.Editor.CommandGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldToolbarCommandGatingTest::RunTest(const FString& Parameters)
{
	TSharedRef<SSyncShieldToolbar> Toolbar = FSyncShieldToolbarTestAccessor::CreateToolbar();
	FSyncShieldToolbarTestAccessor::SetUnsavedState(Toolbar, false, 0, FString());

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::Git,
		true,
		true,
		false,
		false,
		true,
		false,
		0,
		0,
		0,
		0,
		0,
		TEXT("main"),
		TEXT("C:/Repo"),
		FString(),
		FString());
	TestEqual(TEXT("Clean git label is reported"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Clean")));
	TestTrue(TEXT("Clean git enables git commands"), FSyncShieldToolbarTestAccessor::CanExecuteGitCommand(Toolbar));
	TestFalse(TEXT("Clean git state is not degraded"), FSyncShieldToolbarTestAccessor::IsStatusDegraded(Toolbar));

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::Git,
		true,
		true,
		false,
		true,
		true,
		false,
		0,
		0,
		0,
		0,
		0,
		TEXT("main"),
		TEXT("C:/Repo"),
		FString(),
		TEXT("git status --porcelain=v2 -b failed: test error"));
	TestEqual(TEXT("Git status errors are visible in the label"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Status Error")));
	TestTrue(TEXT("Git status errors are visible in the tooltip"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("git status --porcelain=v2 -b failed: test error")));
	TestTrue(TEXT("Git status errors are visible in the summary"), FSyncShieldToolbarTestAccessor::GetSummary(Toolbar).Contains(TEXT("Status: error")));
	TestFalse(TEXT("Git commands are disabled when status is unresolved"), FSyncShieldToolbarTestAccessor::CanExecuteGitCommand(Toolbar));
	TestTrue(TEXT("Status error state is degraded"), FSyncShieldToolbarTestAccessor::IsStatusDegraded(Toolbar));

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::Plastic,
		true,
		true,
		true,
		false,
		false,
		false,
		0,
		0,
		0,
		0,
		0,
		FString(),
		TEXT("C:/Workspace"),
		TEXT("WorkspaceAlpha"),
		TEXT("Plastic SCM login required."));
	TestFalse(TEXT("Plastic update is disabled when login is required"), FSyncShieldToolbarTestAccessor::CanExecutePlasticUpdate(Toolbar));

	FSyncShieldToolbarTestAccessor::SetStatus(
		Toolbar,
		FSyncShieldToolbarTestAccessor::EProvider::Plastic,
		true,
		true,
		false,
		true,
		false,
		false,
		0,
		0,
		0,
		0,
		0,
		TEXT("/main"),
		TEXT("C:/Workspace"),
		TEXT("WorkspaceAlpha"),
		TEXT("Plastic status --machinereadable failed: test error"));
	TestFalse(TEXT("Plastic update is disabled when status is unresolved"), FSyncShieldToolbarTestAccessor::CanExecutePlasticUpdate(Toolbar));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldGitDisposableEndToEndTest,
	"SyncShield.Editor.EndToEnd.GitDisposable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldGitDisposableEndToEndTest::RunTest(const FString& Parameters)
{
	TSharedRef<SSyncShieldToolbar> Toolbar = FSyncShieldToolbarTestAccessor::CreateToolbar();
	const FScopedAutomationDirectory TempRoot(MakeAutomationDirectoryPath(TEXT("GitDisposable")));
	const FString RootDir = TempRoot.GetPath();
	const FString RemoteDir = FPaths::Combine(RootDir, TEXT("Origin.git"));
	const FString RepoDir = FPaths::Combine(RootDir, TEXT("Workspace"));
	const FString PeerDir = FPaths::Combine(RootDir, TEXT("Peer"));
	const FString ReadmePath = FPaths::Combine(RepoDir, TEXT("README.txt"));
	const FString NotesPath = FPaths::Combine(RepoDir, TEXT("Notes.txt"));
	const FString PeerOnlyPath = FPaths::Combine(PeerDir, TEXT("PeerOnly.txt"));

	if (!RunGitChecked(*this, Toolbar, RootDir, FString::Printf(TEXT("init --bare \"%s\""), *RemoteDir), TEXT("Initialize bare git remote")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RootDir, FString::Printf(TEXT("init --initial-branch=main \"%s\""), *RepoDir), TEXT("Initialize disposable git repo")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("config user.name \"SyncShield Automation\""), TEXT("Configure disposable repo user.name")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("config user.email \"syncshield-automation@example.com\""), TEXT("Configure disposable repo user.email")))
	{
		return false;
	}
	if (!WriteTextFileChecked(*this, ReadmePath, TEXT("initial\n"), TEXT("Create initial tracked file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("add README.txt"), TEXT("Stage initial tracked file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("commit -m \"Initial commit\""), TEXT("Create initial commit")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, FString::Printf(TEXT("remote add origin \"%s\""), *RemoteDir), TEXT("Add disposable remote")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("push -u origin main"), TEXT("Push initial branch to remote")))
	{
		return false;
	}

	FSyncShieldToolbarTestAccessor::FStatusSnapshot Status;
	FString Error;
	bool bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo is detected"), bRepoFound);
	TestTrue(TEXT("Disposable git client is available"), Status.bClientAvailable);
	TestTrue(TEXT("Disposable git repo is marked present"), Status.bRepo);
	TestTrue(TEXT("Disposable git upstream is detected"), Status.bHasUpstream);
	TestFalse(TEXT("Disposable git clean state has no status error"), Status.bStatusError);
	TestEqual(TEXT("Disposable git branch is main"), Status.Branch, FString(TEXT("main")));
	TestEqual(TEXT("Disposable git clean staged count"), Status.Staged, 0);
	TestEqual(TEXT("Disposable git clean unstaged count"), Status.Unstaged, 0);
	TestEqual(TEXT("Disposable git clean untracked count"), Status.Untracked, 0);
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestEqual(TEXT("Disposable git clean label"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Clean")));
	TestTrue(TEXT("Disposable git fetch is enabled when clean"), FSyncShieldToolbarTestAccessor::CanExecuteGitCommand(Toolbar));
	TestFalse(TEXT("Disposable git pull is disabled when not behind"), FSyncShieldToolbarTestAccessor::CanExecuteGitPull(Toolbar));
	TestFalse(TEXT("Disposable git push is disabled when not ahead"), FSyncShieldToolbarTestAccessor::CanExecuteGitPush(Toolbar));
	TestTrue(TEXT("Disposable git clean tooltip reports upstream counts"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Ahead: 0  Behind: 0")));

	if (!WriteTextFileChecked(*this, NotesPath, TEXT("draft\n"), TEXT("Create untracked file in disposable repo")))
	{
		return false;
	}

	bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo remains detected with untracked files"), bRepoFound);
	TestEqual(TEXT("Disposable git untracked count reflects new file"), Status.Untracked, 1);
	TestEqual(TEXT("Disposable git staged count remains zero with untracked file"), Status.Staged, 0);
	TestEqual(TEXT("Disposable git unstaged count remains zero with untracked file"), Status.Unstaged, 0);
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestEqual(TEXT("Disposable git label shows changes for untracked file"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Changes")));
	TestFalse(TEXT("Disposable git pull is disabled while working tree is dirty"), FSyncShieldToolbarTestAccessor::CanExecuteGitPull(Toolbar));
	TestFalse(TEXT("Disposable git push is disabled while working tree is dirty"), FSyncShieldToolbarTestAccessor::CanExecuteGitPush(Toolbar));
	TestTrue(TEXT("Disposable git summary reports one untracked file"), FSyncShieldToolbarTestAccessor::GetSummary(Toolbar).Contains(TEXT("Untracked: 1")));

	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("add Notes.txt"), TEXT("Stage disposable untracked file")))
	{
		return false;
	}

	bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo remains detected with staged files"), bRepoFound);
	TestEqual(TEXT("Disposable git staged count reflects staged file"), Status.Staged, 1);
	TestEqual(TEXT("Disposable git untracked count resets after staging"), Status.Untracked, 0);
	TestEqual(TEXT("Disposable git unstaged count remains zero after staging"), Status.Unstaged, 0);
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestTrue(TEXT("Disposable git tooltip reports one staged file"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Staged: 1  Unstaged: 0  Untracked: 0")));

	if (!WriteTextFileChecked(*this, ReadmePath, TEXT("initial\nlocal-edit\n"), TEXT("Modify tracked file without staging")))
	{
		return false;
	}

	bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo remains detected with unstaged files"), bRepoFound);
	TestEqual(TEXT("Disposable git staged count stays at one while one file is unstaged"), Status.Staged, 1);
	TestEqual(TEXT("Disposable git unstaged count reflects tracked edit"), Status.Unstaged, 1);
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestTrue(TEXT("Disposable git tooltip reports staged and unstaged files"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Staged: 1  Unstaged: 1  Untracked: 0")));
	TestFalse(TEXT("Disposable git push stays disabled with unstaged edits"), FSyncShieldToolbarTestAccessor::CanExecuteGitPush(Toolbar));

	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("add README.txt"), TEXT("Stage modified tracked file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("commit -m \"Local snapshot\""), TEXT("Commit staged disposable changes")))
	{
		return false;
	}

	bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo remains detected after local commit"), bRepoFound);
	TestEqual(TEXT("Disposable git ahead count reflects local commit"), Status.Ahead, 1);
	TestEqual(TEXT("Disposable git behind count remains zero after local commit"), Status.Behind, 0);
	TestEqual(TEXT("Disposable git clean staged count after local commit"), Status.Staged, 0);
	TestEqual(TEXT("Disposable git clean unstaged count after local commit"), Status.Unstaged, 0);
	TestEqual(TEXT("Disposable git clean untracked count after local commit"), Status.Untracked, 0);
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestEqual(TEXT("Disposable git label shows ahead state"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Ahead 1")));
	TestTrue(TEXT("Disposable git push is enabled when clean and ahead"), FSyncShieldToolbarTestAccessor::CanExecuteGitPush(Toolbar));
	TestFalse(TEXT("Disposable git pull stays disabled when not behind"), FSyncShieldToolbarTestAccessor::CanExecuteGitPull(Toolbar));

	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("push"), TEXT("Push local disposable commit")))
	{
		return false;
	}

	bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo remains detected after pushing local commit"), bRepoFound);
	TestEqual(TEXT("Disposable git ahead count resets after push"), Status.Ahead, 0);
	TestEqual(TEXT("Disposable git behind count remains zero after push"), Status.Behind, 0);

	if (!RunGitChecked(*this, Toolbar, RootDir, FString::Printf(TEXT("clone --branch main \"%s\" \"%s\""), *RemoteDir, *PeerDir), TEXT("Clone disposable peer repo")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("config user.name \"SyncShield Peer\""), TEXT("Configure peer repo user.name")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("config user.email \"syncshield-peer@example.com\""), TEXT("Configure peer repo user.email")))
	{
		return false;
	}
	if (!WriteTextFileChecked(*this, PeerOnlyPath, TEXT("remote-only\n"), TEXT("Create remote-only peer file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("add PeerOnly.txt"), TEXT("Stage remote-only peer file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("commit -m \"Remote commit\""), TEXT("Create remote-only commit")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("push"), TEXT("Push remote-only commit")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("fetch origin"), TEXT("Fetch remote-only commit into disposable repo")))
	{
		return false;
	}

	bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Disposable git repo remains detected after fetching remote-only commit"), bRepoFound);
	TestTrue(TEXT("Disposable git upstream remains configured after fetch"), Status.bHasUpstream);
	TestEqual(TEXT("Disposable git behind count reflects remote-only commit"), Status.Behind, 1);
	TestEqual(TEXT("Disposable git ahead count remains zero when only remote advanced"), Status.Ahead, 0);
	TestEqual(TEXT("Disposable git clean staged count while behind"), Status.Staged, 0);
	TestEqual(TEXT("Disposable git clean unstaged count while behind"), Status.Unstaged, 0);
	TestEqual(TEXT("Disposable git clean untracked count while behind"), Status.Untracked, 0);
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestEqual(TEXT("Disposable git label shows behind state"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Behind 1")));
	TestTrue(TEXT("Disposable git pull is enabled when clean and behind"), FSyncShieldToolbarTestAccessor::CanExecuteGitPull(Toolbar));
	TestFalse(TEXT("Disposable git push is disabled when behind"), FSyncShieldToolbarTestAccessor::CanExecuteGitPush(Toolbar));
	TestEqual(TEXT("Disposable git behind state uses sync color"), FSyncShieldToolbarTestAccessor::GetColor(Toolbar), FLinearColor(0.0f, 0.45f, 1.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldGitDivergedEndToEndTest,
	"SyncShield.Editor.EndToEnd.GitDiverged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldGitDivergedEndToEndTest::RunTest(const FString& Parameters)
{
	TSharedRef<SSyncShieldToolbar> Toolbar = FSyncShieldToolbarTestAccessor::CreateToolbar();
	const FScopedAutomationDirectory TempRoot(MakeAutomationDirectoryPath(TEXT("GitDiverged")));
	const FString RootDir = TempRoot.GetPath();
	const FString RemoteDir = FPaths::Combine(RootDir, TEXT("Origin.git"));
	const FString RepoDir = FPaths::Combine(RootDir, TEXT("Workspace"));
	const FString PeerDir = FPaths::Combine(RootDir, TEXT("Peer"));
	const FString ReadmePath = FPaths::Combine(RepoDir, TEXT("README.txt"));
	const FString PeerOnlyPath = FPaths::Combine(PeerDir, TEXT("PeerOnly.txt"));

	if (!RunGitChecked(*this, Toolbar, RootDir, FString::Printf(TEXT("init --bare \"%s\""), *RemoteDir), TEXT("Initialize diverged bare git remote")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RootDir, FString::Printf(TEXT("init --initial-branch=main \"%s\""), *RepoDir), TEXT("Initialize diverged disposable git repo")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("config user.name \"SyncShield Automation\""), TEXT("Configure diverged repo user.name")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("config user.email \"syncshield-automation@example.com\""), TEXT("Configure diverged repo user.email")))
	{
		return false;
	}
	if (!WriteTextFileChecked(*this, ReadmePath, TEXT("initial\n"), TEXT("Create diverged initial tracked file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("add README.txt"), TEXT("Stage diverged initial tracked file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("commit -m \"Initial commit\""), TEXT("Create diverged initial commit")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, FString::Printf(TEXT("remote add origin \"%s\""), *RemoteDir), TEXT("Add diverged disposable remote")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("push -u origin main"), TEXT("Push diverged initial branch to remote")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RootDir, FString::Printf(TEXT("clone --branch main \"%s\" \"%s\""), *RemoteDir, *PeerDir), TEXT("Clone diverged peer repo")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("config user.name \"SyncShield Peer\""), TEXT("Configure diverged peer repo user.name")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("config user.email \"syncshield-peer@example.com\""), TEXT("Configure diverged peer repo user.email")))
	{
		return false;
	}
	if (!WriteTextFileChecked(*this, PeerOnlyPath, TEXT("remote-only\n"), TEXT("Create diverged remote-only peer file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("add PeerOnly.txt"), TEXT("Stage diverged remote-only peer file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("commit -m \"Remote commit\""), TEXT("Create diverged remote-only commit")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, PeerDir, TEXT("push"), TEXT("Push diverged remote-only commit")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("fetch origin"), TEXT("Fetch diverged remote-only commit into disposable repo")))
	{
		return false;
	}
	if (!WriteTextFileChecked(*this, ReadmePath, TEXT("initial\nlocal-diverged-edit\n"), TEXT("Modify diverged tracked file locally")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("add README.txt"), TEXT("Stage diverged local tracked file")))
	{
		return false;
	}
	if (!RunGitChecked(*this, Toolbar, RepoDir, TEXT("commit -m \"Local diverged commit\""), TEXT("Create diverged local commit")))
	{
		return false;
	}

	FSyncShieldToolbarTestAccessor::FStatusSnapshot Status;
	FString Error;
	const bool bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulateGitStatus(Toolbar, RepoDir, Status, Error);
	TestTrue(TEXT("Diverged disposable git repo is detected"), bRepoFound);
	TestTrue(TEXT("Diverged disposable git upstream is detected"), Status.bHasUpstream);
	TestFalse(TEXT("Diverged disposable git has no status error"), Status.bStatusError);
	TestEqual(TEXT("Diverged disposable git ahead count reflects local commit"), Status.Ahead, 1);
	TestEqual(TEXT("Diverged disposable git behind count reflects remote commit"), Status.Behind, 1);
	TestEqual(TEXT("Diverged disposable git staged count is clean"), Status.Staged, 0);
	TestEqual(TEXT("Diverged disposable git unstaged count is clean"), Status.Unstaged, 0);
	TestEqual(TEXT("Diverged disposable git untracked count is clean"), Status.Untracked, 0);

	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestEqual(TEXT("Diverged disposable git label shows diverged state"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("main | Diverged")));
	TestTrue(TEXT("Diverged disposable git tooltip reports ahead and behind counts"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Ahead: 1  Behind: 1")));
	TestTrue(TEXT("Diverged disposable git summary reports ahead and behind counts"), FSyncShieldToolbarTestAccessor::GetSummary(Toolbar).Contains(TEXT("Ahead: 1  Behind: 1")));
	TestEqual(TEXT("Diverged disposable git color is critical"), FSyncShieldToolbarTestAccessor::GetColor(Toolbar), FLinearColor(1.0f, 0.2f, 0.2f));
	TestTrue(TEXT("Diverged disposable git state is degraded"), FSyncShieldToolbarTestAccessor::IsStatusDegraded(Toolbar));
	TestTrue(TEXT("Diverged disposable git generic commands remain enabled"), FSyncShieldToolbarTestAccessor::CanExecuteGitCommand(Toolbar));
	TestTrue(TEXT("Diverged disposable git pull is enabled"), FSyncShieldToolbarTestAccessor::CanExecuteGitPull(Toolbar));
	TestFalse(TEXT("Diverged disposable git push is disabled"), FSyncShieldToolbarTestAccessor::CanExecuteGitPush(Toolbar));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldPlasticLiveProbeTest,
	"SyncShield.Editor.EndToEnd.PlasticLiveProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldPlasticLiveProbeTest::RunTest(const FString& Parameters)
{
	TSharedRef<SSyncShieldToolbar> Toolbar = FSyncShieldToolbarTestAccessor::CreateToolbar();
	FSyncShieldToolbarTestAccessor::FStatusSnapshot Status;
	FString Error;

	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const bool bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulatePlasticStatus(Toolbar, ProjectDir, Status, Error);

	if (!Status.bClientAvailable)
	{
		return true;
	}

	if (Status.bAuthRequired)
	{
		TestFalse(TEXT("Plastic auth-required probe does not claim a workspace"), Status.bRepo);
		TestEqual(TEXT("Plastic auth-required probe surfaces canonical error"), Error, FString(TEXT("Plastic SCM login required.")));
		TestTrue(TEXT("Plastic auth-required probe keeps raw CLI details"), !Status.LastError.IsEmpty());
		FSyncShieldToolbarTestAccessor::SetUnsavedState(Toolbar, true, 2, TEXT("/Game/Maps/ValidationMap"));
		FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
		TestEqual(TEXT("Plastic auth-required label includes unsaved count"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar), FString(TEXT("Login Required | Unsaved 2")));
		TestTrue(TEXT("Plastic auth-required tooltip includes unsaved count"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(TEXT("Unsaved assets: 2")));
		return true;
	}

	if (!bRepoFound)
	{
		TestFalse(TEXT("Plastic live probe keeps repo flag clear when project is outside a workspace"), Status.bRepo);
		TestFalse(TEXT("Plastic live probe does not mark missing workspace as auth-required"), Status.bAuthRequired);
		TestFalse(TEXT("Plastic live probe does not raise status-error for plain missing workspace"), Status.bStatusError);
		TestTrue(TEXT("Plastic live probe returns a diagnostic when project is outside a workspace"), !Error.IsEmpty() || !Status.LastError.IsEmpty());
		FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
		TestTrue(TEXT("Plastic live probe missing-workspace label is visible"), FSyncShieldToolbarTestAccessor::GetLabel(Toolbar).StartsWith(TEXT("No SCM Repo")));
		return true;
	}

	TestTrue(TEXT("Plastic live probe marks workspace present when detected"), Status.bRepo);
	TestFalse(TEXT("Plastic live probe configured workspace does not require auth"), Status.bAuthRequired);
	TestFalse(TEXT("Plastic live probe configured workspace has no status error"), Status.bStatusError);
	TestTrue(TEXT("Plastic live probe provides workspace root"), !Status.RepoRoot.IsEmpty());
	TestTrue(TEXT("Plastic live probe provides workspace or branch context"), !Status.WorkspaceName.IsEmpty() || !Status.Branch.IsEmpty());
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestTrue(TEXT("Plastic live probe tooltip includes workspace root"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(Status.RepoRoot));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSyncShieldPlasticConfiguredWorkspaceTest,
	"SyncShield.Editor.EndToEnd.PlasticConfiguredWorkspace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSyncShieldPlasticConfiguredWorkspaceTest::RunTest(const FString& Parameters)
{
	const FString WorkspacePath = FPlatformMisc::GetEnvironmentVariable(TEXT("SYNCSHIELD_PLASTIC_WORKSPACE"));
	if (WorkspacePath.IsEmpty())
	{
		return true;
	}

	TSharedRef<SSyncShieldToolbar> Toolbar = FSyncShieldToolbarTestAccessor::CreateToolbar();
	FSyncShieldToolbarTestAccessor::FStatusSnapshot Status;
	FString Error;

	TestTrue(TEXT("Configured Plastic workspace path exists"), IFileManager::Get().DirectoryExists(*WorkspacePath));
	if (!IFileManager::Get().DirectoryExists(*WorkspacePath))
	{
		return false;
	}

	const bool bRepoFound = FSyncShieldToolbarTestAccessor::TryPopulatePlasticStatus(Toolbar, WorkspacePath, Status, Error);
	TestTrue(TEXT("Configured Plastic workspace is detected"), bRepoFound);
	TestTrue(TEXT("Configured Plastic workspace reports client availability"), Status.bClientAvailable);
	TestTrue(TEXT("Configured Plastic workspace reports repo presence"), Status.bRepo);
	TestFalse(TEXT("Configured Plastic workspace does not require auth"), Status.bAuthRequired);
	TestFalse(TEXT("Configured Plastic workspace has no status error"), Status.bStatusError);
	TestTrue(TEXT("Configured Plastic workspace provides workspace root"), !Status.RepoRoot.IsEmpty());
	TestTrue(TEXT("Configured Plastic workspace provides workspace name"), !Status.WorkspaceName.IsEmpty());
	FSyncShieldToolbarTestAccessor::ApplyStatusSnapshot(Toolbar, Status);
	TestTrue(TEXT("Configured Plastic workspace tooltip includes root"), FSyncShieldToolbarTestAccessor::GetTooltip(Toolbar).Contains(Status.RepoRoot));
	TestTrue(TEXT("Configured Plastic workspace label is populated"), !FSyncShieldToolbarTestAccessor::GetLabel(Toolbar).IsEmpty());

	return true;
}

#endif

