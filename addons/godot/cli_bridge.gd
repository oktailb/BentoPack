@tool
class_name BentoPackCliBridge
extends RefCounted

## Interfacing utility between Godot 4 and the BentoPack Desktop & CLI tools.

const DEFAULT_SETTING_CLI_PATH := "bentopack/general/cli_path"
const LEGACY_SETTING_CLI_PATH := "bentopack/general/cli_path"
const DEFAULT_SETTING_GUI_PATH := "bentopack/general/gui_path"
const LEGACY_SETTING_GUI_PATH := "bentopack/general/gui_path"
const RELEASES_URL := "https://github.com/oktailb/BentoPack/releases"

## Returns official release download page for precompiled CLI & Desktop editor binaries
static func get_releases_url() -> String:
	return RELEASES_URL


## Locates the bentopack-cli (or legacy bentopack-cli) binary path.
static func find_cli_path() -> String:
	# 1. ProjectSettings override
	for setting in [DEFAULT_SETTING_CLI_PATH, LEGACY_SETTING_CLI_PATH]:
		if ProjectSettings.has_setting(setting):
			var custom_path: String = ProjectSettings.get_setting(setting)
			if not custom_path.is_empty() and FileAccess.file_exists(custom_path):
				return custom_path

	# 2. Environment variable
	for env_var in ["BENTOPACK_CLI", "SPRITESTUDIO_CLI"]:
		var env_cli := OS.get_environment(env_var)
		if not env_cli.is_empty() and FileAccess.file_exists(env_cli):
			return env_cli

	# 3. Known relative build paths (development environments)
	var possible_relative_paths := [
		"../../build/bin/bentopack-cli",
		"../../build/bin/bentopack-cli",
		"../build/bin/bentopack-cli",
		"../build/bin/bentopack-cli",
		"build/bin/bentopack-cli",
		"build/bin/bentopack-cli",
		"bin/bentopack-cli",
		"bin/bentopack-cli",
	]
	for rel in possible_relative_paths:
		var global_path := ProjectSettings.globalize_path("res://" + rel)
		if FileAccess.file_exists(global_path):
			return global_path

	# 4. Standard Linux / Unix locations
	var standard_paths := [
		"/usr/local/bin/bentopack-cli",
		"/usr/bin/bentopack-cli",
		"/opt/bentopack/bin/bentopack-cli",
		"/usr/local/bin/bentopack-cli",
		"/usr/bin/bentopack-cli",
		"/opt/bentopack/bin/bentopack-cli",
	]
	for sp in standard_paths:
		if FileAccess.file_exists(sp):
			return sp

	# 5. Fallback: which bentopack-cli / bentopack-cli
	for bin_name in ["bentopack-cli", "bentopack-cli"]:
		var output := []
		var exit_code := OS.execute("which", [bin_name], output)
		if exit_code == 0 and not output.is_empty():
			var found_path: String = output[0].strip_edges()
			if FileAccess.file_exists(found_path):
				return found_path

	return ""

## Locates the BentoPack (or legacy BentoPack) graphical editor executable.
static func find_gui_path() -> String:
	for setting in [DEFAULT_SETTING_GUI_PATH, LEGACY_SETTING_GUI_PATH]:
		if ProjectSettings.has_setting(setting):
			var custom_path: String = ProjectSettings.get_setting(setting)
			if not custom_path.is_empty() and FileAccess.file_exists(custom_path):
				return custom_path

	for env_var in ["BENTOPACK_GUI", "SPRITESTUDIO_GUI"]:
		var env_gui := OS.get_environment(env_var)
		if not env_gui.is_empty() and FileAccess.file_exists(env_gui):
			return env_gui

	var possible_relative_paths := [
		"../../build/bin/bentopack",
		"../../build/bin/BentoPack",
		"../build/bin/bentopack",
		"../build/bin/BentoPack",
		"build/bin/bentopack",
		"build/bin/BentoPack",
		"bin/bentopack",
		"bin/BentoPack",
	]
	for rel in possible_relative_paths:
		var global_path := ProjectSettings.globalize_path("res://" + rel)
		if FileAccess.file_exists(global_path):
			return global_path

	var standard_paths := [
		"/usr/local/bin/bentopack",
		"/usr/bin/bentopack",
		"/opt/bentopack/bin/bentopack",
		"/usr/local/bin/BentoPack",
		"/usr/bin/BentoPack",
		"/opt/bentopack/bin/BentoPack",
	]
	for sp in standard_paths:
		if FileAccess.file_exists(sp):
			return sp

	for bin_name in ["bentopack", "BentoPack"]:
		var output := []
		var exit_code := OS.execute("which", [bin_name], output)
		if exit_code == 0 and not output.is_empty():
			var found_path: String = output[0].strip_edges()
			if FileAccess.file_exists(found_path):
				return found_path

	return ""

## Opens a project or image in the BentoPack desktop editor.
static func open_in_editor(file_path: String) -> Error:
	var gui_path := find_gui_path()
	if gui_path.is_empty():
		push_warning("BentoPack: Could not locate BentoPack GUI binary.")
		return ERR_FILE_NOT_FOUND

	var global_target := ProjectSettings.globalize_path(file_path)
	var pid := OS.create_process(gui_path, [global_target])
	if pid < 0:
		push_error("BentoPack: Failed to launch process: " + gui_path)
		return ERR_CANT_CREATE

	return OK

## Runs a headless CLI command and returns exit code and stdout/stderr output.
static func run_cli(args: Array[String]) -> Dictionary:
	var cli_path := find_cli_path()
	if cli_path.is_empty():
		return {
			"success": false,
			"exit_code": -1,
			"output": "bentopack-cli binary not found."
		}

	var output := []
	var exit_code := OS.execute(cli_path, args, output, true)
	var output_str: String = output[0] if not output.is_empty() else ""

	return {
		"success": (exit_code == 0),
		"exit_code": exit_code,
		"output": output_str
	}
