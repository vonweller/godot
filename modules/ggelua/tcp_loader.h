/**************************************************************************/
/*  tcp_loader_new.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/io/image.h"
#include "core/variant/typed_array.h"
#include "ggelua_core.h"

class TCPLoader : public RefCounted {
	GDCLASS(TCPLoader, RefCounted);

private:
	PackedByteArray tcp_data;
	bool is_loaded = false;
	
	TCPHead header;
	Vector<uint8_t> dts_data;
	Vector<uint32_t> palette;
	Vector<uint32_t> frame_offsets;
	uint32_t total_frames = 0;

protected:
	static void _bind_methods();

public:
	TCPLoader();
	~TCPLoader();

	Error load_from_buffer(const PackedByteArray &p_buffer);
	Error load_from_file(const String &p_path);
	
	Ref<Image> get_frame(int p_frame_id) const;
	Dictionary get_frame_info(int p_frame_id) const;
	
	int get_group_count() const;
	int get_frame_count() const; 
	int get_total_frames() const;
	Vector2i get_size() const;
	Vector2i get_key_point() const;
	
	PackedByteArray get_dts_data() const;
	void set_palette_transform(int start, int end, const Vector3 &r_transform, const Vector3 &g_transform, const Vector3 &b_transform);
};