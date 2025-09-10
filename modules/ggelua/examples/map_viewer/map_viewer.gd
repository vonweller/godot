extends Control

# 地图查看器示例

@onready var file_dialog = $VBoxContainer/FileDialog
@onready var map_info = $VBoxContainer/InfoPanel/MapInfo
@onready var tile_display = $VBoxContainer/DisplayPanel/ScrollContainer/TileDisplay
@onready var tile_grid = $VBoxContainer/DisplayPanel/ScrollContainer/TileDisplay/TileGrid
@onready var coordinates_label = $VBoxContainer/ControlPanel/CoordinatesLabel

var map_loader: MapLoader
var tile_textures: Array[ImageTexture] = []

func _ready():
	# 连接信号
	$VBoxContainer/FilePanel/LoadButton.pressed.connect(_on_load_button_pressed)
	file_dialog.file_selected.connect(_on_file_selected)
	$VBoxContainer/ControlPanel/RefreshButton.pressed.connect(_on_refresh_button_pressed)

func _on_load_button_pressed():
	file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_dialog.add_filter("*.map", "地图文件")
	file_dialog.popup_centered(Vector2i(800, 600))

func _on_file_selected(path: String):
	map_loader = MapLoader.new()
	
	if map_loader.open(path) == OK:
		_update_map_info()
		_setup_tile_grid()
		_load_map_tiles()
		print("成功加载地图文件: ", path)
	else:
		print("加载地图文件失败: ", path)

func _update_map_info():
	if not map_loader:
		return
	
	var info_text = ""
	info_text += "地图格式: %s\n" % map_loader.get_map_format()
	info_text += "地图尺寸: %s\n" % str(map_loader.get_map_size())
	info_text += "行数: %d\n" % map_loader.get_row_count()
	info_text += "列数: %d\n" % map_loader.get_col_count()
	info_text += "地图块数: %d\n" % map_loader.get_map_count()
	info_text += "遮罩数: %d\n" % map_loader.get_mask_count()
	
	map_info.text = info_text

func _setup_tile_grid():
	if not map_loader:
		return
	
	# 清除现有的子节点
	for child in tile_grid.get_children():
		child.queue_free()
	
	# 设置网格列数
	tile_grid.columns = map_loader.get_col_count()

func _load_map_tiles():
	if not map_loader:
		return
	
	tile_textures.clear()
	
	# 异步加载地图块以避免界面卡顿
	for i in range(map_loader.get_map_count()):
		_load_single_tile(i)
		
		# 每加载10个块暂停一帧
		if i % 10 == 9:
			await get_tree().process_frame

func _load_single_tile(tile_id: int):
	# 创建地图块显示节点
	var tile_button = Button.new()
	tile_button.custom_minimum_size = Vector2(64, 48) # 缩放显示
	tile_button.pressed.connect(_on_tile_clicked.bind(tile_id))
	
	# 加载地图块图像
	var tile_image = map_loader.get_map_tile(tile_id)
	if tile_image:
		# 缩放图像以适应显示
		tile_image.resize(64, 48, Image.INTERPOLATE_LANCZOS)
		var texture = ImageTexture.create_from_image(tile_image)
		tile_button.icon = texture
		tile_textures.push_back(texture)
	else:
		# 创建占位符纹理
		var placeholder = Image.create(64, 48, false, Image.FORMAT_RGB8)
		placeholder.fill(Color.DARK_GRAY)
		var texture = ImageTexture.create_from_image(placeholder)
		tile_button.icon = texture
		tile_textures.push_back(texture)
	
	# 添加到网格
	tile_grid.add_child(tile_button)
	
	# 更新坐标显示
	var row = tile_id / map_loader.get_col_count()
	var col = tile_id % map_loader.get_col_count()
	tile_button.tooltip_text = "块 %d (行:%d 列:%d)" % [tile_id, row, col]

func _on_tile_clicked(tile_id: int):
	if not map_loader:
		return
	
	# 显示详细的地图块信息
	_show_tile_details(tile_id)

func _show_tile_details(tile_id: int):
	var row = tile_id / map_loader.get_col_count()
	var col = tile_id % map_loader.get_col_count()
	
	var details = "地图块详情\n"
	details += "ID: %d\n" % tile_id
	details += "位置: 行 %d, 列 %d\n" % [row, col]
	details += "像素坐标: (%d, %d)\n" % [col * 320, row * 240]
	
	# 获取遮罩信息
	var mask_info = map_loader.get_mask_info(tile_id)
	details += "遮罩数量: %d\n" % mask_info.size()
	
	# 获取原始块数据信息
	var block_data = map_loader.get_map_block_data(tile_id)
	if block_data.has("blocks"):
		details += "数据块数量: %d\n" % block_data.blocks.size()
		for i in range(block_data.blocks.size()):
			var block = block_data.blocks[i]
			details += "  块 %d: %s (%d bytes)\n" % [i, block.flag, block.size]
	
	coordinates_label.text = details
	
	# 创建详情窗口
	_create_detail_window(tile_id)

func _create_detail_window(tile_id: int):
	var detail_window = Window.new()
	detail_window.title = "地图块 %d 详情" % tile_id
	detail_window.size = Vector2i(600, 400)
	
	var vbox = VBoxContainer.new()
	detail_window.add_child(vbox)
	
	# 显示原始尺寸的地图块
	var tile_image = map_loader.get_map_tile(tile_id)
	if tile_image:
		var texture_rect = TextureRect.new()
		texture_rect.texture = ImageTexture.create_from_image(tile_image)
		texture_rect.expand_mode = TextureRect.EXPAND_FIT_WIDTH_PROPORTIONAL
		vbox.add_child(texture_rect)
	
	# 显示详细信息
	var info_label = Label.new()
	info_label.text = coordinates_label.text
	info_label.vertical_alignment = VERTICAL_ALIGNMENT_TOP
	vbox.add_child(info_label)
	
	# 添加关闭按钮
	var close_button = Button.new()
	close_button.text = "关闭"
	close_button.pressed.connect(detail_window.queue_free)
	vbox.add_child(close_button)
	
	get_tree().current_scene.add_child(detail_window)
	detail_window.popup_centered()

func _on_refresh_button_pressed():
	if map_loader:
		_load_map_tiles()

# 导出功能
func _on_export_map_pressed():
	if not map_loader:
		return
	
	var export_dialog = FileDialog.new()
	export_dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	export_dialog.add_filter("*.png", "PNG图像")
	add_child(export_dialog)
	export_dialog.popup_centered(Vector2i(800, 600))
	
	var result = await export_dialog.file_selected
	if result:
		_export_full_map(result)
	
	export_dialog.queue_free()

func _export_full_map(output_path: String):
	if not map_loader:
		return
	
	# 创建完整地图图像
	var map_size = map_loader.get_map_size()
	var full_image = Image.create(map_size.x, map_size.y, false, Image.FORMAT_RGB8)
	
	# 拼接所有地图块
	for row in range(map_loader.get_row_count()):
		for col in range(map_loader.get_col_count()):
			var tile_id = row * map_loader.get_col_count() + col
			var tile_image = map_loader.get_map_tile(tile_id)
			
			if tile_image:
				var dst_rect = Rect2i(col * 320, row * 240, 320, 240)
				full_image.blit_rect(tile_image, Rect2i(0, 0, 320, 240), dst_rect.position)
	
	# 保存图像
	full_image.save_png(output_path)
	print("完整地图已导出到: ", output_path)

# 获取障碍数据
func _on_get_cell_data_pressed():
	if not map_loader:
		return
	
	var cell_data = map_loader.get_cell_data()
	print("障碍数据大小: ", cell_data.size(), " bytes")
	
	# 可以在这里处理障碍数据，例如可视化显示
	# 障碍数据格式: 每个像素一个字节，0=可通行，1=障碍，2=特殊区域