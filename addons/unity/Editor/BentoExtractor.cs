using System;
using System.IO;
using System.IO.Compression;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace BentoPack.Editor
{
    /// <summary>
    /// Extracts a .bento package into organized physical Unity project folders:
    /// Animations/ (.anim clips), Metadata/ (atlas & json), and Frames/.
    /// </summary>
    public static class BentoExtractor
    {
        [MenuItem("Assets/BentoPack/Extraire vers des dossiers (Animations, Frames, Metadata)", true)]
        public static bool ValidateExtract()
        {
            string path = GetSelectedBentoPath();
            return !string.IsNullOrEmpty(path);
        }

        [MenuItem("Assets/BentoPack/Extraire vers des dossiers (Animations, Frames, Metadata)", false, 2)]
        public static void ExtractSelected()
        {
            string path = GetSelectedBentoPath();
            if (!string.IsNullOrEmpty(path))
            {
                Extract(path);
            }
        }

        public static bool Extract(string bentoAssetPath)
        {
            if (string.IsNullOrEmpty(bentoAssetPath) || !File.Exists(bentoAssetPath))
            {
                EditorUtility.DisplayDialog("BentoPack", "Fichier .bento introuvable.", "OK");
                return false;
            }

            string assetName = Path.GetFileNameWithoutExtension(bentoAssetPath);
            string parentDir = Path.GetDirectoryName(bentoAssetPath).Replace("\\", "/");
            string targetFolder = $"{parentDir}/{assetName}";

            // Ensure parent directory is valid
            if (!AssetDatabase.IsValidFolder(parentDir) && parentDir != "Assets")
            {
                parentDir = "Assets";
                targetFolder = $"Assets/{assetName}";
            }

            if (!AssetDatabase.IsValidFolder(targetFolder))
            {
                AssetDatabase.CreateFolder(parentDir, assetName);
            }

            string animFolder = $"{targetFolder}/Animations";
            string metaFolder = $"{targetFolder}/Metadata";

            if (!AssetDatabase.IsValidFolder(animFolder)) AssetDatabase.CreateFolder(targetFolder, "Animations");
            if (!AssetDatabase.IsValidFolder(metaFolder)) AssetDatabase.CreateFolder(targetFolder, "Metadata");

            // 1. Extract AnimationClips as standalone .anim files
            UnityEngine.Object[] subAssets = AssetDatabase.LoadAllAssetsAtPath(bentoAssetPath);
            int animCount = 0;

            if (subAssets != null)
            {
                foreach (var obj in subAssets)
                {
                    if (obj is AnimationClip clip)
                    {
                        AnimationClip clone = UnityEngine.Object.Instantiate(clip);
                        clone.name = clip.name;
                        string clipPath = $"{animFolder}/{clip.name}.anim";
                        AssetDatabase.CreateAsset(clone, clipPath);
                        animCount++;
                    }
                }
            }

            // 2. Extract archive contents (project.json, atlas texture) into Metadata/
            int metaCount = 0;
            try
            {
                using (ZipArchive zip = ZipFile.OpenRead(bentoAssetPath))
                {
                    foreach (ZipArchiveEntry entry in zip.Entries)
                    {
                        string eName = entry.Name.ToLowerInvariant();
                        if (string.IsNullOrEmpty(entry.Name)) continue;

                        if (eName.EndsWith(".json") || eName.EndsWith(".png") || eName.EndsWith(".webp") || eName.EndsWith(".jpg"))
                        {
                            string outDiskPath = Path.Combine(Directory.GetCurrentDirectory(), metaFolder, entry.Name);
                            entry.ExtractToFile(outDiskPath, true);
                            metaCount++;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"[BentoPack] Note lors de l'extraction des métadonnées : {ex.Message}");
            }

            AssetDatabase.Refresh();
            Debug.Log($"[BentoPack] ✓ Extraction réussie dans '{targetFolder}' ({animCount} animations, {metaCount} fichiers de métadonnées) !");
            EditorUtility.DisplayDialog(
                "BentoPack - Extraction Terminée",
                $"L'asset a été extrait avec succès dans des dossiers organisés :\n\n" +
                $"📁 {targetFolder}/\n" +
                $"  ├── 🎬 Animations/ ({animCount} AnimationClips .anim)\n" +
                $"  └── 🗄️ Metadata/ (Atlas & JSON)\n",
                "Super !"
            );
            return true;
        }

        private static string GetSelectedBentoPath()
        {
            if (Selection.activeObject == null) return null;
            string path = AssetDatabase.GetAssetPath(Selection.activeObject);
            if (!string.IsNullOrEmpty(path) && path.ToLowerInvariant().EndsWith(".bento"))
            {
                return path;
            }
            return null;
        }
    }
}
