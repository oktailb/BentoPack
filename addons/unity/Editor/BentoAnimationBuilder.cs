using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace BentoPack.Editor
{
    /// <summary>
    /// Builds standard Unity AnimationClip assets from BentoPack animation timelines,
    /// binding SpriteRenderer.m_Sprite keyframes at exact FPS rates with loop settings.
    /// </summary>
    public static class BentoAnimationBuilder
    {
        public static AnimationClip CreateClip(string animName,
                                                BentoAnimationData animData,
                                                Dictionary<string, Sprite> spriteMap)
        {
            if (animData == null || animData.frames == null || animData.frames.Count == 0)
                return null;

            AnimationClip clip = new AnimationClip();
            clip.name = string.IsNullOrEmpty(animName) ? "Default" : animName;
            clip.frameRate = animData.fps > 0f ? animData.fps : 12f;

            EditorCurveBinding binding = new EditorCurveBinding
            {
                type = typeof(SpriteRenderer),
                path = "",
                propertyName = "m_Sprite"
            };

            int frameCount = animData.frames.Count;
            List<ObjectReferenceKeyframe> keyframes = new List<ObjectReferenceKeyframe>(frameCount);

            float frameDuration = 1f / clip.frameRate;

            for (int i = 0; i < frameCount; ++i)
            {
                string spriteName = animData.frames[i];
                Sprite sprite = null;

                if (!spriteMap.TryGetValue(spriteName, out sprite))
                {
                    if (int.TryParse(spriteName, out int idx))
                    {
                        if (!spriteMap.TryGetValue($"frame_{idx:D4}", out sprite) &&
                            !spriteMap.TryGetValue($"sprite_{idx:D4}", out sprite) &&
                            !spriteMap.TryGetValue($"frame_{idx}", out sprite) &&
                            !spriteMap.TryGetValue($"sprite_{idx}", out sprite) &&
                            !spriteMap.TryGetValue(idx.ToString(), out sprite))
                        {
                            foreach (var kvp in spriteMap)
                            {
                                if (kvp.Key.EndsWith($"_{idx:D4}") || kvp.Key.EndsWith($"_{idx}"))
                                {
                                    sprite = kvp.Value;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (sprite == null)
                {
                    continue;
                }

                ObjectReferenceKeyframe keyframe = new ObjectReferenceKeyframe
                {
                    time = i * frameDuration,
                    value = sprite
                };
                keyframes.Add(keyframe);
            }

            if (keyframes.Count == 0)
                return null;

            AnimationUtility.SetObjectReferenceCurve(clip, binding, keyframes.ToArray());

            // Configure loop time settings
            AnimationClipSettings settings = AnimationUtility.GetAnimationClipSettings(clip);
            settings.loopTime = animData.loop;
            AnimationUtility.SetAnimationClipSettings(clip, settings);

            return clip;
        }

        public static List<AnimationClip> CreateAllClips(Dictionary<string, BentoAnimationData> animations,
                                                          Dictionary<string, Sprite> spriteMap)
        {
            List<AnimationClip> clips = new List<AnimationClip>();
            if (animations == null || spriteMap == null)
                return clips;

            foreach (var kvp in animations)
            {
                AnimationClip clip = CreateClip(kvp.Key, kvp.Value, spriteMap);
                if (clip != null)
                {
                    clips.Add(clip);
                }
            }
            return clips;
        }
    }
}
