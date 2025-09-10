/**************************************************************************/
/*  wdf_archive.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "wdf_archive.h"
#include "core/error/error_macros.h"

void WDFArchive::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path"), &WDFArchive::open);
	ClassDB::bind_method(D_METHOD("close"), &WDFArchive::close);
	ClassDB::bind_method(D_METHOD("get_file_data", "index"), &WDFArchive::get_file_data);
	ClassDB::bind_method(D_METHOD("get_file_data_by_hash", "hash"), &WDFArchive::get_file_data_by_hash);
	ClassDB::bind_method(D_METHOD("get_file_header", "index", "size"), &WDFArchive::get_file_header, DEFVAL(4));
	ClassDB::bind_method(D_METHOD("get_tcp_file", "index"), &WDFArchive::get_tcp_file);
	ClassDB::bind_method(D_METHOD("get_tcp_file_by_hash", "hash"), &WDFArchive::get_tcp_file_by_hash);
	ClassDB::bind_method(D_METHOD("get_file_count"), &WDFArchive::get_file_count);
	ClassDB::bind_method(D_METHOD("get_file_list"), &WDFArchive::get_file_list);
	ClassDB::bind_method(D_METHOD("get_file_info", "index"), &WDFArchive::get_file_info);
	ClassDB::bind_method(D_METHOD("find_file_by_hash", "hash"), &WDFArchive::find_file_by_hash);
	ClassDB::bind_method(D_METHOD("get_file_path"), &WDFArchive::get_file_path);
}

WDFArchive::WDFArchive() {
}

WDFArchive::~WDFArchive() {
	close();
}

Error WDFArchive::open(const String &p_path) {
	close();
	
	file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_OPEN, "Cannot open WDF file: " + p_path);
	
	// 读取头部
	Vector<uint8_t> header_data = file->get_buffer(sizeof(WDFHead));
	if (header_data.size() != sizeof(WDFHead)) {
		close();
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Cannot read WDF header");
	}
	memcpy(&header, header_data.ptr(), sizeof(WDFHead));
	
	// 验证格式
	if (header.flag != 0x50464457) { // 'WDFP'
		close();
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid WDF file format");
	}
	
	// 读取文件列表
	file->seek(header.offset);
	file_list.resize(header.number);
	
	for (uint32_t i = 0; i < header.number; i++) {
		Vector<uint8_t> file_info_data = file->get_buffer(sizeof(WDFFileInfo));
		if (file_info_data.size() != sizeof(WDFFileInfo)) {
			close();
			ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Cannot read WDF file list");
		}
		memcpy(&file_list.write[i], file_info_data.ptr(), sizeof(WDFFileInfo));
	}
	
	file_path = p_path;
	is_loaded = true;
	return OK;
}

void WDFArchive::close() {
	if (file.is_valid()) {
		file->close();
		file.unref();
	}
	file_list.clear();
	is_loaded = false;
}

PackedByteArray WDFArchive::get_file_data(int p_index) const {
	ERR_FAIL_COND_V(!is_loaded, PackedByteArray());
	ERR_FAIL_COND_V(p_index < 0 || p_index >= (int)header.number, PackedByteArray());
	
	const WDFFileInfo &info = file_list[p_index];
	file->seek(info.offset);
	
	return file->get_buffer(info.size);
}

PackedByteArray WDFArchive::get_file_data_by_hash(uint32_t p_hash) const {
	int index = find_file_by_hash(p_hash);
	if (index >= 0) {
		return get_file_data(index);
	}
	return PackedByteArray();
}

PackedByteArray WDFArchive::get_file_header(int p_index, int p_size) const {
	ERR_FAIL_COND_V(!is_loaded, PackedByteArray());
	ERR_FAIL_COND_V(p_index < 0 || p_index >= (int)header.number, PackedByteArray());
	ERR_FAIL_COND_V(p_size <= 0, PackedByteArray());
	
	const WDFFileInfo &info = file_list[p_index];
	file->seek(info.offset);
	
	return file->get_buffer(MIN(p_size, (int)info.size));
}

Ref<TCPLoader> WDFArchive::get_tcp_file(int p_index) const {
	PackedByteArray data = get_file_data(p_index);
	if (data.size() == 0) {
		return Ref<TCPLoader>();
	}
	
	Ref<TCPLoader> tcp;
	tcp.instantiate();
	
	Error err = tcp->load_from_buffer(data);
	if (err != OK) {
		return Ref<TCPLoader>();
	}
	
	return tcp;
}

Ref<TCPLoader> WDFArchive::get_tcp_file_by_hash(uint32_t p_hash) const {
	int index = find_file_by_hash(p_hash);
	if (index >= 0) {
		return get_tcp_file(index);
	}
	return Ref<TCPLoader>();
}

int WDFArchive::get_file_count() const {
	return is_loaded ? header.number : 0;
}

Array WDFArchive::get_file_list() const {
	Array result;
	if (!is_loaded) {
		return result;
	}
	
	for (uint32_t i = 0; i < header.number; i++) {
		Dictionary info;
		info["index"] = i;
		info["hash"] = file_list[i].hash;
		info["offset"] = file_list[i].offset;
		info["size"] = file_list[i].size;
		info["unused"] = file_list[i].unused;
		result.push_back(info);
	}
	
	return result;
}

Dictionary WDFArchive::get_file_info(int p_index) const {
	Dictionary info;
	ERR_FAIL_COND_V(!is_loaded, info);
	ERR_FAIL_COND_V(p_index < 0 || p_index >= (int)header.number, info);
	
	const WDFFileInfo &file_info = file_list[p_index];
	info["index"] = p_index;
	info["hash"] = file_info.hash;
	info["offset"] = file_info.offset;
	info["size"] = file_info.size;
	info["unused"] = file_info.unused;
	
	return info;
}

int WDFArchive::find_file_by_hash(uint32_t p_hash) const {
	if (!is_loaded) {
		return -1;
	}
	
	for (uint32_t i = 0; i < header.number; i++) {
		if (file_list[i].hash == p_hash) {
			return i;
		}
	}
	
	return -1;
}

String WDFArchive::get_file_path() const {
	return file_path;
}