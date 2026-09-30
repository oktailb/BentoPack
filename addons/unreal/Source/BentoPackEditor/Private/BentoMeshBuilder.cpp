#include "BentoMeshBuilder.h"

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
	FSpriteGeometryCollection& RenderGeometry = Sprite->RenderGeometry;
	RenderGeometry.GeometryType = ESpritePolygonMode::FullyCustom;
	RenderGeometry.Shapes.Empty();

	FSpriteGeometryShape RenderShape;
	RenderShape.ShapeType = ESpriteShapeType::Polygon;

	for (const FVector2D& V : MeshData.Vertices)
	{
		// Unreal Paper2D local space has (0,0) at top-left or centered around pivot
		RenderShape.Vertices.Add(V);
	}
	RenderGeometry.Shapes.Add(RenderShape);

	// 2. Setup CollisionGeometry
	if (SpriteData.CollisionPolygon.Num() >= 3)
	{
		FSpriteGeometryCollection& CollisionGeometry = Sprite->CollisionGeometry;
		CollisionGeometry.GeometryType = ESpritePolygonMode::FullyCustom;
		CollisionGeometry.Shapes.Empty();

		FSpriteGeometryShape ColShape;
		ColShape.ShapeType = ESpriteShapeType::Polygon;
		for (const FVector2D& CV : SpriteData.CollisionPolygon)
		{
			ColShape.Vertices.Add(CV);
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

	FSpriteGeometryCollection& RenderGeometry = Sprite->RenderGeometry;
	RenderGeometry.GeometryType = ESpritePolygonMode::SourceBoundingBox;
	RenderGeometry.Shapes.Empty();

	FSpriteGeometryShape QuadShape;
	QuadShape.ShapeType = ESpriteShapeType::Polygon;
	QuadShape.Vertices.Add(FVector2D(0.0f, 0.0f));
	QuadShape.Vertices.Add(FVector2D(SourceDim.X, 0.0f));
	QuadShape.Vertices.Add(FVector2D(SourceDim.X, SourceDim.Y));
	QuadShape.Vertices.Add(FVector2D(0.0f, SourceDim.Y));
	RenderGeometry.Shapes.Add(QuadShape);

	Sprite->PostEditChange();
}
