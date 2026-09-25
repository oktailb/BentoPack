@tool
class_name BentoPackInspectorPlugin
extends EditorInspectorPlugin

## Custom Godot 4 inspector panel for 2D Sprite & Animation nodes.
## Adds quick-action buttons to open assets in BentoPack or trigger live re-packs.

func _can_handle(object: Object) -> bool:
	return (object is AnimatedSprite2D or
			object is Sprite2D or
			object is SpriteFrames or
			object is AtlasTexture or
			object is MeshInstance2D)

func _parse_begin(object: Object) -> void:
	var container := VBoxContainer.new()
	container.add_theme_constant_override("separation", 4)

	var panel := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.12, 0.14, 0.20, 0.9)
	style.border_color = Color(0.25, 0.35, 0.60, 1.0)
	style.set_border_width_all(1)
	style.set_corner_radius_all(4)
	style.content_margin_left = 6
	style.content_margin_right = 6
	style.content_margin_top = 6
	style.content_margin_bottom = 6
	panel.add_theme_stylebox_override("panel", style)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 4)

	var title := Label.new()
	title.text = "BentoPack 2D Toolkit"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_color_override("font_color", Color(0.4, 0.7, 1.0))
	vbox.add_child(title)

	var btn_open := Button.new()
	btn_open.text = "🎨 Open in BentoPack"
	btn_open.tooltip_text = "Launch the BentoPack desktop application to edit this asset."
	btn_open.pressed.connect(_on_open_pressed.bind(object))
	vbox.add_child(btn_open)

	panel.add_child(vbox)
	container.add_child(panel)

	add_custom_control(container)

func _on_open_pressed(target_object: Object) -> void:
	var path_to_open := ""

	if target_object is AnimatedSprite2D:
		var as2d := target_object as AnimatedSprite2D
		if as2d.sprite_frames != null and not as2d.sprite_frames.resource_path.is_empty():
			path_to_open = as2d.sprite_frames.resource_path
	elif target_object is Sprite2D:
		var s2d := target_object as Sprite2D
		if s2d.texture != null and not s2d.texture.resource_path.is_empty():
			path_to_open = s2d.texture.resource_path
	elif target_object is Resource:
		var res := target_object as Resource
		if not res.resource_path.is_empty():
			path_to_open = res.resource_path

	# Check if a sibling .ssp project file exists
	if not path_to_open.is_empty():
		var possible_ssp := path_to_open.get_basename() + ".ssp"
		if FileAccess.file_exists(ProjectSettings.globalize_path(possible_ssp)):
			path_to_open = possible_ssp

	if path_to_open.is_empty():
		path_to_open = "res://"

	BentoPackCliBridge.open_in_editor(path_to_open)
