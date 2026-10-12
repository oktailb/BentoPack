@tool
class_name BentoPackPlugin
extends EditorPlugin

const BentoI18n = preload("bento_i18n.gd")

## Main EditorPlugin entrypoint for the official BentoPack Godot 4 addon.

var _importer: BentoImporter
var _inspector_plugin: BentoPackInspectorPlugin
var _dock: BentoPackDock
var _context_menu_plugin: RefCounted
var _menu_open_title: String = ""
var _menu_cli_title: String = ""

func _enter_tree() -> void:
	# 1. Register .bento Import Plugin
	_importer = BentoImporter.new()
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
	_menu_open_title = BentoI18n.t("menu_open_editor")
	_menu_cli_title = BentoI18n.t("menu_show_cli")
	add_tool_menu_item(_menu_open_title, _on_open_editor_menu)
	add_tool_menu_item(_menu_cli_title, _on_show_cli_info)

	# 6. Start Background External Source Auto-Sync Watcher
	_setup_sync_timer()

	print("[BentoPack] Godot 4 Plugin successfully initialized.")

func _exit_tree() -> void:
	if _sync_timer != null:
		_sync_timer.stop()
		_sync_timer.queue_free()
		_sync_timer = null

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

	if not _menu_open_title.is_empty():
		remove_tool_menu_item(_menu_open_title)
	if not _menu_cli_title.is_empty():
		remove_tool_menu_item(_menu_cli_title)

	print("[BentoPack] Godot 4 Plugin unloaded.")

# --- External Source Synchronization System ---
const SYNC_CONFIG_FILE := "user://bentopack_sync_sources.cfg"
const SYNC_SECTION := "external_sources"

var _sync_timer: Timer
var _known_ext_times: Dictionary = {}

static func register_external_source(internal_path: String, external_path: String) -> void:
	if internal_path.is_empty():
		return
	var cfg := ConfigFile.new()
	cfg.load(SYNC_CONFIG_FILE)
	if external_path.is_empty():
		if cfg.has_section_key(SYNC_SECTION, internal_path):
			cfg.erase_section_key(SYNC_SECTION, internal_path)
	else:
		cfg.set_value(SYNC_SECTION, internal_path, external_path)
	cfg.save(SYNC_CONFIG_FILE)

static func get_external_source(internal_path: String) -> String:
	if internal_path.is_empty():
		return ""
	var cfg := ConfigFile.new()
	if cfg.load(SYNC_CONFIG_FILE) == OK:
		return str(cfg.get_value(SYNC_SECTION, internal_path, ""))
	return ""

static func get_all_registered_sources() -> Dictionary:
	var result := {}
	var cfg := ConfigFile.new()
	if cfg.load(SYNC_CONFIG_FILE) == OK and cfg.has_section(SYNC_SECTION):
		for key in cfg.get_section_keys(SYNC_SECTION):
			result[key] = str(cfg.get_value(SYNC_SECTION, key, ""))
	return result

static func sync_from_external_source(internal_res_path: String, external_path: String = "") -> bool:
	if internal_res_path.is_empty():
		return false
	if external_path.is_empty():
		external_path = get_external_source(internal_res_path)
	if external_path.is_empty() or not FileAccess.file_exists(external_path):
		return false

	var global_dest := ProjectSettings.globalize_path(internal_res_path)
	var err := DirAccess.copy_absolute(external_path, global_dest)
	if err == OK:
		print("[BentoPack] 🔄 Synchronisation automatique réussie : '%s' -> '%s'" % [external_path, internal_res_path])
		if Engine.is_editor_hint():
			var fs := EditorInterface.get_resource_filesystem()
			if fs != null and fs.has_method("reimport_files"):
				fs.call_deferred("reimport_files", PackedStringArray([internal_res_path]))
		return true
	else:
		push_warning("[BentoPack] Échec de la copie externe (%d): '%s' -> '%s'" % [err, external_path, global_dest])
		return false

func _setup_sync_timer() -> void:
	_sync_timer = Timer.new()
	_sync_timer.name = "BentoPackSyncTimer"
	_sync_timer.wait_time = 1.5
	_sync_timer.autostart = true
	_sync_timer.one_shot = false
	_sync_timer.timeout.connect(_on_check_external_modifications)
	add_child(_sync_timer)

func _on_check_external_modifications() -> void:
	if not Engine.is_editor_hint():
		return

	var sources: Dictionary = get_all_registered_sources()
	if sources.is_empty():
		return

	for internal_path: String in sources.keys():
		var ext_path: String = sources[internal_path]
		if ext_path.is_empty() or not FileAccess.file_exists(ext_path):
			continue

		var ext_write := FileAccess.get_modified_time(ext_path)
		var key := ext_path
		if not _known_ext_times.has(key):
			_known_ext_times[key] = ext_write
			continue

		var last_write: int = int(_known_ext_times[key])
		if ext_write > last_write:
			_known_ext_times[key] = ext_write
			sync_from_external_source(internal_path, ext_path)


func open_dock_for_file(path: String) -> void:
	if _dock != null:
		_dock.set_source_file(path)
		make_bottom_panel_item_visible(_dock)


func _on_open_editor_menu() -> void:
	BentoPackCliBridge.open_in_editor("")

func _on_show_cli_info() -> void:
	var cli_path := BentoPackCliBridge.find_cli_path()
	if cli_path.is_empty():
		OS.alert("bentopack-cli binary not found on PATH or ProjectSettings.", "BentoPack CLI Status")
	else:
		var res := BentoPackCliBridge.run_cli(["--version"])
		OS.alert("BentoPack CLI found:\nPath: %s\nVersion: %s" % [cli_path, res.get("output", "")], "BentoPack CLI Status")
