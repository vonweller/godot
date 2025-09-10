extends Control

# TCP动画查看器示例

@onready var file_dialog = $VBoxContainer/FileDialog
@onready var animation_info = $VBoxContainer/InfoPanel/AnimationInfo
@onready var frame_display = $VBoxContainer/DisplayPanel/FrameDisplay
@onready var frame_slider = $VBoxContainer/ControlPanel/FrameSlider
@onready var frame_label = $VBoxContainer/ControlPanel/FrameLabel
@onready var play_button = $VBoxContainer/ControlPanel/PlayButton
@onready var group_option = $VBoxContainer/ControlPanel/GroupOption

var tcp_loader: TCPLoader
var current_group = 0
var current_frame = 0
var is_playing = false
var play_timer: Timer

func _ready():
	# 初始化播放计时器
	play_timer = Timer.new()
	play_timer.timeout.connect(_on_timer_timeout)
	add_child(play_timer)
	
	# 连接信号
	$VBoxContainer/FilePanel/LoadButton.pressed.connect(_on_load_button_pressed)
	file_dialog.file_selected.connect(_on_file_selected)
	frame_slider.value_changed.connect(_on_frame_changed)
	play_button.pressed.connect(_on_play_button_pressed)
	group_option.item_selected.connect(_on_group_selected)

func _on_load_button_pressed():
	file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_dialog.add_filter("*.tcp", "TCP动画文件")
	file_dialog.popup_centered(Vector2i(800, 600))

func _on_file_selected(path: String):
	tcp_loader = TCPLoader.new()
	
	if tcp_loader.load_from_file(path) == OK:
		_update_animation_info()
		_setup_controls()
		_display_current_frame()
		print("成功加载TCP文件: ", path)
	else:
		print("加载TCP文件失败: ", path)

func _update_animation_info():
	if not tcp_loader:
		return
	
	var info_text = ""
	info_text += "动画组数: %d\n" % tcp_loader.get_group_count()
	info_text += "每组帧数: %d\n" % tcp_loader.get_frame_count()
	info_text += "总帧数: %d\n" % tcp_loader.get_total_frames()
	info_text += "尺寸: %s\n" % str(tcp_loader.get_size())
	info_text += "关键点: %s\n" % str(tcp_loader.get_key_point())
	
	animation_info.text = info_text

func _setup_controls():
	if not tcp_loader:
		return
	
	# 设置帧滑块
	frame_slider.min_value = 0
	frame_slider.max_value = tcp_loader.get_frame_count() - 1
	frame_slider.value = 0
	
	# 设置组选择
	group_option.clear()
	for i in tcp_loader.get_group_count():
		group_option.add_item("组 %d" % i, i)
	
	current_group = 0
	current_frame = 0

func _display_current_frame():
	if not tcp_loader:
		return
	
	var frame_id = current_group * tcp_loader.get_frame_count() + current_frame
	var frame_image = tcp_loader.get_frame(frame_id)
	
	if frame_image:
		var texture = ImageTexture.create_from_image(frame_image)
		frame_display.texture = texture
		
		# 显示帧信息
		var frame_info = tcp_loader.get_frame_info(frame_id)
		frame_label.text = "帧 %d/%d (组 %d) - 位置: (%d,%d) 尺寸: %dx%d" % [
			current_frame + 1, tcp_loader.get_frame_count(), current_group,
			frame_info.get("x", 0), frame_info.get("y", 0),
			frame_info.get("width", 0), frame_info.get("height", 0)
		]
	else:
		frame_display.texture = null
		frame_label.text = "帧 %d/%d (组 %d) - 空帧" % [
			current_frame + 1, tcp_loader.get_frame_count(), current_group
		]

func _on_frame_changed(value: float):
	current_frame = int(value)
	_display_current_frame()

func _on_group_selected(index: int):
	current_group = index
	_display_current_frame()

func _on_play_button_pressed():
	if not tcp_loader:
		return
	
	is_playing = not is_playing
	play_button.text = "暂停" if is_playing else "播放"
	
	if is_playing:
		play_timer.wait_time = 0.1  # 100ms per frame
		play_timer.start()
	else:
		play_timer.stop()

func _on_timer_timeout():
	if not is_playing or not tcp_loader:
		return
	
	current_frame = (current_frame + 1) % tcp_loader.get_frame_count()
	frame_slider.value = current_frame
	_display_current_frame()

# 调色板变换示例
func _on_color_transform_pressed():
	if not tcp_loader:
		return
	
	# 应用红色调色板变换
	var r_transform = Vector3(1.2, 0.0, 0.0)  # 增强红色
	var g_transform = Vector3(0.0, 0.8, 0.0)  # 减弱绿色
	var b_transform = Vector3(0.0, 0.0, 0.8)  # 减弱蓝色
	
	tcp_loader.set_palette_transform(0, 256, r_transform, g_transform, b_transform)
	_display_current_frame()
	print("应用调色板变换")

func _input(event):
	if not tcp_loader:
		return
	
	if event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_LEFT:
				current_frame = max(0, current_frame - 1)
				frame_slider.value = current_frame
				_display_current_frame()
			KEY_RIGHT:
				current_frame = min(tcp_loader.get_frame_count() - 1, current_frame + 1)
				frame_slider.value = current_frame
				_display_current_frame()
			KEY_SPACE:
				_on_play_button_pressed()