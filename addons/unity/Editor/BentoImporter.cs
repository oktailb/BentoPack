using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEditor.AssetImporters;

namespace BentoPack.Editor
{
    public enum BentoMainAssetType
    {
        Prefab,
        Texture
    }

    /// <summary>
    /// ScriptedImporter for BentoPack (.bento) project archives.
    /// Automatically imports the packed 2D atlas texture, extracts all sliced sprites,
    /// injects tight M8 polygonal meshes via Sprite.OverrideGeometry, builds AnimationClips,
    /// and generates an animated GameObject Prefab ready to drag and drop into the scene.
    /// </summary>
    [ScriptedImporter(7, "bento")]
    public class BentoImporter : ScriptedImporter
    {
        [Header("Main Asset")]
        [Tooltip("Main asset type: 'Prefab' creates an animated character ready to drag & drop directly into the Scene. 'Texture' keeps the raw atlas as the main object.")]
        public BentoMainAssetType mainAssetType = BentoMainAssetType.Prefab;

        [Header("Sprite Settings")]
        [Tooltip("Pixels Per Unit used for generated Sprites (default: 100).")]
        public float pixelsPerUnit = 100f;

        [Tooltip("Extrude border in pixels (avoids edge clamping artifacts).")]
        public uint extrude = 0;

        [Header("Mesh & Optimization")]
        [Tooltip("Inject M8 tight polygonal meshes to eliminate transparent GPU overdraw.")]
        public bool enableTightMesh = true;

        [Header("Animation")]
        [Tooltip("Automatically generate AnimationClips for animations defined in BentoPack.")]
        public bool generateAnimationClips = true;

        public override void OnImportAsset(AssetImportContext ctx)
        {
            string filePath = ctx.assetPath;
            BentoParsedArchive parsed = BentoParser.ParseBentoArchive(filePath);

            if (!parsed.success || parsed.projectData == null)
            {
                ctx.LogImportError($"[BentoPack] Failed to import '{filePath}': {parsed.errorMessage}");
                return;
            }

            Texture2D atlasTex = parsed.atlasTexture;
            if (atlasTex == null)
            {
                ctx.LogImportError($"[BentoPack] Missing or invalid atlas texture in '{filePath}'");
                return;
            }

            // Register atlas texture asset
            ctx.AddObjectToAsset("atlas", atlasTex);

            int atlasW = atlasTex.width;
            int atlasH = atlasTex.height;

            BentoProjectData project = parsed.projectData;
            Dictionary<string, Sprite> spriteMap = new Dictionary<string, Sprite>();
            Sprite firstSprite = null;

            // Generate Sprites
            for (int i = 0; i < project.sprites.Count; ++i)
            {
                BentoSpriteData sData = project.sprites[i];
                string sName = string.IsNullOrEmpty(sData.name) ? $"sprite_{i:D4}" : sData.name;

                int rx = sData.rect.x;
                int ry = sData.rect.y;
                int rw = sData.rect.w;
                int rh = sData.rect.h;

                if (rw <= 0 || rh <= 0) continue;

                // Unity texture origin (0,0) is bottom-left, whereas BentoPack is top-left
                float unityY = atlasH - ry - rh;
                Rect rect = new Rect(rx, unityY, rw, rh);
                Vector2 pivot = new Vector2(sData.pivot.x, sData.pivot.y);

                Sprite sprite = Sprite.Create(
                    atlasTex,
                    rect,
                    pivot,
                    pixelsPerUnit,
                    extrude,
                    SpriteMeshType.Tight
                );
                sprite.name = sName;

                // Inject M8 Tight Polygonal Mesh
                if (enableTightMesh && sData.hasTightMesh)
                {
                    BentoMeshBuilder.ApplyTightMesh(sprite, sData, pixelsPerUnit);
                }

                // Inject Physics Shape for PolygonCollider2D (Sprite.rect pixel space)
                Vector2[] physicsShape = BentoMeshBuilder.BuildPhysicsShape(sData);
                if (physicsShape != null && physicsShape.Length >= 3)
                {
                    sprite.OverridePhysicsShape(new List<Vector2[]> { physicsShape });
                }

                ctx.AddObjectToAsset(sName, sprite);
                spriteMap[sName] = sprite;
                spriteMap[i.ToString()] = sprite;
                spriteMap[$"frame_{i:D4}"] = sprite;
                spriteMap[$"sprite_{i:D4}"] = sprite;
                spriteMap[$"frame_{i}"] = sprite;
                spriteMap[$"sprite_{i}"] = sprite;
                if (firstSprite == null) firstSprite = sprite;
            }

            // Generate AnimationClips
            List<AnimationClip> clips = new List<AnimationClip>();
            if (generateAnimationClips && project.animations != null && project.animations.Count > 0)
            {
                clips = BentoAnimationBuilder.CreateAllClips(project.animations, spriteMap);
                foreach (AnimationClip clip in clips)
                {
                    if (clip != null)
                    {
                        ctx.AddObjectToAsset("anim_" + clip.name, clip);
                    }
                }
            }

            // Setup Main Object
            if (mainAssetType == BentoMainAssetType.Prefab)
            {
                string assetName = Path.GetFileNameWithoutExtension(filePath);
                GameObject root = new GameObject(assetName);
                SpriteRenderer sr = root.AddComponent<SpriteRenderer>();
                sr.sprite = firstSprite;

                if (clips.Count > 0)
                {
                    root.AddComponent<Animator>();
                }

                ctx.AddObjectToAsset("prefab", root);
                ctx.SetMainObject(root);
            }
            else
            {
                ctx.SetMainObject(atlasTex);
            }
        }
    }
}
