@tool
class_name SpriteStudioDock
extends PanelContainer

## Bottom Panel Dock for SpriteStudio in Godot 4 Editor.
## Allows importing raw bitmap atlases, auto-slicing, M8 tight packing, and watch daemon management.

signal process_requested(args: Array[String], output_ssp: String)
signal watch_toggled(active: bool, source_path: String, output_ssp: String, args: Array[String])

var _source_path_edit: LineEdit
var _target_ssp_edit: LineEdit
var _slice_mode_opt: OptionButton
var _tile_w_spin: SpinBox
var _tile_h_spin: SpinBox
var _grid_container: HBoxContainer
var _algo_opt: OptionButton
var _watch_check: CheckBox
var _status_lbl: Label
var _file_dialog: EditorFileDialog
var _cli_status_lbl: Label
var _download_cli_btn: Button

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
	var title_lbl := Label.new()
	title_lbl.text = "SpriteStudio Pipeline & Watcher"
	title_lbl.add_theme_font_size_override("font_size", 14)
	header_hbox.add_child(title_lbl)

	header_hbox.add_child(VSeparator.new())

	_cli_status_lbl = Label.new()
	_cli_status_lbl.text = "Checking CLI..."
	_cli_status_lbl.modulate = Color(0.7, 0.7, 0.7)
	header_hbox.add_child(_cli_status_lbl)

	_download_cli_btn = Button.new()
	_download_cli_btn.text = "📥 Télécharger CLI..."
	_download_cli_btn.tooltip_text = "Ouvrir la page officielle des Releases GitHub pour télécharger les binaires précompilés pour votre système."
	_download_cli_btn.modulate = Color(0.4, 0.8, 1.0)
	_download_cli_btn.visible = false
	_download_cli_btn.pressed.connect(func(): OS.shell_open(SpriteStudioCliBridge.get_releases_url()))
	header_hbox.add_child(_download_cli_btn)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header_hbox.add_child(spacer)

	var open_gui_btn := Button.new()
	open_gui_btn.text = "Ouvrir SpriteStudio GUI"
	open_gui_btn.pressed.connect(func(): SpriteStudioCliBridge.open_in_editor("res://"))
	header_hbox.add_child(open_gui_btn)

	main_vbox.add_child(header_hbox)
	main_vbox.add_child(HSeparator.new())

	# --- Form Controls Grid ---
	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 10)
	grid.add_theme_constant_override("v_separation", 6)
	main_vbox.add_child(grid)

	# 1. Source Image Path (INPUT)
	var src_lbl := Label.new()
	src_lbl.text = "Atlas source (Image Entrée) :"
	src_lbl.tooltip_text = "Fichier image PNG/WebP ou dossier contenant les frames de sprites."
	grid.add_child(src_lbl)

	_source_path_edit = LineEdit.new()
	_source_path_edit.placeholder_text = "res://spritesheet.png ou dossier de frames"
	_source_path_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_child(_source_path_edit)

	var browse_btn := Button.new()
	browse_btn.text = "Parcourir..."
	browse_btn.pressed.connect(_on_browse_pressed)
	grid.add_child(browse_btn)

	# 2. Target Output (OUTPUT)
	var target_lbl := Label.new()
	target_lbl.text = "Fichier projet (Sortie .ssp) :"
	target_lbl.tooltip_text = "Fichier projet .ssp généré par le CLI et automatiquement importé dans Godot avec les animations et le mesh polygonal."
	grid.add_child(target_lbl)

	_target_ssp_edit = LineEdit.new()
	_target_ssp_edit.text = "res://character.ssp"
	_target_ssp_edit.tooltip_text = "Nom du fichier .ssp à créer à partir de l'image source."
	_target_ssp_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_child(_target_ssp_edit)

	var target_empty := Control.new()
	grid.add_child(target_empty)

	# 3. Parameters Bar
	var params_hbox := HBoxContainer.new()
	params_hbox.add_theme_constant_override("separation", 12)

	var slice_lbl := Label.new()
	slice_lbl.text = "Découpage :"
	params_hbox.add_child(slice_lbl)

	_slice_mode_opt = OptionButton.new()
	_slice_mode_opt.add_item("Auto-Slice (Transparence / Silhouettes)", 0)
	_slice_mode_opt.add_item("Grille régulière (Tuiles)", 1)
	_slice_mode_opt.add_item("Images individuelles", 2)
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

	var algo_lbl := Label.new()
	algo_lbl.text = "Packing :"
	params_hbox.add_child(algo_lbl)

	_algo_opt = OptionButton.new()
	_algo_opt.add_item("MaxRects (Rectangulaire)", 0)
	_algo_opt.add_item("Polygonal M8 (Tight Mesh)", 1)
	params_hbox.add_child(_algo_opt)

	_watch_check = CheckBox.new()
	_watch_check.text = "Mode Watcher (--watch background)"
	_watch_check.tooltip_text = "Surveille les modifications de l'image source et re-génère les animations automatiquement à chaque sauvegarde."
	params_hbox.add_child(_watch_check)

	main_vbox.add_child(params_hbox)

	# --- Action Buttons & Status ---
	var action_hbox := HBoxContainer.new()
	action_hbox.add_theme_constant_override("separation", 10)

	var process_btn := Button.new()
	process_btn.text = "⚡ Traiter & Générer Animations"
	process_btn.modulate = Color(0.3, 0.9, 0.4)
	process_btn.pressed.connect(_on_process_pressed)
	action_hbox.add_child(process_btn)

	var stop_watch_btn := Button.new()
	stop_watch_btn.text = "Arrêter Watcher"
	stop_watch_btn.pressed.connect(_stop_watch)
	action_hbox.add_child(stop_watch_btn)

	_status_lbl = Label.new()
	_status_lbl.text = "Prêt."
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

func _check_cli_status() -> void:
	var cli_path := SpriteStudioCliBridge.find_cli_path()
	if cli_path.is_empty():
		_cli_status_lbl.text = "CLI: Non détecté (PATH ou ProjectSettings)"
		_cli_status_lbl.modulate = Color(1.0, 0.4, 0.4)
		if _download_cli_btn != null:
			_download_cli_btn.visible = true
	else:
		_cli_status_lbl.text = "CLI: " + cli_path.get_file() + " (OK)"
		_cli_status_lbl.modulate = Color(0.4, 1.0, 0.4)
		if _download_cli_btn != null:
			_download_cli_btn.visible = false

func set_source_file(path: String) -> void:
	_on_file_selected(path)


func _on_browse_pressed() -> void:
	_file_dialog.popup_file_dialog()

func _on_file_selected(path: String) -> void:
	_source_path_edit.text = path
	if _target_ssp_edit.text.is_empty() or _target_ssp_edit.text == "res://character.ssp":
		_target_ssp_edit.text = path.get_basename() + ".ssp"

func _on_slice_mode_changed(idx: int) -> void:
	_grid_container.visible = (idx == 1)

func _on_process_pressed() -> void:
	var src: String = _source_path_edit.text.strip_edges()
	var target_ssp: String = _target_ssp_edit.text.strip_edges()

	if src.is_empty():
		_status_lbl.text = "Erreur : Veuillez spécifier un fichier image source."
		_status_lbl.modulate = Color(1, 0.3, 0.3)
		return

	if target_ssp.is_empty():
		target_ssp = src.get_basename() + ".ssp"
		_target_ssp_edit.text = target_ssp

	var global_src := ProjectSettings.globalize_path(src)
	var global_target := ProjectSettings.globalize_path(target_ssp)

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
		if target_ssp.ends_with(".ssp"):
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

	_status_lbl.text = "Traitement CLI en cours..."
	_status_lbl.modulate = Color(1.0, 0.8, 0.3)

	if is_watch:
		var cli_path := SpriteStudioCliBridge.find_cli_path()
		if cli_path.is_empty():
			_status_lbl.text = "Erreur : spritestudio-cli introuvable."
			_status_lbl.modulate = Color(1, 0.3, 0.3)
			return
		_watch_pid = OS.create_process(cli_path, cli_args)
		if _watch_pid > 0:
			_status_lbl.text = "Watcher actif (PID %d). Surveillance de %s..." % [_watch_pid, src.get_file()]
			_status_lbl.modulate = Color(0.3, 0.9, 0.4)
		else:
			_status_lbl.text = "Échec du lancement du watcher."
			_status_lbl.modulate = Color(1, 0.3, 0.3)
	else:
		var res := SpriteStudioCliBridge.run_cli(cli_args)
		if res.get("success", false):
			_status_lbl.text = "Succès ! %s généré et prêt dans Godot." % target_ssp.get_file()
			_status_lbl.modulate = Color(0.3, 1.0, 0.4)
			# Refresh Editor FileSystem
			if Engine.is_editor_hint():
				EditorInterface.get_resource_filesystem().scan()
		else:
			_status_lbl.text = "Erreur CLI (code %d): %s" % [res.get("exit_code", -1), res.get("output", "")]
			_status_lbl.modulate = Color(1, 0.4, 0.4)

func _stop_watch() -> void:
	if _watch_pid > 0:
		OS.kill(_watch_pid)
		_watch_pid = -1
		_status_lbl.text = "Watcher arrêté."
		_status_lbl.modulate = Color(0.8, 0.8, 0.8)
	else:
		_status_lbl.text = "Aucun watcher actif."
