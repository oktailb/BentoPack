using System.Collections.Generic;
using UnityEngine;

namespace BentoPack.Editor
{
    /// <summary>
    /// Constructs and injects M8 tight polygonal meshes into Unity Sprites via Sprite.OverrideGeometry,
    /// eliminating transparent pixel overdraw on the GPU without custom shaders.
    /// </summary>
    public static class BentoMeshBuilder
    {
        public static bool ApplyTightMesh(Sprite sprite, BentoSpriteData spriteData, float pixelsPerUnit = 100f)
        {
            if (sprite == null || spriteData == null || !spriteData.hasTightMesh)
                return false;

            if (spriteData.vertices == null || spriteData.vertices.Count < 3 ||
                spriteData.triangles == null || spriteData.triangles.Count < 3)
            {
                return false;
            }

            int w = spriteData.rect.w;
            int h = spriteData.rect.h;
            if (w <= 0 || h <= 0)
                return false;

            int vertCount = spriteData.vertices.Count;
            Vector2[] unityVertices = new Vector2[vertCount];

            for (int i = 0; i < vertCount; ++i)
            {
                float[] pt = spriteData.vertices[i];
                if (pt == null || pt.Length < 2) continue;

                float px = pt[0];
                float py = pt[1];

                // BentoPack coordinates: (0,0) is top-left, Y goes down.
                // Unity Sprite.OverrideGeometry coordinates: (0,0) is bottom-left, Y goes up in Sprite Rect pixel space [0, w] and [0, h].
                // Unity automatically handles pivot offset and pixelsPerUnit transformation.
                float unityPixelX = Mathf.Clamp(px, 0f, (float)w);
                float unityPixelY = Mathf.Clamp(h - py, 0f, (float)h);

                unityVertices[i] = new Vector2(unityPixelX, unityPixelY);
            }

            // Triangle indices: reverse winding order because Y axis is inverted
            int triIndexCount = spriteData.triangles.Count;
            ushort[] unityTriangles = new ushort[triIndexCount];

            for (int t = 0; t < triIndexCount / 3; ++t)
            {
                int i0 = spriteData.triangles[t * 3];
                int i1 = spriteData.triangles[t * 3 + 1];
                int i2 = spriteData.triangles[t * 3 + 2];

                // Swap i1 and i2 to restore correct clockwise front-facing winding
                unityTriangles[t * 3] = (ushort)i0;
                unityTriangles[t * 3 + 1] = (ushort)i2;
                unityTriangles[t * 3 + 2] = (ushort)i1;
            }

            sprite.OverrideGeometry(unityVertices, unityTriangles);
            return true;
        }

        /// <summary>
        /// Generates physics shape outline in Sprite.rect pixel space [0, w] and [0, h] (bottom-left origin)
        /// for Sprite.OverridePhysicsShape. Unity automatically converts this to local units
        /// by subtracting pivot and dividing by pixelsPerUnit.
        /// </summary>
        public static Vector2[] BuildPhysicsShape(BentoSpriteData spriteData)
        {
            if (spriteData == null) return null;

            List<float[]> sourcePoly = (spriteData.polygon != null && spriteData.polygon.Count >= 3)
                ? spriteData.polygon
                : spriteData.vertices;

            if (sourcePoly == null || sourcePoly.Count < 3)
                return null;

            int w = spriteData.rect.w;
            int h = spriteData.rect.h;
            if (w <= 0 || h <= 0) return null;

            int count = sourcePoly.Count;
            // Remove duplicate closing vertex if present
            if (count > 3 &&
                System.Math.Abs(sourcePoly[0][0] - sourcePoly[count - 1][0]) < 1e-4f &&
                System.Math.Abs(sourcePoly[0][1] - sourcePoly[count - 1][1]) < 1e-4f)
            {
                count--;
            }

            Vector2[] path = new Vector2[count];
            for (int i = 0; i < count; ++i)
            {
                float[] pt = sourcePoly[i];
                float unityPixelX = Mathf.Clamp(pt[0], 0f, (float)w);
                float unityPixelY = Mathf.Clamp(h - pt[1], 0f, (float)h);

                path[i] = new Vector2(unityPixelX, unityPixelY);
            }

            return path;
        }

        /// <summary>
        /// Generates collider polygon paths relative to pivot in Unity local units for PolygonCollider2D.
        /// </summary>
        public static Vector2[] BuildColliderPath(BentoSpriteData spriteData, float pixelsPerUnit = 100f)
        {
            if (spriteData == null) return null;

            Vector2[] pixelShape = BuildPhysicsShape(spriteData);
            if (pixelShape == null || pixelShape.Length < 3) return null;

            int w = spriteData.rect.w;
            int h = spriteData.rect.h;
            float pivotPixelX = spriteData.pivot.x * w;
            float pivotPixelY = spriteData.pivot.y * h;

            Vector2[] path = new Vector2[pixelShape.Length];
            for (int i = 0; i < pixelShape.Length; ++i)
            {
                path[i] = new Vector2(
                    (pixelShape[i].x - pivotPixelX) / pixelsPerUnit,
                    (pixelShape[i].y - pivotPixelY) / pixelsPerUnit
                );
            }

            return path;
        }
    }
}
