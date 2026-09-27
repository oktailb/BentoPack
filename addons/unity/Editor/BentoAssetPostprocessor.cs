using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEditor;
using BentoPack.Runtime;

namespace BentoPack.Editor
{
    /// <summary>
    /// Listens for any .bento file import or modification in the project,
    /// and automatically hot-reloads all active BentoMeshSprite characters in open scenes.
    /// </summary>
    public class BentoAssetPostprocessor : AssetPostprocessor
    {
        private static double s_lastCheckTime = 0.0;
        private static readonly Dictionary<string, DateTime> s_knownWriteTimes = new Dictionary<string, DateTime>(StringComparer.OrdinalIgnoreCase);

        [InitializeOnLoadMethod]
        private static void InitializeWatcher()
        {
            EditorApplication.update += CheckDiskModifications;
        }

        private static void CheckDiskModifications()
        {
            if (EditorApplication.isCompiling || EditorApplication.isUpdating || BuildPipeline.isBuildingPlayer) return;

            double now = EditorApplication.timeSinceStartup;
            if (now - s_lastCheckTime < 1.5) return; // Check every 1.5s
            s_lastCheckTime = now;

#if UNITY_2023_1_OR_NEWER
            var players = UnityEngine.Object.FindObjectsByType<BentoMeshSprite>(FindObjectsInactive.Include);
#else
            var players = UnityEngine.Object.FindObjectsOfType<BentoMeshSprite>(true);
#endif
            if (players == null || players.Length == 0) return;

            HashSet<string> pathsToReload = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var p in players)
            {
                if (p == null) continue;
                string bentoPath = GetBentoPathForPlayer(p);
                string extPath = p.ExternalSourcePath;

                // Auto-detect external BentoPack source if not set
                if (string.IsNullOrEmpty(extPath) && !string.IsNullOrEmpty(bentoPath))
                {
                    string candidate = AutoDetectExternalSource(bentoPath);
                    if (!string.IsNullOrEmpty(candidate))
                    {
                        p.ExternalSourcePath = candidate;
                        extPath = candidate;
                        EditorUtility.SetDirty(p);
                    }
                }

                // If external source exists, auto-sync when modified
                if (!string.IsNullOrEmpty(extPath) && File.Exists(extPath) && !string.IsNullOrEmpty(bentoPath))
                {
                    DateTime extWrite = File.GetLastWriteTimeUtc(extPath);
                    string extKey = "ext:" + extPath;
                    if (!s_knownWriteTimes.TryGetValue(extKey, out DateTime lastExtWrite) || extWrite > lastExtWrite)
                    {
                        s_knownWriteTimes[extKey] = extWrite;
                        try
                        {
                            string destAbs = Path.GetFullPath(bentoPath);
                            File.Copy(extPath, destAbs, true);
                            pathsToReload.Add(bentoPath);
                            Debug.Log($"[BentoPack] 🔄 Synchronisation automatique depuis la source externe : '{extPath}' -> '{bentoPath}'");
                        }
                        catch (Exception ex)
                        {
                            Debug.LogWarning($"[BentoPack] Erreur lors de la copie externe : {ex.Message}");
                        }
                        continue;
                    }
                }

                if (string.IsNullOrEmpty(bentoPath) || !File.Exists(bentoPath)) continue;

                DateTime currentWrite = File.GetLastWriteTimeUtc(bentoPath);
                if (s_knownWriteTimes.TryGetValue(bentoPath, out DateTime lastWrite))
                {
                    if (currentWrite > lastWrite)
                    {
                        s_knownWriteTimes[bentoPath] = currentWrite;
                        pathsToReload.Add(bentoPath);
                    }
                }
                else
                {
                    s_knownWriteTimes[bentoPath] = currentWrite;
                }
            }

            foreach (string path in pathsToReload)
            {
                Debug.Log($"[BentoPack] Detected external modification on '{path}'. Reimporting and synchronizing scene...");
                AssetDatabase.ImportAsset(path, ImportAssetOptions.ForceUpdate);
            }
        }

        private static void OnPostprocessAllAssets(
            string[] importedAssets,
            string[] deletedAssets,
            string[] movedAssets,
            string[] movedFromAssetPaths)
        {
            List<string> modifiedBentoPaths = new List<string>();

            if (importedAssets != null)
            {
                foreach (string path in importedAssets)
                {
                    if (path.EndsWith(".bento", StringComparison.OrdinalIgnoreCase))
                    {
                        modifiedBentoPaths.Add(path);
                        if (File.Exists(path))
                        {
                            s_knownWriteTimes[path] = File.GetLastWriteTimeUtc(path);
                        }
                    }
                }
            }

            if (modifiedBentoPaths.Count > 0)
            {
                EditorApplication.delayCall += () =>
                {
                    ReloadSceneCharactersForAssets(modifiedBentoPaths);
                };
            }
        }

        public static void ReloadSceneCharactersForAssets(List<string> bentoAssetPaths)
        {
#if UNITY_2023_1_OR_NEWER
            BentoMeshSprite[] players = UnityEngine.Object.FindObjectsByType<BentoMeshSprite>(FindObjectsInactive.Include);
#else
            BentoMeshSprite[] players = UnityEngine.Object.FindObjectsOfType<BentoMeshSprite>(true);
#endif
            if (players == null || players.Length == 0) return;

            int reloadedCount = 0;
            foreach (var player in players)
            {
                if (player == null) continue;

                string playerBentoPath = GetBentoPathForPlayer(player);
                if (string.IsNullOrEmpty(playerBentoPath)) continue;

                bool matches = false;
                foreach (var bp in bentoAssetPaths)
                {
                    if (string.Equals(Path.GetFullPath(bp), Path.GetFullPath(playerBentoPath), StringComparison.OrdinalIgnoreCase))
                    {
                        matches = true;
                        break;
                    }
                }

                if (matches)
                {
                    ReloadPlayer(player, playerBentoPath);
                    reloadedCount++;
                }
            }

            if (reloadedCount > 0)
            {
                SceneView.RepaintAll();
                Debug.Log($"[BentoPack] ✓ Hot-reloaded and synchronized {reloadedCount} character(s) in Scene following .bento asset update.");
            }
        }

        public static string GetBentoPathForPlayer(BentoMeshSprite player)
        {
            if (player == null) return null;

            if (!string.IsNullOrEmpty(player.SourceBentoPath) && File.Exists(player.SourceBentoPath))
            {
                return player.SourceBentoPath;
            }

            // Fallback: check frames
            if (player.Frames != null && player.Frames.Count > 0 && player.Frames[0] != null)
            {
                string p = AssetDatabase.GetAssetPath(player.Frames[0]);
                if (!string.IsNullOrEmpty(p) && p.EndsWith(".bento", StringComparison.OrdinalIgnoreCase))
                {
                    player.SourceBentoPath = p;
                    return p;
                }
            }

            // Fallback: check SpriteRenderer
            var sr = player.GetComponent<SpriteRenderer>();
            if (sr != null && sr.sprite != null)
            {
                string p = AssetDatabase.GetAssetPath(sr.sprite);
                if (!string.IsNullOrEmpty(p) && p.EndsWith(".bento", StringComparison.OrdinalIgnoreCase))
                {
                    player.SourceBentoPath = p;
                    return p;
                }
            }

            // Fallback: search project by GameObject prefix name
            string goName = player.gameObject.name;
            int under = goName.IndexOf('_');
            string basePrefix = under > 0 ? goName.Substring(0, under) : goName;
            string[] guids = AssetDatabase.FindAssets($"{basePrefix} t:DefaultAsset");
            foreach (var g in guids)
            {
                string cand = AssetDatabase.GUIDToAssetPath(g);
                if (cand.EndsWith(".bento", StringComparison.OrdinalIgnoreCase))
                {
                    player.SourceBentoPath = cand;
                    return cand;
                }
            }

            return null;
        }

        public static string AutoDetectExternalSource(string bentoPath)
        {
            if (string.IsNullOrEmpty(bentoPath)) return null;
            string fileName = Path.GetFileName(bentoPath);

            string homeDir = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            string[] searchPaths = new string[]
            {
                "/home/oktail/Documents/GitHub/BentoPack/sample/" + fileName,
                "/home/oktail/Documents/GitHub/BentoPack/" + fileName,
                Path.Combine(homeDir, "Documents/GitHub/BentoPack/sample", fileName),
                Path.Combine(homeDir, "Documents/GitHub/BentoPack", fileName)
            };

            foreach (var sp in searchPaths)
            {
                if (File.Exists(sp)) return sp;
            }

            return null;
        }

        public static bool ReloadPlayer(BentoMeshSprite player, string bentoAssetPath)
        {
            if (player == null || string.IsNullOrEmpty(bentoAssetPath)) return false;

            UnityEngine.Object[] allSubAssets = AssetDatabase.LoadAllAssetsAtPath(bentoAssetPath);
            if (allSubAssets == null || allSubAssets.Length == 0) return false;

            Undo.RecordObject(player, "Reload Bento Character from Asset");

            List<AnimationClip> allClips = new List<AnimationClip>();
            List<Sprite> allSprites = new List<Sprite>();

            foreach (var asset in allSubAssets)
            {
                if (asset is AnimationClip clip)
                {
                    allClips.Add(clip);
                }
                else if (asset is Sprite sp)
                {
                    allSprites.Add(sp);
                }
            }

            // Remember previous animation name and frame index
            string prevAnimName = (player.Animations != null && player.CurrentAnimationIndex >= 0 && player.CurrentAnimationIndex < player.Animations.Count)
                ? player.Animations[player.CurrentAnimationIndex].name
                : null;
            int prevFrame = player.CurrentFrame;

            // Clear and repopulate animations
            player.Animations.Clear();
            foreach (var clip in allClips)
            {
                List<Sprite> cFrames = BentoSceneDragDrop.ExtractFramesFromClip(clip);
                float cFps = clip.frameRate > 0 ? clip.frameRate : 12f;
                player.AddAnimation(clip.name, cFrames, cFps, true);
            }

            player.SourceBentoPath = bentoAssetPath;

            // Restore active animation
            if (!string.IsNullOrEmpty(prevAnimName))
            {
                player.PlayAnimation(prevAnimName);
            }
            else if (player.Animations.Count > 0)
            {
                player.PlayAnimation(0);
            }
            else if (allSprites.Count > 0)
            {
                player.SetAnimation(allSprites, 12f, false);
            }

            player.SetFrame(prevFrame);

            var sr = player.GetComponent<SpriteRenderer>();
            if (sr != null && player.Frames != null && player.CurrentFrame < player.Frames.Count)
            {
                sr.sprite = player.Frames[player.CurrentFrame];
                EditorUtility.SetDirty(sr);
            }

            var col = player.SyncCollider != null ? player.SyncCollider : player.GetComponent<PolygonCollider2D>();
            if (col != null)
            {
                Undo.RecordObject(col, "Update Hitbox on Reload");
                EditorUtility.SetDirty(col);
            }

            EditorUtility.SetDirty(player);
            if (player.gameObject.scene.IsValid())
            {
                UnityEditor.SceneManagement.EditorSceneManager.MarkSceneDirty(player.gameObject.scene);
            }

            return true;
        }
    }
}
