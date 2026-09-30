using System;
using System.IO;
using UnityEngine;
using UnityEditor;

namespace BentoPack.Editor
{
    /// <summary>
    /// Dashboard EditorWindow for BentoPack in Unity.
    /// Provides atlas packing, auto-slicing, fixed grid cutting, M8 tight packing, and watch daemon management.
    /// </summary>
    public class BentoPackWindow : EditorWindow
    {
        [MenuItem("Window/2D/BentoPack Dashboard", false, 100)]
        [MenuItem("Window/BentoPack Dashboard", false, 500)]
        public static void ShowWindow()
        {
            var window = GetWindow<BentoPackWindow>(false, "BentoPack", true);
            window.minSize = new Vector2(420, 360);
            window.Show();
        }

        private string m_sourcePath = "";
        private string m_targetBentoPath = "Assets/character.bento";
        private int m_sliceMode = 0; // 0: Auto, 1: Grid, 2: Loose Files
        private int m_tileW = 32;
        private int m_tileH = 32;
        private int m_algo = 0; // 0: MaxRects, 1: M8 Tight Polygon
        private bool m_watchMode = false;

        private string m_statusMessage = "";
        private MessageType m_statusType = MessageType.None;
        private Vector2 m_scrollPos;

        private void OnEnable()
        {
            if (string.IsNullOrEmpty(m_statusMessage))
            {
                m_statusMessage = BentoI18n.Tr("status_ready");
                m_statusType = MessageType.Info;
            }
        }

        private void OnGUI()
        {
            m_scrollPos = EditorGUILayout.BeginScrollView(m_scrollPos);

            EditorGUILayout.Space(6);

            // --- Header Bar ---
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.BeginHorizontal();
            GUILayout.Label("🍱 " + BentoI18n.Tr("dashboard_title"), EditorStyles.boldLabel);
            GUILayout.FlexibleSpace();
            BentoI18n.DrawLanguageSelector();
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.Space(2);

            // CLI Status Badge & Controls
            string cliPath = BentoCliBridge.FindCliPath();
            bool cliFound = !string.IsNullOrEmpty(cliPath);

            EditorGUILayout.BeginHorizontal();
            if (cliFound)
            {
                GUI.color = new Color(0.4f, 1f, 0.4f);
                EditorGUILayout.LabelField(string.Format(BentoI18n.Tr("cli_ok"), Path.GetFileName(cliPath)), EditorStyles.miniLabel);
                GUI.color = Color.white;
            }
            else
            {
                GUI.color = new Color(1f, 0.4f, 0.4f);
                EditorGUILayout.LabelField(BentoI18n.Tr("cli_not_found"), EditorStyles.miniLabel);
                GUI.color = Color.white;

                if (GUILayout.Button(BentoI18n.Tr("download_cli"), EditorStyles.miniButton, GUILayout.Width(130)))
                {
                    Application.OpenURL(BentoCliBridge.RELEASES_URL);
                }
            }

            GUILayout.FlexibleSpace();

            if (GUILayout.Button(BentoI18n.Tr("open_gui"), EditorStyles.miniButton, GUILayout.Width(140)))
            {
                string targetToOpen = !string.IsNullOrEmpty(m_targetBentoPath) ? m_targetBentoPath : m_sourcePath;
                if (!string.IsNullOrEmpty(targetToOpen))
                {
                    targetToOpen = Path.GetFullPath(targetToOpen);
                }
                BentoCliBridge.OpenInBentoPack(targetToOpen);
            }
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.EndVertical();

            EditorGUILayout.Space(8);

            // --- Section 1: Paths ---
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.LabelField(BentoI18n.Tr("source_atlas"), EditorStyles.boldLabel);

            EditorGUILayout.BeginHorizontal();
            m_sourcePath = EditorGUILayout.TextField(m_sourcePath);
            HandleDragAndDropSource();

            if (GUILayout.Button(BentoI18n.Tr("browse"), GUILayout.Width(80)))
            {
                string chosen = EditorUtility.OpenFilePanel("Select Source Atlas or Directory", "Assets", "png,webp,jpg,jpeg");
                if (!string.IsNullOrEmpty(chosen))
                {
                    // Convert to relative Assets path if within project
                    m_sourcePath = ToRelativeAssetPath(chosen);
                    if (string.IsNullOrEmpty(m_targetBentoPath) || m_targetBentoPath == "Assets/character.bento")
                    {
                        string baseName = Path.GetFileNameWithoutExtension(m_sourcePath);
                        string dir = Path.GetDirectoryName(m_sourcePath).Replace("\\", "/");
                        m_targetBentoPath = $"{dir}/{baseName}.bento";
                    }
                }
            }
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.Space(4);
            EditorGUILayout.LabelField(BentoI18n.Tr("target_project"), EditorStyles.boldLabel);

            EditorGUILayout.BeginHorizontal();
            m_targetBentoPath = EditorGUILayout.TextField(m_targetBentoPath);
            if (GUILayout.Button(BentoI18n.Tr("browse"), GUILayout.Width(80)))
            {
                string chosen = EditorUtility.SaveFilePanel("Target BentoPack Project", "Assets", "character", "bento");
                if (!string.IsNullOrEmpty(chosen))
                {
                    m_targetBentoPath = ToRelativeAssetPath(chosen);
                }
            }
            EditorGUILayout.EndHorizontal();
            EditorGUILayout.EndVertical();

            EditorGUILayout.Space(8);

            // --- Section 2: Parameters ---
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.LabelField(BentoI18n.Tr("import_settings"), EditorStyles.boldLabel);

            string[] sliceOptions = {
                BentoI18n.Tr("slice_mode_auto"),
                BentoI18n.Tr("slice_mode_grid"),
                BentoI18n.Tr("slice_mode_files")
            };
            m_sliceMode = EditorGUILayout.Popup(BentoI18n.Tr("slice_label"), m_sliceMode, sliceOptions);

            if (m_sliceMode == 1) // Grid
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.BeginHorizontal();
                m_tileW = EditorGUILayout.IntField("Tile Width (px)", Mathf.Clamp(m_tileW, 4, 2048));
                m_tileH = EditorGUILayout.IntField("Tile Height (px)", Mathf.Clamp(m_tileH, 4, 2048));
                EditorGUILayout.EndHorizontal();
                EditorGUI.indentLevel--;
            }

            string[] algoOptions = {
                BentoI18n.Tr("algo_maxrects"),
                BentoI18n.Tr("algo_m8")
            };
            m_algo = EditorGUILayout.Popup(BentoI18n.Tr("packing_label"), m_algo, algoOptions);

            EditorGUILayout.Space(4);
            m_watchMode = EditorGUILayout.ToggleLeft(" " + BentoI18n.Tr("watcher_mode"), m_watchMode);

            EditorGUILayout.EndVertical();

            EditorGUILayout.Space(8);

            // --- Section 3: Action Buttons ---
            EditorGUILayout.BeginHorizontal();

            GUI.backgroundColor = new Color(0.3f, 0.9f, 0.4f);
            if (GUILayout.Button(BentoI18n.Tr("process_btn"), GUILayout.Height(34)))
            {
                ProcessAtlas();
            }
            GUI.backgroundColor = Color.white;

            bool isWatchActive = BentoCliBridge.IsWatchActive;
            GUI.enabled = isWatchActive;
            if (GUILayout.Button(BentoI18n.Tr("stop_watcher"), GUILayout.Width(130), GUILayout.Height(34)))
            {
                BentoCliBridge.StopWatchDaemon();
                m_statusMessage = BentoI18n.Tr("status_watcher_stopped");
                m_statusType = MessageType.Info;
            }
            GUI.enabled = true;

            EditorGUILayout.EndHorizontal();

            EditorGUILayout.Space(8);

            // --- Section 4: Status / Feedback ---
            if (isWatchActive)
            {
                EditorGUILayout.HelpBox(string.Format(BentoI18n.Tr("status_watcher_active"), BentoCliBridge.WatchPid), MessageType.Warning);
            }
            else if (!string.IsNullOrEmpty(m_statusMessage))
            {
                EditorGUILayout.HelpBox(m_statusMessage, m_statusType);
            }

            EditorGUILayout.EndScrollView();
        }

        private void HandleDragAndDropSource()
        {
            Event evt = Event.current;
            Rect dropRect = GUILayoutUtility.GetLastRect();

            if (dropRect.Contains(evt.mousePosition))
            {
                if (evt.type == EventType.DragUpdated)
                {
                    DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
                    evt.Use();
                }
                else if (evt.type == EventType.DragPerform)
                {
                    DragAndDrop.AcceptDrag();
                    foreach (var dragged in DragAndDrop.objectReferences)
                    {
                        string p = AssetDatabase.GetAssetPath(dragged);
                        if (!string.IsNullOrEmpty(p))
                        {
                            m_sourcePath = p;
                            if (string.IsNullOrEmpty(m_targetBentoPath) || m_targetBentoPath == "Assets/character.bento")
                            {
                                string baseName = Path.GetFileNameWithoutExtension(m_sourcePath);
                                string dir = Path.GetDirectoryName(m_sourcePath).Replace("\\", "/");
                                m_targetBentoPath = $"{dir}/{baseName}.bento";
                            }
                            GUI.changed = true;
                            break;
                        }
                    }
                    evt.Use();
                }
            }
        }

        private void ProcessAtlas()
        {
            string src = m_sourcePath.Trim();
            string target = m_targetBentoPath.Trim();

            if (string.IsNullOrEmpty(src))
            {
                m_statusMessage = BentoI18n.Tr("error_specify_source");
                m_statusType = MessageType.Error;
                return;
            }

            if (string.IsNullOrEmpty(target))
            {
                string baseName = Path.GetFileNameWithoutExtension(src);
                string dir = Path.GetDirectoryName(src).Replace("\\", "/");
                target = $"{dir}/{baseName}.bento";
                m_targetBentoPath = target;
            }

            string fullSrc = Path.GetFullPath(src);
            string fullTarget = Path.GetFullPath(target);

            string args = "";

            if (m_watchMode)
            {
                args += "--watch ";
            }

            if (m_sliceMode == 0) // Auto-Slice
            {
                args += $"slice ";
                if (target.EndsWith(".bento", StringComparison.OrdinalIgnoreCase))
                {
                    args += $"--output-project \"{fullTarget}\" ";
                }
                else
                {
                    args += $"--output-dir \"{Path.GetDirectoryName(fullTarget)}\" ";
                }
                args += $"\"{fullSrc}\"";
            }
            else if (m_sliceMode == 1) // Grid
            {
                args += $"pack --algorithm Grid --data \"{fullTarget}\" \"{fullSrc}\"";
            }
            else // Loose Files
            {
                string algoName = m_algo == 0 ? "MaxRects" : "TightPolygon";
                args += $"pack --algorithm {algoName} --data \"{fullTarget}\" \"{fullSrc}\"";
            }

            m_statusMessage = BentoI18n.Tr("status_processing");
            m_statusType = MessageType.Info;
            Repaint();

            if (m_watchMode)
            {
                if (BentoCliBridge.StartWatchDaemon(args, out int pid, out string err))
                {
                    m_statusMessage = string.Format(BentoI18n.Tr("status_watcher_active"), pid);
                    m_statusType = MessageType.Warning;
                }
                else
                {
                    m_statusMessage = string.Format(BentoI18n.Tr("status_error"), err);
                    m_statusType = MessageType.Error;
                }
            }
            else
            {
                EditorUtility.DisplayProgressBar("BentoPack", BentoI18n.Tr("status_processing"), 0.5f);
                try
                {
                    if (BentoCliBridge.RunCliCommand(args, out string stdout, out string stderr))
                    {
                        m_statusMessage = string.Format(BentoI18n.Tr("status_success"), Path.GetFileName(target));
                        m_statusType = MessageType.Info;
                        Debug.Log($"[BentoPack] {m_statusMessage}\n{stdout}");

                        // Force refresh of Unity AssetDatabase so newly generated .bento is imported immediately
                        AssetDatabase.Refresh(ImportAssetOptions.ForceUpdate);
                    }
                    else
                    {
                        m_statusMessage = string.Format(BentoI18n.Tr("status_error"), stderr);
                        m_statusType = MessageType.Error;
                        Debug.LogError($"[BentoPack] {m_statusMessage}\n{stderr}");
                    }
                }
                finally
                {
                    EditorUtility.ClearProgressBar();
                }
            }
        }

        private static string ToRelativeAssetPath(string fullPath)
        {
            string projectRoot = Path.GetFullPath(Application.dataPath + "/..").Replace("\\", "/");
            string normalized = fullPath.Replace("\\", "/");

            if (normalized.StartsWith(projectRoot, StringComparison.OrdinalIgnoreCase))
            {
                return normalized.Substring(projectRoot.Length).TrimStart('/');
            }
            return fullPath;
        }
    }
}
