#include "BentoFactory.h"
#include "BentoMeshBuilder.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

UBentoFactory::UBentoFactory()
{
	bCreateNew = false;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UPaperFlipbook::StaticClass();

	Formats.Add(TEXT("paper2d.json;BentoPack Paper2D JSON"));
	Formats.Add(TEXT("bento;BentoPack Project Archive"));
}

bool UBentoFactory::FactoryCanImport(const FString& Filename)
{
	FString Extension = FPaths::GetExtension(Filename).ToLower();
	if (Extension == TEXT("bento"))
	{
		return true;
	}
	if (Filename.EndsWith(TEXT(".paper2d.json"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	return false;
}

UObject* UBentoFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *Filename))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("[BentoPack] Failed to read file: %s"), *Filename);
		return nullptr;
	}

	return ImportFromJsonString(JsonContent, Filename, InParent, InName, Flags, Warn);
}

UObject* UBentoFactory::FactoryCreateText(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	const TCHAR* Type,
	const TCHAR*& Buffer,
	const TCHAR* BufferEnd,
	FFeedbackContext* Warn)
{
	FString JsonContent(BufferEnd - Buffer, Buffer);
	return ImportFromJsonString(JsonContent, InName.ToString(), InParent, InName, Flags, Warn);
}

UObject* UBentoFactory::ImportFromJsonString(
	const FString& JsonContent,
	const FString& SourceFilePath,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	FFeedbackContext* Warn)
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("[BentoPack] Failed to parse JSON descriptor for: %s"), *SourceFilePath);
		return nullptr;
	}

	FString Directory = FPaths::GetPath(SourceFilePath);
	FString PackagePath = InParent->GetPathName();

	// 1. Locate Texture Atlas
	FString SourceTextureName = RootObject->GetStringField(TEXT("sourceTexture"));
	FString FullTexturePath = FPaths::Combine(Directory, SourceTextureName);

	UTexture2D* AtlasTexture = nullptr;
	if (FPaths::FileExists(FullTexturePath))
	{
		// Try to load existing texture or find in package
		FString TextureAssetName = FPaths::GetBaseFilename(SourceTextureName);
		FString TexturePackageName = FPaths::Combine(PackagePath, TextureAssetName);
		AtlasTexture = FindObject<UTexture2D>(nullptr, *TexturePackageName);
	}

	TMap<FString, UPaperSprite*> SpriteMap;
	UPaperSprite* FirstSprite = nullptr;

	// 2. Parse and Create Sprites
	const TArray<TSharedPtr<FJsonValue>>* SpritesJson = nullptr;
	if (RootObject->TryGetArrayField(TEXT("sprites"), SpritesJson) && SpritesJson != nullptr)
	{
		for (int32 Index = 0; Index < SpritesJson->Num(); ++Index)
		{
			TSharedPtr<FJsonObject> SpriteObj = (*SpritesJson)[Index]->AsObject();
			if (!SpriteObj.IsValid()) continue;

			FBentoSpriteDescriptor SpriteDesc;
			SpriteDesc.Name = SpriteObj->GetStringField(TEXT("name"));

			TSharedPtr<FJsonObject> UVObj = SpriteObj->GetObjectField(TEXT("sourceUV"));
			TSharedPtr<FJsonObject> DimObj = SpriteObj->GetObjectField(TEXT("sourceDimension"));
			TSharedPtr<FJsonObject> PivObj = SpriteObj->GetObjectField(TEXT("pivot"));

			int32 X = UVObj.IsValid() ? UVObj->GetIntegerField(TEXT("x")) : 0;
			int32 Y = UVObj.IsValid() ? UVObj->GetIntegerField(TEXT("y")) : 0;
			int32 W = DimObj.IsValid() ? DimObj->GetIntegerField(TEXT("x")) : 32;
			int32 H = DimObj.IsValid() ? DimObj->GetIntegerField(TEXT("y")) : 32;

			SpriteDesc.SourceRect = FIntRect(X, Y, X + W, Y + H);
			SpriteDesc.Pivot = PivObj.IsValid() ? FVector2D(PivObj->GetNumberField(TEXT("x")), PivObj->GetNumberField(TEXT("y"))) : FVector2D(0.5f, 0.5f);

			// Render Geometry (M8 Tight Mesh)
			const TSharedPtr<FJsonObject>* RenderGeomObj = nullptr;
			if (SpriteObj->TryGetObjectField(TEXT("renderGeometry"), RenderGeomObj) && RenderGeomObj != nullptr)
			{
				SpriteDesc.TightMesh.bHasTightMesh = (*RenderGeomObj)->GetBoolField(TEXT("hasTightMesh"));
				const TArray<TSharedPtr<FJsonValue>>* VertsJson = nullptr;
				if ((*RenderGeomObj)->TryGetArrayField(TEXT("vertices"), VertsJson) && VertsJson != nullptr)
				{
					for (auto& VVal : *VertsJson)
					{
						TSharedPtr<FJsonObject> VObj = VVal->AsObject();
						if (VObj.IsValid())
						{
							SpriteDesc.TightMesh.Vertices.Add(FVector2D(VObj->GetNumberField(TEXT("x")), VObj->GetNumberField(TEXT("y"))));
						}
					}
				}
			}

			// Collision Geometry
			const TSharedPtr<FJsonObject>* ColGeomObj = nullptr;
			if (SpriteObj->TryGetObjectField(TEXT("collisionGeometry"), ColGeomObj) && ColGeomObj != nullptr)
			{
				const TArray<TSharedPtr<FJsonValue>>* ColPolyJson = nullptr;
				if ((*ColGeomObj)->TryGetArrayField(TEXT("polygon"), ColPolyJson) && ColPolyJson != nullptr)
				{
					for (auto& VVal : *ColPolyJson)
					{
						TSharedPtr<FJsonObject> VObj = VVal->AsObject();
						if (VObj.IsValid())
						{
							SpriteDesc.CollisionPolygon.Add(FVector2D(VObj->GetNumberField(TEXT("x")), VObj->GetNumberField(TEXT("y"))));
						}
					}
				}
			}

			// Construct UPaperSprite
			FName SpriteFName(*SpriteDesc.Name);
			UPaperSprite* NewSprite = NewObject<UPaperSprite>(InParent, SpriteFName, Flags | RF_Public);
			if (NewSprite)
			{
				if (AtlasTexture)
				{
					NewSprite->SetSourceTexture(AtlasTexture);
				}
				NewSprite->SetSourceUV(FVector2D(X, Y));
				NewSprite->SetSourceDimension(FVector2D(W, H));
				NewSprite->SetPivotMode(ESpritePivotMode::Custom, SpriteDesc.Pivot);

				// Apply M8 Tight Mesh RenderGeometry and CollisionGeometry
				FBentoMeshBuilder::ApplyTightMesh(NewSprite, SpriteDesc);

				FAssetRegistryModule::AssetCreated(NewSprite);
				NewSprite->MarkPackageDirty();

				SpriteMap.Add(SpriteDesc.Name, NewSprite);
				if (!FirstSprite) FirstSprite = NewSprite;
			}
		}
	}

	// 3. Parse and Create Flipbooks
	UPaperFlipbook* PrimaryFlipbook = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* FlipbooksJson = nullptr;
	if (RootObject->TryGetArrayField(TEXT("flipbooks"), FlipbooksJson) && FlipbooksJson != nullptr)
	{
		for (auto& FbVal : *FlipbooksJson)
		{
			TSharedPtr<FJsonObject> FbObj = FbVal->AsObject();
			if (!FbObj.IsValid()) continue;

			FString AnimName = FbObj->GetStringField(TEXT("name"));
			float FPS = FbObj->HasField(TEXT("framesPerSecond")) ? (float)FbObj->GetNumberField(TEXT("framesPerSecond")) : 12.0f;

			FName FbFName(*AnimName);
			UPaperFlipbook* NewFlipbook = NewObject<UPaperFlipbook>(InParent, FbFName, Flags | RF_Public);
			if (NewFlipbook)
			{
				NewFlipbook->SetFramesPerSecond(FPS > 0.0f ? FPS : 12.0f);

				const TArray<TSharedPtr<FJsonValue>>* KeyframesJson = nullptr;
				if (FbObj->TryGetArrayField(TEXT("keyframes"), KeyframesJson) && KeyframesJson != nullptr)
				{
					for (auto& KfVal : *KeyframesJson)
					{
						FString SpriteName = KfVal->AsString();
						if (SpriteMap.Contains(SpriteName))
						{
							FPaperFlipbookKeyFrame KeyFrame;
							KeyFrame.Sprite = SpriteMap[SpriteName];
							KeyFrame.FrameRun = 1;
							NewFlipbook->KeyFrames.Add(KeyFrame);
						}
					}
				}

				FAssetRegistryModule::AssetCreated(NewFlipbook);
				NewFlipbook->MarkPackageDirty();

				if (!PrimaryFlipbook) PrimaryFlipbook = NewFlipbook;
			}
		}
	}

	return PrimaryFlipbook ? (UObject*)PrimaryFlipbook : (UObject*)FirstSprite;
}
