using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Events;

namespace BentoPack.Runtime
{
    [Serializable]
    public class BentoAnimationEntry
    {
        public string name = "Default";
        public float fps = 12f;
        public bool loop = true;
        public List<Sprite> frames = new List<Sprite>();
    }

    /// <summary>
    /// 2D Sprite Player component that renders tight polygonal meshes with zero transparent overdraw,
    /// and optionally synchronizes a PolygonCollider2D hitbox per frame.
    /// </summary>
    [RequireComponent(typeof(SpriteRenderer))]
    [ExecuteAlways]
    public class BentoMeshSprite : MonoBehaviour
    {
        [Header("Asset Source")]
        [SerializeField] private string m_sourceBentoPath = "";
        [Tooltip("Absolute path to the original .bento file on disk (edited in BentoPack).")]
        [SerializeField] private string m_externalSourcePath = "";

        public string SourceBentoPath
        {
            get => m_sourceBentoPath;
            set => m_sourceBentoPath = value;
        }

        public string ExternalSourcePath
        {
            get => m_externalSourcePath;
            set => m_externalSourcePath = value;
        }

        [Header("Animations")]
        [SerializeField] private List<BentoAnimationEntry> m_animations = new List<BentoAnimationEntry>();
        [SerializeField] private int m_currentAnimationIndex = 0;

        [Header("Frame Data")]
        [SerializeField] private List<Sprite> m_frames = new List<Sprite>();
        [SerializeField] private float m_fps = 12f;
        [SerializeField] private bool m_loop = true;
        [SerializeField] private bool m_playOnAwake = true;

        public List<BentoAnimationEntry> Animations => m_animations;
        public List<Sprite> Frames => m_frames;
        public int CurrentAnimationIndex => m_currentAnimationIndex;
        public float Fps { get => m_fps; set => m_fps = value; }
        public bool Loop { get => m_loop; set => m_loop = value; }

        public void AddAnimation(string name, List<Sprite> frames, float fps = 12f, bool loop = true)
        {
            if (m_animations == null) m_animations = new List<BentoAnimationEntry>();
            m_animations.Add(new BentoAnimationEntry
            {
                name = name,
                frames = frames ?? new List<Sprite>(),
                fps = fps > 0 ? fps : 12f,
                loop = loop
            });
        }

        public void PlayAnimation(int index)
        {
            if (m_animations == null || index < 0 || index >= m_animations.Count) return;
            m_currentAnimationIndex = index;
            var anim = m_animations[index];
#if UNITY_EDITOR
            if (!Application.isPlaying && gameObject != null && anim != null)
            {
                string cur = gameObject.name;
                int u = cur.IndexOf('_');
                string pfx = u > 0 ? cur.Substring(0, u) : cur;
                gameObject.name = $"{pfx}_{anim.name}";
            }
#endif
            SetAnimation(anim.frames, anim.fps, anim.loop);
        }

        public void PlayAnimation(string name)
        {
            if (m_animations == null || string.IsNullOrEmpty(name)) return;
            for (int i = 0; i < m_animations.Count; ++i)
            {
                if (m_animations[i].name.Equals(name, StringComparison.OrdinalIgnoreCase))
                {
                    PlayAnimation(i);
                    return;
                }
            }
        }

        [Header("Hitbox Synchronization")]
        [Tooltip("Composant PolygonCollider2D cible synchronisé à chaque frame avec la silhouette de l'animation active (par défaut le PolygonCollider2D sur ce GameObject).")]
        [SerializeField] private PolygonCollider2D m_syncCollider = null;

        public PolygonCollider2D SyncCollider => m_syncCollider;

        [Header("Events")]
        public UnityEvent<int> onFrameChanged;
        public UnityEvent onAnimationFinished;

        private SpriteRenderer m_renderer;
        private int m_currentFrame = 0;
        private float m_timer = 0f;
        private bool m_isPlaying = false;
        private bool m_lastFlipX = false;
        private bool m_lastFlipY = false;

        public int CurrentFrame
        {
            get => m_currentFrame;
            set => SetFrame(value);
        }

        public bool IsPlaying => m_isPlaying;

        private void CheckFlipState()
        {
            if (m_renderer == null) m_renderer = GetComponent<SpriteRenderer>();
            if (m_renderer != null)
            {
                if (m_renderer.flipX != m_lastFlipX || m_renderer.flipY != m_lastFlipY)
                {
                    m_lastFlipX = m_renderer.flipX;
                    m_lastFlipY = m_renderer.flipY;
                    SetFrame(m_currentFrame);
                }
            }
        }

        private void OnValidate()
        {
            CheckFlipState();
        }

        private void Awake()
        {
            m_renderer = GetComponent<SpriteRenderer>();
            if (m_renderer != null)
            {
                m_lastFlipX = m_renderer.flipX;
                m_lastFlipY = m_renderer.flipY;
            }
            if (m_syncCollider == null)
            {
                m_syncCollider = GetComponent<PolygonCollider2D>();
            }
        }

        private void Start()
        {
            if (m_playOnAwake && Application.isPlaying)
            {
                Play();
            }
            else
            {
                SetFrame(0);
            }
        }

        public void Play()
        {
            m_isPlaying = true;
            m_timer = 0f;
#if UNITY_EDITOR
            m_lastEditorTime = UnityEditor.EditorApplication.timeSinceStartup;
#endif
        }

        public void Stop()
        {
            m_isPlaying = false;
        }

        private void Update()
        {
            CheckFlipState();
            if (!m_isPlaying || m_frames == null || m_frames.Count <= 1 || m_fps <= 0f)
                return;

            m_timer += Time.deltaTime;
            float frameInterval = 1f / m_fps;

            if (m_timer >= frameInterval)
            {
                m_timer -= frameInterval;
                int nextFrame = m_currentFrame + 1;

                if (nextFrame >= m_frames.Count)
                {
                    if (m_loop)
                    {
                        SetFrame(0);
                    }
                    else
                    {
                        m_isPlaying = false;
                        onAnimationFinished?.Invoke();
                    }
                }
                else
                {
                    SetFrame(nextFrame);
                }
            }
        }

        public void SetFrame(int index)
        {
            if (m_frames == null || m_frames.Count == 0) return;

            m_currentFrame = Mathf.Clamp(index, 0, m_frames.Count - 1);
            Sprite sprite = m_frames[m_currentFrame];

            if (m_renderer == null) m_renderer = GetComponent<SpriteRenderer>();
            if (m_renderer != null)
            {
                m_renderer.sprite = sprite;
            }

            // Sync PolygonCollider2D if present and sprite has physics shape
            if (m_syncCollider == null) m_syncCollider = GetComponent<PolygonCollider2D>();
            if (m_syncCollider != null && sprite != null)
            {
                int shapeCount = sprite.GetPhysicsShapeCount();
                if (shapeCount > 0)
                {
                    m_syncCollider.pathCount = shapeCount;

                    bool flipX = m_renderer != null && m_renderer.flipX;
                    bool flipY = m_renderer != null && m_renderer.flipY;

                    List<Vector2> path = new List<Vector2>();
                    for (int i = 0; i < shapeCount; ++i)
                    {
                        path.Clear();
                        sprite.GetPhysicsShape(i, path);

                        if (flipX || flipY)
                        {
                            for (int p = 0; p < path.Count; ++p)
                            {
                                Vector2 pt = path[p];
                                path[p] = new Vector2(
                                    flipX ? -pt.x : pt.x,
                                    flipY ? -pt.y : pt.y
                                );
                            }

                            // If reflection across one axis (flipX XOR flipY),
                            // reverse vertex order to maintain counter-clockwise normal
                            if (flipX ^ flipY)
                            {
                                path.Reverse();
                            }
                        }

                        m_syncCollider.SetPath(i, path.ToArray());
                    }
#if UNITY_EDITOR
                    if (!Application.isPlaying)
                    {
                        UnityEditor.EditorUtility.SetDirty(m_syncCollider);
                    }
#endif
                }
            }

            onFrameChanged?.Invoke(m_currentFrame);
        }

        public void SetAnimation(List<Sprite> frames, float fps = 12f, bool loop = true)
        {
            m_frames = frames ?? new List<Sprite>();
            m_fps = fps > 0f ? fps : 12f;
            m_loop = loop;
            m_currentFrame = 0;
            m_timer = 0f;
            m_isPlaying = true;
#if UNITY_EDITOR
            m_lastEditorTime = UnityEditor.EditorApplication.timeSinceStartup;
#endif
            SetFrame(0);
        }

#if UNITY_EDITOR
        private double m_lastEditorTime;

        private void OnEnable()
        {
            m_lastEditorTime = UnityEditor.EditorApplication.timeSinceStartup;
            UnityEditor.EditorApplication.update += EditorUpdate;
        }

        private void OnDisable()
        {
            UnityEditor.EditorApplication.update -= EditorUpdate;
        }

        private void EditorUpdate()
        {
            if (Application.isPlaying) return;
            CheckFlipState();
            if (!m_isPlaying) return;
            if (m_frames == null || m_frames.Count <= 1 || m_fps <= 0f) return;

            double time = UnityEditor.EditorApplication.timeSinceStartup;
            double dt = time - m_lastEditorTime;
            m_lastEditorTime = time;

            if (dt <= 0 || dt > 1.0) return;

            m_timer += (float)dt;
            float frameInterval = 1f / m_fps;
            if (m_timer >= frameInterval)
            {
                m_timer -= frameInterval;
                int nextFrame = (m_currentFrame + 1) % m_frames.Count;
                SetFrame(nextFrame);
                UnityEditor.EditorUtility.SetDirty(this);
                UnityEditor.SceneView.RepaintAll();
            }
        }

        private void OnDrawGizmos()
        {
            DrawColliderGizmo(new Color(0f, 1f, 0.3f, 0.5f));
        }

        private void OnDrawGizmosSelected()
        {
            DrawColliderGizmo(new Color(0f, 1f, 0.3f, 1.0f));

            // Ground pivot / fixed point anchor crosshair
            Gizmos.color = Color.red;
            Gizmos.DrawWireSphere(transform.position, 0.05f);
            Gizmos.DrawLine(transform.position - Vector3.right * 0.15f, transform.position + Vector3.right * 0.15f);
            Gizmos.DrawLine(transform.position - Vector3.up * 0.15f, transform.position + Vector3.up * 0.15f);
        }

        private void DrawColliderGizmo(Color color)
        {
            if (m_syncCollider == null) m_syncCollider = GetComponent<PolygonCollider2D>();
            if (m_syncCollider == null || m_syncCollider.pathCount == 0) return;

            Vector2[] path = m_syncCollider.GetPath(0);
            if (path == null || path.Length < 3) return;

            Gizmos.color = color;
            for (int i = 0; i < path.Length; ++i)
            {
                Vector3 p1 = transform.TransformPoint(path[i] + m_syncCollider.offset);
                Vector3 p2 = transform.TransformPoint(path[(i + 1) % path.Length] + m_syncCollider.offset);
                Gizmos.DrawLine(p1, p2);
            }
        }
#endif
    }
}
