using System.IO;
using UnityEngine;
using UnityEditor;

namespace BentoPack.Editor
{
    /// <summary>
    /// Context menu actions in the Unity Project Window for BentoPack files and textures.
    /// </summary>
    public static class BentoContextMenu
    {
        private const string MENU_OPEN = "Assets/BentoPack/Open in BentoPack Studio";
        private const string MENU_REPACK = "Assets/BentoPack/Re-Pack with CLI";

        [MenuItem(MENU_OPEN, true)]
        public static bool ValidateOpenInBentoPack()
        {
            return GetSelectedAssetPath() != null;
        }

        [MenuItem(MENU_OPEN, false, 20)]
        public static void OpenInBentoPack()
        {
            string path = GetSelectedAssetPath();
            if (string.IsNullOrEmpty(path)) return;

            string fullPath = Path.GetFullPath(path);
            BentoCliBridge.OpenInBentoPack(fullPath);
        }

        [MenuItem(MENU_REPACK, true)]
        public static bool ValidateRepack()
        {
            string path = GetSelectedAssetPath();
            return !string.IsNullOrEmpty(path) && path.ToLowerInvariant().EndsWith(".bento");
        }

        [MenuItem(MENU_REPACK, false, 21)]
        public static void RepackArchive()
        {
            string path = GetSelectedAssetPath();
            if (string.IsNullOrEmpty(path)) return;

            string fullPath = Path.GetFullPath(path);
            string args = $"pack \"{fullPath}\" -o \"{fullPath}\"";

            EditorUtility.DisplayProgressBar("BentoPack", "Re-packing atlas with bentopack-cli...", 0.5f);
            try
            {
                if (BentoCliBridge.RunCliCommand(args, out string stdout, out string stderr))
                {
                    AssetDatabase.ImportAsset(path, ImportAssetOptions.ForceUpdate);
                    Debug.Log($"[BentoPack] Successfully re-packed '{path}'!\n{stdout}");
                }
                else
                {
                    Debug.LogError($"[BentoPack] Failed to re-pack archive:\n{stderr}");
                }
            }
            finally
            {
                EditorUtility.ClearProgressBar();
            }
        }

        private static string GetSelectedAssetPath()
        {
            if (Selection.activeObject == null) return null;
            string path = AssetDatabase.GetAssetPath(Selection.activeObject);
            if (string.IsNullOrEmpty(path)) return null;

            string ext = Path.GetExtension(path).ToLowerInvariant();
            if (ext == ".bento" || ext == ".png" || ext == ".webp" || ext == ".jpg" || ext == ".json")
            {
                return path;
            }
            return null;
        }
    }
}
