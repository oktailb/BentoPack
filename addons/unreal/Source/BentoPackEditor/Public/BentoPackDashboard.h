#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class BENTOPACKEDITOR_API SBentoPackDashboard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBentoPackDashboard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText SourcePath;
	FText TargetPath;
	int32 SliceMode = 0; // 0: Auto, 1: Grid, 2: Files
	int32 TileWidth = 32;
	int32 TileHeight = 32;
	int32 PackingAlgorithm = 0; // 0: MaxRects, 1: M8 Tight
	bool bWatchMode = false;
	FText StatusMessage;

	FReply OnProcessClicked();
	FReply OnStopWatchClicked();
	FReply OnOpenGuiClicked();
	FReply OnDownloadCliClicked();
	FReply OnBrowseSourceClicked();
	FReply OnBrowseTargetClicked();
};
