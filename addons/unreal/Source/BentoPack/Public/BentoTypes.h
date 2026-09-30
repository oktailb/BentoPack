#pragma once

#include "CoreMinimal.h"
#include "BentoTypes.generated.h"

USTRUCT(BlueprintType)
struct BENTOPACK_API FBentoVertex
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FVector2D UV = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct BENTOPACK_API FBentoTightMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	bool bHasTightMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	TArray<FVector2D> Vertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	TArray<int32> Triangles;
};

USTRUCT(BlueprintType)
struct BENTOPACK_API FBentoSpriteDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FIntRect SourceRect = FIntRect(0, 0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FVector2D Pivot = FVector2D(0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FBentoTightMesh TightMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	TArray<FVector2D> CollisionPolygon;
};

USTRUCT(BlueprintType)
struct BENTOPACK_API FBentoAnimationDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	float FramesPerSecond = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	TArray<int32> FrameIndices;
};
