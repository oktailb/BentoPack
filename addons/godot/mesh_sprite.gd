@tool
class_name BentoPackMeshSprite
extends MeshInstance2D

## 2D Polygonal Sprite Player for BentoPack M8 tight meshes.
## Plays frame-by-frame 2D polygonal animations with zero transparency overdraw on GPU.

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
	if texture_atlas_valid():
		texture = atlas_texture
	if not autoplay.is_empty() and has_animation(autoplay):
		play(autoplay)
	else:
		_update_mesh_for_current_frame()

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
				var base_poly: PackedVector2Array = poly_frames[frame]
				if flip_h or flip_v:
					var flipped_poly := PackedVector2Array()
					var sx: float = -1.0 if flip_h else 1.0
					var sy: float = -1.0 if flip_v else 1.0
					for pt in base_poly:
						flipped_poly.append(Vector2(pt.x * sx, pt.y * sy))
					col_node.polygon = flipped_poly
				else:
					col_node.polygon = base_poly

	frame_changed.emit(frame)
