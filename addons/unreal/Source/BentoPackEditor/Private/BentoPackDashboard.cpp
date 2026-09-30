#include "BentoPackDashboard.h"
#include "BentoCliBridge.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SComboBox.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorDirectories.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "SBentoPackDashboard"

void SBentoPackDashboard::Construct(const FArguments& InArgs)
{
	StatusMessage = LOCTEXT("StatusReady", "Ready.");
	TargetPath = FText::FromString(TEXT("Content/2D/Character.paper2d.json"));

	bool bCliFound = !FBentoCliBridge::FindCliPath().IsEmpty();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// Header Bar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DashboardTitle", "BentoPack 2D Dashboard"))
					.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenStudio", "Open BentoPack Studio"))
					.OnClicked(this, &SBentoPackDashboard::OnOpenGuiClicked)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(SSeparator)
			]

			// CLI Status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(bCliFound ? LOCTEXT("CliOk", "CLI Status: Connected") : LOCTEXT("CliMissing", "CLI Status: Not Detected"))
					.ColorAndOpacity(bCliFound ? FLinearColor(0.3f, 1.0f, 0.4f) : FLinearColor(1.0f, 0.4f, 0.4f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("DownloadCli", "Download CLI"))
					.Visibility(bCliFound ? EVisibility::Collapsed : EVisibility::Visible)
					.OnClicked(this, &SBentoPackDashboard::OnDownloadCliClicked)
				]
			]

			// Source Atlas
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SourceLabel", "Source Texture Atlas or Frame Folder:"))
				.Font(FAppStyle::GetFontStyle("NormalFontBold"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, [this]() { return SourcePath; })
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type) { SourcePath = NewText; })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseBtn", "Browse..."))
					.OnClicked(this, &SBentoPackDashboard::OnBrowseSourceClicked)
				]
			]

			// Target Project
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TargetLabel", "Target Output (.bento or .paper2d.json):"))
				.Font(FAppStyle::GetFontStyle("NormalFontBold"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, [this]() { return TargetPath; })
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type) { TargetPath = NewText; })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseTargetBtn", "Browse..."))
					.OnClicked(this, &SBentoPackDashboard::OnBrowseTargetClicked)
				]
			]

			// Watch Mode Checkbox
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 12)
			[
				SNew(SCheckBox)
				.IsChecked(this, [this]() { return bWatchMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bWatchMode = (NewState == ECheckBoxState::Checked); })
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WatchModeLabel", " Watch Daemon: Auto-repack and hot-reload upon file changes"))
				]
			]

			// Action Buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("ProcessAtlasBtn", "Process & Pack Atlas"))
					.ButtonColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.3f))
					.OnClicked(this, &SBentoPackDashboard::OnProcessClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("StopWatcherBtn", "Stop Watcher"))
					.IsEnabled_Lambda([]() { return FBentoCliBridge::IsWatchActive(); })
					.OnClicked(this, &SBentoPackDashboard::OnStopWatchClicked)
				]
			]

			// Status Text Block
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.Text(this, [this]() { return StatusMessage; })
			]
		]
	];
}

FReply SBentoPackDashboard::OnProcessClicked()
{
	FString Src = SourcePath.ToString().TrimStartAndEnd();
	FString Dst = TargetPath.ToString().TrimStartAndEnd();

	if (Src.IsEmpty())
	{
		StatusMessage = LOCTEXT("ErrSpecifySrc", "Error: Please specify a source atlas or directory.");
		return FReply::Handled();
	}

	FString Args = FString::Printf(TEXT("pack --data \"%s\" \"%s\""), *Dst, *Src);
	if (bWatchMode)
	{
		Args = TEXT("--watch ") + Args;
		int32 Pid = -1;
		FString Err;
		if (FBentoCliBridge::StartWatchDaemon(Args, Pid, Err))
		{
			StatusMessage = FText::Format(LOCTEXT("WatchActive", "Watch Daemon active (PID: {0})"), FText::AsNumber(Pid));
		}
		else
		{
			StatusMessage = FText::Format(LOCTEXT("WatchErr", "Error starting watch daemon: {0}"), FText::FromString(Err));
		}
	}
	else
	{
		FString Out, Err;
		if (FBentoCliBridge::RunCliCommand(Args, Out, Err))
		{
			StatusMessage = LOCTEXT("PackSuccess", "Successfully packed atlas! Refreshing content...");
			// Refresh Asset Registry
			FAssetRegistryModule::GetRegistry().SearchAllAssets(true);
		}
		else
		{
			StatusMessage = FText::Format(LOCTEXT("PackFail", "Pack failed: {0}"), FText::FromString(Err));
		}
	}

	return FReply::Handled();
}

FReply SBentoPackDashboard::OnStopWatchClicked()
{
	FBentoCliBridge::StopWatchDaemon();
	StatusMessage = LOCTEXT("WatchStopped", "Watch Daemon stopped.");
	return FReply::Handled();
}

FReply SBentoPackDashboard::OnOpenGuiClicked()
{
	FString Target = TargetPath.ToString();
	FBentoCliBridge::OpenInBentoPack(Target);
	return FReply::Handled();
}

FReply SBentoPackDashboard::OnDownloadCliClicked()
{
	FPlatformProcess::LaunchURL(*FBentoCliBridge::ReleasesUrl, nullptr, nullptr);
	return FReply::Handled();
}

FReply SBentoPackDashboard::OnBrowseSourceClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		if (DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Select Source Atlas"),
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN),
			TEXT(""),
			TEXT("Atlas Images (*.png;*.webp;*.jpg)|*.png;*.webp;*.jpg|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			OutFiles))
		{
			if (OutFiles.Num() > 0)
			{
				SourcePath = FText::FromString(OutFiles[0]);
			}
		}
	}
	return FReply::Handled();
}

FReply SBentoPackDashboard::OnBrowseTargetClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		if (DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Target Output File"),
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_SAVE),
			TEXT("character.paper2d.json"),
			TEXT("Paper2D JSON (*.paper2d.json)|*.paper2d.json|BentoPack Archive (*.bento)|*.bento"),
			EFileDialogFlags::None,
			OutFiles))
		{
			if (OutFiles.Num() > 0)
			{
				TargetPath = FText::FromString(OutFiles[0]);
			}
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
