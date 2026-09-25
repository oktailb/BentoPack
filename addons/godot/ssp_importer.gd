@tool
class_name SpriteStudioSspImporter
extends EditorImportPlugin

## Automatic Godot 4 Import Plugin for SpriteStudio .ssp projects.

const SspParser = preload("ssp_parser.gd")

enum Presets { PRESET_DEFAULT }

func _get_importer_name() -> String:
	return "spritestudio.ssp"

func _get_visible_name() -> String:
	return "SpriteStudio Project (.ssp)"

func _get_recognized_extensions() -> PackedStringArray:
	return PackedStringArray(["ssp"])

func _get_save_extension() -> String:
	return "tres"

func _get_resource_type() -> String:
	return "SpriteFrames"

func _get_preset_count() -> int:
	return 1

func _get_preset_name(preset_index: int) -> String:
	return "Default"

func _get_import_options(path: String, preset_index: int) -> Array[Dictionary]:
	return [
		{
			"name": "generate_scene",
			"default_value": true,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "generate_meshes",
			"default_value": true,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "generate_colliders",
			"default_value": true,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "collider_visible",
			"default_value": false,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "pixel_art_filter",
			"default_value": true,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "repack_for_animated_sprite",
			"default_value": false,
			"property_hint": PROPERTY_HINT_NONE,
		}
	]

func _get_option_visibility(path: String, option_name: StringName, options: Dictionary) -> bool:
	return true

func _get_priority() -> float:
	return 1.0

func _get_import_order() -> int:
	return 0

func _import(source_file: String, save_path: String, options: Dictionary, platform_variants: Array[String], gen_files: Array[String]) -> Error:
	var repack_mode: bool = bool(options.get("repack_for_animated_sprite", false))
	var parse_result: SspParser.ParseResult = SspParser.parse_ssp_file(source_file, repack_mode)
	if not parse_result.success:
		push_error("SpriteStudio Import Error: " + parse_result.error_message)
		return ERR_FILE_CORRUPT

	var base_path := source_file.get_basename()
	var atlas_png_path := base_path + "_atlas.png"

	# 1. Save extracted atlas image alongside the project
	if parse_result.atlas_image != null and not parse_result.raw_atlas_png_bytes.is_empty():
		var fa := FileAccess.open(atlas_png_path, FileAccess.WRITE)
		if fa != null:
			fa.store_buffer(parse_result.raw_atlas_png_bytes)
			fa.close()
			gen_files.append(atlas_png_path)

	# 2. Update AtlasTexture references to point to the saved texture resource
	var atlas_res: Texture2D = null
	if ResourceLoader.exists(atlas_png_path):
		atlas_res = load(atlas_png_path)
	elif parse_result.atlas_texture != null:
		atlas_res = parse_result.atlas_texture

	if atlas_res != null:
		if atlas_res.resource_path.is_empty():
			atlas_res.resource_path = atlas_png_path
			atlas_res.take_over_path(atlas_png_path)

		if parse_result.sprite_frames != null:
			for anim_name in parse_result.sprite_frames.get_animation_names():
				var count: int = parse_result.sprite_frames.get_frame_count(anim_name)
				for i in range(count):
					var frame_tex: Texture2D = parse_result.sprite_frames.get_frame_texture(anim_name, i)
					if frame_tex is AtlasTexture:
						frame_tex.atlas = atlas_res

	# 3. Save SpriteFrames resource
	var out_tres_path := "%s.%s" % [save_path, _get_save_extension()]
	var save_err := ResourceSaver.save(parse_result.sprite_frames, out_tres_path)
	if save_err != OK:
		push_error("SpriteStudio: Failed to save SpriteFrames to: " + out_tres_path)
		return save_err

	# 4. Optional: Generate instantiable companion PackedScene (.tscn)
	if options.get("generate_scene", true):
		var scene := PackedScene.new()
		var root := Node2D.new()
		root.name = parse_result.project_name.validate_node_name()

		var has_m8_meshes: bool = options.get("generate_meshes", true) and not parse_result.meshes.is_empty()

		var anim_sprite := AnimatedSprite2D.new()
		anim_sprite.name = "AnimatedSprite2D"
		anim_sprite.sprite_frames = parse_result.sprite_frames
		if parse_result.sprite_frames.has_animation("default"):
			anim_sprite.animation = "default"
		elif parse_result.sprite_frames.get_animation_names().size() > 0:
			anim_sprite.animation = parse_result.sprite_frames.get_animation_names()[0]
		anim_sprite.autoplay = anim_sprite.animation

		if options.get("pixel_art_filter", true):
			anim_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

		# If polygonal meshes are present, keep AnimatedSprite2D secondary
		anim_sprite.visible = not has_m8_meshes
		root.add_child(anim_sprite)
		anim_sprite.owner = root

		# If polygonal meshes exist, add a native SpriteStudioMeshSprite node and AnimationPlayer
		if has_m8_meshes:
			var mesh_sprite := MeshInstance2D.new()
			mesh_sprite.set_script(preload("mesh_sprite.gd"))
			mesh_sprite.name = "PolygonalMeshSprite"
			mesh_sprite.set("atlas_texture", atlas_res)

			var anim_map := {}
			var col_map := {}
			var fps_map := {}
			var loop_map := {}
			var anims_data: Array = parse_result.project_data.get("animations", [])
			if anims_data.is_empty():
				var default_frames: Array = []
				var default_col_frames: Array = []
				for b_idx in parse_result.meshes.keys():
					default_frames.append(parse_result.meshes[b_idx])
					if parse_result.collision_polygons.has(b_idx):
						default_col_frames.append(parse_result.collision_polygons[b_idx])
				anim_map["default"] = default_frames
				col_map["default"] = default_col_frames
				fps_map["default"] = 10.0
				loop_map["default"] = true
			else:
				for a in anims_data:
					if a is Dictionary:
						var a_name: String = a.get("name", "anim")
						var a_frames: Array = []
						var a_col_frames: Array = []
						for f_idx in a.get("frames", []):
							var b_idx: int = int(f_idx)
							if parse_result.meshes.has(b_idx):
								a_frames.append(parse_result.meshes[b_idx])
							if parse_result.collision_polygons.has(b_idx):
								a_col_frames.append(parse_result.collision_polygons[b_idx])
						anim_map[a_name] = a_frames
						col_map[a_name] = a_col_frames
						fps_map[a_name] = float(a.get("fps", 10.0))
						loop_map[a_name] = bool(a.get("loop", true))

			mesh_sprite.set("animations", anim_map)
			mesh_sprite.set("animation_fps", fps_map)
			mesh_sprite.set("animation_loops", loop_map)
			var first_anim: String = ""
			if anim_map.size() > 0:
				first_anim = anim_map.keys()[0]
				mesh_sprite.set("current_animation", first_anim)
				mesh_sprite.set("autoplay", first_anim)
				if (anim_map[first_anim] as Array).size() > 0:
					mesh_sprite.mesh = anim_map[first_anim][0]

			# Optional: Generate synchronized 2D Hitbox / CollisionPolygon2D
			var generate_colliders: bool = options.get("generate_colliders", true) and not parse_result.collision_polygons.is_empty()
			var col_polygon_node: CollisionPolygon2D = null

			if generate_colliders:
				var hitbox_area := Area2D.new()
				hitbox_area.name = "Hitbox"
				col_polygon_node = CollisionPolygon2D.new()
				col_polygon_node.name = "CollisionPolygon2D"
				col_polygon_node.visible = options.get("collider_visible", false)

				# Initialize with first frame polygon
				if not first_anim.is_empty() and col_map.has(first_anim):
					var first_col_frames: Array = col_map[first_anim]
					if not first_col_frames.is_empty():
						col_polygon_node.polygon = first_col_frames[0]

				hitbox_area.add_child(col_polygon_node)
				root.add_child(hitbox_area)
				hitbox_area.owner = root
				col_polygon_node.owner = root

				# Hook up collision synchronization on PolygonalMeshSprite
				mesh_sprite.set("collision_polygon_node", NodePath("../Hitbox/CollisionPolygon2D"))
				mesh_sprite.set("collision_polygons", col_map)
				mesh_sprite.set("sync_collision", true)

			if options.get("pixel_art_filter", true):
				mesh_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
			mesh_sprite.visible = true # Active by default for clean M8 polygonal rendering
			root.add_child(mesh_sprite)
			mesh_sprite.owner = root

			# Also create an AnimationPlayer with tracks on PolygonalMeshSprite:mesh and Hitbox:polygon
			var anim_player := AnimationPlayer.new()
			anim_player.name = "AnimationPlayer"
			var anim_lib := AnimationLibrary.new()

			for a_name in anim_map.keys():
				var a_frames: Array = anim_map[a_name]
				if a_frames.is_empty():
					continue
				var a_fps: float = float(fps_map.get(a_name, 10.0))
				var step: float = 1.0 / maxf(a_fps, 1.0)
				var anim := Animation.new()
				anim.length = maxf(step * a_frames.size(), 0.001)
				anim.loop_mode = Animation.LOOP_LINEAR if loop_map.get(a_name, true) else Animation.LOOP_NONE
				anim.step = step

				var track_idx := anim.add_track(Animation.TYPE_VALUE)
				anim.track_set_path(track_idx, "PolygonalMeshSprite:mesh")
				anim.track_set_interpolation_type(track_idx, Animation.INTERPOLATION_NEAREST)

				for f_i in range(a_frames.size()):
					var time: float = f_i * step
					anim.track_insert_key(track_idx, time, a_frames[f_i])

				# Synchronize CollisionPolygon2D track in AnimationPlayer if colliders active
				if generate_colliders and col_map.has(a_name):
					var a_cols: Array = col_map[a_name]
					if not a_cols.is_empty():
						var col_track_idx := anim.add_track(Animation.TYPE_VALUE)
						anim.track_set_path(col_track_idx, "Hitbox/CollisionPolygon2D:polygon")
						anim.track_set_interpolation_type(col_track_idx, Animation.INTERPOLATION_NEAREST)
						for f_i in range(mini(a_frames.size(), a_cols.size())):
							var time: float = f_i * step
							anim.track_insert_key(col_track_idx, time, a_cols[f_i])

				anim_lib.add_animation(a_name, anim)

			anim_player.add_animation_library("", anim_lib)
			if not first_anim.is_empty():
				anim_player.autoplay = first_anim
			root.add_child(anim_player)
			anim_player.owner = root

		scene.pack(root)
		var tscn_path := base_path + ".tscn"
		ResourceSaver.save(scene, tscn_path)
		gen_files.append(tscn_path)

	# 5. Optional: Save standalone M8 ArrayMesh resources
	if options.get("generate_meshes", true) and not parse_result.meshes.is_empty():
		for box_idx in parse_result.meshes.keys():
			var m: ArrayMesh = parse_result.meshes[box_idx]
			var mesh_file := "%s_mesh_%d.tres" % [base_path, box_idx]
			ResourceSaver.save(m, mesh_file)
			gen_files.append(mesh_file)

	return OK
