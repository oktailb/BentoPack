using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using UnityEditor.AssetImporters;

namespace BentoPack.Editor
{
    /// <summary>
    /// Custom Inspector for .bento asset files, displaying organized sections:
    /// Animations, Frames/Sprites, and Metadata/Atlas, with 1-click folder extraction.
    /// </summary>
    [CustomEditor(typeof(BentoImporter))]
    public class BentoImporterEditor : ScriptedImporterEditor
    {
        private bool m_showAnimations = true;
        private bool m_showFrames = false;
        private bool m_showMetadata = true;
        private Vector2 m_framesScroll;

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            string assetPath = (target as AssetImporter)?.assetPath;
            string assetName = !string.IsNullOrEmpty(assetPath) ? Path.GetFileName(assetPath) : ".bento";

            // Load sub-assets for inspection
            List<AnimationClip> clips = new List<AnimationClip>();
            List<Sprite> sprites = new List<Sprite>();
            Texture2D atlas = null;

            if (!string.IsNullOrEmpty(assetPath))
            {
                UnityEngine.Object[] subAssets = AssetDatabase.LoadAllAssetsAtPath(assetPath);
                if (subAssets != null)
                {
                    foreach (var obj in subAssets)
                    {
                        if (obj is AnimationClip clip) clips.Add(clip);
                        else if (obj is Sprite sp) sprites.Add(sp);
                        else if (obj is Texture2D tex && atlas == null) atlas = tex;
                    }
                }
            }

            // Header Banner
            EditorGUILayout.Space();
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField($"🍱 BentoPack : {assetName}", EditorStyles.boldLabel);
            BentoI18n.DrawLanguageSelector();
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.LabelField(string.Format(BentoI18n.Tr("content_summary"), clips.Count, sprites.Count, atlas != null ? $"{atlas.width}x{atlas.height}" : ""), EditorStyles.miniLabel);

            EditorGUILayout.Space(4);
            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Button(BentoI18n.Tr("extract_folders"), GUILayout.Height(26)))
            {
                BentoExtractor.Extract(assetPath);
            }
            if (GUILayout.Button(BentoI18n.Tr("open_studio"), GUILayout.Height(26)))
            {
                BentoCliBridge.OpenInBentoPack(Path.GetFullPath(assetPath));
            }
            EditorGUILayout.EndHorizontal();
            EditorGUILayout.EndVertical();

            EditorGUILayout.Space();

            // Section 1: Animations
            m_showAnimations = EditorGUILayout.BeginFoldoutHeaderGroup(m_showAnimations, $"{BentoI18n.Tr("animations_group")} ({clips.Count})");
            if (m_showAnimations)
            {
                EditorGUI.indentLevel++;
                if (clips.Count == 0)
                {
                    EditorGUILayout.LabelField(BentoI18n.Tr("no_animations"), EditorStyles.miniLabel);
                }
                else
                {
                    foreach (var clip in clips)
                    {
                        EditorGUILayout.BeginHorizontal(EditorStyles.helpBox);
                        EditorGUILayout.LabelField($"▶ {clip.name}", EditorStyles.boldLabel, GUILayout.Width(140));
                        EditorGUILayout.LabelField($"{clip.frameRate} FPS", GUILayout.Width(60));
                        EditorGUILayout.LabelField($"{clip.length:F2}s", GUILayout.Width(50));
                        if (GUILayout.Button(BentoI18n.Tr("inspect"), GUILayout.Width(70)))
                        {
                            Selection.activeObject = clip;
                        }
                        EditorGUILayout.EndHorizontal();
                    }
                }
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndFoldoutHeaderGroup();

            EditorGUILayout.Space();

            // Section 2: Frames / Sprites
            m_showFrames = EditorGUILayout.BeginFoldoutHeaderGroup(m_showFrames, $"{BentoI18n.Tr("frames_group")} ({sprites.Count})");
            if (m_showFrames)
            {
                EditorGUI.indentLevel++;
                int displayCount = Mathf.Min(sprites.Count, 30);
                for (int i = 0; i < displayCount; ++i)
                {
                    var sp = sprites[i];
                    if (sp == null) continue;
                    EditorGUILayout.BeginHorizontal();
                    EditorGUILayout.LabelField(sp.name, GUILayout.Width(150));
                    EditorGUILayout.LabelField($"{(int)sp.rect.width}x{(int)sp.rect.height}px", GUILayout.Width(80));
                    float px = sp.rect.width > 0 ? sp.pivot.x / sp.rect.width : 0.5f;
                    float py = sp.rect.height > 0 ? sp.pivot.y / sp.rect.height : 0.0f;
                    EditorGUILayout.LabelField($"{BentoI18n.Tr("mire")}: ({px:F2}, {py:F2})", EditorStyles.miniLabel);
                    EditorGUILayout.EndHorizontal();
                }
                if (sprites.Count > 30)
                {
                    EditorGUILayout.LabelField(string.Format(BentoI18n.Tr("more_frames"), sprites.Count - 30), EditorStyles.centeredGreyMiniLabel);
                }
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndFoldoutHeaderGroup();

            EditorGUILayout.Space();

            // Section 3: Metadata / Atlas
            m_showMetadata = EditorGUILayout.BeginFoldoutHeaderGroup(m_showMetadata, BentoI18n.Tr("metadata_group"));
            if (m_showMetadata)
            {
                EditorGUI.indentLevel++;
                if (atlas != null)
                {
                    EditorGUILayout.LabelField(string.Format(BentoI18n.Tr("texture_atlas_info"), atlas.width, atlas.height, atlas.format));
                }
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndFoldoutHeaderGroup();

            EditorGUILayout.Space();
            EditorGUILayout.LabelField(BentoI18n.Tr("import_settings"), EditorStyles.boldLabel);

            SerializedProperty ppuProp = serializedObject.FindProperty("pixelsPerUnit");
            if (ppuProp != null) EditorGUILayout.PropertyField(ppuProp);

            SerializedProperty meshProp = serializedObject.FindProperty("enableTightMesh");
            if (meshProp != null) EditorGUILayout.PropertyField(meshProp, new GUIContent(BentoI18n.Tr("tight_mesh_label")));

            SerializedProperty animProp = serializedObject.FindProperty("generateAnimationClips");
            if (animProp != null) EditorGUILayout.PropertyField(animProp, new GUIContent(BentoI18n.Tr("generate_animation_clips")));

            serializedObject.ApplyModifiedProperties();
            ApplyRevertGUI();
        }
    }
}
