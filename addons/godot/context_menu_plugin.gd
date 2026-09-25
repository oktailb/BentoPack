@tool
class_name SpriteStudioContextMenuPlugin
extends EditorContextMenuPlugin

## Editor Context Menu Plugin for SpriteStudio.
## Adds 1-click actions to the Godot FileSystem dock for images (.png, .webp, .jpg) and projects (.ssp).

var _plugin: EditorPlugin

func _init(plugin: EditorPlugin = null) -> void:
	_plugin = plugin

func _popup_menu(paths: PackedStringArray) -> void:
	if paths.is_empty():
		return

	var has_image := false
	var has_ssp := false
	var target_image := ""
	var target_ssp := ""

	for p in paths:
		var ext := p.get_extension().to_lower()
		if ext in ["png", "webp", "jpg", "jpeg"]:
			has_image = true
			if target_image.is_empty():
				target_image = p
		elif ext == "ssp":
			has_ssp = true
			if target_ssp.is_empty():
				target_ssp = p

	if has_image:
		add_context_menu_item("⚡ SpriteStudio: Auto-Slice & Générer Scène", _on_auto_slice.bind(target_image))
		add_context_menu_item("🔧 SpriteStudio: Configurer dans le Dock", _on_open_in_dock.bind(target_image))

	if has_ssp:
		add_context_menu_item("🎨 SpriteStudio: Ouvrir dans l'éditeur Desktop", _on_open_in_desktop.bind(target_ssp))

func _on_auto_slice(arg1: Variant = null, arg2: Variant = null) -> void:
	var path: String = ""
	if arg1 is String:
		path = arg1
	elif arg2 is String:
		path = arg2
	elif arg1 is PackedStringArray and not (arg1 as PackedStringArray).is_empty():
		path = (arg1 as PackedStringArray)[0]

	if path.is_empty():
		return

	var target_ssp := path.get_basename() + ".ssp"
	var global_src := ProjectSettings.globalize_path(path)
	var global_target := ProjectSettings.globalize_path(target_ssp)

	var cli_args: Array[String] = ["slice", "--output-project", global_target, global_src]
	var res := SpriteStudioCliBridge.run_cli(cli_args)

	if res.get("success", false):
		print("[SpriteStudio] Auto-Slice réussi pour %s -> %s" % [path.get_file(), target_ssp.get_file()])
		if Engine.is_editor_hint():
			EditorInterface.get_resource_filesystem().scan()
	else:
		var err: String = res.get("error", "Échec de l'exécution CLI")
		push_error("[SpriteStudio] Auto-Slice a échoué: " + err)
		if Engine.is_editor_hint():
			OS.alert("Échec de l'Auto-Slice pour " + path.get_file() + "\n" + err, "SpriteStudio")

func _on_open_in_dock(arg1: Variant = null, arg2: Variant = null) -> void:
	var path: String = ""
	if arg1 is String:
		path = arg1
	elif arg2 is String:
		path = arg2
	elif arg1 is PackedStringArray and not (arg1 as PackedStringArray).is_empty():
		path = (arg1 as PackedStringArray)[0]

	if _plugin != null and not path.is_empty():
		if _plugin.has_method("open_dock_for_file"):
			_plugin.call("open_dock_for_file", path)

func _on_open_in_desktop(arg1: Variant = null, arg2: Variant = null) -> void:
	var path: String = ""
	if arg1 is String:
		path = arg1
	elif arg2 is String:
		path = arg2
	elif arg1 is PackedStringArray and not (arg1 as PackedStringArray).is_empty():
		path = (arg1 as PackedStringArray)[0]

	if not path.is_empty():
		SpriteStudioCliBridge.open_in_editor(path)
