using System;
using System.Diagnostics;
using System.IO;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace BentoPack.Editor
{
    /// <summary>
    /// Connects the Unity Editor to the external bentopack-cli and desktop BentoPack Studio app.
    /// </summary>
    public static class BentoCliBridge
    {
        public static string FindCliPath()
        {
            // 1. Environment variable override
            string envCli = Environment.GetEnvironmentVariable("BENTOPACK_CLI");
            if (!string.IsNullOrEmpty(envCli) && File.Exists(envCli))
            {
                return envCli;
            }

            // 2. Relative repository build paths (common dev layout)
            string[] relativeCandidates = {
                "/home/oktail/Documents/GitHub/BentoPack/build/Desktop-Debug/bin/bentopack-cli",
                "/home/oktail/Documents/GitHub/BentoPack/build/Desktop-Release/bin/bentopack-cli",
                "/home/oktail/Documents/GitHub/BentoPack/bin/bentopack-cli",
                Path.Combine(Application.dataPath, "../../build/Desktop-Debug/bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../../build/Desktop-Release/bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../../build/bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../../bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../build/Desktop-Debug/bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../build/Desktop-Release/bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../build/bin/bentopack-cli"),
                Path.Combine(Application.dataPath, "../bin/bentopack-cli")
            };

            foreach (string cand in relativeCandidates)
            {
                string full = Path.GetFullPath(cand);
                if (File.Exists(full))
                {
                    return full;
                }
            }

            // 3. Search system PATH
            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (!string.IsNullOrEmpty(pathEnv))
            {
                char sep = Path.PathSeparator;
                string exeName = Application.platform == RuntimePlatform.WindowsEditor ? "bentopack-cli.exe" : "bentopack-cli";

                foreach (string dir in pathEnv.Split(sep))
                {
                    if (string.IsNullOrWhiteSpace(dir)) continue;
                    try
                    {
                        string p = Path.Combine(dir.Trim(), exeName);
                        if (File.Exists(p)) return p;
                    }
                    catch { }
                }
            }

            return null;
        }

        public static string FindGuiPath()
        {
            string envGui = Environment.GetEnvironmentVariable("BENTOPACK_GUI");
            if (!string.IsNullOrEmpty(envGui) && File.Exists(envGui))
            {
                return envGui;
            }

            string[] relativeCandidates = {
                Path.Combine(Application.dataPath, "../../build/Desktop-Debug/bin/bentopack"),
                Path.Combine(Application.dataPath, "../../build/Desktop-Release/bin/bentopack"),
                Path.Combine(Application.dataPath, "../../build/bin/bentopack"),
                Path.Combine(Application.dataPath, "../../bin/bentopack"),
                Path.Combine(Application.dataPath, "../build/Desktop-Debug/bin/bentopack"),
                Path.Combine(Application.dataPath, "../build/Desktop-Release/bin/bentopack"),
                Path.Combine(Application.dataPath, "../build/bin/bentopack"),
                Path.Combine(Application.dataPath, "../bin/bentopack")
            };

            foreach (string cand in relativeCandidates)
            {
                string full = Path.GetFullPath(cand);
                if (File.Exists(full))
                {
                    return full;
                }
            }

            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (!string.IsNullOrEmpty(pathEnv))
            {
                char sep = Path.PathSeparator;
                string exeName = Application.platform == RuntimePlatform.WindowsEditor ? "bentopack.exe" : "bentopack";

                foreach (string dir in pathEnv.Split(sep))
                {
                    if (string.IsNullOrWhiteSpace(dir)) continue;
                    try
                    {
                        string p = Path.Combine(dir.Trim(), exeName);
                        if (File.Exists(p)) return p;
                    }
                    catch { }
                }
            }

            return null;
        }

        public static bool OpenInBentoPack(string targetFilePath)
        {
            string gui = FindGuiPath();
            if (string.IsNullOrEmpty(gui))
            {
                Debug.LogWarning("[BentoPack] bentopack desktop executable not found. Please set BENTOPACK_GUI or ensure it is on PATH.");
                return false;
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = gui,
                    Arguments = $"\"{targetFilePath}\"",
                    UseShellExecute = true
                };
                Process.Start(psi);
                return true;
            }
            catch (Exception ex)
            {
                Debug.LogError("[BentoPack] Failed to start BentoPack Studio: " + ex.Message);
                return false;
            }
        }

        public static bool RunCliCommand(string arguments, out string output, out string error)
        {
            output = "";
            error = "";

            string cli = FindCliPath();
            if (string.IsNullOrEmpty(cli))
            {
                error = "bentopack-cli executable not found on PATH or in build directory.";
                return false;
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = cli,
                    Arguments = arguments,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                using (Process proc = Process.Start(psi))
                {
                    output = proc.StandardOutput.ReadToEnd();
                    error = proc.StandardError.ReadToEnd();
                    proc.WaitForExit(15000);
                    return proc.ExitCode == 0;
                }
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }
    }
}
