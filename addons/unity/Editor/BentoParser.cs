using System;
using System.IO;
using System.IO.Compression;
using System.Collections.Generic;
using UnityEngine;

namespace BentoPack.Editor
{
    public class BentoParsedArchive
    {
        public bool success = false;
        public string errorMessage = "";
        public BentoProjectData projectData;
        public Texture2D atlasTexture;
        public byte[] rawAtlasBytes;
        public string atlasFormat = "png";
    }

    /// <summary>
    /// Parses BentoPack archives (.bento) and JSON descriptors (.unity.json, project.json)
    /// into structured BentoProjectData with decoded textures.
    /// </summary>
    public static class BentoParser
    {
        public static BentoParsedArchive ParseBentoArchive(string filePath)
        {
            BentoParsedArchive result = new BentoParsedArchive();

            if (!File.Exists(filePath))
            {
                result.errorMessage = "File not found: " + filePath;
                return result;
            }

            try
            {
                using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read))
                using (ZipArchive zip = new ZipArchive(fs, ZipArchiveMode.Read))
                {
                    // 1. Find JSON metadata
                    ZipArchiveEntry jsonEntry = null;
                    foreach (ZipArchiveEntry entry in zip.Entries)
                    {
                        string name = entry.FullName.ToLowerInvariant();
                        if (name.EndsWith("project.json") || name.EndsWith(".unity.json"))
                        {
                            jsonEntry = entry;
                            break;
                        }
                    }

                    if (jsonEntry == null)
                    {
                        result.errorMessage = "Missing project.json or .unity.json in .bento archive";
                        return result;
                    }

                    string jsonText;
                    using (StreamReader reader = new StreamReader(jsonEntry.Open()))
                    {
                        jsonText = reader.ReadToEnd();
                    }

                    result.projectData = ParseJson(jsonText);
                    if (result.projectData == null)
                    {
                        result.errorMessage = "Failed to parse JSON metadata in archive";
                        return result;
                    }

                    // 2. Locate texture entry
                    ZipArchiveEntry texEntry = null;
                    string targetTexName = !string.IsNullOrEmpty(result.projectData.texture)
                        ? Path.GetFileName(result.projectData.texture).ToLowerInvariant()
                        : "";

                    if (!string.IsNullOrEmpty(targetTexName))
                    {
                        foreach (ZipArchiveEntry entry in zip.Entries)
                        {
                            if (entry.Name.ToLowerInvariant() == targetTexName)
                            {
                                texEntry = entry;
                                break;
                            }
                        }
                    }

                    if (texEntry == null)
                    {
                        foreach (ZipArchiveEntry entry in zip.Entries)
                        {
                            string n = entry.Name.ToLowerInvariant();
                            if (n.EndsWith(".png") || n.EndsWith(".webp") || n.EndsWith(".jpg") || n.EndsWith(".jpeg"))
                            {
                                texEntry = entry;
                                break;
                            }
                        }
                    }

                    if (texEntry != null)
                    {
                        using (MemoryStream ms = new MemoryStream())
                        {
                            texEntry.Open().CopyTo(ms);
                            result.rawAtlasBytes = ms.ToArray();
                        }

                        string ext = Path.GetExtension(texEntry.Name).ToLowerInvariant();
                        result.atlasFormat = ext.Replace(".", "");

                        Texture2D tex = new Texture2D(2, 2, TextureFormat.RGBA32, false);
                        tex.name = Path.GetFileNameWithoutExtension(filePath) + "_atlas";
                        tex.filterMode = FilterMode.Point;
                        tex.wrapMode = TextureWrapMode.Clamp;

                        if (tex.LoadImage(result.rawAtlasBytes))
                        {
                            result.atlasTexture = tex;
                        }
                        else
                        {
                            // If direct in-memory LoadImage failed (e.g. WebP format which Unity does not support in LoadImage):
                            // 1. First attempt direct loss-less image decompression (dwebp, python/PIL, convert, ffmpeg)
                            // This ensures the atlas is preserved 100% byte-for-byte WITHOUT any repacking or artifacts!
                            byte[] decodedPngBytes = TryDecompressWebpBytes(result.rawAtlasBytes);
                            if (decodedPngBytes != null && tex.LoadImage(decodedPngBytes))
                            {
                                result.atlasTexture = tex;
                                result.rawAtlasBytes = decodedPngBytes;
                                result.atlasFormat = "png";
                            }
                            else
                            {
                                // 2. Fallback to bentopack-cli (configured with KeepLayout)
                                string absPath = Path.GetFullPath(filePath);
                                string tempOut = Path.Combine(Path.GetTempPath(), "bentopack_temp_" + Guid.NewGuid().ToString("N"));
                                string tempJson = tempOut + ".unity.json";
                                string tempPng = tempOut + ".unity.png";
                                string args = $"bento \"{absPath}\" --export unity --output \"{tempJson}\"";

                                if (BentoCliBridge.RunCliCommand(args, out _, out _) && File.Exists(tempPng))
                                {
                                    byte[] pngBytes = File.ReadAllBytes(tempPng);
                                    if (tex.LoadImage(pngBytes))
                                    {
                                        result.atlasTexture = tex;
                                        result.rawAtlasBytes = pngBytes;
                                        result.atlasFormat = "png";

                                        if (File.Exists(tempJson))
                                        {
                                            string exportedJson = File.ReadAllText(tempJson);
                                            BentoProjectData updatedData = ParseJson(exportedJson);
                                            if (updatedData != null && updatedData.sprites.Count > 0)
                                            {
                                                if (result.projectData != null && result.projectData.animations != null && result.projectData.animations.Count > 0)
                                                {
                                                    updatedData.animations = result.projectData.animations;
                                                }
                                                result.projectData = updatedData;
                                            }
                                        }
                                    }
                                    try { File.Delete(tempJson); File.Delete(tempPng); } catch { }
                                }
                            }

                            if (result.atlasTexture == null)
                            {
                                Debug.LogWarning("[BentoPack] Failed to decode atlas texture from archive: " + texEntry.Name + 
                                    ". Unity does not natively decode WebP in memory. " +
                                    "Ensure 'dwebp', 'python3', or 'bentopack-cli' is available, or export as PNG/Unity format from BentoPack Studio.");
                            }
                        }
                    }

                    result.success = (result.atlasTexture != null);
                    return result;
                }
            }
            catch (Exception ex)
            {
                result.errorMessage = "Exception while parsing .bento archive: " + ex.Message;
                return result;
            }
        }

        public static BentoProjectData ParseJson(string jsonText)
        {
            if (string.IsNullOrWhiteSpace(jsonText)) return null;

            BentoProjectData data = new BentoProjectData();

            try
            {
                // Simple fast parser supporting Unity2D_SpriteMesh and standard BentoPack project format
                var dict = MiniJson.Deserialize(jsonText) as Dictionary<string, object>;
                if (dict == null) return null;

                if (dict.ContainsKey("generator")) data.generator = dict["generator"]?.ToString();
                if (dict.ContainsKey("version")) data.version = dict["version"]?.ToString();
                if (dict.ContainsKey("format")) data.format = dict["format"]?.ToString();
                if (dict.ContainsKey("texture")) data.texture = dict["texture"]?.ToString();

                // If texture is inside "atlas": { "file": "assets/atlas.webp" }
                if (string.IsNullOrEmpty(data.texture) && dict.ContainsKey("atlas") && dict["atlas"] is Dictionary<string, object> atlasDict)
                {
                    if (atlasDict.ContainsKey("file")) data.texture = atlasDict["file"]?.ToString();
                    if (atlasDict.ContainsKey("width")) data.textureSize.w = Convert.ToInt32(atlasDict["width"]);
                    if (atlasDict.ContainsKey("height")) data.textureSize.h = Convert.ToInt32(atlasDict["height"]);
                }

                if (dict.ContainsKey("textureSize") && dict["textureSize"] is Dictionary<string, object> ts)
                {
                    if (ts.ContainsKey("w")) data.textureSize.w = Convert.ToInt32(ts["w"]);
                    if (ts.ContainsKey("h")) data.textureSize.h = Convert.ToInt32(ts["h"]);
                }

                // 1. Format: Unity2D_SpriteMesh ("sprites" key)
                if (dict.ContainsKey("sprites") && dict["sprites"] is List<object> sList)
                {
                    foreach (var item in sList)
                    {
                        if (item is Dictionary<string, object> sDict)
                        {
                            BentoSpriteData s = ParseSpriteItem(sDict);
                            data.sprites.Add(s);
                        }
                    }
                }
                // 2. Format: BentoPack project.json ("boxes" key)
                else if (dict.ContainsKey("boxes") && dict["boxes"] is List<object> bList)
                {
                    int idx = 0;
                    foreach (var item in bList)
                    {
                        if (item is Dictionary<string, object> bDict)
                        {
                            BentoSpriteData s = ParseBoxItem(bDict, idx++);
                            data.sprites.Add(s);
                        }
                    }
                }
                // 3. Format: TexturePacker / CLI exported Unity JSON ("frames" key)
                else if (dict.ContainsKey("frames"))
                {
                    if (dict["frames"] is Dictionary<string, object> fDict)
                    {
                        int idx = 0;
                        foreach (var kvp in fDict)
                        {
                            if (kvp.Value is Dictionary<string, object> itemDict)
                            {
                                BentoSpriteData s = ParseFrameItem(kvp.Key, itemDict, idx++);
                                data.sprites.Add(s);
                            }
                        }
                    }
                    else if (dict["frames"] is List<object> fList)
                    {
                        int idx = 0;
                        foreach (var item in fList)
                        {
                            if (item is Dictionary<string, object> itemDict)
                            {
                                string fName = itemDict.ContainsKey("filename") ? itemDict["filename"]?.ToString() : $"frame_{idx:D4}";
                                BentoSpriteData s = ParseFrameItem(fName, itemDict, idx++);
                                data.sprites.Add(s);
                            }
                        }
                    }
                }

                // Parse Animations
                if (dict.ContainsKey("animations"))
                {
                    // Can be Dictionary<string, object> or List<object>
                    if (dict["animations"] is Dictionary<string, object> animMap)
                    {
                        foreach (var kvp in animMap)
                        {
                            if (kvp.Value is Dictionary<string, object> aDict)
                            {
                                BentoAnimationData a = ParseAnimationDict(aDict);
                                data.animations[kvp.Key] = a;
                            }
                        }
                    }
                    else if (dict["animations"] is List<object> animList)
                    {
                        foreach (var item in animList)
                        {
                            if (item is Dictionary<string, object> aDict)
                            {
                                string aName = aDict.ContainsKey("name") ? aDict["name"]?.ToString() : "anim";
                                BentoAnimationData a = ParseAnimationDict(aDict);
                                data.animations[aName] = a;
                            }
                        }
                    }
                }

                // 4. FrameTags in "meta" (TexturePacker format from CLI export)
                if (dict.ContainsKey("meta") && dict["meta"] is Dictionary<string, object> metaDict)
                {
                    if (metaDict.ContainsKey("frameTags") && metaDict["frameTags"] is List<object> tags)
                    {
                        foreach (var tagObj in tags)
                        {
                            if (tagObj is Dictionary<string, object> tag)
                            {
                                string tName = tag.ContainsKey("name") ? tag["name"]?.ToString() : "anim";
                                if (data.animations.ContainsKey(tName))
                                {
                                    // Never overwrite an explicit non-linear frame list with a contiguous range
                                    continue;
                                }
                                int from = tag.ContainsKey("from") ? Convert.ToInt32(tag["from"]) : 0;
                                int to = tag.ContainsKey("to") ? Convert.ToInt32(tag["to"]) : 0;
                                float fps = tag.ContainsKey("fps") ? Convert.ToSingle(tag["fps"]) : 12f;

                                BentoAnimationData a = new BentoAnimationData();
                                a.fps = fps;
                                a.loop = true;
                                int step = from <= to ? 1 : -1;
                                for (int fi = from; ; fi += step)
                                {
                                    a.frames.Add(fi.ToString());
                                    if (fi == to) break;
                                }
                                data.animations[tName] = a;
                            }
                        }
                    }
                }

                return data;
            }
            catch (Exception ex)
            {
                Debug.LogError("[BentoPack] Error parsing JSON: " + ex.Message);
                return null;
            }
        }

        private static BentoSpriteData ParseSpriteItem(Dictionary<string, object> sDict)
        {
            BentoSpriteData s = new BentoSpriteData();
            if (sDict.ContainsKey("name")) s.name = sDict["name"]?.ToString();

            if (sDict.ContainsKey("rect") && sDict["rect"] is Dictionary<string, object> r)
            {
                s.rect.x = Convert.ToInt32(r["x"]);
                s.rect.y = Convert.ToInt32(r["y"]);
                s.rect.w = Convert.ToInt32(r["w"]);
                s.rect.h = Convert.ToInt32(r["h"]);
            }

            if (sDict.ContainsKey("pivot") && sDict["pivot"] is Dictionary<string, object> piv)
            {
                s.pivot.x = Convert.ToSingle(piv["x"]);
                s.pivot.y = Convert.ToSingle(piv["y"]);
            }

            if (sDict.ContainsKey("hasTightMesh"))
            {
                s.hasTightMesh = Convert.ToBoolean(sDict["hasTightMesh"]);
            }

            if (sDict.ContainsKey("vertices") && sDict["vertices"] is List<object> vList && vList.Count > 0)
            {
                if (vList[0] is List<object>)
                {
                    foreach (var v in vList)
                    {
                        if (v is List<object> xy && xy.Count >= 2)
                        {
                            s.vertices.Add(new float[] { Convert.ToSingle(xy[0]), Convert.ToSingle(xy[1]) });
                        }
                    }
                }
                else
                {
                    for (int vi = 0; vi + 1 < vList.Count; vi += 2)
                    {
                        s.vertices.Add(new float[] { Convert.ToSingle(vList[vi]), Convert.ToSingle(vList[vi + 1]) });
                    }
                }
                if (s.vertices.Count > 0) s.hasTightMesh = true;
            }

            if (sDict.ContainsKey("triangles") && sDict["triangles"] is List<object> tList && tList.Count > 0)
            {
                if (tList[0] is List<object>)
                {
                    foreach (var t in tList)
                    {
                        if (t is List<object> tri)
                        {
                            foreach (var idx in tri)
                                s.triangles.Add(Convert.ToInt32(idx));
                        }
                    }
                }
                else
                {
                    foreach (var t in tList)
                    {
                        s.triangles.Add(Convert.ToInt32(t));
                    }
                }
            }

            if (sDict.ContainsKey("polygon") && sDict["polygon"] is List<object> pList && pList.Count > 0)
            {
                if (pList[0] is List<object>)
                {
                    foreach (var pt in pList)
                    {
                        if (pt is List<object> xy && xy.Count >= 2)
                        {
                            s.polygon.Add(new float[] { Convert.ToSingle(xy[0]), Convert.ToSingle(xy[1]) });
                        }
                    }
                }
                else
                {
                    for (int pi = 0; pi + 1 < pList.Count; pi += 2)
                    {
                        s.polygon.Add(new float[] { Convert.ToSingle(pList[pi]), Convert.ToSingle(pList[pi + 1]) });
                    }
                }
            }

            return s;
        }

        private static BentoSpriteData ParseBoxItem(Dictionary<string, object> bDict, int fallbackIndex)
        {
            BentoSpriteData s = new BentoSpriteData();
            s.name = bDict.ContainsKey("name") ? bDict["name"]?.ToString() : $"frame_{fallbackIndex:D4}";

            if (bDict.ContainsKey("rect"))
            {
                if (bDict["rect"] is Dictionary<string, object> rd)
                {
                    if (rd.ContainsKey("x")) s.rect.x = Convert.ToInt32(rd["x"]);
                    if (rd.ContainsKey("y")) s.rect.y = Convert.ToInt32(rd["y"]);
                    if (rd.ContainsKey("w")) s.rect.w = Convert.ToInt32(rd["w"]);
                    else if (rd.ContainsKey("width")) s.rect.w = Convert.ToInt32(rd["width"]);
                    if (rd.ContainsKey("h")) s.rect.h = Convert.ToInt32(rd["h"]);
                    else if (rd.ContainsKey("height")) s.rect.h = Convert.ToInt32(rd["height"]);
                }
                else if (bDict["rect"] is List<object> r && r.Count >= 4)
                {
                    s.rect.x = Convert.ToInt32(r[0]);
                    s.rect.y = Convert.ToInt32(r[1]);
                    s.rect.w = Convert.ToInt32(r[2]);
                    s.rect.h = Convert.ToInt32(r[3]);
                }
            }

            if (bDict.ContainsKey("pivot"))
            {
                float px = 0.5f * s.rect.w;
                float py = s.rect.h; // default feet
                bool hasPivot = false;

                if (bDict["pivot"] is Dictionary<string, object> pivDict)
                {
                    if (pivDict.ContainsKey("x")) px = Convert.ToSingle(pivDict["x"]);
                    if (pivDict.ContainsKey("y")) py = Convert.ToSingle(pivDict["y"]);
                    hasPivot = true;
                }
                else if (bDict["pivot"] is List<object> pivList && pivList.Count >= 2)
                {
                    px = Convert.ToSingle(pivList[0]);
                    py = Convert.ToSingle(pivList[1]);
                    hasPivot = true;
                }

                if (hasPivot && s.rect.w > 0 && s.rect.h > 0)
                {
                    if (px <= 1.0f && py <= 1.0f && s.rect.w > 2 && s.rect.h > 2)
                    {
                        s.pivot.x = px;
                        s.pivot.y = 1.0f - py;
                    }
                    else
                    {
                        s.pivot.x = px / (float)s.rect.w;
                        s.pivot.y = (s.rect.h - py) / (float)s.rect.h;
                    }
                }
                else
                {
                    s.pivot.x = 0.5f;
                    s.pivot.y = 0.0f; // Default feet
                }
            }
            else
            {
                s.pivot.x = 0.5f;
                s.pivot.y = 0.0f;
            }

            if (bDict.ContainsKey("hasPolygonMesh"))
            {
                s.hasTightMesh = Convert.ToBoolean(bDict["hasPolygonMesh"]);
            }
            else if (bDict.ContainsKey("has_polygon_mesh"))
            {
                s.hasTightMesh = Convert.ToBoolean(bDict["has_polygon_mesh"]);
            }

            if (bDict.ContainsKey("vertices") && bDict["vertices"] is List<object> vList && vList.Count > 0)
            {
                if (vList[0] is List<object>)
                {
                    foreach (var v in vList)
                    {
                        if (v is List<object> xy && xy.Count >= 2)
                        {
                            s.vertices.Add(new float[] { Convert.ToSingle(xy[0]), Convert.ToSingle(xy[1]) });
                        }
                    }
                }
                else
                {
                    for (int vi = 0; vi + 1 < vList.Count; vi += 2)
                    {
                        s.vertices.Add(new float[] { Convert.ToSingle(vList[vi]), Convert.ToSingle(vList[vi + 1]) });
                    }
                }
                if (s.vertices.Count > 0) s.hasTightMesh = true;
            }

            if (bDict.ContainsKey("triangles") && bDict["triangles"] is List<object> tList && tList.Count > 0)
            {
                if (tList[0] is List<object>)
                {
                    foreach (var t in tList)
                    {
                        if (t is List<object> tri)
                        {
                            foreach (var idx in tri)
                                s.triangles.Add(Convert.ToInt32(idx));
                        }
                    }
                }
                else
                {
                    foreach (var t in tList)
                    {
                        s.triangles.Add(Convert.ToInt32(t));
                    }
                }
            }

            if (bDict.ContainsKey("polygon") && bDict["polygon"] is List<object> pList && pList.Count > 0)
            {
                if (pList[0] is List<object>)
                {
                    foreach (var pt in pList)
                    {
                        if (pt is List<object> xy && xy.Count >= 2)
                        {
                            s.polygon.Add(new float[] { Convert.ToSingle(xy[0]), Convert.ToSingle(xy[1]) });
                        }
                    }
                }
                else
                {
                    for (int pi = 0; pi + 1 < pList.Count; pi += 2)
                    {
                        s.polygon.Add(new float[] { Convert.ToSingle(pList[pi]), Convert.ToSingle(pList[pi + 1]) });
                    }
                }
            }

            return s;
        }

        private static BentoSpriteData ParseFrameItem(string name, Dictionary<string, object> fDict, int fallbackIndex)
        {
            BentoSpriteData s = new BentoSpriteData();
            if (string.IsNullOrEmpty(name))
            {
                s.name = $"frame_{fallbackIndex:D4}";
            }
            else
            {
                // Clean up temporary export prefixes like "bentopack_temp_9bff120d...unity_0077"
                if (name.Contains(".unity_"))
                {
                    string suffix = name.Substring(name.LastIndexOf(".unity_") + 7);
                    s.name = $"frame_{suffix}";
                }
                else if (name.StartsWith("bentopack_temp_"))
                {
                    s.name = $"frame_{fallbackIndex:D4}";
                }
                else
                {
                    s.name = name;
                }
            }

            if (fDict.ContainsKey("frame") && fDict["frame"] is Dictionary<string, object> r)
            {
                s.rect.x = Convert.ToInt32(r["x"]);
                s.rect.y = Convert.ToInt32(r["y"]);
                s.rect.w = Convert.ToInt32(r["w"]);
                s.rect.h = Convert.ToInt32(r["h"]);
            }

            if (fDict.ContainsKey("pivot") && fDict["pivot"] is Dictionary<string, object> piv)
            {
                float px = Convert.ToSingle(piv["x"]);
                float py = Convert.ToSingle(piv["y"]);
                s.pivot.x = px;
                // Unity pivot: y=0 is bottom (feet), y=1 is top
                s.pivot.y = (py >= 0f && py <= 1.0f) ? (1.0f - py) : (s.rect.h > 0 ? (s.rect.h - py) / (float)s.rect.h : 0.5f);
            }

            if (fDict.ContainsKey("vertices") && fDict["vertices"] is List<object> vList && vList.Count > 0)
            {
                s.hasTightMesh = true;
                if (vList[0] is List<object>)
                {
                    foreach (var v in vList)
                    {
                        if (v is List<object> xy && xy.Count >= 2)
                            s.vertices.Add(new float[] { Convert.ToSingle(xy[0]), Convert.ToSingle(xy[1]) });
                    }
                }
                else
                {
                    for (int vi = 0; vi + 1 < vList.Count; vi += 2)
                        s.vertices.Add(new float[] { Convert.ToSingle(vList[vi]), Convert.ToSingle(vList[vi + 1]) });
                }
            }

            if (fDict.ContainsKey("triangles") && fDict["triangles"] is List<object> tList && tList.Count > 0)
            {
                if (tList[0] is List<object>)
                {
                    foreach (var t in tList)
                    {
                        if (t is List<object> tri)
                        {
                            foreach (var idx in tri)
                                s.triangles.Add(Convert.ToInt32(idx));
                        }
                    }
                }
                else
                {
                    foreach (var t in tList)
                        s.triangles.Add(Convert.ToInt32(t));
                }
            }

            if (fDict.ContainsKey("polygon") && fDict["polygon"] is List<object> pList && pList.Count > 0)
            {
                if (pList[0] is List<object>)
                {
                    foreach (var pt in pList)
                    {
                        if (pt is List<object> xy && xy.Count >= 2)
                            s.polygon.Add(new float[] { Convert.ToSingle(xy[0]), Convert.ToSingle(xy[1]) });
                    }
                }
                else
                {
                    for (int pi = 0; pi + 1 < pList.Count; pi += 2)
                        s.polygon.Add(new float[] { Convert.ToSingle(pList[pi]), Convert.ToSingle(pList[pi + 1]) });
                }
            }

            return s;
        }

        private static BentoAnimationData ParseAnimationDict(Dictionary<string, object> aDict)
        {
            BentoAnimationData a = new BentoAnimationData();
            if (aDict.ContainsKey("fps")) a.fps = Convert.ToSingle(aDict["fps"]);
            if (aDict.ContainsKey("loop")) a.loop = Convert.ToBoolean(aDict["loop"]);

            if (aDict.ContainsKey("frames") && aDict["frames"] is List<object> fList)
            {
                foreach (var f in fList)
                {
                    a.frames.Add(f?.ToString());
                }
            }

            return a;
        }

        private static byte[] TryDecompressWebpBytes(byte[] webpBytes)
        {
            if (webpBytes == null || webpBytes.Length < 12) return null;
            string tempWebp = Path.Combine(Path.GetTempPath(), "bento_atlas_" + Guid.NewGuid().ToString("N") + ".webp");
            string tempPng = Path.Combine(Path.GetTempPath(), "bento_atlas_" + Guid.NewGuid().ToString("N") + ".png");

            try
            {
                File.WriteAllBytes(tempWebp, webpBytes);

                // Option A: dwebp
                if (RunShellTool("dwebp", $"\"{tempWebp}\" -o \"{tempPng}\"") && File.Exists(tempPng))
                {
                    return File.ReadAllBytes(tempPng);
                }

                // Option B: python3 with PIL
                string pyCmd = $"-c \"import sys, PIL.Image; PIL.Image.open(sys.argv[1]).save(sys.argv[2], 'PNG')\" \"{tempWebp}\" \"{tempPng}\"";
                if (RunShellTool("python3", pyCmd) && File.Exists(tempPng))
                {
                    return File.ReadAllBytes(tempPng);
                }

                // Option C: ImageMagick / ffmpeg
                if (RunShellTool("convert", $"\"{tempWebp}\" \"{tempPng}\"") && File.Exists(tempPng))
                {
                    return File.ReadAllBytes(tempPng);
                }
                if (RunShellTool("ffmpeg", $"-y -i \"{tempWebp}\" \"{tempPng}\"") && File.Exists(tempPng))
                {
                    return File.ReadAllBytes(tempPng);
                }
            }
            catch { }
            finally
            {
                try { if (File.Exists(tempWebp)) File.Delete(tempWebp); } catch { }
                try { if (File.Exists(tempPng)) File.Delete(tempPng); } catch { }
            }

            return null;
        }

        private static bool RunShellTool(string tool, string args)
        {
            try
            {
                var psi = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = tool,
                    Arguments = args,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };
                using (var proc = System.Diagnostics.Process.Start(psi))
                {
                    if (proc != null && proc.WaitForExit(4000))
                    {
                        return proc.ExitCode == 0;
                    }
                }
            }
            catch { }
            return false;
        }
    }

    /// <summary>
    /// Self-contained lightweight JSON parser to avoid third-party JSON library dependencies in Unity.
    /// </summary>
    internal static class MiniJson
    {
        public static object Deserialize(string json)
        {
            if (json == null) return null;
            return Parser.Parse(json);
        }

        private sealed class Parser : IDisposable
        {
            private const string WORD_BREAK = "{}[],:\"";
            private StringReader json;

            private Parser(string jsonString)
            {
                json = new StringReader(jsonString);
            }

            public static object Parse(string jsonString)
            {
                using (var instance = new Parser(jsonString))
                {
                    return instance.ParseValue();
                }
            }

            public void Dispose()
            {
                json.Dispose();
                json = null;
            }

            private Dictionary<string, object> ParseObject()
            {
                Dictionary<string, object> table = new Dictionary<string, object>();
                json.Read(); // '{'

                while (true)
                {
                    switch (NextToken)
                    {
                        case TOKEN.NONE: return null;
                        case TOKEN.CURLY_CLOSE:
                            json.Read();
                            return table;
                        case TOKEN.COMMA:
                            json.Read();
                            break;
                        default:
                            string name = ParseString();
                            if (name == null) return null;
                            if (NextToken != TOKEN.COLON) return null;
                            json.Read();
                            table[name] = ParseValue();
                            break;
                    }
                }
            }

            private List<object> ParseArray()
            {
                List<object> array = new List<object>();
                json.Read(); // '['

                while (true)
                {
                    switch (NextToken)
                    {
                        case TOKEN.NONE: return null;
                        case TOKEN.SQUARED_CLOSE:
                            json.Read();
                            return array;
                        case TOKEN.COMMA:
                            json.Read();
                            break;
                        default:
                            array.Add(ParseValue());
                            break;
                    }
                }
            }

            private object ParseValue()
            {
                switch (NextToken)
                {
                    case TOKEN.STRING: return ParseString();
                    case TOKEN.NUMBER: return ParseNumber();
                    case TOKEN.CURLY_OPEN: return ParseObject();
                    case TOKEN.SQUARED_OPEN: return ParseArray();
                    case TOKEN.TRUE: json.Read(); return true;
                    case TOKEN.FALSE: json.Read(); return false;
                    case TOKEN.NULL: json.Read(); return null;
                    default: return null;
                }
            }

            private string ParseString()
            {
                System.Text.StringBuilder s = new System.Text.StringBuilder();
                json.Read(); // '"'

                while (true)
                {
                    if (json.Peek() == -1) break;
                    char c = (char)json.Read();
                    if (c == '"') return s.ToString();
                    if (c == '\\')
                    {
                        if (json.Peek() == -1) break;
                        c = (char)json.Read();
                        if (c == '"') s.Append('"');
                        else if (c == '\\') s.Append('\\');
                        else if (c == '/') s.Append('/');
                        else if (c == 'b') s.Append('\b');
                        else if (c == 'f') s.Append('\f');
                        else if (c == 'n') s.Append('\n');
                        else if (c == 'r') s.Append('\r');
                        else if (c == 't') s.Append('\t');
                    }
                    else
                    {
                        s.Append(c);
                    }
                }
                return s.ToString();
            }

            private object ParseNumber()
            {
                string word = NextWord;
                if (word.IndexOf('.') == -1)
                {
                    if (long.TryParse(word, out long l)) return l;
                }
                if (double.TryParse(word, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out double d))
                {
                    return d;
                }
                return 0;
            }

            private void EatWhitespace()
            {
                while (char.IsWhiteSpace((char)json.Peek()))
                {
                    json.Read();
                    if (json.Peek() == -1) break;
                }
            }

            private string NextWord
            {
                get
                {
                    System.Text.StringBuilder word = new System.Text.StringBuilder();
                    while (!IsWordBreak((char)json.Peek()))
                    {
                        word.Append((char)json.Read());
                        if (json.Peek() == -1) break;
                    }
                    return word.ToString();
                }
            }

            private static bool IsWordBreak(char c)
            {
                return char.IsWhiteSpace(c) || WORD_BREAK.IndexOf(c) != -1;
            }

            private enum TOKEN
            {
                NONE,
                CURLY_OPEN,
                CURLY_CLOSE,
                SQUARED_OPEN,
                SQUARED_CLOSE,
                COLON,
                COMMA,
                STRING,
                NUMBER,
                TRUE,
                FALSE,
                NULL
            }

            private TOKEN NextToken
            {
                get
                {
                    EatWhitespace();
                    if (json.Peek() == -1) return TOKEN.NONE;
                    char c = (char)json.Peek();
                    switch (c)
                    {
                        case '{': return TOKEN.CURLY_OPEN;
                        case '}': return TOKEN.CURLY_CLOSE;
                        case '[': return TOKEN.SQUARED_OPEN;
                        case ']': return TOKEN.SQUARED_CLOSE;
                        case ',': return TOKEN.COMMA;
                        case '"': return TOKEN.STRING;
                        case ':': return TOKEN.COLON;
                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                        case '-': return TOKEN.NUMBER;
                    }

                    string word = NextWord;
                    switch (word)
                    {
                        case "false": return TOKEN.FALSE;
                        case "true": return TOKEN.TRUE;
                        case "null": return TOKEN.NULL;
                    }
                    return TOKEN.NONE;
                }
            }
        }
    }
}
