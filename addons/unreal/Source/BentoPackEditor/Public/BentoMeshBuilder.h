#pragma once

#include "CoreMinimal.h"
#include "PaperSprite.h"
#include "BentoTypes.h"

/**
 * Builds M8 tight polygonal render geometry and collision geometry on UPaperSprite assets.
 * Eliminates transparent overdraw on the GPU (60% to 80% fillrate savings).
 */
class BENTOPACKEDITOR_API FBentoMeshBuilder
{
public:
	/** Applies M8 tight polygonal mesh to the sprite's RenderGeometry and CollisionGeometry */
	static bool ApplyTightMesh(UPaperSprite* Sprite, const FBentoSpriteDescriptor& SpriteData);

	/** Builds default fallback quad geometry if tight mesh is not present */
	static void ApplyFallbackQuad(UPaperSprite* Sprite, const FVector2D& SourceDim);
};
