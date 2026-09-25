@tool
class_name SpriteStudioPlugin
extends EditorPlugin

## Main EditorPlugin entrypoint for the official SpriteStudio Godot 4 addon.

var _importer: SpriteStudioSspImporter
var _inspector_plugin: SpriteStudioInspectorPlugin
var _dock: SpriteStudioDock
var _context_menu_plugin: RefCounted

func _enter_tree() -> void:
	# 1. Register .ssp Import Plugin
	_importer = SpriteStudioSspImporter.new()
	add_import_plugin(_importer)

	# 2. Register Custom Inspector Plugin
	_inspector_plugin = SpriteStudioInspectorPlugin.new()
	add_inspector_plugin(_inspector_plugin)

	# 3. Register Bottom Panel Dock
	_dock = SpriteStudioDock.new()
	add_control_to_bottom_panel(_dock, "SpriteStudio")

	# 4. Register 1-Click FileSystem Context Menu (Godot 4.2+)
	if ClassDB.class_exists("EditorContextMenuPlugin"):
		_context_menu_plugin = SpriteStudioContextMenuPlugin.new(self)
		add_context_menu_plugin(EditorContextMenuPlugin.CONTEXT_SLOT_FILESYSTEM, _context_menu_plugin)

	# 5. Add Custom Tool Menu Items
	add_tool_menu_item("SpriteStudio: Open Desktop Editor", _on_open_editor_menu)
	add_tool_menu_item("SpriteStudio: Show CLI Info", _on_show_cli_info)

	print("[SpriteStudio] Godot 4 Plugin successfully initialized.")

func _exit_tree() -> void:
	if _importer != null:
		remove_import_plugin(_importer)
		_importer = null

	if _inspector_plugin != null:
		remove_inspector_plugin(_inspector_plugin)
		_inspector_plugin = null

	if _context_menu_plugin != null:
		remove_context_menu_plugin(_context_menu_plugin)
		_context_menu_plugin = null

	if _dock != null:
		remove_control_from_bottom_panel(_dock)
		_dock.queue_free()
		_dock = null

	remove_tool_menu_item("SpriteStudio: Open Desktop Editor")
	remove_tool_menu_item("SpriteStudio: Show CLI Info")

	print("[SpriteStudio] Godot 4 Plugin unloaded.")

func open_dock_for_file(path: String) -> void:
	if _dock != null:
		_dock.set_source_file(path)
		make_bottom_panel_item_visible(_dock)


func _on_open_editor_menu() -> void:
	SpriteStudioCliBridge.open_in_editor("res://")

func _on_show_cli_info() -> void:
	var cli_path := SpriteStudioCliBridge.find_cli_path()
	if cli_path.is_empty():
		OS.alert("spritestudio-cli binary not found on PATH or ProjectSettings.", "SpriteStudio CLI Status")
	else:
		var res := SpriteStudioCliBridge.run_cli(["--version"])
		OS.alert("SpriteStudio CLI found:\nPath: %s\nVersion: %s" % [cli_path, res.get("output", "")], "SpriteStudio CLI Status")
