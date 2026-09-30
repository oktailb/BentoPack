#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class BENTOPACKEDITOR_API FBentoAssetTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
};
