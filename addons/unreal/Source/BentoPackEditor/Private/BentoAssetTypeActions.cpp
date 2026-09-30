#include "BentoAssetTypeActions.h"
#include "BentoCliBridge.h"
#include "PaperFlipbook.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "BentoAssetTypeActions"

FText FBentoAssetTypeActions::GetName() const
{
	return LOCTEXT("BentoAssetTypeActions_Name", "BentoPack Flipbook");
}

FColor FBentoAssetTypeActions::GetTypeColor() const
{
	return FColor(50, 180, 255);
}

UClass* FBentoAssetTypeActions::GetSupportedClass() const
{
	return UPaperFlipbook::StaticClass();
}

uint32 FBentoAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Animation | EAssetTypeCategories::Basic;
}

void FBentoAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UPaperFlipbook>> Flipbooks;
	for (UObject* Obj : InObjects)
	{
		if (UPaperFlipbook* Fb = Cast<UPaperFlipbook>(Obj))
		{
			Flipbooks.Add(Fb);
		}
	}

	Section.AddMenuEntry(
		"OpenInBentoPack",
		LOCTEXT("OpenInBentoPack_Label", "Open in BentoPack Studio"),
		LOCTEXT("OpenInBentoPack_Tooltip", "Opens this asset or its source atlas directly in BentoPack Studio."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([Flipbooks]()
			{
				if (Flipbooks.Num() > 0 && Flipbooks[0].IsValid())
				{
					FString AssetPath = Flipbooks[0]->GetPathName();
					FBentoCliBridge::OpenInBentoPack(AssetPath);
				}
			})
		)
	);
}

#undef LOCTEXT_NAMESPACE
