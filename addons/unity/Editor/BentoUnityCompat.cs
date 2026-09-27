#if !UNITY_5_3_OR_NEWER
using System;
using System.Collections.Generic;

namespace UnityEngine
{
    public struct Vector2
    {
        public float x;
        public float y;

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public static Vector2 zero => new Vector2(0f, 0f);
        public static Vector2 one => new Vector2(1f, 1f);

        public override string ToString() => $"({x}, {y})";
    }

    public struct Rect
    {
        public float x;
        public float y;
        public float width;
        public float height;

        public Rect(float x, float y, float width, float height)
        {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        }

        public override string ToString() => $"Rect(x:{x}, y:{y}, w:{width}, h:{height})";
    }

    public static class Mathf
    {
        public static float Clamp(float value, float min, float max) => Math.Clamp(value, min, max);
    }

    public enum FilterMode
    {
        Point = 0,
        Bilinear = 1,
        Trilinear = 2
    }

    public enum TextureWrapMode
    {
        Repeat = 0,
        Clamp = 1
    }

    public enum TextureFormat
    {
        RGBA32 = 4
    }

    public enum SpriteMeshType
    {
        FullRect = 0,
        Tight = 1
    }

    public class Object
    {
        public string name = "";
    }

    public class Texture2D : Object
    {
        public int width { get; set; } = 2;
        public int height { get; set; } = 2;
        public FilterMode filterMode { get; set; }
        public TextureWrapMode wrapMode { get; set; }

        public Texture2D(int width, int height, TextureFormat format, bool mipChain)
        {
            this.width = width;
            this.height = height;
        }

        public bool LoadImage(byte[] data)
        {
            if (data == null || data.Length == 0) return false;
            // Mock dimensions for testing
            this.width = 128;
            this.height = 128;
            return true;
        }
    }

    public class Sprite : Object
    {
        public Rect rect;
        public Vector2 pivot;
        public float pixelsPerUnit;
        public Vector2[] overrideVertices;
        public ushort[] overrideTriangles;

        public static Sprite Create(Texture2D texture, Rect rect, Vector2 pivot, float pixelsPerUnit, uint extrude, SpriteMeshType meshType)
        {
            return new Sprite
            {
                rect = rect,
                pivot = pivot,
                pixelsPerUnit = pixelsPerUnit
            };
        }

        public void OverrideGeometry(Vector2[] vertices, ushort[] triangles)
        {
            this.overrideVertices = vertices;
            this.overrideTriangles = triangles;
        }

        public int GetPhysicsShapeCount() => 0;
        public void GetPhysicsShape(int index, List<Vector2> path) {}
    }

    public class SpriteRenderer
    {
        public Sprite sprite;
    }

    public class AnimationClip : Object
    {
        public float frameRate = 12f;
    }

    public enum RuntimePlatform
    {
        OSXEditor = 0,
        OSXPlayer = 1,
        WindowsPlayer = 2,
        WindowsEditor = 7,
        LinuxPlayer = 13,
        LinuxEditor = 16
    }

    public enum SystemLanguage
    {
        English = 10,
        French = 14,
        German = 15,
        Japanese = 22,
        Korean = 23,
        Portuguese = 28,
        Spanish = 34,
        Chinese = 6,
        ChineseSimplified = 40,
        ChineseTraditional = 41
    }

    public static class Application
    {
        public static string dataPath => AppDomain.CurrentDomain.BaseDirectory;
        public static SystemLanguage systemLanguage => SystemLanguage.English;
        public static RuntimePlatform platform =>
            OperatingSystem.IsWindows() ? RuntimePlatform.WindowsEditor :
            OperatingSystem.IsMacOS() ? RuntimePlatform.OSXEditor :
            RuntimePlatform.LinuxEditor;
    }

    public static class GUILayout
    {
        public static object Width(float width) => null;
        public static object Height(float height) => null;
    }

    public static class Debug
    {
        public static void Log(object message) => Console.WriteLine(message);
        public static void LogWarning(object message) => Console.WriteLine("[WARN] " + message);
        public static void LogError(object message) => Console.WriteLine("[ERROR] " + message);
    }
}

namespace UnityEditor
{
    using UnityEngine;

    public static class EditorPrefs
    {
        private static readonly Dictionary<string, int> s_prefs = new Dictionary<string, int>();
        public static int GetInt(string key, int defaultValue) => s_prefs.TryGetValue(key, out int v) ? v : defaultValue;
        public static void SetInt(string key, int val) => s_prefs[key] = val;
    }

    public static class EditorGUI
    {
        public static void BeginChangeCheck() {}
        public static bool EndChangeCheck() => false;
    }

    public static class EditorGUILayout
    {
        public static int Popup(int selectedIndex, string[] displayedOptions, params object[] options) => selectedIndex;
    }

    public static class SceneView
    {
        public static void RepaintAll() {}
    }

    public struct EditorCurveBinding
    {
        public Type type;
        public string path;
        public string propertyName;
    }

    public struct ObjectReferenceKeyframe
    {
        public float time;
        public UnityEngine.Object value;
    }

    public class AnimationClipSettings
    {
        public bool loopTime;
    }

    public static class AnimationUtility
    {
        public static void SetObjectReferenceCurve(AnimationClip clip, EditorCurveBinding binding, ObjectReferenceKeyframe[] keyframes) {}
        public static AnimationClipSettings GetAnimationClipSettings(AnimationClip clip) => new AnimationClipSettings();
        public static void SetAnimationClipSettings(AnimationClip clip, AnimationClipSettings settings) {}
    }
}
#endif
