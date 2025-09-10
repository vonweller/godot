extends Control

# WDF资源包浏览器示例

@onready var file_dialog = $VBoxContainer/FileDialog
@onready var file_list = $VBoxContainer/MainPanel/FileList
@onready var preview_panel = $VBoxContainer/MainPanel/PreviewPanel
@onready var info_label = $VBoxContainer/InfoPanel/InfoLabel

var wdf_archive: WDFArchive
var current_files: Array = []

func _ready():
	# 连接信号
	$VBoxContainer/FilePanel/OpenButton.pressed.connect(_on_open_button_pressed)
	file_dialog.file_selected.connect(_on_file_selected)
	file_list.item_selected.connect(_on_file_item_selected)
	$VBoxContainer/FilePanel/SearchEdit.text_changed.connect(_on_search_changed)

func _on_open_button_pressed():
	file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_dialog.add_filter("*.wdf", "WDF资源包文件")
	file_dialog.popup_centered(Vector2i(800, 600))

func _on_file_selected(path: String):
	wdf_archive = WDFArchive.new()
	
	if wdf_archive.open(path) == OK:
		_load_file_list()
		info_label.text = "WDF文件: %s | 文件数量: %d" % [path, wdf_archive.get_file_count()]
		print("成功打开WDF文件: ", path)
	else:
		print("打开WDF文件失败: ", path)

func _load_file_list():
	if not wdf_archive:
		return
	
	file_list.clear()
	current_files = wdf_archive.get_file_list()
	
	for i in range(current_files.size()):
		var file_info = current_files[i]
		var item_text = "索引: %d | 哈希: %08X | 大小: %d bytes" % [
			file_info.index, file_info.hash, file_info.size
		]
		file_list.add_item(item_text)

func _on_file_item_selected(index: int):
	if not wdf_archive or index >= current_files.size():
		return
	
	var file_info = current_files[index]
	_show_file_preview(file_info.index)

func _show_file_preview(file_index: int):
	# 获取文件头部用于格式检测
	var header = wdf_archive.get_file_header(file_index, 4)
	if header.size() < 4:
		_clear_preview()
		return
	
	var format_signature = header.decode_u32(0)
	
	match format_signature:
		0x5053: # 'PS' - TCP文件
			_preview_tcp_file(file_index)
		0x5052: # 'PR' - TCP文件
			_preview_tcp_file(file_index)
		0xD8FF: # JPEG文件
			_preview_image_file(file_index, "jpg")
		0x4E50: # PNG文件
			_preview_image_file(file_index, "png")
		_:
			_preview_raw_data(file_index)

func _preview_tcp_file(file_index: int):
	var tcp = wdf_archive.get_tcp_file(file_index)
	if tcp:
		var frame = tcp.get_frame(0) # 显示第一帧
		if frame:
			var texture = ImageTexture.create_from_image(frame)
			_show_image_preview(texture)
			
			var preview_info = "TCP动画文件\n"
			preview_info += "组数: %d\n" % tcp.get_group_count()
			preview_info += "帧数: %d\n" % tcp.get_frame_count()
			preview_info += "尺寸: %s" % str(tcp.get_size())
			
			_show_preview_info(preview_info)
			return
	
	_clear_preview()

func _preview_image_file(file_index: int, format: String):
	var data = wdf_archive.get_file_data(file_index)
	if data.size() > 0:
		var image = Image.new()
		var error = OK
		
		match format:
			"jpg":
				error = image.load_jpg_from_buffer(data)
			"png":
				error = image.load_png_from_buffer(data)
		
		if error == OK:
			var texture = ImageTexture.create_from_image(image)
			_show_image_preview(texture)
			
			var preview_info = "%s 图像文件\n" % format.to_upper()
			preview_info += "尺寸: %dx%d\n" % [image.get_width(), image.get_height()]
			preview_info += "格式: %s" % Image.Format.keys()[image.get_format()]
			
			_show_preview_info(preview_info)
			return
	
	_clear_preview()

func _preview_raw_data(file_index: int):
	var data = wdf_archive.get_file_data(file_index)
	if data.size() > 0:
		var preview_text = "原始数据文件\n"
		preview_text += "大小: %d bytes\n" % data.size()
		
		# 显示前64字节的十六进制内容
		preview_text += "内容预览 (前64字节):\n"
		var preview_size = min(64, data.size())
		for i in range(0, preview_size, 16):
			var line = "%04X: " % i
			for j in range(16):
				if i + j < preview_size:
					line += "%02X " % data[i + j]
				else:
					line += "   "
			line += " "
			for j in range(16):
				if i + j < preview_size:
					var byte_val = data[i + j]
					if byte_val >= 32 and byte_val <= 126:
						line += char(byte_val)
					else:
						line += "."
				else:
					break
			preview_text += line + "\n"
		
		_show_preview_info(preview_text)
	else:
		_clear_preview()

func _show_image_preview(texture: Texture2D):
	var texture_rect = TextureRect.new()
	texture_rect.texture = texture
	texture_rect.expand_mode = TextureRect.EXPAND_FIT_WIDTH_PROPORTIONAL
	texture_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	
	_clear_preview()
	preview_panel.add_child(texture_rect)

func _show_preview_info(text: String):
	var label = Label.new()
	label.text = text
	label.vertical_alignment = VERTICAL_ALIGNMENT_TOP
	
	if preview_panel.get_child_count() == 0:
		_clear_preview()
	
	preview_panel.add_child(label)

func _clear_preview():
	for child in preview_panel.get_children():
		child.queue_free()

func _on_search_changed(text: String):
	if not wdf_archive:
		return
	
	file_list.clear()
	
	if text.is_empty():
		_load_file_list()
		return
	
	# 简单的搜索功能 - 按哈希值搜索
	if text.begins_with("0x") or text.is_valid_hex_number():
		var search_hash = text.hex_to_int() if text.begins_with("0x") else int("0x" + text)
		var found_index = wdf_archive.find_file_by_hash(search_hash)
		if found_index >= 0:
			var file_info = wdf_archive.get_file_info(found_index)
			var item_text = "索引: %d | 哈希: %08X | 大小: %d bytes" % [
				file_info.index, file_info.hash, file_info.size
			]
			file_list.add_item(item_text)

# 导出文件功能
func _on_export_button_pressed():
	if not wdf_archive or file_list.get_selected_items().is_empty():
		return
	
	var selected_index = file_list.get_selected_items()[0]
	var file_info = current_files[selected_index]
	var data = wdf_archive.get_file_data(file_info.index)
	
	if data.size() > 0:
		var export_dialog = FileDialog.new()
		export_dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE
		export_dialog.add_filter("*.*", "所有文件")
		add_child(export_dialog)
		export_dialog.popup_centered(Vector2i(800, 600))
		
		var result = await export_dialog.file_selected
		if result:
			var file = FileAccess.open(result, FileAccess.WRITE)
			if file:
				file.store_buffer(data)
				file.close()
				print("文件已导出到: ", result)
		
		export_dialog.queue_free()