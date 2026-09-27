using System;
using System.Collections.Generic;
using UnityEngine;

namespace BentoPack.Editor
{
    [Serializable]
    public class BentoProjectData
    {
        public string generator = "BentoPack";
        public string version = "1.0.0";
        public string format = "Unity2D_SpriteMesh";
        public string texture = "";
        public BentoSize textureSize = new BentoSize();
        public List<BentoSpriteData> sprites = new List<BentoSpriteData>();
        [NonSerialized]
        public Dictionary<string, BentoAnimationData> animations = new Dictionary<string, BentoAnimationData>();
    }

    [Serializable]
    public class BentoSize
    {
        public int w = 0;
        public int h = 0;
    }

    [Serializable]
    public class BentoRect
    {
        public int x = 0;
        public int y = 0;
        public int w = 0;
        public int h = 0;
    }

    [Serializable]
    public class BentoVector2
    {
        public float x = 0f;
        public float y = 0f;
    }

    [Serializable]
    public class BentoSpriteData
    {
        public string name = "";
        public BentoRect rect = new BentoRect();
        public BentoVector2 pivot = new BentoVector2 { x = 0.5f, y = 0.5f };
        public bool hasTightMesh = false;
        public List<float[]> vertices = new List<float[]>();
        public List<float[]> uvs = new List<float[]>();
        public List<int> triangles = new List<int>();
        public List<float[]> polygon = new List<float[]>();
    }

    [Serializable]
    public class BentoAnimationData
    {
        public float fps = 12f;
        public bool loop = true;
        public List<string> frames = new List<string>();
    }
}
