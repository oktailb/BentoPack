@tool
class_name BentoImporter
extends EditorImportPlugin

## Automatic Godot 4 Import Plugin for BentoPack (.bento) projects.

const BentoParser = preload("bento_parser.gd")

enum Presets { PRESET_DEFAULT }

func _get_importer_name() -> String:
	return "bentopack.bento"

func _get_visible_name() -> String:
	return "BentoPack Project (.bento)"

func _get_recognized_extensions() -> PackedStringArray:
	return PackedStringArray(["bento"])

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
			"name": "external_source_path",
			"default_value": "",
			"property_hint": PROPERTY_HINT_GLOBAL_FILE,
			"hint_string": "*.bento;BentoPack Project",
		},
		{
			"name": "generate_tres",
			"default_value": true,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "generate_scene",
			"default_value": true,
			"property_hint": PROPERTY_HINT_NONE,
		},
		{
			"name": "overwrite_existing_scene",
			"default_value": false,
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
			"default_value": true,
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
	var parse_result: BentoParser.ParseResult = BentoParser.parse_bento_file(source_file, repack_mode)
	if not parse_result.success:
		push_error("BentoPack Import Error: " + parse_result.error_message)
		return ERR_FILE_CORRUPT

	var base_path := source_file.get_basename()
	var atlas_ext := parse_result.atlas_format
	if atlas_ext.is_empty():
		atlas_ext = "webp"
	var atlas_file_path := base_path + "_atlas." + atlas_ext

	# Register external source path if specified in import options
	var ext_source: String = str(options.get("external_source_path", "")).strip_edges()
	if not ext_source.is_empty():
		BentoPackPlugin.register_external_source(source_file, ext_source)

	# 1. Save extracted atlas image alongside the project
	if parse_result.atlas_image != null and not parse_result.raw_atlas_bytes.is_empty():
		var fa := FileAccess.open(atlas_file_path, FileAccess.WRITE)
		if fa != null:
			fa.store_buffer(parse_result.raw_atlas_bytes)
			fa.close()
			gen_files.append(atlas_file_path)

	# 2. Update AtlasTexture references to point to the saved texture resource
	var atlas_res: Texture2D = null
	if ResourceLoader.has_cached(atlas_file_path):
		var cached_res = ResourceLoader.load(atlas_file_path, "", ResourceLoader.CACHE_MODE_REUSE)
		if cached_res is ImageTexture and parse_result.atlas_image != null:
			cached_res.set_image(parse_result.atlas_image)
			cached_res.emit_changed()
			atlas_res = cached_res
		elif parse_result.atlas_texture != null:
			# If cached_res is CompressedTexture2D, Godot's C++ core prohibits set_image().
			# Displace the stale cache entry and take over the path with the fresh ImageTexture!
			if cached_res != null:
				cached_res.take_over_path("")
			parse_result.atlas_texture.resource_path = atlas_file_path
			parse_result.atlas_texture.take_over_path(atlas_file_path)
			atlas_res = parse_result.atlas_texture

	if atlas_res == null:
		if parse_result.atlas_texture != null:
			atlas_res = parse_result.atlas_texture
		elif ResourceLoader.exists(atlas_file_path):
			atlas_res = load(atlas_file_path)

	if atlas_res != null:
		atlas_res.resource_path = atlas_file_path
		atlas_res.take_over_path(atlas_file_path)
		atlas_res.emit_changed()

		if parse_result.sprite_frames != null:
			for anim_name in parse_result.sprite_frames.get_animation_names():
				var count: int = parse_result.sprite_frames.get_frame_count(anim_name)
				for i in range(count):
					var frame_tex: Texture2D = parse_result.sprite_frames.get_frame_texture(anim_name, i)
					if frame_tex is AtlasTexture:
						frame_tex.atlas = atlas_res

	# Notify Godot's EditorFileSystem to asynchronously refresh the atlas texture .ctex on disk
	if Engine.is_editor_hint():
		var fs := EditorInterface.get_resource_filesystem()
		if fs != null and fs.has_method("reimport_files"):
			fs.call_deferred("reimport_files", PackedStringArray([atlas_file_path]))

	# 3. Synchronize in-place with existing cached SpriteFrames instances in RAM (active scenes / editor)
	var companion_tres_path := base_path + ".tres"
	var target_resource_path := companion_tres_path if options.get("generate_tres", true) else source_file

	# In-place sync for companion .tres if already active in memory
	if ResourceLoader.has_cached(companion_tres_path):
		var cached_tres = ResourceLoader.load(companion_tres_path, "", ResourceLoader.CACHE_MODE_REUSE)
		if cached_tres is SpriteFrames and cached_tres != parse_result.sprite_frames:
			_sync_sprite_frames(cached_tres, parse_result.sprite_frames)
			parse_result.sprite_frames = cached_tres

	# In-place sync for direct source_file (.bento) if already assigned to a node's sprite_frames
	if ResourceLoader.has_cached(source_file):
		var cached_src = ResourceLoader.load(source_file, "", ResourceLoader.CACHE_MODE_REUSE)
		if cached_src is SpriteFrames and cached_src != parse_result.sprite_frames:
			_sync_sprite_frames(cached_src, parse_result.sprite_frames)

	if parse_result.sprite_frames != null:
		parse_result.sprite_frames.resource_path = target_resource_path
		parse_result.sprite_frames.take_over_path(target_resource_path)
		parse_result.sprite_frames.emit_changed()

	var out_tres_path := "%s.%s" % [save_path, _get_save_extension()]
	var save_err := ResourceSaver.save(parse_result.sprite_frames, out_tres_path)
	if save_err != OK:
		push_error("BentoPack: Failed to save SpriteFrames to: " + out_tres_path)
		return save_err

	# 3b. Optional: Save companion .tres directly in project alongside .bento
	if options.get("generate_tres", true):
		var tres_err := ResourceSaver.save(parse_result.sprite_frames, companion_tres_path)
		if tres_err == OK:
			gen_files.append(companion_tres_path)

	# 4. Optional: Generate instantiable companion PackedScene (.tscn)
	var tscn_path := base_path + ".tscn"
	var should_gen_scene: bool = bool(options.get("generate_scene", true))
	var overwrite_scene: bool = bool(options.get("overwrite_existing_scene", false))
	var tscn_exists := FileAccess.file_exists(ProjectSettings.globalize_path(tscn_path))

	if should_gen_scene and (not tscn_exists or overwrite_scene):
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

		# If polygonal meshes exist, add a native BentoMeshSprite node and AnimationPlayer
		if has_m8_meshes:
			var mesh_sprite := MeshInstance2D.new()
			var bento_script = load("res://addons/bentopack/bento_mesh_sprite.gd")
			if bento_script == null:
				bento_script = preload("bento_mesh_sprite.gd")
			mesh_sprite.set_script(bento_script)
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
				col_polygon_node.visible = options.get("collider_visible", true)

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
		ResourceSaver.save(scene, tscn_path)
		gen_files.append(tscn_path)
		root.free()
	elif tscn_exists:
		gen_files.append(tscn_path)

	# 5. Optional: Save standalone M8 ArrayMesh resources
	if options.get("generate_meshes", true) and not parse_result.meshes.is_empty():
		for box_idx in parse_result.meshes.keys():
			var m: ArrayMesh = parse_result.meshes[box_idx]
			var mesh_file := "%s_mesh_%d.tres" % [base_path, box_idx]
			ResourceSaver.save(m, mesh_file)
			gen_files.append(mesh_file)

	return OK

## Synchronizes an existing in-memory SpriteFrames instance in place so all active viewport nodes update immediately.
static func _sync_sprite_frames(target: SpriteFrames, source: SpriteFrames) -> void:
	if target == null or source == null or target == source:
		return
	for anim in target.get_animation_names():
		target.remove_animation(anim)
	for anim in source.get_animation_names():
		target.add_animation(anim)
		target.set_animation_speed(anim, source.get_animation_speed(anim))
		target.set_animation_loop(anim, source.get_animation_loop(anim))
		var count: int = source.get_frame_count(anim)
		for i in range(count):
			var tex: Texture2D = source.get_frame_texture(anim, i)
			var dur: float = source.get_frame_duration(anim, i)
			target.add_frame(anim, tex, dur)
	target.emit_changed()

