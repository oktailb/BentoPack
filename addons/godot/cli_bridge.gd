@tool
class_name SpriteStudioCliBridge
extends RefCounted

## Interfacing utility between Godot 4 and the SpriteStudio Desktop & CLI tools.

const DEFAULT_SETTING_CLI_PATH := "spritestudio/general/cli_path"
const DEFAULT_SETTING_GUI_PATH := "spritestudio/general/gui_path"
const RELEASES_URL := "https://github.com/oktailb/SpriteStudio/releases"

## Returns official release download page for precompiled CLI & Desktop editor binaries
static func get_releases_url() -> String:
	return RELEASES_URL


## Locates the spritestudio-cli binary path.
static func find_cli_path() -> String:
	# 1. ProjectSettings override
	if ProjectSettings.has_setting(DEFAULT_SETTING_CLI_PATH):
		var custom_path: String = ProjectSettings.get_setting(DEFAULT_SETTING_CLI_PATH)
		if not custom_path.is_empty() and FileAccess.file_exists(custom_path):
			return custom_path

	# 2. Environment variable
	var env_cli := OS.get_environment("SPRITESTUDIO_CLI")
	if not env_cli.is_empty() and FileAccess.file_exists(env_cli):
		return env_cli

	# 3. Known relative build paths (development environments)
	var possible_relative_paths := [
		"../../build/bin/spritestudio-cli",
		"../build/bin/spritestudio-cli",
		"build/bin/spritestudio-cli",
		"bin/spritestudio-cli",
	]
	for rel in possible_relative_paths:
		var global_path := ProjectSettings.globalize_path("res://" + rel)
		if FileAccess.file_exists(global_path):
			return global_path

	# 4. Standard Linux / Unix locations
	var standard_paths := [
		"/usr/local/bin/spritestudio-cli",
		"/usr/bin/spritestudio-cli",
		"/opt/spritestudio/bin/spritestudio-cli",
	]
	for sp in standard_paths:
		if FileAccess.file_exists(sp):
			return sp

	# 5. Fallback: which spritestudio-cli
	var output := []
	var exit_code := OS.execute("which", ["spritestudio-cli"], output)
	if exit_code == 0 and not output.is_empty():
		var found_path: String = output[0].strip_edges()
		if FileAccess.file_exists(found_path):
			return found_path

	return ""

## Locates the SpriteStudio graphical editor executable.
static func find_gui_path() -> String:
	if ProjectSettings.has_setting(DEFAULT_SETTING_GUI_PATH):
		var custom_path: String = ProjectSettings.get_setting(DEFAULT_SETTING_GUI_PATH)
		if not custom_path.is_empty() and FileAccess.file_exists(custom_path):
			return custom_path

	var env_gui := OS.get_environment("SPRITESTUDIO_GUI")
	if not env_gui.is_empty() and FileAccess.file_exists(env_gui):
		return env_gui

	var possible_relative_paths := [
		"../../build/bin/SpriteStudio",
		"../build/bin/SpriteStudio",
		"build/bin/SpriteStudio",
		"bin/SpriteStudio",
	]
	for rel in possible_relative_paths:
		var global_path := ProjectSettings.globalize_path("res://" + rel)
		if FileAccess.file_exists(global_path):
			return global_path

	var standard_paths := [
		"/usr/local/bin/SpriteStudio",
		"/usr/bin/SpriteStudio",
		"/opt/spritestudio/bin/SpriteStudio",
	]
	for sp in standard_paths:
		if FileAccess.file_exists(sp):
			return sp

	var output := []
	var exit_code := OS.execute("which", ["SpriteStudio"], output)
	if exit_code == 0 and not output.is_empty():
		var found_path: String = output[0].strip_edges()
		if FileAccess.file_exists(found_path):
			return found_path

	return ""

## Opens a project or image in the SpriteStudio desktop editor.
static func open_in_editor(file_path: String) -> Error:
	var gui_path := find_gui_path()
	if gui_path.is_empty():
		push_warning("SpriteStudio: Could not locate SpriteStudio GUI binary.")
		return ERR_FILE_NOT_FOUND

	var global_target := ProjectSettings.globalize_path(file_path)
	var pid := OS.create_process(gui_path, [global_target])
	if pid < 0:
		push_error("SpriteStudio: Failed to launch process: " + gui_path)
		return ERR_CANT_CREATE

	return OK

## Runs a headless CLI command and returns exit code and stdout/stderr output.
static func run_cli(args: Array[String]) -> Dictionary:
	var cli_path := find_cli_path()
	if cli_path.is_empty():
		return {
			"success": false,
			"exit_code": -1,
			"output": "spritestudio-cli binary not found."
		}

	var output := []
	var exit_code := OS.execute(cli_path, args, output, true)
	var output_str: String = output[0] if not output.is_empty() else ""

	return {
		"success": (exit_code == 0),
		"exit_code": exit_code,
		"output": output_str
	}
