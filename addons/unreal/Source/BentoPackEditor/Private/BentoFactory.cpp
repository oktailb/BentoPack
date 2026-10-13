#include "BentoFactory.h"
#include "BentoMeshBuilder.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PackageTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

class FPaperSpriteInternalAccessor : public UPaperSprite
{
public:
	static void SetSourceData(UPaperSprite* Sprite, UTexture2D* Texture, const FVector2D& UV, const FVector2D& Dim)
	{
		auto* Acc = static_cast<FPaperSpriteInternalAccessor*>(Sprite);
		Acc->SourceTexture = Texture;
		Acc->SourceUV = UV;
		Acc->SourceDimension = Dim;
	}
};

UBentoFactory::UBentoFactory()
{
	bCreateNew = false;
	bEditAfterNew = true;
	bEditorImport = true;
	bText = false; // Must be false so Unreal doesn't treat binary .bento archives as raw text buffers
	SupportedClass = UPaperFlipbook::StaticClass();

	Formats.Add(TEXT("bento;BentoPack Project Archive"));
	Formats.Add(TEXT("paper2d.json;BentoPack Paper2D JSON"));
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

	FString Extension = FPaths::GetExtension(Filename).ToLower();
	if (Extension == TEXT("bento"))
	{
		FString BaseName = FPaths::GetBaseFilename(Filename);

		// Automated extraction of .bento archive
		FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BentoPackTemp"), BaseName);
		IFileManager::Get().MakeDirectory(*TempDir, true);

		// Run unzip on the .bento archive
		FString UnzipArgs = FString::Printf(TEXT("-q -o \"%s\" -d \"%s\""), *Filename, *TempDir);
		FProcHandle Proc = FPlatformProcess::CreateProc(TEXT("unzip"), *UnzipArgs, true, true, true, nullptr, 0, nullptr, nullptr);
		if (Proc.IsValid())
		{
			FPlatformProcess::WaitForProc(Proc);
		}

		// Decompress WebP atlas to PNG from the extracted archive
		FString WebpPath = FPaths::Combine(TempDir, TEXT("bento"), TEXT("assets"), TEXT("atlas.webp"));
		if (!FPaths::FileExists(WebpPath))
		{
			WebpPath = FPaths::Combine(TempDir, TEXT("assets"), TEXT("atlas.webp"));
		}

		FString ExtractedPng = FPaths::Combine(TempDir, BaseName + TEXT("_Atlas.png"));
		if (FPaths::FileExists(WebpPath))
		{
			FString DwebpArgs = FString::Printf(TEXT("\"%s\" -o \"%s\""), *WebpPath, *ExtractedPng);
			FProcHandle DwebpProc = FPlatformProcess::CreateProc(TEXT("dwebp"), *DwebpArgs, true, true, true, nullptr, 0, nullptr, nullptr);
			if (DwebpProc.IsValid())
			{
				FPlatformProcess::WaitForProc(DwebpProc);
			}
		}

		// Locate metadata descriptor
		FString ExtractedJson = FPaths::Combine(TempDir, TEXT("bento"), TEXT("project.json"));
		if (!FPaths::FileExists(ExtractedJson))
		{
			ExtractedJson = FPaths::Combine(TempDir, TEXT("project.json"));
		}
		if (!FPaths::FileExists(ExtractedJson))
		{
			ExtractedJson = FPaths::Combine(TempDir, BaseName + TEXT(".paper2d.json"));
		}

		if (FPaths::FileExists(ExtractedJson))
		{
			FString JsonContent;
			if (FFileHelper::LoadFileToString(JsonContent, *ExtractedJson))
			{
				return ImportFromJsonString(JsonContent, ExtractedJson, InParent, InName, Flags, Warn, ExtractedPng);
			}
		}

		Warn->Logf(ELogVerbosity::Error, TEXT("[BentoPack] Failed to extract archive: %s"), *Filename);
		return nullptr;
	}

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
	if (!Buffer || BufferEnd <= Buffer)
	{
		return nullptr;
	}

	// Verify buffer looks like text JSON (starts with '{' or '[')
	const TCHAR* Cur = Buffer;
	while (Cur < BufferEnd && FChar::IsWhitespace(*Cur))
	{
		Cur++;
	}
	if (Cur >= BufferEnd || (*Cur != TEXT('{') && *Cur != TEXT('[')))
	{
		// Binary container (e.g. .bento zip) passed via text interface: reject to force FactoryCreateFile
		return nullptr;
	}

	FString JsonContent(BufferEnd - Buffer, Buffer);
	return ImportFromJsonString(JsonContent, InName.ToString(), InParent, InName, Flags, Warn);
}

UObject* UBentoFactory::ImportFromJsonString(
	const FString& JsonContent,
	const FString& SourceFilePath,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	FFeedbackContext* Warn,
	const FString& OverrideTexturePath)
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("[BentoPack] Failed to parse JSON descriptor for: %s"), *SourceFilePath);
		return nullptr;
	}

	FString Directory = FPaths::GetPath(SourceFilePath);
	FString BaseName = FPaths::GetBaseFilename(SourceFilePath);
	if (BaseName.EndsWith(TEXT(".paper2d"), ESearchCase::IgnoreCase))
	{
		BaseName.LeftChopInline(8);
	}

	FString LongPackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName());
	if (LongPackagePath.IsEmpty())
	{
		LongPackagePath = TEXT("/Game");
	}

	FString SpritesFolderPath = LongPackagePath / (BaseName + TEXT("_Sprites"));
	FString AnimsFolderPath = LongPackagePath / (BaseName + TEXT("_Animations"));

	// 1. Locate and Import Texture Atlas
	FString FullTexturePath;
	if (!OverrideTexturePath.IsEmpty() && FPaths::FileExists(OverrideTexturePath))
	{
		FullTexturePath = OverrideTexturePath;
	}
	else
	{
		FString SourceTextureName;
		if (RootObject->HasField(TEXT("sourceTexture")))
		{
			SourceTextureName = RootObject->GetStringField(TEXT("sourceTexture"));
		}
		else if (RootObject->HasTypedField<EJson::Object>(TEXT("atlas")))
		{
			SourceTextureName = RootObject->GetObjectField(TEXT("atlas"))->GetStringField(TEXT("file"));
		}
		else if (RootObject->HasTypedField<EJson::Object>(TEXT("meta")))
		{
			SourceTextureName = RootObject->GetObjectField(TEXT("meta"))->GetStringField(TEXT("image"));
		}

		if (!SourceTextureName.IsEmpty())
		{
			FullTexturePath = FPaths::Combine(Directory, SourceTextureName);
			if (!FPaths::FileExists(FullTexturePath))
			{
				FullTexturePath = FPaths::Combine(Directory, FPaths::GetCleanFilename(SourceTextureName));
			}
		}
		if (FullTexturePath.IsEmpty() || !FPaths::FileExists(FullTexturePath))
		{
			FString Candidate = FPaths::Combine(Directory, BaseName + TEXT(".png"));
			if (FPaths::FileExists(Candidate))
			{
				FullTexturePath = Candidate;
			}
		}
	}

	UTexture2D* AtlasTexture = nullptr;
	FString TextureAssetName = BaseName + TEXT("_Atlas");
	FString TexturePackagePath = LongPackagePath / TextureAssetName;

	if (FPaths::FileExists(FullTexturePath))
	{
		UPackage* TexturePackage = CreatePackage(*TexturePackagePath);
		UTextureFactory* TexFactory = NewObject<UTextureFactory>();
		bool bCanceled = false;
		AtlasTexture = Cast<UTexture2D>(TexFactory->FactoryCreateFile(
			UTexture2D::StaticClass(),
			TexturePackage,
			*TextureAssetName,
			Flags | RF_Public | RF_Standalone,
			FullTexturePath,
			nullptr,
			Warn,
			bCanceled
		));

		if (AtlasTexture)
		{
			AtlasTexture->Filter = TF_Nearest;
			AtlasTexture->LODGroup = TEXTUREGROUP_Pixels2D;
			AtlasTexture->UpdateResource();
			AtlasTexture->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(AtlasTexture);
		}
	}
	if (!AtlasTexture)
	{
		AtlasTexture = FindObject<UTexture2D>(nullptr, *TexturePackagePath);
	}

	TMap<FString, UPaperSprite*> SpriteMap;
	UPaperSprite* FirstSprite = nullptr;

	auto CreateSpriteAsset = [&](const FBentoSpriteDescriptor& SpriteDesc, int32 Index, int32 X, int32 Y, int32 W, int32 H) -> UPaperSprite*
	{
		FString SpriteCleanName = SpriteDesc.Name.Replace(TEXT(" "), TEXT("_"));
		FString SpritePackagePath = SpritesFolderPath / SpriteCleanName;
		UPackage* SpritePackage = CreatePackage(*SpritePackagePath);

		UPaperSprite* NewSprite = NewObject<UPaperSprite>(SpritePackage, *SpriteCleanName, Flags | RF_Public | RF_Standalone);
		if (NewSprite)
		{
			FSpriteAssetInitParameters SpriteInitParams;
			SpriteInitParams.Texture = AtlasTexture;
			SpriteInitParams.Offset = FIntPoint(X, Y);
			SpriteInitParams.Dimension = FIntPoint(W, H);
			SpriteInitParams.SetPixelsPerUnrealUnit(1.0f);
			NewSprite->InitializeSprite(SpriteInitParams, false);

			FVector2D CustomPivot(X + W * SpriteDesc.Pivot.X, Y + H * SpriteDesc.Pivot.Y);
			NewSprite->SetPivotMode(ESpritePivotMode::Custom, CustomPivot, false);

			FBentoMeshBuilder::ApplyTightMesh(NewSprite, SpriteDesc);
			NewSprite->RebuildData();
			NewSprite->PostEditChange();

			FAssetRegistryModule::AssetCreated(NewSprite);
			NewSprite->MarkPackageDirty();

			SpriteMap.Add(SpriteDesc.Name, NewSprite);
			SpriteMap.Add(SpriteCleanName, NewSprite);
			SpriteMap.Add(FString::FromInt(Index), NewSprite);
			if (!FirstSprite) FirstSprite = NewSprite;
		}
		return NewSprite;
	};

	// 2. Parse and Create Sprites
	// Format A: Standard Paper2D "sprites" array
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

			CreateSpriteAsset(SpriteDesc, Index, X, Y, W, H);
		}
	}
	// Format B: Native BentoPack "boxes" array
	else if (RootObject->TryGetArrayField(TEXT("boxes"), SpritesJson) && SpritesJson != nullptr)
	{
		for (int32 Index = 0; Index < SpritesJson->Num(); ++Index)
		{
			TSharedPtr<FJsonObject> BoxObj = (*SpritesJson)[Index]->AsObject();
			if (!BoxObj.IsValid()) continue;

			FBentoSpriteDescriptor SpriteDesc;
			SpriteDesc.Name = BoxObj->HasField(TEXT("name")) && !BoxObj->GetStringField(TEXT("name")).IsEmpty()
				? BoxObj->GetStringField(TEXT("name"))
				: FString::Printf(TEXT("%s_%04d"), *BaseName, Index);

			TSharedPtr<FJsonObject> RectObj = BoxObj->GetObjectField(TEXT("rect"));
			int32 X = RectObj.IsValid() ? RectObj->GetIntegerField(TEXT("x")) : 0;
			int32 Y = RectObj.IsValid() ? RectObj->GetIntegerField(TEXT("y")) : 0;
			int32 W = RectObj.IsValid() ? RectObj->GetIntegerField(TEXT("w")) : 32;
			int32 H = RectObj.IsValid() ? RectObj->GetIntegerField(TEXT("h")) : 32;

			SpriteDesc.SourceRect = FIntRect(X, Y, X + W, Y + H);

			TSharedPtr<FJsonObject> PivObj = BoxObj->GetObjectField(TEXT("pivot"));
			if (PivObj.IsValid())
			{
				float Px = (float)PivObj->GetNumberField(TEXT("x"));
				float Py = (float)PivObj->GetNumberField(TEXT("y"));
				SpriteDesc.Pivot = FVector2D(W > 0 ? Px / (float)W : 0.5f, H > 0 ? Py / (float)H : 1.0f);
			}
			else
			{
				SpriteDesc.Pivot = FVector2D(0.5f, 1.0f);
			}

			// Render Geometry
			const TArray<TSharedPtr<FJsonValue>>* VertsJson = nullptr;
			if (BoxObj->TryGetArrayField(TEXT("vertices"), VertsJson) && VertsJson != nullptr)
			{
				for (int32 vi = 0; vi + 1 < VertsJson->Num(); vi += 2)
				{
					SpriteDesc.TightMesh.Vertices.Add(FVector2D((*VertsJson)[vi]->AsNumber(), (*VertsJson)[vi + 1]->AsNumber()));
				}
				if (SpriteDesc.TightMesh.Vertices.Num() >= 3)
				{
					SpriteDesc.TightMesh.bHasTightMesh = true;
				}
			}

			// Collision Geometry
			const TArray<TSharedPtr<FJsonValue>>* PolyJson = nullptr;
			if (BoxObj->TryGetArrayField(TEXT("polygon"), PolyJson) && PolyJson != nullptr)
			{
				for (int32 pi = 0; pi + 1 < PolyJson->Num(); pi += 2)
				{
					SpriteDesc.CollisionPolygon.Add(FVector2D((*PolyJson)[pi]->AsNumber(), (*PolyJson)[pi + 1]->AsNumber()));
				}
			}

			CreateSpriteAsset(SpriteDesc, Index, X, Y, W, H);
		}
	}

	// 3. Parse and Create Flipbooks
	TMap<FString, UPaperFlipbook*> FlipbookMap;

	// Format A: Standard Paper2D "flipbooks" array
	const TArray<TSharedPtr<FJsonValue>>* FlipbooksJson = nullptr;
	if (RootObject->TryGetArrayField(TEXT("flipbooks"), FlipbooksJson) && FlipbooksJson != nullptr)
	{
		for (auto& FbVal : *FlipbooksJson)
		{
			TSharedPtr<FJsonObject> FbObj = FbVal->AsObject();
			if (!FbObj.IsValid()) continue;

			FString AnimName = FbObj->GetStringField(TEXT("name"));
			float FPS = FbObj->HasField(TEXT("framesPerSecond")) ? (float)FbObj->GetNumberField(TEXT("framesPerSecond")) : 12.0f;

			FString CleanAnimName = AnimName.Replace(TEXT(" "), TEXT("_"));
			FString AnimPackagePath = AnimsFolderPath / CleanAnimName;
			UPackage* AnimPackage = CreatePackage(*AnimPackagePath);

			UPaperFlipbook* NewFlipbook = NewObject<UPaperFlipbook>(AnimPackage, *CleanAnimName, Flags | RF_Public | RF_Standalone);
			if (NewFlipbook)
			{
				FScopedFlipbookMutator Mutator(NewFlipbook);
				Mutator.FramesPerSecond = (FPS > 0.0f ? FPS : 12.0f);

				const TArray<TSharedPtr<FJsonValue>>* KeyframesJson = nullptr;
				if (FbObj->TryGetArrayField(TEXT("keyframes"), KeyframesJson) && KeyframesJson != nullptr)
				{
					for (auto& KfVal : *KeyframesJson)
					{
						FString SpriteName = KfVal->AsString();
						UPaperSprite* Sprite = SpriteMap.FindRef(SpriteName);
						if (!Sprite)
						{
							Sprite = SpriteMap.FindRef(SpriteName.Replace(TEXT(" "), TEXT("_")));
						}
						if (Sprite)
						{
							FPaperFlipbookKeyFrame KeyFrame;
							KeyFrame.Sprite = Sprite;
							KeyFrame.FrameRun = 1;
							Mutator.KeyFrames.Add(KeyFrame);
						}
					}
				}

				FAssetRegistryModule::AssetCreated(NewFlipbook);
				NewFlipbook->MarkPackageDirty();

				FlipbookMap.Add(AnimName, NewFlipbook);
				FlipbookMap.Add(CleanAnimName, NewFlipbook);
			}
		}
	}
	// Format B: Native BentoPack "animations" array
	else if (RootObject->TryGetArrayField(TEXT("animations"), FlipbooksJson) && FlipbooksJson != nullptr)
	{
		for (auto& FbVal : *FlipbooksJson)
		{
			TSharedPtr<FJsonObject> AnimObj = FbVal->AsObject();
			if (!AnimObj.IsValid()) continue;

			FString AnimName = AnimObj->GetStringField(TEXT("name"));
			float FPS = AnimObj->HasField(TEXT("fps")) ? (float)AnimObj->GetNumberField(TEXT("fps")) : 12.0f;

			FString CleanAnimName = AnimName.Replace(TEXT(" "), TEXT("_"));
			FString AnimPackagePath = AnimsFolderPath / CleanAnimName;
			UPackage* AnimPackage = CreatePackage(*AnimPackagePath);

			UPaperFlipbook* NewFlipbook = NewObject<UPaperFlipbook>(AnimPackage, *CleanAnimName, Flags | RF_Public | RF_Standalone);
			if (NewFlipbook)
			{
				FScopedFlipbookMutator Mutator(NewFlipbook);
				Mutator.FramesPerSecond = (FPS > 0.0f ? FPS : 12.0f);

				const TArray<TSharedPtr<FJsonValue>>* FramesJson = nullptr;
				if (AnimObj->TryGetArrayField(TEXT("frames"), FramesJson) && FramesJson != nullptr)
				{
					for (auto& FVal : *FramesJson)
					{
						int32 FrameIdx = (int32)FVal->AsNumber();
						FString FrameName = FString::Printf(TEXT("%s_%04d"), *BaseName, FrameIdx);
						UPaperSprite* Sprite = SpriteMap.FindRef(FrameName);
						if (!Sprite)
						{
							Sprite = SpriteMap.FindRef(FString::FromInt(FrameIdx));
						}
						if (Sprite)
						{
							FPaperFlipbookKeyFrame KeyFrame;
							KeyFrame.Sprite = Sprite;
							KeyFrame.FrameRun = 1;
							Mutator.KeyFrames.Add(KeyFrame);
						}
					}
				}

				FAssetRegistryModule::AssetCreated(NewFlipbook);
				NewFlipbook->MarkPackageDirty();

				FlipbookMap.Add(AnimName, NewFlipbook);
				FlipbookMap.Add(CleanAnimName, NewFlipbook);
			}
		}
	}

	// 4. Create Primary Asset in InParent (the target package requested by Unreal)
	UPaperFlipbook* PrimaryFlipbook = NewObject<UPaperFlipbook>(InParent, InName, Flags | RF_Public | RF_Standalone);
	if (PrimaryFlipbook)
	{
		UPaperFlipbook* SourceFb = nullptr;
		if (FlipbookMap.Contains(TEXT("stand"))) SourceFb = FlipbookMap[TEXT("stand")];
		else if (FlipbookMap.Contains(TEXT("idle"))) SourceFb = FlipbookMap[TEXT("idle")];
		else if (FlipbookMap.Num() > 0)
		{
			for (auto& Pair : FlipbookMap)
			{
				if (Pair.Value)
				{
					SourceFb = Pair.Value;
					break;
				}
			}
		}

		if (SourceFb)
		{
			FScopedFlipbookMutator Mutator(PrimaryFlipbook);
			Mutator.FramesPerSecond = SourceFb->GetFramesPerSecond();
			for (int32 i = 0; i < SourceFb->GetNumKeyFrames(); ++i)
			{
				Mutator.KeyFrames.Add(SourceFb->GetKeyFrameChecked(i));
			}
		}

		FAssetRegistryModule::AssetCreated(PrimaryFlipbook);
		PrimaryFlipbook->MarkPackageDirty();
	}

	// 5. Sync Content Browser to newly created assets
	TArray<UObject*> AssetsToSync;
	if (PrimaryFlipbook) AssetsToSync.Add(PrimaryFlipbook);
	if (AtlasTexture) AssetsToSync.Add(AtlasTexture);
	for (auto& Pair : FlipbookMap)
	{
		if (Pair.Value && !AssetsToSync.Contains(Pair.Value))
		{
			AssetsToSync.Add(Pair.Value);
		}
	}
	if (FirstSprite && !AssetsToSync.Contains(FirstSprite))
	{
		AssetsToSync.Add(FirstSprite);
	}

	if (AssetsToSync.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
	}

	return PrimaryFlipbook ? (UObject*)PrimaryFlipbook : (UObject*)FirstSprite;
}
