using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEditor;
using BentoPack.Runtime;

namespace BentoPack.Editor
{
    /// <summary>
    /// Handles drag & drop of .bento archives, AnimationClips, and Sprites directly onto the Unity Scene View
    /// and Hierarchy window, and provides context menu options to instantly place animated characters into the active scene.
    /// </summary>
    [InitializeOnLoad]
    public static class BentoSceneDragDrop
    {
        static BentoSceneDragDrop()
        {
            SceneView.duringSceneGui += OnSceneGUI;
        }

        private static void OnSceneGUI(SceneView sceneView)
        {
            Event evt = Event.current;
            if (evt.type != EventType.DragUpdated && evt.type != EventType.DragPerform)
                return;

            string bentoPath = null;
            AnimationClip droppedClip = null;
            Sprite droppedSprite = null;

            if (!ResolveDragData(out bentoPath, out droppedClip, out droppedSprite))
                return;

            if (evt.type == EventType.DragUpdated)
            {
                DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
                evt.Use();
                return;
            }

            if (evt.type == EventType.DragPerform)
            {
                DragAndDrop.AcceptDrag();
                evt.Use();

                Ray ray = HandleUtility.GUIPointToWorldRay(evt.mousePosition);
                Vector3 spawnPos = new Vector3(ray.origin.x, ray.origin.y, 0f);

                SpawnBentoCharacter(bentoPath, droppedClip, droppedSprite, spawnPos);
            }
        }

        private static bool ResolveDragData(out string bentoPath, out AnimationClip droppedClip, out Sprite droppedSprite)
        {
            bentoPath = null;
            droppedClip = null;
            droppedSprite = null;

            foreach (var obj in DragAndDrop.objectReferences)
            {
                if (obj == null) continue;

                if (obj is AnimationClip clip)
                {
                    droppedClip = clip;
                    string p = AssetDatabase.GetAssetPath(clip);
                    if (!string.IsNullOrEmpty(p) && p.ToLowerInvariant().EndsWith(".bento"))
                    {
                        bentoPath = p;
                        return true;
                    }
                }
                else if (obj is Sprite sp)
                {
                    droppedSprite = sp;
                    string p = AssetDatabase.GetAssetPath(sp);
                    if (!string.IsNullOrEmpty(p) && p.ToLowerInvariant().EndsWith(".bento"))
                    {
                        bentoPath = p;
                        return true;
                    }
                }
                else if (obj is GameObject go)
                {
                    string p = AssetDatabase.GetAssetPath(go);
                    if (!string.IsNullOrEmpty(p) && p.ToLowerInvariant().EndsWith(".bento"))
                    {
                        bentoPath = p;
                        return true;
                    }
                }
            }

            foreach (string path in DragAndDrop.paths)
            {
                if (!string.IsNullOrEmpty(path) && path.ToLowerInvariant().EndsWith(".bento"))
                {
                    bentoPath = path;
                    return true;
                }
            }

            return (bentoPath != null || droppedClip != null || droppedSprite != null);
        }

        [MenuItem("Assets/BentoPack/Place Character in Scene", false, 1)]
        public static void PlaceSelectedCharacterInScene()
        {
            string path = GetSelectedBentoPath();
            if (string.IsNullOrEmpty(path))
            {
                EditorUtility.DisplayDialog("BentoPack", "Please select a .bento asset or animation first.", "OK");
                return;
            }

            Vector3 spawnPos = Vector3.zero;
            if (SceneView.lastActiveSceneView != null)
            {
                spawnPos = SceneView.lastActiveSceneView.camera.transform.position;
                spawnPos.z = 0f;
            }

            AnimationClip selectedClip = Selection.activeObject as AnimationClip;
            Sprite selectedSprite = Selection.activeObject as Sprite;

            SpawnBentoCharacter(path, selectedClip, selectedSprite, spawnPos);
        }

        [MenuItem("Assets/BentoPack/Place Character in Scene", true)]
        public static bool ValidatePlaceSelectedCharacterInScene()
        {
            return !string.IsNullOrEmpty(GetSelectedBentoPath());
        }

        [MenuItem("GameObject/2D Object/BentoPack Character", false, 10)]
        public static void CreateBentoGameObjectMenu()
        {
            PlaceSelectedCharacterInScene();
        }

        public static GameObject SpawnBentoCharacter(string bentoAssetPath, AnimationClip requestedClip, Sprite sprite, Vector3 position)
        {
            if (string.IsNullOrEmpty(bentoAssetPath) && requestedClip != null)
            {
                bentoAssetPath = AssetDatabase.GetAssetPath(requestedClip);
            }
            if (string.IsNullOrEmpty(bentoAssetPath) && sprite != null)
            {
                bentoAssetPath = AssetDatabase.GetAssetPath(sprite);
            }

            string externalSourcePath = null;
            if (Path.IsPathRooted(bentoAssetPath) && !bentoAssetPath.StartsWith(Application.dataPath))
            {
                externalSourcePath = bentoAssetPath;
                string fileName = Path.GetFileName(bentoAssetPath);
                string targetRelativePath = "Assets/" + fileName;
                string targetFullPath = Path.Combine(Directory.GetCurrentDirectory(), targetRelativePath);
                File.Copy(bentoAssetPath, targetFullPath, true);
                AssetDatabase.ImportAsset(targetRelativePath, ImportAssetOptions.ForceUpdate);
                bentoAssetPath = targetRelativePath;
            }

            string baseAssetName = Path.GetFileNameWithoutExtension(bentoAssetPath);
            string charName = (requestedClip != null) ? $"{baseAssetName}_{requestedClip.name}" : baseAssetName;

            GameObject go = new GameObject(charName);
            go.transform.position = position;

            SpriteRenderer sr = go.AddComponent<SpriteRenderer>();
            BentoMeshSprite player = go.AddComponent<BentoMeshSprite>();
            player.SourceBentoPath = bentoAssetPath;
            if (!string.IsNullOrEmpty(externalSourcePath))
            {
                player.ExternalSourcePath = externalSourcePath;
            }
            PolygonCollider2D collider = go.AddComponent<PolygonCollider2D>();
            Animator animator = go.AddComponent<Animator>();

            // Load sub-assets from the .bento file
            UnityEngine.Object[] allSubAssets = AssetDatabase.LoadAllAssetsAtPath(bentoAssetPath);
            List<Sprite> allSprites = new List<Sprite>();
            Dictionary<string, Sprite> spriteMap = new Dictionary<string, Sprite>();
            List<AnimationClip> allClips = new List<AnimationClip>();

            foreach (var asset in allSubAssets)
            {
                if (asset is Sprite sp)
                {
                    allSprites.Add(sp);
                    spriteMap[sp.name] = sp;
                }
                else if (asset is AnimationClip cl)
                {
                    allClips.Add(cl);
                }
            }

            // Populate all animations into BentoMeshSprite
            foreach (var clip in allClips)
            {
                List<Sprite> cFrames = ExtractFramesFromClip(clip);
                float cFps = clip.frameRate > 0 ? clip.frameRate : 12f;
                bool cLoop = true;
                player.AddAnimation(clip.name, cFrames, cFps, cLoop);
            }

            // Determine active animation clip
            AnimationClip activeClip = requestedClip;
            if (activeClip == null && allClips.Count > 0)
            {
                // Prefer "stand" or "idle", otherwise first clip
                foreach (var c in allClips)
                {
                    string lower = c.name.ToLowerInvariant();
                    if (lower.Contains("stand") || lower.Contains("idle"))
                    {
                        activeClip = c;
                        break;
                    }
                }
                if (activeClip == null) activeClip = allClips[0];
            }

            // Play the requested or active animation
            if (activeClip != null)
            {
                player.PlayAnimation(activeClip.name);
            }
            else if (sprite != null)
            {
                sr.sprite = sprite;
                player.SetAnimation(new List<Sprite> { sprite }, 12f, false);
            }
            else if (allSprites.Count > 0)
            {
                sr.sprite = allSprites[0];
                player.SetAnimation(new List<Sprite> { allSprites[0] }, 12f, false);
            }

            Undo.RegisterCreatedObjectUndo(go, "Create BentoPack Character");
            Selection.activeGameObject = go;

            string animInfo = activeClip != null ? activeClip.name : "default";
            Debug.Log($"[BentoPack] Successfully placed '{go.name}' in Scene with animation '{animInfo}' ({player.Animations.Count} animations available)!");
            return go;
        }

        public static List<Sprite> ExtractFramesFromClip(AnimationClip clip)
        {
            List<Sprite> frames = new List<Sprite>();
            if (clip == null) return frames;

            EditorCurveBinding[] bindings = AnimationUtility.GetObjectReferenceCurveBindings(clip);
            foreach (var b in bindings)
            {
                if (b.propertyName == "m_Sprite")
                {
                    ObjectReferenceKeyframe[] keyframes = AnimationUtility.GetObjectReferenceCurve(clip, b);
                    foreach (var kf in keyframes)
                    {
                        if (kf.value is Sprite frameSp)
                        {
                            frames.Add(frameSp);
                        }
                    }
                }
            }
            return frames;
        }

        private static string GetSelectedBentoPath()
        {
            if (Selection.activeObject != null)
            {
                string p = AssetDatabase.GetAssetPath(Selection.activeObject);
                if (!string.IsNullOrEmpty(p) && p.ToLowerInvariant().EndsWith(".bento"))
                {
                    return p;
                }
            }
            return null;
        }
    }
}
