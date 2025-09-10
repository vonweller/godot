/**************************************************************************/
/*  tcp_loader_new.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "tcp_loader.h"
#include "core/io/file_access.h"
#include "core/error/error_macros.h"

void TCPLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &TCPLoader::load_from_file);
	ClassDB::bind_method(D_METHOD("load_from_buffer", "buffer"), &TCPLoader::load_from_buffer);
	ClassDB::bind_method(D_METHOD("get_frame", "frame_id"), &TCPLoader::get_frame);
	ClassDB::bind_method(D_METHOD("get_frame_info", "frame_id"), &TCPLoader::get_frame_info);
	ClassDB::bind_method(D_METHOD("get_group_count"), &TCPLoader::get_group_count);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &TCPLoader::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_total_frames"), &TCPLoader::get_total_frames);
	ClassDB::bind_method(D_METHOD("get_size"), &TCPLoader::get_size);
	ClassDB::bind_method(D_METHOD("get_key_point"), &TCPLoader::get_key_point);
	ClassDB::bind_method(D_METHOD("get_dts_data"), &TCPLoader::get_dts_data);
	ClassDB::bind_method(D_METHOD("set_palette_transform", "start", "end", "r_transform", "g_transform", "b_transform"), &TCPLoader::set_palette_transform);
}

TCPLoader::TCPLoader() {
	palette.resize(256);
}

TCPLoader::~TCPLoader() {
}

Error TCPLoader::load_from_file(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_OPEN, "Cannot open TCP file: " + p_path);
	
	PackedByteArray buffer = file->get_buffer(file->get_length());
	file->close();
	
	return load_from_buffer(buffer);
}

Error TCPLoader::load_from_buffer(const PackedByteArray &p_buffer) {
	ERR_FAIL_COND_V(p_buffer.size() < sizeof(TCPHead), ERR_INVALID_DATA);
	
	tcp_data = p_buffer;
	const uint8_t *data = tcp_data.ptr();
	
	// 读取头部
	memcpy(&header, data, sizeof(TCPHead));
	
	// 验证格式
	if (header.flag != 0x5053 && header.flag != 0x5052) { // 'PS' or 'PR'
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid TCP file format");
	}
	
	data += 16; // 跳过完整头部
	
	if (header.flag == 0x5053) { // 'PS' 格式
		// 读取DTS数据
		int dts_len = header.len - 12;
		dts_data.resize(dts_len);
		for (int i = 0; i < dts_len; i++) {
			dts_data.write[i] = *data++;
		}
		
		// 读取调色板
		const uint16_t *pal16 = (const uint16_t *)data;
		for (int i = 0; i < 256; i++) {
			palette.write[i] = GGELUACore::rgb565_to_888(pal16[i], 255);
		}
		data += 512;
		
		// 读取帧偏移表
		total_frames = header.group * header.frame;
		frame_offsets.resize(total_frames);
		const uint32_t *offsets = (const uint32_t *)data;
		for (uint32_t i = 0; i < total_frames; i++) {
			frame_offsets.write[i] = offsets[i];
			if (frame_offsets[i] != 0) {
				frame_offsets.write[i] += (header.len + 4); // 添加偏移
			}
		}
	}
	
	is_loaded = true;
	return OK;
}

Ref<Image> TCPLoader::get_frame(int p_frame_id) const {
	ERR_FAIL_COND_V(!is_loaded, Ref<Image>());
	ERR_FAIL_COND_V(p_frame_id < 0 || p_frame_id >= (int)total_frames, Ref<Image>());
	
	if (header.flag != 0x5053) { // 只支持PS格式
		return Ref<Image>();
	}
	
	if (frame_offsets[p_frame_id] == 0) {
		return Ref<Image>(); // 空帧
	}
	
	const uint8_t *data = tcp_data.ptr() + frame_offsets[p_frame_id];
	TCPFrameInfo frame_info;
	
	// 创建输出图像数据
	PackedByteArray image_data;
	image_data.resize(1024 * 1024 * 4); // 预分配足够大的空间
	
	// 使用核心解码函数
	if (GGELUACore::decode_tcp_frame(data, tcp_data.size() - frame_offsets[p_frame_id], 
		palette.ptr(), frame_info, image_data.ptrw())) {
		
		// 调整数据大小
		image_data.resize(frame_info.width * frame_info.height * 4);
		
		Ref<Image> image = Image::create_from_data(frame_info.width, frame_info.height, 
			false, Image::FORMAT_RGBA8, image_data);
		return image;
	}
	
	return Ref<Image>();
}

Dictionary TCPLoader::get_frame_info(int p_frame_id) const {
	Dictionary info;
	ERR_FAIL_COND_V(!is_loaded, info);
	ERR_FAIL_COND_V(p_frame_id < 0 || p_frame_id >= (int)total_frames, info);
	
	if (frame_offsets[p_frame_id] == 0) {
		return info; // 空帧
	}
	
	const uint8_t *data = tcp_data.ptr() + frame_offsets[p_frame_id];
	const TCPFrameInfo *frame_info = (const TCPFrameInfo *)data;
	
	info["x"] = frame_info->x;
	info["y"] = frame_info->y;
	info["width"] = frame_info->width;
	info["height"] = frame_info->height;
	
	return info;
}

int TCPLoader::get_group_count() const {
	return is_loaded ? header.group : 0;
}

int TCPLoader::get_frame_count() const {
	return is_loaded ? header.frame : 0;
}

int TCPLoader::get_total_frames() const {
	return is_loaded ? total_frames : 0;
}

Vector2i TCPLoader::get_size() const {
	return is_loaded ? Vector2i(header.width, header.height) : Vector2i();
}

Vector2i TCPLoader::get_key_point() const {
	return is_loaded ? Vector2i(header.x, header.y) : Vector2i();
}

PackedByteArray TCPLoader::get_dts_data() const {
	return dts_data;
}

void TCPLoader::set_palette_transform(int start, int end, const Vector3 &r_transform, const Vector3 &g_transform, const Vector3 &b_transform) {
	ERR_FAIL_COND(!is_loaded);
	ERR_FAIL_COND(start < 0 || end > 256);
	
	// 重新读取原始调色板
	const uint8_t *data = tcp_data.ptr() + 16 + (header.len - 12);
	const uint16_t *pal16 = (const uint16_t *)data;
	
	for (int i = start; i < end; i++) {
		uint16_t color16 = pal16[i];
		
		uint32_t r1 = (uint32_t)(r_transform.x * 256);
		uint32_t g1 = (uint32_t)(r_transform.y * 256);
		uint32_t b1 = (uint32_t)(r_transform.z * 256);
		uint32_t r2 = (uint32_t)(g_transform.x * 256);
		uint32_t g2 = (uint32_t)(g_transform.y * 256);
		uint32_t b2 = (uint32_t)(g_transform.z * 256);
		uint32_t r3 = (uint32_t)(b_transform.x * 256);
		uint32_t g3 = (uint32_t)(b_transform.y * 256);
		uint32_t b3 = (uint32_t)(b_transform.z * 256);
		
		palette.write[i] = GGELUACore::rgb565_to_888_transform(color16,
			r1, g1, b1, r2, g2, b2, r3, g3, b3);
	}
}