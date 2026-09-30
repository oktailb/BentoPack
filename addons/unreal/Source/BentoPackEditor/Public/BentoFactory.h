#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "BentoTypes.h"
#include "BentoFactory.generated.h"

class UPaperSprite;
class UPaperFlipbook;
class UTexture2D;

/**
 * Factory for importing BentoPack (.bento & .paper2d.json) project archives into Unreal Engine.
 * Automatically creates Texture2D, UPaperSprite (with M8 tight polygonal meshes), and UPaperFlipbook assets.
 */
UCLASS()
class BENTOPACKEDITOR_API UBentoFactory : public UFactory
{
	GENERATED_BODY()

public:
	UBentoFactory();

	// UFactory interface
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;

private:
	UObject* ImportFromJsonString(const FString& JsonContent, const FString& SourceFilePath, UObject* InParent, FName InName, EObjectFlags Flags, FFeedbackContext* Warn);
};
