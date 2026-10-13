#include "BentoMeshBuilder.h"

class FPaperSpriteAccessor : public UPaperSprite
{
public:
	static FSpriteGeometryCollection& GetRenderGeometry(UPaperSprite* Sprite)
	{
		return static_cast<FPaperSpriteAccessor*>(Sprite)->RenderGeometry;
	}
	static FSpriteGeometryCollection& GetCollisionGeometry(UPaperSprite* Sprite)
	{
		return static_cast<FPaperSpriteAccessor*>(Sprite)->CollisionGeometry;
	}
};

bool FBentoMeshBuilder::ApplyTightMesh(UPaperSprite* Sprite, const FBentoSpriteDescriptor& SpriteData)
{
	if (!Sprite)
	{
		return false;
	}

	const FBentoTightMesh& MeshData = SpriteData.TightMesh;
	if (!MeshData.bHasTightMesh || MeshData.Vertices.Num() < 3)
	{
		ApplyFallbackQuad(Sprite, FVector2D(SpriteData.SourceRect.Width(), SpriteData.SourceRect.Height()));
		return false;
	}

	// 1. Setup RenderGeometry (Tight Polygonal Mesh)
	FSpriteGeometryCollection& RenderGeometry = FPaperSpriteAccessor::GetRenderGeometry(Sprite);
	RenderGeometry.GeometryType = ESpritePolygonMode::FullyCustom;
	RenderGeometry.Shapes.Empty();

	FSpriteGeometryShape RenderShape;
	RenderShape.ShapeType = ESpriteShapeType::Polygon;

	for (const FVector2D& V : MeshData.Vertices)
	{
		// In Unreal Paper2D, vertices in FSpriteGeometryShape must be in texture coordinates (offset by SourceRect.Min)
		RenderShape.Vertices.Add(FVector2D(SpriteData.SourceRect.Min.X + V.X, SpriteData.SourceRect.Min.Y + V.Y));
	}
	RenderGeometry.Shapes.Add(RenderShape);

	// 2. Setup CollisionGeometry
	if (SpriteData.CollisionPolygon.Num() >= 3)
	{
		FSpriteGeometryCollection& CollisionGeometry = FPaperSpriteAccessor::GetCollisionGeometry(Sprite);
		CollisionGeometry.GeometryType = ESpritePolygonMode::FullyCustom;
		CollisionGeometry.Shapes.Empty();

		FSpriteGeometryShape ColShape;
		ColShape.ShapeType = ESpriteShapeType::Polygon;
		for (const FVector2D& CV : SpriteData.CollisionPolygon)
		{
			ColShape.Vertices.Add(FVector2D(SpriteData.SourceRect.Min.X + CV.X, SpriteData.SourceRect.Min.Y + CV.Y));
		}
		CollisionGeometry.Shapes.Add(ColShape);
	}

	// Rebuild and bake geometry into render buffers
	Sprite->PostEditChange();
	return true;
}

void FBentoMeshBuilder::ApplyFallbackQuad(UPaperSprite* Sprite, const FVector2D& SourceDim)
{
	if (!Sprite)
	{
		return;
	}

	FSpriteGeometryCollection& RenderGeometry = FPaperSpriteAccessor::GetRenderGeometry(Sprite);
	RenderGeometry.GeometryType = ESpritePolygonMode::SourceBoundingBox;
	RenderGeometry.Shapes.Empty();

	Sprite->PostEditChange();
}
