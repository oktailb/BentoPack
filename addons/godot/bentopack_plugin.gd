@tool
class_name BentoPackPlugin
extends EditorPlugin

## Main EditorPlugin entrypoint for the official BentoPack Godot 4 addon.

var _importer: BentoPackSspImporter
var _inspector_plugin: BentoPackInspectorPlugin
var _dock: BentoPackDock
var _context_menu_plugin: RefCounted

func _enter_tree() -> void:
	# 1. Register .ssp Import Plugin
	_importer = BentoPackSspImporter.new()
	add_import_plugin(_importer)

	# 2. Register Custom Inspector Plugin
	_inspector_plugin = BentoPackInspectorPlugin.new()
	add_inspector_plugin(_inspector_plugin)

	# 3. Register Bottom Panel Dock
	_dock = BentoPackDock.new()
	add_control_to_bottom_panel(_dock, "BentoPack")

	# 4. Register 1-Click FileSystem Context Menu (Godot 4.2+)
	if ClassDB.class_exists("EditorContextMenuPlugin"):
		_context_menu_plugin = BentoPackContextMenuPlugin.new(self)
		add_context_menu_plugin(EditorContextMenuPlugin.CONTEXT_SLOT_FILESYSTEM, _context_menu_plugin)

	# 5. Add Custom Tool Menu Items
	add_tool_menu_item("BentoPack: Open Desktop Editor", _on_open_editor_menu)
	add_tool_menu_item("BentoPack: Show CLI Info", _on_show_cli_info)

	print("[BentoPack] Godot 4 Plugin successfully initialized.")

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

	remove_tool_menu_item("BentoPack: Open Desktop Editor")
	remove_tool_menu_item("BentoPack: Show CLI Info")

	print("[BentoPack] Godot 4 Plugin unloaded.")

func open_dock_for_file(path: String) -> void:
	if _dock != null:
		_dock.set_source_file(path)
		make_bottom_panel_item_visible(_dock)


func _on_open_editor_menu() -> void:
	BentoPackCliBridge.open_in_editor("res://")

func _on_show_cli_info() -> void:
	var cli_path := BentoPackCliBridge.find_cli_path()
	if cli_path.is_empty():
		OS.alert("bentopack-cli binary not found on PATH or ProjectSettings.", "BentoPack CLI Status")
	else:
		var res := BentoPackCliBridge.run_cli(["--version"])
		OS.alert("BentoPack CLI found:\nPath: %s\nVersion: %s" % [cli_path, res.get("output", "")], "BentoPack CLI Status")
