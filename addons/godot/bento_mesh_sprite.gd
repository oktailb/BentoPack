@tool
@icon("icon.png")
class_name BentoMeshSprite
extends MeshInstance2D

## 2D Polygonal Sprite Player for BentoPack M8 tight meshes.
## Plays frame-by-frame 2D polygonal animations with zero transparency overdraw on GPU.

const BentoParser = preload("bento_parser.gd")

## Path to .bento project file in project (e.g. res://character.bento)
@export_file("*.bento") var bento_project: String = "":
	set(val):
		bento_project = val
		if not val.is_empty():
			load_from_bento(val)

## Path to external .bento source file on disk. When modified, BentoPack hot-reloads this character.
@export_global_file("*.bento") var external_source_path: String = "":
	set(val):
		external_source_path = val
		if not val.is_empty():
			load_from_bento(val)

## Optional SpriteFrames or BentoPack imported resource. Dragging a .bento or companion .tres here unpacks it.
@export var frames: Resource = null:
	set(val):
		frames = val
		if val != null:
			_on_frames_assigned(val)

@export var atlas_texture: Texture2D:
	set(val):
		atlas_texture = val
		texture = val

@export var autoplay: String = ""

@export var current_animation: String = "default":
	set(val):
		current_animation = val
		frame = 0
		_update_mesh_for_current_frame()
		notify_property_list_changed()

@export var frame: int = 0:
	set(val):
		frame = val
		_update_mesh_for_current_frame()

@export var speed_scale: float = 1.0
@export var playing: bool = false
@export var flip_h: bool = false:
	set(val):
		flip_h = val
		scale.x = -abs(scale.x) if flip_h else abs(scale.x)
		_update_mesh_for_current_frame()
@export var flip_v: bool = false:
	set(val):
		flip_v = val
		scale.y = -abs(scale.y) if flip_v else abs(scale.y)
		_update_mesh_for_current_frame()

## Dictionary of animation_name -> Array[ArrayMesh]
@export var animations: Dictionary = {}:
	set(val):
		animations = val
		notify_property_list_changed()

## Dictionary of animation_name -> float (FPS)
@export var animation_fps: Dictionary = {}

## Dictionary of animation_name -> bool (loop)
@export var animation_loops: Dictionary = {}

## Target CollisionPolygon2D node to synchronize with current animation frame contour
@export_node_path("CollisionPolygon2D") var collision_polygon_node: NodePath = NodePath(""):
	set(val):
		collision_polygon_node = val
		_update_mesh_for_current_frame()

## Synchronize target collision polygon dynamically with sprite frame contour
@export var sync_collision: bool = true:
	set(val):
		sync_collision = val
		_update_mesh_for_current_frame()

## Dictionary of animation_name -> Array[PackedVector2Array]
@export var collision_polygons: Dictionary = {}

## Emitted whenever the active animation frame changes
signal frame_changed(current_frame: int)

## Emitted when a non-looping animation reaches its end
signal animation_finished(anim_name: String)

var _time_accumulator: float = 0.0

func _validate_property(property: Dictionary) -> void:
	if property.name == "current_animation" or property.name == "autoplay":
		var anim_list := animations.keys()
		property.hint = PROPERTY_HINT_ENUM
		property.hint_string = ",".join(PackedStringArray(anim_list))
	elif property.name == "frame":
		var max_frame := 0
		if animations.has(current_animation):
			max_frame = maxi(0, (animations[current_animation] as Array).size() - 1)
		property.hint = PROPERTY_HINT_RANGE
		property.hint_string = "0,%d,1" % max_frame

func _ready() -> void:
	if animations.is_empty():
		if not bento_project.is_empty():
			load_from_bento(bento_project)
		elif not external_source_path.is_empty():
			load_from_bento(external_source_path)
		elif frames != null:
			_on_frames_assigned(frames)

	if texture_atlas_valid():
		texture = atlas_texture
	if not autoplay.is_empty() and has_animation(autoplay):
		play(autoplay)
	else:
		_update_mesh_for_current_frame()

func _on_frames_assigned(res: Resource) -> void:
	if res == null:
		return
	var rpath := res.resource_path
	if rpath.ends_with(".bento"):
		load_from_bento(rpath)
		return
	var base := rpath.get_basename()
	var bento_cand := base + ".bento"
	if FileAccess.file_exists(ProjectSettings.globalize_path(bento_cand)):
		load_from_bento(bento_cand)
		return
	var dir := rpath.get_base_dir()
	var bento_in_dir := dir.path_join(rpath.get_file().get_basename() + ".bento")
	if FileAccess.file_exists(ProjectSettings.globalize_path(bento_in_dir)):
		load_from_bento(bento_in_dir)
		return

## Load and unpack all animations, meshes, collision polygons, and textures from a .bento project file
func load_from_bento(path: String) -> bool:
	if path.is_empty():
		return false
	var global_path := ProjectSettings.globalize_path(path)
	if not FileAccess.file_exists(global_path):
		push_error("[BentoMeshSprite] Fichier introuvable : " + path)
		return false

	var parse_result: BentoParser.ParseResult = BentoParser.parse_bento_file(global_path)
	if not parse_result.success:
		push_error("[BentoMeshSprite] Échec de la lecture du bento : " + parse_result.error_message)
		return false

	# 1. Atlas Texture
	var assigned_tex: Texture2D = null
	if path.begins_with("res://"):
		var base_path := path.get_basename()
		var candidate_webp := base_path + "_atlas.webp"
		var candidate_png := base_path + "_atlas.png"
		if ResourceLoader.exists(candidate_webp):
			assigned_tex = load(candidate_webp)
		elif ResourceLoader.exists(candidate_png):
			assigned_tex = load(candidate_png)

	if assigned_tex == null:
		assigned_tex = parse_result.atlas_texture

	if assigned_tex != null:
		atlas_texture = assigned_tex
		texture = assigned_tex

	# 2. Extract animations & meshes
	var anim_map := {}
	var col_map := {}
	var fps_map := {}
	var loop_map := {}

	var anims_data: Array = parse_result.project_data.get("animations", [])
	var boxes: Array = parse_result.project_data.get("boxes", [])

	if anims_data.is_empty():
		var default_frames: Array = []
		var default_col_frames: Array = []
		for i in range(boxes.size()):
			if parse_result.meshes.has(i):
				default_frames.append(parse_result.meshes[i])
			if parse_result.collision_polygons.has(i):
				default_col_frames.append(parse_result.collision_polygons[i])
		anim_map["default"] = default_frames
		col_map["default"] = default_col_frames
		fps_map["default"] = 10.0
		loop_map["default"] = true
	else:
		for a in anims_data:
			if a is Dictionary:
				var a_name: String = str(a.get("name", "anim"))
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

	animations = anim_map
	collision_polygons = col_map
	animation_fps = fps_map
	animation_loops = loop_map

	# 3. Set default active animation
	if not anim_map.is_empty():
		var target_anim: String = ""
		if anim_map.has("default"):
			target_anim = "default"
		elif anim_map.has("idle"):
			target_anim = "idle"
		else:
			target_anim = anim_map.keys()[0]
		current_animation = target_anim
		autoplay = target_anim
		frame = 0
		if (anim_map[target_anim] as Array).size() > 0:
			mesh = anim_map[target_anim][0]

	_update_mesh_for_current_frame()
	notify_property_list_changed()

	# Register external source if path is outside project and bento_project is inside
	if not path.begins_with("res://"):
		if not bento_project.is_empty():
			_save_sync_config(bento_project, path)
		else:
			var local_cand := "res://" + path.get_file()
			if FileAccess.file_exists(ProjectSettings.globalize_path(local_cand)):
				_save_sync_config(local_cand, path)

	print("[BentoMeshSprite] ✓ Animations BentoPack chargées avec succès depuis '%s' (%d animation(s), %d mesh(es))" % [path.get_file(), anim_map.size(), parse_result.meshes.size()])
	return true

func _save_sync_config(internal_res: String, ext_file: String) -> void:
	if internal_res.is_empty() or ext_file.is_empty():
		return
	var cfg := ConfigFile.new()
	cfg.load("user://bentopack_sync_sources.cfg")
	cfg.set_value("external_sources", internal_res, ext_file)
	cfg.save("user://bentopack_sync_sources.cfg")

func texture_atlas_valid() -> bool:
	return atlas_texture != null

func has_animation(anim_name: String) -> bool:
	return animations.has(anim_name)

func get_animation_names() -> Array:
	return animations.keys()

func get_frame_count(anim_name: String) -> int:
	if animations.has(anim_name):
		return (animations[anim_name] as Array).size()
	return 0

func play(anim_name: String = "") -> void:
	if not anim_name.is_empty():
		current_animation = anim_name
		frame = 0
	playing = true
	_time_accumulator = 0.0
	_update_mesh_for_current_frame()

func stop() -> void:
	playing = false
	frame = 0
	_update_mesh_for_current_frame()

func pause() -> void:
	playing = false

func _process(delta: float) -> void:
	if not playing or not animations.has(current_animation):
		return
	var frames: Array = animations[current_animation]
	if frames.is_empty():
		return

	var fps: float = float(animation_fps.get(current_animation, 10.0)) * speed_scale
	if fps <= 0.0:
		return

	_time_accumulator += delta
	var frame_duration := 1.0 / fps
	if _time_accumulator >= frame_duration:
		_time_accumulator -= frame_duration
		var next_frame := frame + 1
		if next_frame >= frames.size():
			if animation_loops.get(current_animation, true):
				next_frame = 0
			else:
				next_frame = frames.size() - 1
				playing = false
				animation_finished.emit(current_animation)
		frame = next_frame

func _update_mesh_for_current_frame() -> void:
	# 1. Update visual mesh
	if animations.has(current_animation):
		var frames: Array = animations[current_animation]
		if frame >= 0 and frame < frames.size():
			var m = frames[frame]
			if m is ArrayMesh:
				mesh = m

	# 2. Synchronize collision polygon shape if configured
	if sync_collision and not collision_polygon_node.is_empty():
		var col_node := get_node_or_null(collision_polygon_node) as CollisionPolygon2D
		if col_node != null and collision_polygons.has(current_animation):
			var poly_frames: Array = collision_polygons[current_animation]
			if frame >= 0 and frame < poly_frames.size():
				var base_poly: PackedVector2Array = _sanitize_poly(poly_frames[frame])

				if flip_h or flip_v:
					var flipped_poly := PackedVector2Array()
					var sx: float = -1.0 if flip_h else 1.0
					var sy: float = -1.0 if flip_v else 1.0
					for pt in base_poly:
						flipped_poly.append(Vector2(pt.x * sx, pt.y * sy))
					col_node.polygon = _sanitize_poly(flipped_poly)
				else:
					col_node.polygon = base_poly

	frame_changed.emit(frame)

func _sanitize_poly(poly: PackedVector2Array) -> PackedVector2Array:
	if poly.size() < 3:
		return poly
	if not Geometry2D.triangulate_polygon(poly).is_empty():
		return poly
	var merged := Geometry2D.merge_polygons(poly, PackedVector2Array())
	if not merged.is_empty():
		for m_poly in merged:
			if not Geometry2D.triangulate_polygon(m_poly).is_empty():
				return m_poly
	var rev := poly.duplicate()
	rev.reverse()
	if not Geometry2D.triangulate_polygon(rev).is_empty():
		return rev
	var hull := Geometry2D.convex_hull(poly)
	if not hull.is_empty():
		return hull
	return poly
