@tool
class_name BentoPackDock
extends PanelContainer

const BentoI18n = preload("bento_i18n.gd")

## Bottom Panel Dock for BentoPack in Godot 4 Editor.
## Allows importing raw bitmap atlases, auto-slicing, M8 tight packing, and watch daemon management.

signal process_requested(args: Array[String], output_bento: String)
signal watch_toggled(active: bool, source_path: String, output_bento: String, args: Array[String])

var _title_lbl: Label
var _source_path_edit: LineEdit
var _src_lbl: Label
var _browse_btn: Button
var _target_lbl: Label
var _target_bento_edit: LineEdit
var _slice_lbl: Label
var _slice_mode_opt: OptionButton
var _tile_w_spin: SpinBox
var _tile_h_spin: SpinBox
var _grid_container: HBoxContainer
var _algo_lbl: Label
var _algo_opt: OptionButton
var _watch_check: CheckBox
var _process_btn: Button
var _stop_watch_btn: Button
var _status_lbl: Label
var _file_dialog: EditorFileDialog
var _cli_status_lbl: Label
var _download_cli_btn: Button
var _open_gui_btn: Button

var _watch_pid: int = -1

func _init() -> void:
	custom_minimum_size = Vector2(0, 180)
	_build_ui()

func _ready() -> void:
	_check_cli_status()

func _build_ui() -> void:
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 12)
	margin.add_theme_constant_override("margin_right", 12)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	add_child(margin)

	var main_vbox := VBoxContainer.new()
	main_vbox.add_theme_constant_override("separation", 8)
	margin.add_child(main_vbox)

	# --- Header Bar ---
	var header_hbox := HBoxContainer.new()
	_title_lbl = Label.new()
	_title_lbl.add_theme_font_size_override("font_size", 14)
	header_hbox.add_child(_title_lbl)

	header_hbox.add_child(VSeparator.new())

	_cli_status_lbl = Label.new()
	_cli_status_lbl.modulate = Color(0.7, 0.7, 0.7)
	header_hbox.add_child(_cli_status_lbl)

	_download_cli_btn = Button.new()
	_download_cli_btn.modulate = Color(0.4, 0.8, 1.0)
	_download_cli_btn.visible = false
	_download_cli_btn.pressed.connect(func(): OS.shell_open(BentoPackCliBridge.get_releases_url()))
	header_hbox.add_child(_download_cli_btn)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header_hbox.add_child(spacer)

	# Language Selector
	var lang_sel := BentoI18n.create_language_selector(func(_new_lang): _update_i18n())
	header_hbox.add_child(lang_sel)

	_open_gui_btn = Button.new()
	_open_gui_btn.pressed.connect(func(): BentoPackCliBridge.open_in_editor("res://"))
	header_hbox.add_child(_open_gui_btn)

	main_vbox.add_child(header_hbox)
	main_vbox.add_child(HSeparator.new())

	# --- Form Controls Grid ---
	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 10)
	grid.add_theme_constant_override("v_separation", 6)
	main_vbox.add_child(grid)

	# 1. Source Image Path (INPUT)
	_src_lbl = Label.new()
	grid.add_child(_src_lbl)

	_source_path_edit = LineEdit.new()
	_source_path_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_child(_source_path_edit)

	_browse_btn = Button.new()
	_browse_btn.pressed.connect(_on_browse_pressed)
	grid.add_child(_browse_btn)

	# 2. Target Output (OUTPUT)
	_target_lbl = Label.new()
	grid.add_child(_target_lbl)

	_target_bento_edit = LineEdit.new()
	_target_bento_edit.text = "res://character.bento"
	_target_bento_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_child(_target_bento_edit)

	var target_empty := Control.new()
	grid.add_child(target_empty)

	# 3. Parameters Bar
	var params_hbox := HBoxContainer.new()
	params_hbox.add_theme_constant_override("separation", 12)

	_slice_lbl = Label.new()
	params_hbox.add_child(_slice_lbl)

	_slice_mode_opt = OptionButton.new()
	_slice_mode_opt.item_selected.connect(_on_slice_mode_changed)
	params_hbox.add_child(_slice_mode_opt)

	_grid_container = HBoxContainer.new()
	_grid_container.visible = false
	var gw_lbl := Label.new()
	gw_lbl.text = "W:"
	_grid_container.add_child(gw_lbl)
	_tile_w_spin = SpinBox.new()
	_tile_w_spin.min_value = 4
	_tile_w_spin.max_value = 1024
	_tile_w_spin.value = 32
	_grid_container.add_child(_tile_w_spin)

	var gh_lbl := Label.new()
	gh_lbl.text = "H:"
	_grid_container.add_child(gh_lbl)
	_tile_h_spin = SpinBox.new()
	_tile_h_spin.min_value = 4
	_tile_h_spin.max_value = 1024
	_tile_h_spin.value = 32
	_grid_container.add_child(_tile_h_spin)
	params_hbox.add_child(_grid_container)

	_algo_lbl = Label.new()
	params_hbox.add_child(_algo_lbl)

	_algo_opt = OptionButton.new()
	params_hbox.add_child(_algo_opt)

	_watch_check = CheckBox.new()
	params_hbox.add_child(_watch_check)

	main_vbox.add_child(params_hbox)

	# --- Action Buttons & Status ---
	var action_hbox := HBoxContainer.new()
	action_hbox.add_theme_constant_override("separation", 10)

	_process_btn = Button.new()
	_process_btn.modulate = Color(0.3, 0.9, 0.4)
	_process_btn.pressed.connect(_on_process_pressed)
	action_hbox.add_child(_process_btn)

	_stop_watch_btn = Button.new()
	_stop_watch_btn.pressed.connect(_stop_watch)
	action_hbox.add_child(_stop_watch_btn)

	_status_lbl = Label.new()
	_status_lbl.modulate = Color(0.8, 0.8, 0.8)
	_status_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	action_hbox.add_child(_status_lbl)

	main_vbox.add_child(action_hbox)

	# Setup File Dialog
	_file_dialog = EditorFileDialog.new()
	_file_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILE
	_file_dialog.filters = ["*.png, *.webp, *.jpg ; Images Atlas"]
	_file_dialog.file_selected.connect(_on_file_selected)
	add_child(_file_dialog)

	_update_i18n()

func _update_i18n() -> void:
	if _title_lbl != null:
		_title_lbl.text = BentoI18n.t("dock_title")
	if _download_cli_btn != null:
		_download_cli_btn.text = BentoI18n.t("download_cli")
		_download_cli_btn.tooltip_text = BentoI18n.t("download_cli_tooltip")
	if _open_gui_btn != null:
		_open_gui_btn.text = BentoI18n.t("open_gui")

	if _src_lbl != null:
		_src_lbl.text = BentoI18n.t("source_atlas")
		_src_lbl.tooltip_text = BentoI18n.t("source_atlas_tooltip")
	if _source_path_edit != null:
		_source_path_edit.placeholder_text = BentoI18n.t("source_placeholder")
	if _browse_btn != null:
		_browse_btn.text = BentoI18n.t("browse")

	if _target_lbl != null:
		_target_lbl.text = BentoI18n.t("target_project")
		_target_lbl.tooltip_text = BentoI18n.t("target_project_tooltip")
	if _target_bento_edit != null:
		_target_bento_edit.tooltip_text = BentoI18n.t("target_project_tooltip")

	if _slice_lbl != null:
		_slice_lbl.text = BentoI18n.t("slice_label")
	if _slice_mode_opt != null:
		var cur_slice: int = _slice_mode_opt.selected
		_slice_mode_opt.clear()
		_slice_mode_opt.add_item(BentoI18n.t("slice_mode_auto"), 0)
		_slice_mode_opt.add_item(BentoI18n.t("slice_mode_grid"), 1)
		_slice_mode_opt.add_item(BentoI18n.t("slice_mode_files"), 2)
		if cur_slice >= 0:
			_slice_mode_opt.selected = cur_slice

	if _algo_lbl != null:
		_algo_lbl.text = BentoI18n.t("packing_label")
	if _algo_opt != null:
		var cur_algo: int = _algo_opt.selected
		_algo_opt.clear()
		_algo_opt.add_item(BentoI18n.t("algo_maxrects"), 0)
		_algo_opt.add_item(BentoI18n.t("algo_m8"), 1)
		if cur_algo >= 0:
			_algo_opt.selected = cur_algo

	if _watch_check != null:
		_watch_check.text = BentoI18n.t("watcher_mode")
		_watch_check.tooltip_text = BentoI18n.t("watcher_tooltip")

	if _process_btn != null:
		_process_btn.text = BentoI18n.t("process_btn")
	if _stop_watch_btn != null:
		_stop_watch_btn.text = BentoI18n.t("stop_watcher")

	if _status_lbl != null and (_status_lbl.text == "Prêt." or _status_lbl.text == "Ready." or _status_lbl.text.is_empty()):
		_status_lbl.text = BentoI18n.t("status_ready")

	_check_cli_status()

func _check_cli_status() -> void:
	var cli_path := BentoPackCliBridge.find_cli_path()
	if cli_path.is_empty():
		_cli_status_lbl.text = BentoI18n.t("cli_not_found")
		_cli_status_lbl.modulate = Color(1.0, 0.4, 0.4)
		if _download_cli_btn != null:
			_download_cli_btn.visible = true
	else:
		_cli_status_lbl.text = BentoI18n.t("cli_ok") % cli_path.get_file()
		_cli_status_lbl.modulate = Color(0.4, 1.0, 0.4)
		if _download_cli_btn != null:
			_download_cli_btn.visible = false

func set_source_file(path: String) -> void:
	_on_file_selected(path)

func _on_browse_pressed() -> void:
	_file_dialog.popup_file_dialog()

func _on_file_selected(path: String) -> void:
	_source_path_edit.text = path
	if _target_bento_edit.text.is_empty() or _target_bento_edit.text == "res://character.bento":
		_target_bento_edit.text = path.get_basename() + ".bento"

func _on_slice_mode_changed(idx: int) -> void:
	_grid_container.visible = (idx == 1)

func _on_process_pressed() -> void:
	var src: String = _source_path_edit.text.strip_edges()
	var target_bento: String = _target_bento_edit.text.strip_edges()

	if src.is_empty():
		_status_lbl.text = BentoI18n.t("error_specify_source")
		_status_lbl.modulate = Color(1, 0.3, 0.3)
		return

	if target_bento.is_empty():
		target_bento = src.get_basename() + ".bento"
		_target_bento_edit.text = target_bento

	var global_src := ProjectSettings.globalize_path(src)
	var global_target := ProjectSettings.globalize_path(target_bento)

	var cli_args: Array[String] = []

	# Check watch mode
	var is_watch: bool = _watch_check.button_pressed

	if is_watch:
		cli_args.append("--watch")
		cli_args.append("--daemon")

	# Command: pack or slice
	var slice_mode := _slice_mode_opt.selected
	if slice_mode == 0: # Auto-Slice (Transparence / Silhouettes)
		cli_args.append("slice")
		if target_bento.ends_with(".bento"):
			cli_args.append("--output-project")
			cli_args.append(global_target)
		else:
			cli_args.append("--output-dir")
			cli_args.append(global_target.get_base_dir())
		cli_args.append(global_src)
	elif slice_mode == 1: # Grid
		cli_args.append("pack")
		cli_args.append("--algorithm")
		cli_args.append("Grid")
		cli_args.append("--data")
		cli_args.append(global_target)
		cli_args.append(global_src)
	else: # Images individuelles / dossier
		cli_args.append("pack")
		if _algo_opt.selected == 0:
			cli_args.append("--algorithm")
			cli_args.append("MaxRects")
		else:
			cli_args.append("--algorithm")
			cli_args.append("TightPolygon")
		cli_args.append("--data")
		cli_args.append(global_target)
		cli_args.append(global_src)

	_status_lbl.text = BentoI18n.t("status_processing")
	_status_lbl.modulate = Color(1.0, 0.8, 0.3)

	if is_watch:
		var cli_path := BentoPackCliBridge.find_cli_path()
		if cli_path.is_empty():
			_status_lbl.text = BentoI18n.t("error_cli_not_found")
			_status_lbl.modulate = Color(1, 0.3, 0.3)
			return
		_watch_pid = OS.create_process(cli_path, cli_args)
		if _watch_pid > 0:
			_status_lbl.text = BentoI18n.t("status_watcher_active") % [_watch_pid, src.get_file()]
			_status_lbl.modulate = Color(0.3, 0.9, 0.4)
		else:
			_status_lbl.text = BentoI18n.t("status_watcher_failed")
			_status_lbl.modulate = Color(1, 0.3, 0.3)
	else:
		var res := BentoPackCliBridge.run_cli(cli_args)
		if res.get("success", false):
			_status_lbl.text = BentoI18n.t("status_success") % target_bento.get_file()
			_status_lbl.modulate = Color(0.3, 1.0, 0.4)
			# Refresh Editor FileSystem
			if Engine.is_editor_hint():
				EditorInterface.get_resource_filesystem().scan()
		else:
			_status_lbl.text = BentoI18n.t("status_error") % [res.get("exit_code", -1), res.get("output", "")]
			_status_lbl.modulate = Color(1, 0.4, 0.4)

func _stop_watch() -> void:
	if _watch_pid > 0:
		OS.kill(_watch_pid)
		_watch_pid = -1
		_status_lbl.text = BentoI18n.t("status_watcher_stopped")
		_status_lbl.modulate = Color(0.8, 0.8, 0.8)
	else:
		_status_lbl.text = BentoI18n.t("status_no_watcher")
