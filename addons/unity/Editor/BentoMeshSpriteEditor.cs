using System;
using System.Linq;
using UnityEngine;
using UnityEditor;
using BentoPack.Runtime;

namespace BentoPack.Editor
{
    /// <summary>
    /// Custom Inspector for BentoMeshSprite offering an instant animation switcher dropdown,
    /// play/pause controls, and frame scrubber right inside the Unity Inspector.
    /// </summary>
    [CustomEditor(typeof(BentoMeshSprite)), CanEditMultipleObjects]
    public class BentoMeshSpriteEditor : UnityEditor.Editor
    {
        private bool m_showAdvanced = false;

        public override void OnInspectorGUI()
        {
            if (targets.Length > 1)
            {
                EditorGUILayout.HelpBox(string.Format(BentoI18n.Tr("multi_selected"), targets.Length), MessageType.Info);
                DrawDefaultInspector();
                return;
            }

            BentoMeshSprite player = (BentoMeshSprite)target;

            EditorGUILayout.Space();

            // Reimport & Synchronization Card
            string bentoPath = BentoAssetPostprocessor.GetBentoPathForPlayer(player);
            string bentoFileName = !string.IsNullOrEmpty(bentoPath) ? System.IO.Path.GetFileName(bentoPath) : BentoI18n.Tr("no_bento_asset");
            string extPath = player.ExternalSourcePath;
            if (string.IsNullOrEmpty(extPath) && !string.IsNullOrEmpty(bentoPath))
            {
                extPath = BentoAssetPostprocessor.AutoDetectExternalSource(bentoPath);
                if (!string.IsNullOrEmpty(extPath))
                {
                    player.ExternalSourcePath = extPath;
                    EditorUtility.SetDirty(player);
                }
            }

            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField($"{BentoI18n.Tr("unity_asset")} : {bentoFileName}", EditorStyles.boldLabel);
            BentoI18n.DrawLanguageSelector();
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField(bentoPath, EditorStyles.miniLabel);
            if (!string.IsNullOrEmpty(bentoPath) && GUILayout.Button(BentoI18n.Tr("reimport"), GUILayout.Width(170), GUILayout.Height(22)))
            {
                if (!string.IsNullOrEmpty(player.ExternalSourcePath) && System.IO.File.Exists(player.ExternalSourcePath))
                {
                    System.IO.File.Copy(player.ExternalSourcePath, System.IO.Path.GetFullPath(bentoPath), true);
                }
                AssetDatabase.ImportAsset(bentoPath, ImportAssetOptions.ForceUpdate);
                bool ok = BentoAssetPostprocessor.ReloadPlayer(player, bentoPath);
                if (ok)
                {
                    EditorUtility.SetDirty(player);
                    SceneView.RepaintAll();
                    Debug.Log($"[BentoPack] {BentoI18n.Tr("sync_success")} ('{bentoFileName}')");
                }
            }
            EditorGUILayout.EndHorizontal();

            // External Source File section
            EditorGUILayout.Space(2);
            EditorGUILayout.BeginHorizontal();
            EditorGUI.BeginChangeCheck();
            string newExt = EditorGUILayout.TextField(BentoI18n.Tr("external_source"), player.ExternalSourcePath);
            if (EditorGUI.EndChangeCheck())
            {
                Undo.RecordObject(player, "Change External Bento Source");
                player.ExternalSourcePath = newExt;
                EditorUtility.SetDirty(player);
            }
            if (GUILayout.Button("📂", GUILayout.Width(28), GUILayout.Height(18)))
            {
                string picked = EditorUtility.OpenFilePanel(BentoI18n.Tr("pick_source_title"), "", "bento");
                if (!string.IsNullOrEmpty(picked))
                {
                    Undo.RecordObject(player, "Link External Bento Source");
                    player.ExternalSourcePath = picked;
                    EditorUtility.SetDirty(player);
                    // Sync immediately
                    if (!string.IsNullOrEmpty(bentoPath))
                    {
                        System.IO.File.Copy(picked, System.IO.Path.GetFullPath(bentoPath), true);
                        AssetDatabase.ImportAsset(bentoPath, ImportAssetOptions.ForceUpdate);
                        BentoAssetPostprocessor.ReloadPlayer(player, bentoPath);
                        SceneView.RepaintAll();
                    }
                }
            }
            EditorGUILayout.EndHorizontal();

            if (!string.IsNullOrEmpty(player.ExternalSourcePath))
            {
                if (System.IO.File.Exists(player.ExternalSourcePath))
                {
                    DateTime extTime = System.IO.File.GetLastWriteTime(player.ExternalSourcePath);
                    EditorGUILayout.LabelField($"{BentoI18n.Tr("ext_linked")}{player.ExternalSourcePath} ({extTime:HH:mm:ss})", EditorStyles.miniLabel);
                }
                else
                {
                    EditorGUILayout.HelpBox(string.Format(BentoI18n.Tr("file_not_found"), player.ExternalSourcePath), MessageType.Warning);
                }
            }
            else
            {
                EditorGUILayout.HelpBox(BentoI18n.Tr("ext_not_linked"), MessageType.Info);
            }

            EditorGUILayout.EndVertical();

            EditorGUILayout.Space();
            EditorGUILayout.LabelField(BentoI18n.Tr("controller_title"), EditorStyles.boldLabel);

            // Animation Selector Dropdown
            if (player.Animations != null && player.Animations.Count > 0)
            {
                string[] names = player.Animations.Select(a => $"{a.name} ({a.frames.Count} f @ {a.fps} FPS)").ToArray();
                int currentIdx = Mathf.Clamp(player.CurrentAnimationIndex, 0, names.Length - 1);

                var targetCol = player.SyncCollider != null ? player.SyncCollider : player.GetComponent<PolygonCollider2D>();

                EditorGUI.BeginChangeCheck();
                int newIdx = EditorGUILayout.Popup(BentoI18n.Tr("active_animation"), currentIdx, names);
                if (EditorGUI.EndChangeCheck() || newIdx != player.CurrentAnimationIndex)
                {
                    Undo.RecordObject(player, "Switch BentoPack Animation");
                    if (targetCol != null) Undo.RecordObject(targetCol, "Switch BentoPack Animation Hitbox");

                    // Automatically update GameObject name so it matches the active animation!
                    string curGoName = player.gameObject.name;
                    int underIdx = curGoName.IndexOf('_');
                    string basePrefix = underIdx > 0 ? curGoName.Substring(0, underIdx) : curGoName;
                    string targetName = $"{basePrefix}_{player.Animations[newIdx].name}";
                    if (player.gameObject.name != targetName)
                    {
                        Undo.RecordObject(player.gameObject, "Update Character Name");
                        player.gameObject.name = targetName;
                    }

                    player.PlayAnimation(newIdx);
                    EditorUtility.SetDirty(player);
                    if (targetCol != null) EditorUtility.SetDirty(targetCol);
                    SceneView.RepaintAll();
                }

                // Playback Controls
                EditorGUILayout.BeginHorizontal();
                if (GUILayout.Button(player.IsPlaying ? BentoI18n.Tr("pause") : BentoI18n.Tr("play"), GUILayout.Height(24)))
                {
                    Undo.RecordObject(player, "Toggle BentoPack Playback");
                    if (player.IsPlaying) player.Stop(); else player.Play();
                    EditorUtility.SetDirty(player);
                    SceneView.RepaintAll();
                }
                if (GUILayout.Button(BentoI18n.Tr("stop"), GUILayout.Height(24)))
                {
                    Undo.RecordObject(player, "Stop BentoPack Playback");
                    if (targetCol != null) Undo.RecordObject(targetCol, "Stop BentoPack Animation Hitbox");
                    player.Stop();
                    player.SetFrame(0);
                    EditorUtility.SetDirty(player);
                    if (targetCol != null) EditorUtility.SetDirty(targetCol);
                    SceneView.RepaintAll();
                }
                EditorGUILayout.EndHorizontal();

                // Frame Slider
                if (player.Frames != null && player.Frames.Count > 1)
                {
                    EditorGUI.BeginChangeCheck();
                    int newFrame = EditorGUILayout.IntSlider(BentoI18n.Tr("frame"), player.CurrentFrame, 0, player.Frames.Count - 1);
                    if (EditorGUI.EndChangeCheck())
                    {
                        Undo.RecordObject(player, "Seek BentoPack Frame");
                        if (targetCol != null) Undo.RecordObject(targetCol, "Seek BentoPack Frame Hitbox");
                        player.SetFrame(newFrame);
                        EditorUtility.SetDirty(player);
                        if (targetCol != null) EditorUtility.SetDirty(targetCol);
                        SceneView.RepaintAll();
                    }
                }

                // Quick Flip Controls
                var sr = player.GetComponent<SpriteRenderer>();
                if (sr != null)
                {
                    EditorGUILayout.BeginHorizontal();
                    EditorGUI.BeginChangeCheck();
                    bool newFlipX = EditorGUILayout.ToggleLeft(BentoI18n.Tr("flip_x"), sr.flipX, GUILayout.Width(85));
                    bool newFlipY = EditorGUILayout.ToggleLeft(BentoI18n.Tr("flip_y"), sr.flipY, GUILayout.Width(85));
                    if (EditorGUI.EndChangeCheck())
                    {
                        Undo.RecordObject(sr, "Toggle Sprite Flip");
                        if (targetCol != null) Undo.RecordObject(targetCol, "Toggle Sprite Flip Hitbox");
                        sr.flipX = newFlipX;
                        sr.flipY = newFlipY;
                        player.SetFrame(player.CurrentFrame);
                        EditorUtility.SetDirty(sr);
                        if (targetCol != null) EditorUtility.SetDirty(targetCol);
                        SceneView.RepaintAll();
                    }
                    EditorGUILayout.EndHorizontal();
                }
            }

            // Hitbox status banner
            var col = player.SyncCollider != null ? player.SyncCollider : player.GetComponent<PolygonCollider2D>();
            if (col != null && col.pathCount > 0)
            {
                EditorGUILayout.Space();
                int ptCount = col.GetPath(0).Length;
                string animName = (player.Animations != null && player.CurrentAnimationIndex >= 0 && player.CurrentAnimationIndex < player.Animations.Count)
                    ? player.Animations[player.CurrentAnimationIndex].name
                    : "Default";
                int totalFrames = player.Frames != null ? player.Frames.Count : 0;
                string goName = col.gameObject.name;

                Bounds b = col.bounds;
                bool isMicroscopic = (b.size.x > 0 && b.size.x < 0.05f && b.size.y < 0.05f);

                if (isMicroscopic)
                {
                    EditorGUILayout.HelpBox(BentoI18n.Tr("microscopic_warning"), MessageType.Warning);
                    if (GUILayout.Button(BentoI18n.Tr("reimport_auto"), GUILayout.Height(30)))
                    {
                        if (player.Frames != null && player.Frames.Count > 0 && player.Frames[0] != null)
                        {
                            string assetPath = AssetDatabase.GetAssetPath(player.Frames[0]);
                            if (!string.IsNullOrEmpty(assetPath))
                            {
                                AssetDatabase.ImportAsset(assetPath, ImportAssetOptions.ForceUpdate);
                                player.PlayAnimation(player.CurrentAnimationIndex);
                                SceneView.RepaintAll();
                            }
                        }
                    }
                }
                else
                {
                    string infoMsg = string.Format(BentoI18n.Tr("hitbox_status"), ptCount, animName, player.CurrentFrame + 1, totalFrames);
                    EditorGUILayout.HelpBox(infoMsg, MessageType.Info);
                }
            }

            // Advanced properties foldout (collapsed by default to keep the inspector clean)
            EditorGUILayout.Space();
            m_showAdvanced = EditorGUILayout.Foldout(m_showAdvanced, BentoI18n.Tr("advanced_properties"), true);
            if (m_showAdvanced)
            {
                EditorGUI.indentLevel++;
                DrawDefaultInspector();
                EditorGUI.indentLevel--;
            }
        }

        private void OnEnable()
        {
            SceneView.duringSceneGui += OnDuringSceneGUI;
            SceneView.RepaintAll();
        }

        private void OnDisable()
        {
            SceneView.duringSceneGui -= OnDuringSceneGUI;
            SceneView.RepaintAll();
        }

        private void OnDuringSceneGUI(SceneView sceneView)
        {
            if (targets == null || targets.Length == 0) return;

            foreach (var t in targets)
            {
                if (t is BentoMeshSprite player && player != null)
                {
                    DrawPlayerSceneGUI(player);
                }
            }
        }

        private void OnSceneGUI()
        {
            // Fallback for standard OnSceneGUI calls
            if (targets == null || targets.Length == 0) return;
            foreach (var t in targets)
            {
                if (t is BentoMeshSprite player && player != null)
                {
                    DrawPlayerSceneGUI(player);
                }
            }
        }

        private void DrawPlayerSceneGUI(BentoMeshSprite player)
        {
            var col = player.SyncCollider != null ? player.SyncCollider : player.GetComponent<PolygonCollider2D>();
            if (col == null || col.pathCount == 0) return;

            Vector2[] path = col.GetPath(0);
            if (path == null || path.Length < 3) return;

            Vector3[] worldPoints = new Vector3[path.Length + 1];
            for (int i = 0; i < path.Length; ++i)
            {
                worldPoints[i] = player.transform.TransformPoint(path[i] + col.offset);
            }
            worldPoints[path.Length] = worldPoints[0];

            // Always render on top of meshes, wireframes, and selection outlines
            Handles.zTest = UnityEngine.Rendering.CompareFunction.Always;

            // 1. Draw prominent neon-green anti-aliased polyline outline (thick 6px)
            Handles.color = new Color(0.0f, 1.0f, 0.35f, 1.0f);
            Handles.DrawAAPolyLine(6.0f, worldPoints);

            // 2. Draw yellow spheres on every vertex so they POP against black/orange wireframe
            Handles.color = new Color(1.0f, 0.9f, 0.1f, 1.0f);
            float dotSize = HandleUtility.GetHandleSize(player.transform.position) * 0.035f;
            for (int i = 0; i < path.Length; ++i)
            {
                Handles.SphereHandleCap(0, worldPoints[i], Quaternion.identity, dotSize, EventType.Repaint);
            }

            // 3. Draw Mire (ground pivot / anchor crosshair) + Ground Line (Ligne de Sol)
            Vector3 pivotWorld = player.transform.position;
            float discRadius = HandleUtility.GetHandleSize(pivotWorld) * 0.08f;
            float lineLen = discRadius * 2.0f;

            // Reticle / Mire crosshair in vibrant red + cyan center
            Handles.color = new Color(1.0f, 0.2f, 0.2f, 1.0f);
            Handles.DrawWireDisc(pivotWorld, Vector3.forward, discRadius);
            Handles.DrawLine(pivotWorld - Vector3.right * lineLen, pivotWorld + Vector3.right * lineLen);
            Handles.DrawLine(pivotWorld - Vector3.up * lineLen, pivotWorld + Vector3.up * lineLen);

            Handles.color = new Color(0.0f, 0.9f, 1.0f, 0.9f);
            Handles.DrawSolidDisc(pivotWorld, Vector3.forward, discRadius * 0.35f);

            // Ground line (Ligne de Sol) spanning across the character
            Handles.color = new Color(1.0f, 0.85f, 0.2f, 0.8f);
            float groundWidth = HandleUtility.GetHandleSize(pivotWorld) * 1.5f;
            Handles.DrawDottedLine(pivotWorld - Vector3.right * groundWidth, pivotWorld + Vector3.right * groundWidth, 4f);

            // 4. Label indicating active animation, frame, and pivot coords
            string animName = (player.Animations != null && player.CurrentAnimationIndex >= 0 && player.CurrentAnimationIndex < player.Animations.Count)
                ? player.Animations[player.CurrentAnimationIndex].name
                : "Hitbox";

            Sprite curSprite = (player.Frames != null && player.CurrentFrame >= 0 && player.CurrentFrame < player.Frames.Count)
                ? player.Frames[player.CurrentFrame]
                : (player.GetComponent<SpriteRenderer>()?.sprite);

            string pivotInfo = "";
            if (curSprite != null && curSprite.rect.width > 0 && curSprite.rect.height > 0)
            {
                float px = curSprite.pivot.x / curSprite.rect.width;
                float py = curSprite.pivot.y / curSprite.rect.height;
                string presetStr = (py < 0.05f) ? BentoI18n.Tr("ground") : (Mathf.Abs(py - 0.5f) < 0.05f ? BentoI18n.Tr("center") : "Custom");
                pivotInfo = $" | {BentoI18n.Tr("mire")}: ({px:0.00}, {py:0.00}) [{presetStr}]";
            }

            GUIStyle labelStyle = new GUIStyle(EditorStyles.boldLabel);
            labelStyle.normal.textColor = Color.white;
            Handles.Label(pivotWorld + Vector3.down * (discRadius * 1.5f), $"🎯 {player.gameObject.name} ({animName} F:{player.CurrentFrame + 1}{pivotInfo})", labelStyle);
        }
    }
}
