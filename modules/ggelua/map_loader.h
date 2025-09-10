/**************************************************************************/
/*  map_loader.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "ggelua_core.h"

class MapLoader : public RefCounted {
	GDCLASS(MapLoader, RefCounted);

private:
	struct MaskInfo {
		int32_t x, y;
		uint32_t width, height;
		uint32_t offset;
		uint32_t size;
	};
	
	Ref<FileAccess> file;
	MapHeader header;
	Vector<uint32_t> map_offsets;  // 地表偏移列表
	Vector<uint32_t> mask_offsets; // 遮罩偏移列表 (M1.0)
	
	uint32_t row_count = 0;    // 行数
	uint32_t col_count = 0;    // 列数
	uint32_t map_count = 0;    // 地图块数量
	uint32_t mask_count = 0;   // 遮罩数量
	
	String file_path;
	bool is_loaded = false;
	
	// JPEG头部数据 (MAPX格式)
	PackedByteArray jpeg_header;

protected:
	static void _bind_methods();
	
	Ref<Image> decode_map_block(uint32_t map_id);
	PackedByteArray fix_jpeg_format(const PackedByteArray &input_data);

public:
	MapLoader();
	~MapLoader();

	Error open(const String &p_path);
	void close();
	
	Ref<Image> get_map_tile(int map_id);
	Array get_mask_info(int map_id);
	Ref<Image> get_mask_image(int map_id, int mask_index);
	PackedByteArray get_cell_data();
	Dictionary get_map_block_data(int map_id);
	
	int get_row_count() const;
	int get_col_count() const; 
	int get_map_count() const;
	int get_mask_count() const;
	Vector2i get_map_size() const;
	String get_map_format() const;
	String get_file_path() const;
};