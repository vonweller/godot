#pragma once

#include "core/io/image.h"
#include "core/math/rect2.h"
#include "core/object/object.h"
#include "core/variant/variant.h"

class MHXY : public Object {
	GDCLASS(MHXY, Object);

protected:
	static void _bind_methods();

public:
	String get_module_name() const;
	Ref<Image> load_was_frame(const String &p_path, int64_t p_hash, int p_frame = 0) const;
	Dictionary load_was_sprite(const String &p_path, int64_t p_hash, int p_direction = 0) const;
	Dictionary load_was_frame_info(const String &p_path, int64_t p_hash, int p_frame) const;
	// 一个方向的全部帧（图 + key），只打开一次 WDF。
	Array load_was_direction(const String &p_path, int64_t p_hash, int p_direction = 0) const;
	Dictionary load_map_info(const String &p_path) const;
	Ref<Image> load_map_tile(const String &p_path, int p_tile) const;
	PackedByteArray load_map_cell(const String &p_path, int p_tile) const;
	Array load_map_masks(const String &p_path, int p_tile) const;
	// 出生点周围要加载的地图块序号。margin 是块数，不是像素。
	PackedInt32Array map_tiles_in_view(const String &p_path, const Vector2 &p_origin, int p_margin = 4) const;
	// 点在矩形内的下标。世界里用它做 AOI，远处 NPC 不解码、不播动画。
	PackedInt32Array query_aoi(const PackedVector2Array &p_points, const Rect2 &p_view) const;
	Dictionary cache_stats() const;
	void clear_resource_cache();
};
