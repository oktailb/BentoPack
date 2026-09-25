@tool
class_name BentoPackSspParser
extends RefCounted

## Pure GDScript parser for native BentoPack project archives (.ssp)
## and JSON atlas descriptors. Generates SpriteFrames, AtlasTextures, and M8 Meshes.

class ParseResult:
	var success: bool = false
	var error_message: String = ""
	var project_name: String = ""
	var atlas_image: Image = null
	var atlas_texture: Texture2D = null
	var sprite_frames: SpriteFrames = null
	var meshes: Dictionary = {} # box_index -> ArrayMesh
	var collision_polygons: Dictionary = {} # box_index -> PackedVector2Array
	var project_data: Dictionary = {}
	var raw_atlas_png_bytes: PackedByteArray

## Parses a .bento or .ssp ZIP archive or project JSON and produces ready-to-use Godot 2D resources.
static func parse_bento_file(file_path: String, repack_for_animated_sprite: bool = false) -> ParseResult:
	return parse_ssp_file(file_path, repack_for_animated_sprite)

## Parses a .ssp or .bento ZIP archive or project JSON and produces ready-to-use Godot 2D resources.
static func parse_ssp_file(file_path: String, repack_for_animated_sprite: bool = false) -> ParseResult:
	var res := ParseResult.new()
	var global_path := ProjectSettings.globalize_path(file_path)

	if not FileAccess.file_exists(global_path):
		res.error_message = "File does not exist: " + file_path
		return res

	var reader := ZIPReader.new()
	var err := reader.open(global_path)
	if err != OK:
		res.error_message = "Failed to open ZIP archive (code %d): %s" % [err, file_path]
		return res

	var files := reader.get_files()

	# Locate project.json and atlas.png
	var json_entry := ""
	var atlas_entry := ""

	for f in files:
		var norm := f.to_lower()
		if norm.ends_with("project.json"):
			json_entry = f
		elif norm.ends_with("atlas.png") or norm.ends_with("sheet.png"):
			atlas_entry = f

	if json_entry.is_empty():
		reader.close()
		res.error_message = "Corrupt .ssp archive: missing project.json"
		return res

	var json_bytes := reader.read_file(json_entry)
	var json_str := json_bytes.get_string_from_utf8()
	var parsed_json: Variant = JSON.parse_string(json_str)

	if not (parsed_json is Dictionary):
		reader.close()
		res.error_message = "Invalid JSON structure in project.json"
		return res

	res.project_data = parsed_json as Dictionary
	res.project_name = res.project_data.get("name", file_path.get_file().get_basename())

	# Read atlas texture if present
	if not atlas_entry.is_empty():
		res.raw_atlas_png_bytes = reader.read_file(atlas_entry)
		var img := Image.new()
		var img_err := img.load_png_from_buffer(res.raw_atlas_png_bytes)
		if img_err == OK:
			res.atlas_image = img
			res.atlas_texture = ImageTexture.create_from_image(img)

	reader.close()

	# Build SpriteFrames and ArrayMeshes from parsed project metadata
	_build_resources(res, repack_for_animated_sprite)
	res.success = true
	return res

## Parses an uncompressed project.dict directly (useful for tests or pipelines).
static func parse_project_dict(project_dict: Dictionary, atlas_img: Image = null, repack_for_animated_sprite: bool = false) -> ParseResult:
	var res := ParseResult.new()
	res.project_data = project_dict
	res.project_name = project_dict.get("name", "Project")
	if atlas_img != null:
		res.atlas_image = atlas_img
		res.atlas_texture = ImageTexture.create_from_image(atlas_img)

	_build_resources(res, repack_for_animated_sprite)
	res.success = true
	return res

static func _build_resources(res: ParseResult, repack_for_animated_sprite: bool = false) -> void:
	# Only repack if explicitly requested (e.g. for legacy AnimatedSprite2D compatibility).
	# Otherwise, keep the dense M8 packed atlas 1:1 intact to preserve VRAM.
	if repack_for_animated_sprite:
		_mask_and_repack_polygonal_boxes(res)

	var data := res.project_data
	var boxes: Array = data.get("boxes", [])
	var animations: Array = data.get("animations", [])

	var atlas_w: float = float(data.get("atlas", {}).get("width", 1024))
	var atlas_h: float = float(data.get("atlas", {}).get("height", 1024))
	if res.atlas_image != null:
		atlas_w = float(res.atlas_image.get_width())
		atlas_h = float(res.atlas_image.get_height())

	# Map box index -> box data dictionary
	var box_map := {}
	for b in boxes:
		if b is Dictionary:
			var idx: int = int(b.get("index", box_map.size()))
			box_map[idx] = b

	var sf := SpriteFrames.new()
	if sf.has_animation("default"):
		sf.remove_animation("default")

	# Process animations
	if animations.is_empty():
		# Create a default animation containing all boxes sequentially
		sf.add_animation("default")
		sf.set_animation_speed("default", 10.0)
		sf.set_animation_loop("default", true)

		for idx in box_map.keys():
			var b: Dictionary = box_map[idx]
			var tex := _create_atlas_texture(res.atlas_texture, b)
			sf.add_frame("default", tex)
	else:
		for anim in animations:
			if not (anim is Dictionary):
				continue
			var a_dict := anim as Dictionary
			var a_name: String = a_dict.get("name", "anim")
			var fps: float = float(a_dict.get("fps", 10.0))
			var loop: bool = bool(a_dict.get("loop", true))

			sf.add_animation(a_name)
			sf.set_animation_speed(a_name, fps)
			sf.set_animation_loop(a_name, loop)

			var frame_indices: Array = a_dict.get("frames", [])
			for f_idx in frame_indices:
				var box_idx := int(f_idx)
				if box_map.has(box_idx):
					var b: Dictionary = box_map[box_idx]
					var tex := _create_atlas_texture(res.atlas_texture, b)
					sf.add_frame(a_name, tex)

	res.sprite_frames = sf

	# Build M8 polygonal tight meshes and collision polygons if available
	for idx in box_map.keys():
		var b: Dictionary = box_map[idx]
		if b.get("hasPolygonMesh", false):
			var mesh := _create_m8_array_mesh(b, atlas_w, atlas_h)
			if mesh != null:
				res.meshes[idx] = mesh
		
		var col_poly := _create_collision_polygon(b)
		if not col_poly.is_empty():
			res.collision_polygons[idx] = col_poly

static func _mask_and_repack_polygonal_boxes(res: ParseResult) -> void:
	var orig_img: Image = res.atlas_image
	if orig_img == null:
		return

	var boxes: Array = res.project_data.get("boxes", [])
	var has_polygons := false
	for b in boxes:
		if b is Dictionary and b.get("hasPolygonMesh", false) and not b.get("polygon", []).is_empty():
			has_polygons = true
			break

	if not has_polygons:
		return

	var box_images := {}
	var total_area := 0
	for b in boxes:
		if not (b is Dictionary):
			continue
		var idx: int = int(b.get("index", box_images.size()))
		var r: Dictionary = b.get("rect", {})
		var rx := int(r.get("x", 0))
		var ry := int(r.get("y", 0))
		var rw := int(r.get("w", 0))
		var rh := int(r.get("h", 0))
		if rw <= 0 or rh <= 0:
			continue

		total_area += rw * rh
		var sub := orig_img.get_region(Rect2i(rx, ry, rw, rh))

		if b.get("hasPolygonMesh", false):
			var poly_raw: Array = b.get("polygon", [])
			if not poly_raw.is_empty():
				var poly_pts := PackedVector2Array()
				var p_i := 0
				while p_i + 1 < poly_raw.size():
					poly_pts.append(Vector2(float(poly_raw[p_i]), float(poly_raw[p_i + 1])))
					p_i += 2

				# Clear alpha for all pixels outside the polygon contour
				for y in range(rh):
					for x in range(rw):
						if not Geometry2D.is_point_in_polygon(Vector2(x + 0.5, y + 0.5), poly_pts):
							sub.set_pixel(x, y, Color(0, 0, 0, 0))

		box_images[idx] = sub

	# Repack disjointly with 2px padding to avoid bleeding from neighbours
	var target_w := 2048
	if total_area > 2048 * 2048:
		target_w = 4096
	elif total_area <= 1024 * 1024:
		target_w = 1024

	var max_h := 4096
	var packed_img := Image.create(target_w, max_h, false, Image.FORMAT_RGBA8)
	packed_img.fill(Color(0, 0, 0, 0))

	var cur_x := 2
	var cur_y := 2
	var shelf_h := 0

	for b in boxes:
		if not (b is Dictionary):
			continue
		var idx: int = int(b.get("index", -1))
		if not box_images.has(idx):
			continue

		var sub: Image = box_images[idx]
		var sw := sub.get_width()
		var sh := sub.get_height()

		if cur_x + sw + 2 > target_w:
			cur_x = 2
			cur_y += shelf_h + 2
			shelf_h = 0

		packed_img.blit_rect(sub, Rect2i(0, 0, sw, sh), Vector2i(cur_x, cur_y))
		b["rect"] = {"x": cur_x, "y": cur_y, "w": sw, "h": sh}

		cur_x += sw + 2
		if sh > shelf_h:
			shelf_h = sh

	var final_h := cur_y + shelf_h + 4
	var clean_atlas := packed_img.get_region(Rect2i(0, 0, target_w, final_h))

	res.atlas_image = clean_atlas
	res.raw_atlas_png_bytes = clean_atlas.save_png_to_buffer()
	res.atlas_texture = ImageTexture.create_from_image(clean_atlas)

static func _create_atlas_texture(atlas_tex: Texture2D, box: Dictionary) -> AtlasTexture:
	var tex := AtlasTexture.new()
	tex.atlas = atlas_tex

	var r: Dictionary = box.get("rect", {})
	var rx: float = float(r.get("x", 0))
	var ry: float = float(r.get("y", 0))
	var rw: float = float(r.get("w", 0))
	var rh: float = float(r.get("h", 0))
	tex.region = Rect2(rx, ry, rw, rh)

	# If the box defines an explicit trimmed source rect / margin, apply standard Godot margin
	var orig_w: float = float(box.get("original_width", rw))
	var orig_h: float = float(box.get("original_height", rh))
	var crop_x: float = float(box.get("crop_x", 0.0))
	var crop_y: float = float(box.get("crop_y", 0.0))
	if crop_x > 0.0 or crop_y > 0.0 or orig_w > rw or orig_h > rh:
		tex.margin = Rect2(crop_x, crop_y, orig_w - rw, orig_h - rh)

	return tex

static func _create_m8_array_mesh(box: Dictionary, atlas_w: float, atlas_h: float) -> ArrayMesh:
	var verts_raw: Array = box.get("vertices", [])
	var indices_raw: Array = box.get("indices", box.get("triangles", []))
	if verts_raw.is_empty() or indices_raw.is_empty():
		return null

	var vertices := PackedVector2Array()
	var uvs := PackedVector2Array()
	var indices := PackedInt32Array()

	var r: Dictionary = box.get("rect", {})
	var rx: float = float(r.get("x", 0))
	var ry: float = float(r.get("y", 0))
	var rw: float = float(r.get("w", 0))
	var rh: float = float(r.get("h", 0))

	# Pivot offset to center the mesh around anchor point
	var p: Dictionary = box.get("pivot", {})
	var px: float = float(p.get("x", rw / 2.0))
	var py: float = float(p.get("y", rh / 2.0))

	# Vertices are serialised as [x0, y0, x1, y1, ...]
	var i := 0
	while i + 1 < verts_raw.size():
		var vx := float(verts_raw[i])
		var vy := float(verts_raw[i + 1])
		vertices.append(Vector2(vx - px, vy - py))

		# UV coordinates normalized to the entire atlas dimensions
		var uv_x: float = (rx + vx) / maxf(atlas_w, 1.0)
		var uv_y: float = (ry + vy) / maxf(atlas_h, 1.0)
		uvs.append(Vector2(uv_x, uv_y))
		i += 2

	for idx in indices_raw:
		indices.append(int(idx))

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)

	# Convert 2D vertices to 3D representation for ArrayMesh
	var verts_3d := PackedVector3Array()
	for v2 in vertices:
		verts_3d.append(Vector3(v2.x, v2.y, 0.0))

	arrays[Mesh.ARRAY_VERTEX] = verts_3d
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_INDEX] = indices

	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh

static func _create_collision_polygon(box: Dictionary) -> PackedVector2Array:
	var r: Dictionary = box.get("rect", {})
	var rw: float = float(r.get("w", 0.0))
	var rh: float = float(r.get("h", 0.0))
	if rw <= 0.0 or rh <= 0.0:
		return PackedVector2Array()

	var p: Dictionary = box.get("pivot", {})
	var px: float = float(p.get("x", rw / 2.0))
	var py: float = float(p.get("y", rh / 2.0))

	# 1. Check for explicit tight contour polygon
	var poly_raw: Array = box.get("polygon", [])
	if poly_raw.size() >= 6:
		var poly := PackedVector2Array()
		var i := 0
		while i + 1 < poly_raw.size():
			var vx := float(poly_raw[i])
			var vy := float(poly_raw[i + 1])
			poly.append(Vector2(vx - px, vy - py))
			i += 2
		return poly

	# 2. Check for vertices list (from M8 mesh)
	var verts_raw: Array = box.get("vertices", [])
	if verts_raw.size() >= 6:
		var raw_pts := PackedVector2Array()
		var i := 0
		while i + 1 < verts_raw.size():
			var vx := float(verts_raw[i])
			var vy := float(verts_raw[i + 1])
			raw_pts.append(Vector2(vx - px, vy - py))
			i += 2
		var hull := Geometry2D.convex_hull(raw_pts)
		if not hull.is_empty():
			return hull

	# 3. Fallback: Rectangular bounding box centered on pivot
	var rect_poly := PackedVector2Array()
	rect_poly.append(Vector2(-px, -py))
	rect_poly.append(Vector2(rw - px, -py))
	rect_poly.append(Vector2(rw - px, rh - py))
	rect_poly.append(Vector2(-px, rh - py))
	return rect_poly

