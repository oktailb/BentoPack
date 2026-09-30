@tool
class_name BentoPackInspectorPlugin
extends EditorInspectorPlugin

const BentoI18n = preload("bento_i18n.gd")

## Custom Godot 4 inspector panel for 2D Sprite & Animation nodes.
## Adds quick-action buttons to open assets in BentoPack or trigger live re-packs.

func _can_handle(object: Object) -> bool:
	return (object is AnimatedSprite2D or
			object is Sprite2D or
			object is SpriteFrames or
			object is AtlasTexture or
			object is MeshInstance2D or
			object is Texture2D)

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
	title.text = BentoI18n.t("inspector_title")
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_color_override("font_color", Color(0.4, 0.7, 1.0))
	vbox.add_child(title)

	var btn_open := Button.new()
	btn_open.text = BentoI18n.t("inspector_open_btn")
	btn_open.tooltip_text = BentoI18n.t("inspector_open_tooltip")
	btn_open.pressed.connect(_on_open_pressed.bind(object))
	vbox.add_child(btn_open)

	panel.add_child(vbox)
	container.add_child(panel)

	add_custom_control(container)

func _on_open_pressed(target_object: Object) -> void:
	var path_to_open := _resolve_asset_path(target_object)
	if path_to_open.is_empty():
		print("[BentoPack] Opening BentoPack Desktop Editor (standalone mode).")
	else:
		print("[BentoPack] Opening asset in BentoPack: ", path_to_open)

	BentoPackCliBridge.open_in_editor(path_to_open)

func _resolve_asset_path(target_object: Object) -> String:
	var candidate_paths: Array[String] = []

	# 1. AnimatedSprite2D
	if target_object is AnimatedSprite2D:
		var as2d := target_object as AnimatedSprite2D
		if as2d.sprite_frames != null:
			if not as2d.sprite_frames.resource_path.is_empty():
				candidate_paths.append(as2d.sprite_frames.resource_path)
			for anim in as2d.sprite_frames.get_animation_names():
				var frame_count := as2d.sprite_frames.get_frame_count(anim)
				for f in range(frame_count):
					var tex := as2d.sprite_frames.get_frame_texture(anim, f)
					if tex is AtlasTexture and (tex as AtlasTexture).atlas != null:
						var a_path: String = (tex as AtlasTexture).atlas.resource_path
						if not a_path.is_empty():
							candidate_paths.append(a_path)
					elif tex != null and not tex.resource_path.is_empty():
						candidate_paths.append(tex.resource_path)
					if candidate_paths.size() > 5:
						break
				if candidate_paths.size() > 5:
					break
		if not as2d.scene_file_path.is_empty():
			candidate_paths.append(as2d.scene_file_path)
		if as2d.owner != null and not as2d.owner.scene_file_path.is_empty():
			candidate_paths.append(as2d.owner.scene_file_path)

	# 2. Sprite2D
	elif target_object is Sprite2D:
		var s2d := target_object as Sprite2D
		if s2d.texture != null:
			if s2d.texture is AtlasTexture and (s2d.texture as AtlasTexture).atlas != null:
				candidate_paths.append((s2d.texture as AtlasTexture).atlas.resource_path)
			if not s2d.texture.resource_path.is_empty():
				candidate_paths.append(s2d.texture.resource_path)
		if not s2d.scene_file_path.is_empty():
			candidate_paths.append(s2d.scene_file_path)
		if s2d.owner != null and not s2d.owner.scene_file_path.is_empty():
			candidate_paths.append(s2d.owner.scene_file_path)

	# 3. MeshInstance2D / BentoMeshSprite
	elif target_object is MeshInstance2D:
		var mi := target_object as MeshInstance2D
		var atlas_tex = mi.get("atlas_texture")
		if atlas_tex is Texture2D and not (atlas_tex as Texture2D).resource_path.is_empty():
			candidate_paths.append((atlas_tex as Texture2D).resource_path)
		if mi.texture != null and not mi.texture.resource_path.is_empty():
			candidate_paths.append(mi.texture.resource_path)
		if mi.mesh != null and not mi.mesh.resource_path.is_empty():
			candidate_paths.append(mi.mesh.resource_path)
		if not mi.scene_file_path.is_empty():
			candidate_paths.append(mi.scene_file_path)
		if mi.owner != null and not mi.owner.scene_file_path.is_empty():
			candidate_paths.append(mi.owner.scene_file_path)

	# 4. AtlasTexture
	elif target_object is AtlasTexture:
		var at := target_object as AtlasTexture
		if at.atlas != null and not at.atlas.resource_path.is_empty():
			candidate_paths.append(at.atlas.resource_path)
		if not at.resource_path.is_empty():
			candidate_paths.append(at.resource_path)

	# 5. SpriteFrames
	elif target_object is SpriteFrames:
		var sf := target_object as SpriteFrames
		if not sf.resource_path.is_empty():
			candidate_paths.append(sf.resource_path)
		for anim in sf.get_animation_names():
			var count := sf.get_frame_count(anim)
			for f in range(count):
				var tex := sf.get_frame_texture(anim, f)
				if tex is AtlasTexture and (tex as AtlasTexture).atlas != null:
					candidate_paths.append((tex as AtlasTexture).atlas.resource_path)
				elif tex != null and not tex.resource_path.is_empty():
					candidate_paths.append(tex.resource_path)
				if candidate_paths.size() > 5:
					break
			if candidate_paths.size() > 5:
				break

	# 6. Generic Resource (Texture2D, ArrayMesh, etc.)
	elif target_object is Resource:
		var res := target_object as Resource
		if not res.resource_path.is_empty():
			candidate_paths.append(res.resource_path)

	# Resolve best existing .bento or image file
	for cand in candidate_paths:
		var resolved := _find_best_file(cand)
		if not resolved.is_empty():
			return resolved

	return ""

func _find_best_file(candidate: String) -> String:
	if candidate.is_empty() or candidate == "res://" or candidate == "res:/":
		return ""

	# Strip sub-resource identifier if present (e.g. res://scene.tscn::AtlasTexture_123)
	if candidate.contains("::"):
		candidate = candidate.get_slice("::", 0)

	var global_cand := ProjectSettings.globalize_path(candidate)

	# 1. Exact match .bento file
	if candidate.get_extension().to_lower() == "bento":
		if FileAccess.file_exists(global_cand):
			return candidate

	# 2. Check direct sibling .bento (e.g. hero.tres -> hero.bento, hero.tscn -> hero.bento)
	var base := candidate.get_basename()
	var bento_direct := base + ".bento"
	if FileAccess.file_exists(ProjectSettings.globalize_path(bento_direct)):
		return bento_direct

	# 3. Strip _atlas suffix (e.g. hero_atlas.webp -> hero.bento)
	if base.ends_with("_atlas"):
		var without_atlas := base.trim_suffix("_atlas") + ".bento"
		if FileAccess.file_exists(ProjectSettings.globalize_path(without_atlas)):
			return without_atlas

	# 4. Strip _mesh_N suffix (e.g. hero_mesh_0.tres -> hero.bento)
	var mesh_pos := base.rfind("_mesh_")
	if mesh_pos != -1:
		var without_mesh := base.substr(0, mesh_pos) + ".bento"
		if FileAccess.file_exists(ProjectSettings.globalize_path(without_mesh)):
			return without_mesh

	# 5. Check if the candidate itself is an image file and exists
	var ext := candidate.get_extension().to_lower()
	if ext in ["png", "webp", "jpg", "jpeg"]:
		if FileAccess.file_exists(global_cand):
			return candidate

	# 6. Check companion atlas image alongside the descriptor
	for img_ext in ["webp", "png", "jpg", "jpeg"]:
		var atlas_img := base + "_atlas." + img_ext
		if FileAccess.file_exists(ProjectSettings.globalize_path(atlas_img)):
			return atlas_img
		var direct_img := base + "." + img_ext
		if FileAccess.file_exists(ProjectSettings.globalize_path(direct_img)):
			return direct_img

	# 7. Check if there is a matching .bento in the same directory
	var dir_path := candidate.get_base_dir()
	var global_dir := ProjectSettings.globalize_path(dir_path)
	if DirAccess.dir_exists_absolute(global_dir):
		var dir := DirAccess.open(global_dir)
		if dir != null:
			dir.list_dir_begin()
			var file_name := dir.get_next()
			var target_base := candidate.get_file().get_basename().to_lower()
			target_base = target_base.trim_suffix("_atlas")
			if mesh_pos != -1:
				target_base = target_base.substr(0, target_base.rfind("_mesh_"))

			var candidate_bentos: Array[String] = []
			while not file_name.is_empty():
				if not dir.current_is_dir() and file_name.to_lower().ends_with(".bento"):
					var b_name := file_name.get_basename().to_lower()
					if b_name == target_base or target_base.begins_with(b_name) or b_name.begins_with(target_base):
						dir.list_dir_end()
						return dir_path.path_join(file_name)
					candidate_bentos.append(dir_path.path_join(file_name))
				file_name = dir.get_next()
			dir.list_dir_end()

			if candidate_bentos.size() == 1:
				return candidate_bentos[0]

	return ""
