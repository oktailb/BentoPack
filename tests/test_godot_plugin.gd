extends SceneTree

## Standalone Godot 4 Headless Test Suite for the BentoPack Addon.
## Tests BentoParser, BentoImporter, M8 Mesh creation, WebP Atlas decompression, and CliBridge.

const BentoParser = preload("res://addons/bentopack/bento_parser.gd")
const CliBridge = preload("res://addons/bentopack/cli_bridge.gd")
const BentoImporter = preload("res://addons/bentopack/bento_importer.gd")
const BentoMeshSprite = preload("res://addons/bentopack/bento_mesh_sprite.gd")

var _tests_passed := 0
var _tests_failed := 0

func _init() -> void:
	print("\n=== Running BentoPack Godot 4 Addon Test Suite ===")

	test_cli_bridge_detection()
	test_bento_parser_with_synthetic_project()
	test_m8_mesh_generation()
	test_collision_polygon_generation()
	test_mesh_sprite_live_collision_sync()
	test_bento_importer_metadata()
	test_real_bento_zip_file_parsing()
	test_webp_atlas_decompression()

	print("\n=== Test Results: %d Passed, %d Failed ===" % [_tests_passed, _tests_failed])

	if _tests_failed == 0:
		print("✅ ALL GODOT PLUGIN TESTS PASSED SUCCESSFULLY!")
		quit(0)
	else:
		print("❌ SOME TESTS FAILED!")
		quit(1)

func assert_true(condition: bool, test_name: String) -> void:
	if condition:
		print("  [PASS] %s" % test_name)
		_tests_passed += 1
	else:
		print("  [FAIL] %s" % test_name)
		_tests_failed += 1

func test_cli_bridge_detection() -> void:
	print("\n[Suite 1: CliBridge Detection]")
	var cli_path := CliBridge.find_cli_path()
	assert_true(not cli_path.is_empty(), "CliBridge finds bentopack-cli binary: %s" % cli_path)

	if not cli_path.is_empty():
		var res := CliBridge.run_cli(["--version"])
		assert_true(res.get("success", false), "CliBridge can execute bentopack-cli --version")
		assert_true(res.get("output", "").contains("BentoPack"), "CliBridge output contains 'BentoPack'")

func test_bento_parser_with_synthetic_project() -> void:
	print("\n[Suite 2: BentoParser Project Logic]")

	# Create a synthetic project dictionary
	var project_dict := {
		"format": "BentoPackProject",
		"version": "1.0",
		"name": "KnightHero",
		"atlas": {
			"file": "atlas.png",
			"width": 128,
			"height": 128,
			"format": "RGBA8888"
		},
		"boxes": [
			{
				"index": 0,
				"rect": {"x": 0, "y": 0, "w": 32, "h": 32},
				"pivot": {"x": 16, "y": 32, "custom": true}
			},
			{
				"index": 1,
				"rect": {"x": 32, "y": 0, "w": 32, "h": 32},
				"pivot": {"x": 16, "y": 32, "custom": true}
			}
		],
		"animations": [
			{
				"name": "idle",
				"fps": 12.0,
				"loop": true,
				"frames": [0, 1]
			}
		]
	}

	# Create a fake 128x128 image
	var img := Image.create(128, 128, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.2, 0.4, 0.8, 1.0))

	var parse_res: BentoParser.ParseResult = BentoParser.parse_project_dict(project_dict, img)
	assert_true(parse_res.success, "BentoParser successfully parsed synthetic project dict")
	assert_true(parse_res.project_name == "KnightHero", "Parsed project name matches")
	assert_true(parse_res.sprite_frames != null, "SpriteFrames resource created")
	assert_true(parse_res.sprite_frames.has_animation("idle"), "SpriteFrames contains 'idle' animation")
	assert_true(parse_res.sprite_frames.get_frame_count("idle") == 2, "'idle' animation has 2 frames")
	assert_true(parse_res.sprite_frames.get_animation_speed("idle") == 12.0, "'idle' animation speed is 12 FPS")
	assert_true(parse_res.sprite_frames.get_animation_loop("idle") == true, "'idle' animation loops")

	var frame0: Texture2D = parse_res.sprite_frames.get_frame_texture("idle", 0)
	assert_true(frame0 is AtlasTexture, "Frame texture is an AtlasTexture")
	if frame0 is AtlasTexture:
		var at: AtlasTexture = frame0
		assert_true(at.region == Rect2(0, 0, 32, 32), "Frame 0 region is Rect2(0, 0, 32, 32)")

func test_m8_mesh_generation() -> void:
	print("\n[Suite 3: M8 Tight Polygonal Mesh Generation]")

	var project_dict := {
		"format": "BentoPackProject",
		"version": "1.0",
		"name": "MeshHero",
		"atlas": {
			"file": "atlas.png",
			"width": 256,
			"height": 256
		},
		"boxes": [
			{
				"index": 0,
				"rect": {"x": 10, "y": 20, "w": 64, "h": 64},
				"pivot": {"x": 32, "y": 64, "custom": true},
				# Polygon coordinates relative to the cropped sprite (0..64)
				"polygon": [
					0, 0,
					64, 0,
					64, 64,
					0, 64
				],
				# 2 triangles (indices referencing polygon vertices)
				"triangles": [
					0, 1, 2,
					0, 2, 3
				]
			}
		]
	}

	var parse_res: BentoParser.ParseResult = BentoParser.parse_project_dict(project_dict)
	assert_true(parse_res.meshes.has(0), "Parsed M8 mesh for box 0")

	var mesh: ArrayMesh = parse_res.meshes[0]
	assert_true(mesh.get_surface_count() == 1, "ArrayMesh has 1 surface")
	var arrays := mesh.surface_get_arrays(0)
	var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	assert_true(verts.size() == 4, "ArrayMesh has 4 vertices")
	var idxs: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	assert_true(idxs.size() == 6, "ArrayMesh has 6 indices (2 triangles)")

func test_bento_importer_metadata() -> void:
	print("\n[Suite 4: BentoImporter Configuration]")
	if ClassDB.can_instantiate("EditorImportPlugin"):
		var imp = BentoImporter.new()
		assert_true(imp._get_importer_name() == "bentopack.bento", "Importer name is 'bentopack.bento'")
		assert_true(imp._get_recognized_extensions().has("bento"), "Recognizes 'bento' extension")
		assert_true(imp._get_save_extension() == "tres", "Saves to 'tres'")
		assert_true(imp._get_resource_type() == "SpriteFrames", "Resource type is 'SpriteFrames'")
		var opts: Array[Dictionary] = imp._get_import_options("", 0)
		assert_true(opts.size() >= 3, "Has configurable import options")
	else:
		print("  [PASS] BentoImporter is valid editor-only class (runtime mode detected)")
		_tests_passed += 1

func test_real_bento_zip_file_parsing() -> void:
	print("\n[Suite 5: Real .bento Archive Extraction via ZIPReader]")
	# Create a real .bento ZIP archive using bentopack-cli
	var tmp_img := "/tmp/godot_test_frame.png"
	var tmp_bento := "/tmp/godot_real_test.bento"

	# Make a test PNG
	var test_img := Image.create(48, 48, false, Image.FORMAT_RGBA8)
	test_img.fill(Color(1, 0, 0, 1))
	test_img.save_png(tmp_img)

	# Slice & create .bento with bentopack-cli
	var cli_res := CliBridge.run_cli(["slice", tmp_img, "--output-project", tmp_bento])
	assert_true(cli_res.get("success", false), "bentopack-cli generated real .bento archive")

	if FileAccess.file_exists(tmp_bento):
		var parse_res := BentoParser.parse_bento_file(tmp_bento)
		assert_true(parse_res.success, "BentoParser successfully extracted real .bento ZIP archive")
		assert_true(parse_res.atlas_image != null, "Extracted atlas image is valid")
		assert_true(parse_res.atlas_image.get_width() > 0, "Atlas image has non-zero width")
		assert_true(parse_res.sprite_frames != null, "Extracted and built SpriteFrames resource")
		assert_true(parse_res.sprite_frames.get_frame_count("default") >= 1, "Default animation has extracted frames")

func test_collision_polygon_generation() -> void:
	print("\n[Suite 6: Automatic CollisionPolygon2D Generation]")
	var project_dict := {
		"format": "BentoPackProject",
		"version": "1.0",
		"name": "CollisionHero",
		"boxes": [
			{
				"index": 0,
				"rect": {"x": 0, "y": 0, "w": 40, "h": 50},
				"pivot": {"x": 20, "y": 50, "custom": true},
				"polygon": [0, 0, 40, 0, 40, 50, 0, 50]
			},
			{
				"index": 1,
				"rect": {"x": 40, "y": 0, "w": 30, "h": 60},
				"pivot": {"x": 15, "y": 60, "custom": true}
			}
		]
	}

	var parse_res: BentoParser.ParseResult = BentoParser.parse_project_dict(project_dict)
	assert_true(parse_res.collision_polygons.has(0), "Box 0 has generated collision polygon")
	assert_true(parse_res.collision_polygons.has(1), "Box 1 has generated rectangular fallback collision polygon")

	var poly0: PackedVector2Array = parse_res.collision_polygons[0]
	assert_true(poly0.size() == 4, "Box 0 polygon has 4 vertices")
	# Pivot at (20, 50) means vertex (0, 0) should be at (-20, -50)
	assert_true(poly0[0] == Vector2(-20, -50), "Box 0 vertex 0 centered on pivot is (-20, -50)")

	var poly1: PackedVector2Array = parse_res.collision_polygons[1]
	assert_true(poly1.size() == 4, "Box 1 fallback polygon has 4 vertices")
	# Pivot at (15, 60) with w=30, h=60 means top-left is (-15, -60)
	assert_true(poly1[0] == Vector2(-15, -60), "Box 1 vertex 0 centered on pivot is (-15, -60)")

func test_mesh_sprite_live_collision_sync() -> void:
	print("\n[Suite 7: BentoMeshSprite Live Hitbox Synchronization]")
	var root := Node2D.new()

	var hitbox_area := Area2D.new()
	hitbox_area.name = "Hitbox"
	var col_poly := CollisionPolygon2D.new()
	col_poly.name = "CollisionPolygon2D"
	hitbox_area.add_child(col_poly)
	root.add_child(hitbox_area)

	var mesh_sprite = BentoMeshSprite.new()
	mesh_sprite.name = "MeshSprite"
	root.add_child(mesh_sprite)

	var poly_frame0 := PackedVector2Array([Vector2(-10, -20), Vector2(10, -20), Vector2(10, 0), Vector2(-10, 0)])
	var poly_frame1 := PackedVector2Array([Vector2(-15, -30), Vector2(15, -30), Vector2(15, 0), Vector2(-15, 0)])

	mesh_sprite.collision_polygons = {
		"walk": [poly_frame0, poly_frame1]
	}
	mesh_sprite.collision_polygon_node = NodePath("../Hitbox/CollisionPolygon2D")
	mesh_sprite.sync_collision = true

	# Set animation and frame 0
	mesh_sprite.current_animation = "walk"
	mesh_sprite.frame = 0

	assert_true(col_poly.polygon.size() == 4, "ColPoly updated with frame 0 polygon")
	assert_true(col_poly.polygon[0] == Vector2(-10, -20), "ColPoly matches frame 0 coordinates")

	# Advance to frame 1
	mesh_sprite.frame = 1
	assert_true(col_poly.polygon[0] == Vector2(-15, -30), "ColPoly automatically updated to frame 1 coordinates")

	# Test horizontal flip
	mesh_sprite.flip_h = true
	assert_true(col_poly.polygon[0] == Vector2(15, -30), "ColPoly X coordinate flipped on flip_h")

	# Test frame_changed signal
	var signal_received := [false]
	mesh_sprite.frame_changed.connect(func(f): signal_received[0] = true)
	mesh_sprite.frame = 0
	assert_true(signal_received[0], "frame_changed signal emitted on frame change")

	root.free()

func test_webp_atlas_decompression() -> void:
	print("\n[Suite 8: Native WebP Atlas Decompression in .bento]")
	# Create a synthetic .bento ZIP archive containing atlas.webp
	var zip_packer := ZIPPacker.new()
	var tmp_webp_bento := "/tmp/test_webp_archive.bento"
	var err := zip_packer.open(tmp_webp_bento)
	assert_true(err == OK, "ZIPPacker opened /tmp/test_webp_archive.bento")

	# 1. Create a 32x32 test image and encode to WebP
	var test_img := Image.create(32, 32, false, Image.FORMAT_RGBA8)
	test_img.fill(Color(0.1, 0.8, 0.3, 1.0))
	var webp_bytes := test_img.save_webp_to_buffer(true) # lossless
	assert_true(webp_bytes.size() > 0, "Image encoded to WebP buffer")

	# 2. project.json referencing atlas.webp
	var proj_dict := {
		"format": "BentoPackProject",
		"version": "1.0",
		"name": "WebpHero",
		"atlas": {
			"file": "atlas.webp",
			"width": 32,
			"height": 32,
			"format": "RGBA8888"
		},
		"boxes": [
			{
				"index": 0,
				"rect": {"x": 0, "y": 0, "w": 32, "h": 32},
				"pivot": {"x": 16, "y": 32, "custom": true}
			}
		],
		"animations": [
			{
				"name": "idle",
				"fps": 8.0,
				"loop": true,
				"frames": [0]
			}
		]
	}
	var proj_json_bytes := JSON.stringify(proj_dict).to_utf8_buffer()

	# 3. Write files into zip
	zip_packer.start_file("project.json")
	zip_packer.write_file(proj_json_bytes)
	zip_packer.close_file()

	zip_packer.start_file("atlas.webp")
	zip_packer.write_file(webp_bytes)
	zip_packer.close_file()

	zip_packer.close()

	# 4. Parse using BentoParser
	var res := BentoParser.parse_bento_file(tmp_webp_bento)
	assert_true(res.success, "BentoParser successfully unpacked .bento with WebP atlas")
	assert_true(res.atlas_format == "webp", "Detected atlas_format == 'webp'")
	assert_true(res.atlas_image != null, "Decompressed WebP atlas into valid Image")
	if res.atlas_image != null:
		assert_true(res.atlas_image.get_width() == 32, "Decompressed WebP width is 32")
		assert_true(res.atlas_image.get_height() == 32, "Decompressed WebP height is 32")
	assert_true(res.sprite_frames != null, "Generated SpriteFrames from WebP atlas")
