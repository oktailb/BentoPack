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


## Locates the bentopack-cli binary path.
static func find_cli_path() -> String:
	# 1. ProjectSettings override
	for setting in [DEFAULT_SETTING_CLI_PATH, LEGACY_SETTING_CLI_PATH]:
		if ProjectSettings.has_setting(setting):
			var custom_path: String = ProjectSettings.get_setting(setting)
			if not custom_path.is_empty() and FileAccess.file_exists(custom_path):
				return custom_path

	# 2. Environment variable override
	var env_cli := OS.get_environment("BENTOPACK_CLI")
	if not env_cli.is_empty() and FileAccess.file_exists(env_cli):
		return env_cli

	var is_win := OS.get_name() == "Windows"
	var exe_name := "bentopack-cli.exe" if is_win else "bentopack-cli"

	# 3. Official System and User PATH
	var path_env := OS.get_environment("PATH")
	if not path_env.is_empty():
		var sep := ";" if is_win else ":"
		for dir in path_env.split(sep):
			var cleaned := dir.strip_edges()
			if cleaned.is_empty():
				continue
			var p := cleaned.path_join(exe_name)
			if FileAccess.file_exists(p):
				return p

	# 4. Standard OS installation directories
	var standard_paths: Array[String] = []
	if is_win:
		var pf := OS.get_environment("ProgramFiles")
		var pfx86 := OS.get_environment("ProgramFiles(x86)")
		var local_app := OS.get_environment("LOCALAPPDATA")
		if not pf.is_empty():
			standard_paths.append(pf.path_join("BentoPack Studio/bin/bentopack-cli.exe"))
			standard_paths.append(pf.path_join("BentoPack Studio/bentopack-cli.exe"))
			standard_paths.append(pf.path_join("BentoPack/bin/bentopack-cli.exe"))
			standard_paths.append(pf.path_join("BentoPack/bentopack-cli.exe"))
		if not pfx86.is_empty():
			standard_paths.append(pfx86.path_join("BentoPack Studio/bin/bentopack-cli.exe"))
			standard_paths.append(pfx86.path_join("BentoPack/bin/bentopack-cli.exe"))
		if not local_app.is_empty():
			standard_paths.append(local_app.path_join("Programs/BentoPack Studio/bin/bentopack-cli.exe"))
			standard_paths.append(local_app.path_join("Programs/BentoPack/bin/bentopack-cli.exe"))
	elif OS.get_name() == "macOS":
		standard_paths.append("/Applications/BentoPack Studio.app/Contents/MacOS/bentopack-cli")
		standard_paths.append("/Applications/BentoPack.app/Contents/MacOS/bentopack-cli")
		standard_paths.append("/usr/local/bin/bentopack-cli")
		standard_paths.append("/opt/homebrew/bin/bentopack-cli")
	else: # Linux / BSD
		var home := OS.get_environment("HOME")
		standard_paths.append("/usr/bin/bentopack-cli")
		standard_paths.append("/usr/local/bin/bentopack-cli")
		if not home.is_empty():
			standard_paths.append(home.path_join(".local/bin/bentopack-cli"))
		standard_paths.append("/opt/bentopack/bin/bentopack-cli")

	for sp in standard_paths:
		if FileAccess.file_exists(sp):
			return sp

	return ""

## Locates the BentoPack graphical editor executable.
static func find_gui_path() -> String:
	# 1. ProjectSettings override
	for setting in [DEFAULT_SETTING_GUI_PATH, LEGACY_SETTING_GUI_PATH]:
		if ProjectSettings.has_setting(setting):
			var custom_path: String = ProjectSettings.get_setting(setting)
			if not custom_path.is_empty() and FileAccess.file_exists(custom_path):
				return custom_path

	# 2. Environment variable override
	var env_gui := OS.get_environment("BENTOPACK_GUI")
	if not env_gui.is_empty() and FileAccess.file_exists(env_gui):
		return env_gui

	var is_win := OS.get_name() == "Windows"
	var exe_name := "bentopack.exe" if is_win else "bentopack"

	# 3. Official System and User PATH
	var path_env := OS.get_environment("PATH")
	if not path_env.is_empty():
		var sep := ";" if is_win else ":"
		for dir in path_env.split(sep):
			var cleaned := dir.strip_edges()
			if cleaned.is_empty():
				continue
			var p := cleaned.path_join(exe_name)
			if FileAccess.file_exists(p):
				return p

	# 4. Standard OS installation directories
	var standard_paths: Array[String] = []
	if is_win:
		var pf := OS.get_environment("ProgramFiles")
		var pfx86 := OS.get_environment("ProgramFiles(x86)")
		var local_app := OS.get_environment("LOCALAPPDATA")
		if not pf.is_empty():
			standard_paths.append(pf.path_join("BentoPack Studio/bin/bentopack.exe"))
			standard_paths.append(pf.path_join("BentoPack Studio/bentopack.exe"))
			standard_paths.append(pf.path_join("BentoPack/bin/bentopack.exe"))
			standard_paths.append(pf.path_join("BentoPack/bentopack.exe"))
		if not pfx86.is_empty():
			standard_paths.append(pfx86.path_join("BentoPack Studio/bin/bentopack.exe"))
			standard_paths.append(pfx86.path_join("BentoPack/bin/bentopack.exe"))
		if not local_app.is_empty():
			standard_paths.append(local_app.path_join("Programs/BentoPack Studio/bin/bentopack.exe"))
			standard_paths.append(local_app.path_join("Programs/BentoPack/bin/bentopack.exe"))
	elif OS.get_name() == "macOS":
		standard_paths.append("/Applications/BentoPack Studio.app/Contents/MacOS/bentopack")
		standard_paths.append("/Applications/BentoPack.app/Contents/MacOS/bentopack")
		standard_paths.append("/usr/local/bin/bentopack")
		standard_paths.append("/opt/homebrew/bin/bentopack")
	else: # Linux / BSD
		var home := OS.get_environment("HOME")
		standard_paths.append("/usr/bin/bentopack")
		standard_paths.append("/usr/local/bin/bentopack")
		if not home.is_empty():
			standard_paths.append(home.path_join(".local/bin/bentopack"))
		standard_paths.append("/opt/bentopack/bin/bentopack")

	for sp in standard_paths:
		if FileAccess.file_exists(sp):
			return sp

	return ""

## Opens a project or image in the BentoPack desktop editor.
static func open_in_editor(file_path: String = "") -> Error:
	var gui_path := find_gui_path()
	if gui_path.is_empty():
		push_warning("BentoPack: Could not locate BentoPack GUI binary.")
		return ERR_FILE_NOT_FOUND

	var args: Array[String] = []
	if not file_path.is_empty() and file_path != "res://" and file_path != "res:/":
		var global_target := ProjectSettings.globalize_path(file_path)
		if FileAccess.file_exists(global_target):
			args.append(global_target)
		elif DirAccess.dir_exists_absolute(global_target):
			var bento_file := _find_bento_in_dir(global_target)
			if not bento_file.is_empty():
				args.append(bento_file)
			else:
				print("[BentoPack] Launching Desktop Editor without file arguments (folder given: %s)." % file_path)
		elif not global_target.get_extension().is_empty():
			args.append(global_target)

	var pid := OS.create_process(gui_path, args)
	if pid < 0:
		push_error("BentoPack: Failed to launch process: " + gui_path)
		return ERR_CANT_CREATE

	return OK

static func _find_bento_in_dir(dir_path: String) -> String:
	var dir := DirAccess.open(dir_path)
	if dir != null:
		dir.list_dir_begin()
		var file_name := dir.get_next()
		var bento_files: Array[String] = []
		while not file_name.is_empty():
			if not dir.current_is_dir() and file_name.to_lower().ends_with(".bento"):
				bento_files.append(dir_path.path_join(file_name))
			file_name = dir.get_next()
		dir.list_dir_end()
		if not bento_files.is_empty():
			return bento_files[0]
	return ""

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
