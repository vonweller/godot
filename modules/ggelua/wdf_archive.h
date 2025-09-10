/**************************************************************************/
/*  wdf_archive.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/io/file_access.h"
#include "tcp_loader.h"
#include "ggelua_core.h"

class WDFArchive : public RefCounted {
	GDCLASS(WDFArchive, RefCounted);

private:
	Ref<FileAccess> file;
	WDFHead header;
	Vector<WDFFileInfo> file_list;
	String file_path;
	bool is_loaded = false;

protected:
	static void _bind_methods();

public:
	WDFArchive();
	~WDFArchive();

	Error open(const String &p_path);
	void close();
	
	PackedByteArray get_file_data(int p_index) const;
	PackedByteArray get_file_data_by_hash(uint32_t p_hash) const;
	PackedByteArray get_file_header(int p_index, int p_size = 4) const;
	
	Ref<TCPLoader> get_tcp_file(int p_index) const;
	Ref<TCPLoader> get_tcp_file_by_hash(uint32_t p_hash) const;
	
	int get_file_count() const;
	Array get_file_list() const;
	Dictionary get_file_info(int p_index) const;
	
	int find_file_by_hash(uint32_t p_hash) const;
	String get_file_path() const;
};