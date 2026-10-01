using System;
using System.Diagnostics;
using System.IO;
using UnityEngine;
using UnityEditor;
#if UNITY_EDITOR_WIN
using Microsoft.Win32;
#endif
using Debug = UnityEngine.Debug;

namespace BentoPack.Editor
{
    /// <summary>
    /// Connects the Unity Editor to the external bentopack-cli and desktop BentoPack Studio app.
    /// Searches official system PATH, Windows Registry installation keys, and standard OS install directories.
    /// </summary>
    public static class BentoCliBridge
    {
        public const string PREF_CLI_PATH = "BentoPack_Cli_Path";
        public const string PREF_GUI_PATH = "BentoPack_Gui_Path";

        public static void SetCustomCliPath(string path)
        {
            if (string.IsNullOrEmpty(path))
                EditorPrefs.DeleteKey(PREF_CLI_PATH);
            else
                EditorPrefs.SetString(PREF_CLI_PATH, path);
        }

        public static void SetCustomGuiPath(string path)
        {
            if (string.IsNullOrEmpty(path))
                EditorPrefs.DeleteKey(PREF_GUI_PATH);
            else
                EditorPrefs.SetString(PREF_GUI_PATH, path);
        }

        private static bool CheckExecutableExists(string basePath, bool isWindows, out string foundPath)
        {
            foundPath = null;
            if (string.IsNullOrEmpty(basePath)) return false;

            try
            {
                string full = Path.GetFullPath(basePath);
                if (File.Exists(full))
                {
                    foundPath = full;
                    return true;
                }

                if (isWindows && !full.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
                {
                    string fullExe = full + ".exe";
                    if (File.Exists(fullExe))
                    {
                        foundPath = fullExe;
                        return true;
                    }
                }
            }
            catch { }

            return false;
        }

        private static string GetWindowsRegistryInstallPath()
        {
#if UNITY_EDITOR_WIN
            try
            {
                string[] registryKeys = {
                    @"Software\BentoPack Studio",
                    @"Software\Microsoft\Windows\CurrentVersion\Uninstall\BentoPack Studio",
                    @"Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\BentoPack Studio"
                };

                foreach (string key in registryKeys)
                {
                    using (RegistryKey rk = Registry.LocalMachine.OpenSubKey(key))
                    {
                        if (rk != null)
                        {
                            object val = rk.GetValue("Path") ?? rk.GetValue("InstallLocation");
                            if (val != null)
                            {
                                string p = val.ToString().Trim();
                                if (!string.IsNullOrEmpty(p)) return p;
                            }
                        }
                    }

                    using (RegistryKey rk = Registry.CurrentUser.OpenSubKey(key))
                    {
                        if (rk != null)
                        {
                            object val = rk.GetValue("Path") ?? rk.GetValue("InstallLocation");
                            if (val != null)
                            {
                                string p = val.ToString().Trim();
                                if (!string.IsNullOrEmpty(p)) return p;
                            }
                        }
                    }
                }
            }
            catch { }
#endif
            return null;
        }

        private static string GetWindowsRegistryPathEnv()
        {
#if UNITY_EDITOR_WIN
            try
            {
                string sysPath = null;
                string userPath = null;

                using (RegistryKey sysEnv = Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Control\Session Manager\Environment"))
                {
                    if (sysEnv != null) sysPath = sysEnv.GetValue("Path")?.ToString();
                }

                using (RegistryKey userEnv = Registry.CurrentUser.OpenSubKey(@"Environment"))
                {
                    if (userEnv != null) userPath = userEnv.GetValue("Path")?.ToString();
                }

                return (sysPath ?? "") + ";" + (userPath ?? "");
            }
            catch { }
#endif
            return null;
        }

        public static string FindCliPath()
        {
            bool isWin = Application.platform == RuntimePlatform.WindowsEditor;
            string exeName = isWin ? "bentopack-cli.exe" : "bentopack-cli";

            // 1. User EditorPrefs preference
            string prefCli = EditorPrefs.GetString(PREF_CLI_PATH, null);
            if (CheckExecutableExists(prefCli, isWin, out string resolvedPref))
            {
                return resolvedPref;
            }

            // 2. Environment variable override
            string envCli = Environment.GetEnvironmentVariable("BENTOPACK_CLI");
            if (CheckExecutableExists(envCli, isWin, out string resolvedEnv))
            {
                return resolvedEnv;
            }

            // 3. Official System and User PATH
            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (isWin)
            {
                string regPath = GetWindowsRegistryPathEnv();
                if (!string.IsNullOrEmpty(regPath))
                {
                    pathEnv = (pathEnv ?? "") + ";" + regPath;
                }
            }

            if (!string.IsNullOrEmpty(pathEnv))
            {
                char sep = Path.PathSeparator;
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

            // 4. Official OS Installation Directories
            if (isWin)
            {
                string regInstall = GetWindowsRegistryInstallPath();
                if (!string.IsNullOrEmpty(regInstall))
                {
                    string candBin = Path.Combine(regInstall, "bin", exeName);
                    if (File.Exists(candBin)) return candBin;

                    string candRoot = Path.Combine(regInstall, exeName);
                    if (File.Exists(candRoot)) return candRoot;
                }

                string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
                string programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
                string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

                string[] winDirs = {
                    Path.Combine(programFiles, "BentoPack Studio", "bin"),
                    Path.Combine(programFiles, "BentoPack Studio"),
                    Path.Combine(programFiles, "BentoPack", "bin"),
                    Path.Combine(programFiles, "BentoPack"),
                    Path.Combine(programFilesX86, "BentoPack Studio", "bin"),
                    Path.Combine(programFilesX86, "BentoPack", "bin"),
                    Path.Combine(localAppData, "Programs", "BentoPack Studio", "bin"),
                    Path.Combine(localAppData, "Programs", "BentoPack", "bin")
                };

                foreach (string dir in winDirs)
                {
                    string p = Path.Combine(dir, exeName);
                    if (File.Exists(p)) return p;
                }
            }
            else if (Application.platform == RuntimePlatform.OSXEditor)
            {
                string[] macPaths = {
                    "/Applications/BentoPack Studio.app/Contents/MacOS/bentopack-cli",
                    "/Applications/BentoPack.app/Contents/MacOS/bentopack-cli",
                    "/usr/local/bin/bentopack-cli",
                    "/opt/homebrew/bin/bentopack-cli"
                };
                foreach (string p in macPaths)
                {
                    if (File.Exists(p)) return p;
                }
            }
            else // Linux
            {
                string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                string[] linuxPaths = {
                    "/usr/bin/bentopack-cli",
                    "/usr/local/bin/bentopack-cli",
                    Path.Combine(home, ".local/bin/bentopack-cli"),
                    "/opt/bentopack/bin/bentopack-cli"
                };
                foreach (string p in linuxPaths)
                {
                    if (File.Exists(p)) return p;
                }
            }

            return null;
        }

        public static string FindGuiPath()
        {
            bool isWin = Application.platform == RuntimePlatform.WindowsEditor;
            string exeName = isWin ? "bentopack.exe" : "bentopack";

            // 1. User EditorPrefs preference
            string prefGui = EditorPrefs.GetString(PREF_GUI_PATH, null);
            if (CheckExecutableExists(prefGui, isWin, out string resolvedPref))
            {
                return resolvedPref;
            }

            // 2. Environment variable override
            string envGui = Environment.GetEnvironmentVariable("BENTOPACK_GUI");
            if (CheckExecutableExists(envGui, isWin, out string resolvedEnv))
            {
                return resolvedEnv;
            }

            // 3. Official System and User PATH
            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (isWin)
            {
                string regPath = GetWindowsRegistryPathEnv();
                if (!string.IsNullOrEmpty(regPath))
                {
                    pathEnv = (pathEnv ?? "") + ";" + regPath;
                }
            }

            if (!string.IsNullOrEmpty(pathEnv))
            {
                char sep = Path.PathSeparator;
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

            // 4. Official OS Installation Directories
            if (isWin)
            {
                string regInstall = GetWindowsRegistryInstallPath();
                if (!string.IsNullOrEmpty(regInstall))
                {
                    string candBin = Path.Combine(regInstall, "bin", exeName);
                    if (File.Exists(candBin)) return candBin;

                    string candRoot = Path.Combine(regInstall, exeName);
                    if (File.Exists(candRoot)) return candRoot;
                }

                string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
                string programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
                string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

                string[] winDirs = {
                    Path.Combine(programFiles, "BentoPack Studio", "bin"),
                    Path.Combine(programFiles, "BentoPack Studio"),
                    Path.Combine(programFiles, "BentoPack", "bin"),
                    Path.Combine(programFiles, "BentoPack"),
                    Path.Combine(programFilesX86, "BentoPack Studio", "bin"),
                    Path.Combine(programFilesX86, "BentoPack", "bin"),
                    Path.Combine(localAppData, "Programs", "BentoPack Studio", "bin"),
                    Path.Combine(localAppData, "Programs", "BentoPack", "bin")
                };

                foreach (string dir in winDirs)
                {
                    string p = Path.Combine(dir, exeName);
                    if (File.Exists(p)) return p;
                }
            }
            else if (Application.platform == RuntimePlatform.OSXEditor)
            {
                string[] macPaths = {
                    "/Applications/BentoPack Studio.app/Contents/MacOS/bentopack",
                    "/Applications/BentoPack.app/Contents/MacOS/bentopack",
                    "/usr/local/bin/bentopack",
                    "/opt/homebrew/bin/bentopack"
                };
                foreach (string p in macPaths)
                {
                    if (File.Exists(p)) return p;
                }
            }
            else // Linux
            {
                string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                string[] linuxPaths = {
                    "/usr/bin/bentopack",
                    "/usr/local/bin/bentopack",
                    Path.Combine(home, ".local/bin/bentopack"),
                    "/opt/bentopack/bin/bentopack"
                };
                foreach (string p in linuxPaths)
                {
                    if (File.Exists(p)) return p;
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
                error = "bentopack-cli executable not found on PATH or in standard installation directories.";
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

        public const string RELEASES_URL = "https://github.com/oktailb/BentoPack/releases";

        private static Process s_watchProcess = null;

        public static bool IsWatchActive => s_watchProcess != null && !s_watchProcess.HasExited;
        public static int WatchPid => IsWatchActive ? s_watchProcess.Id : -1;

        public static bool StartWatchDaemon(string arguments, out int pid, out string error)
        {
            pid = -1;
            error = "";

            StopWatchDaemon();

            string cli = FindCliPath();
            if (string.IsNullOrEmpty(cli))
            {
                error = "bentopack-cli executable not found on PATH or in standard installation directories.";
                return false;
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = cli,
                    Arguments = arguments,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                s_watchProcess = Process.Start(psi);
                if (s_watchProcess != null && !s_watchProcess.HasExited)
                {
                    pid = s_watchProcess.Id;
                    return true;
                }
                else
                {
                    error = "Failed to launch background daemon process.";
                    return false;
                }
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }

        public static void StopWatchDaemon()
        {
            if (s_watchProcess != null)
            {
                try
                {
                    if (!s_watchProcess.HasExited)
                    {
                        s_watchProcess.Kill();
                    }
                }
                catch { }
                finally
                {
                    s_watchProcess.Dispose();
                    s_watchProcess = null;
                }
            }
        }
    }
}
