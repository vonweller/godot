/**************************************************************************/
/*  gdscript_cache.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gdscript_cache.h"

#include "gdscript.h"
#include "gdscript_analyzer.h"
#include "gdscript_compiler.h"
#include "gdscript_parser.h"

#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/dir_access.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/config/project_settings.h"
#include "core/templates/vector.h"

// Global script encryption key for runtime decryption
static uint8_t script_encryption_key[32] = { 0 };

GDScriptParserRef::Status GDScriptParserRef::get_status() const {
	return status;
}

String GDScriptParserRef::get_path() const {
	return path;
}

uint32_t GDScriptParserRef::get_source_hash() const {
	return source_hash;
}

GDScriptParser *GDScriptParserRef::get_parser() {
	if (parser == nullptr) {
		parser = memnew(GDScriptParser);
	}
	return parser;
}

GDScriptAnalyzer *GDScriptParserRef::get_analyzer() {
	if (analyzer == nullptr) {
		analyzer = memnew(GDScriptAnalyzer(get_parser()));
	}
	return analyzer;
}

Error GDScriptParserRef::raise_status(Status p_new_status) {
	ERR_FAIL_COND_V(clearing, ERR_BUG);
	ERR_FAIL_COND_V(parser == nullptr && status != EMPTY, ERR_BUG);

	while (result == OK && p_new_status > status) {
		switch (status) {
			case EMPTY: {
				// Calling parse will clear the parser, which can destruct another GDScriptParserRef which can clear the last reference to the script with this path, calling remove_script, which clears this GDScriptParserRef.
				// It's ok if its the first thing done here.
				get_parser()->clear();
				status = PARSED;
				String remapped_path = ResourceLoader::path_remap(path);
				if (remapped_path.has_extension("gdc")) {
					Vector<uint8_t> tokens = GDScriptCache::get_binary_tokens(remapped_path);
					source_hash = hash_djb2_buffer(tokens.ptr(), tokens.size());
					result = get_parser()->parse_binary(tokens, path);
				} else {
					String source = GDScriptCache::get_source_code(remapped_path);
					source_hash = source.hash();
					result = get_parser()->parse(source, path, false);
				}
			} break;
			case PARSED: {
				status = INHERITANCE_SOLVED;
				result = get_analyzer()->resolve_inheritance();
			} break;
			case INHERITANCE_SOLVED: {
				status = INTERFACE_SOLVED;
				result = get_analyzer()->resolve_interface();
			} break;
			case INTERFACE_SOLVED: {
				status = FULLY_SOLVED;
				result = get_analyzer()->resolve_body();
			} break;
			case FULLY_SOLVED: {
				return result;
			}
		}
	}

	return result;
}

void GDScriptParserRef::clear() {
	if (clearing) {
		return;
	}
	clearing = true;

	GDScriptParser *lparser = parser;
	GDScriptAnalyzer *lanalyzer = analyzer;

	parser = nullptr;
	analyzer = nullptr;
	status = EMPTY;
	result = OK;
	source_hash = 0;

	clearing = false;

	if (lanalyzer != nullptr) {
		memdelete(lanalyzer);
	}

	if (lparser != nullptr) {
		memdelete(lparser);
	}
}

GDScriptParserRef::~GDScriptParserRef() {
	clear();

	if (!abandoned) {
		MutexLock lock(GDScriptCache::singleton->mutex);
		GDScriptCache::singleton->parser_map.erase(path);
	}
}

GDScriptCache *GDScriptCache::singleton = nullptr;

SafeBinaryMutex<GDScriptCache::BINARY_MUTEX_TAG> &_get_gdscript_cache_mutex() {
	return GDScriptCache::mutex;
}

template <>
thread_local SafeBinaryMutex<GDScriptCache::BINARY_MUTEX_TAG>::TLSData SafeBinaryMutex<GDScriptCache::BINARY_MUTEX_TAG>::tls_data(_get_gdscript_cache_mutex());
SafeBinaryMutex<GDScriptCache::BINARY_MUTEX_TAG> GDScriptCache::mutex;

void GDScriptCache::move_script(const String &p_from, const String &p_to) {
	if (singleton == nullptr || p_from == p_to) {
		return;
	}

	MutexLock lock(singleton->mutex);

	if (singleton->cleared) {
		return;
	}

	remove_parser(p_from);

	if (singleton->shallow_gdscript_cache.has(p_from) && !p_from.is_empty()) {
		singleton->shallow_gdscript_cache[p_to] = singleton->shallow_gdscript_cache[p_from];
	}
	singleton->shallow_gdscript_cache.erase(p_from);

	if (singleton->full_gdscript_cache.has(p_from) && !p_from.is_empty()) {
		singleton->full_gdscript_cache[p_to] = singleton->full_gdscript_cache[p_from];
	}
	singleton->full_gdscript_cache.erase(p_from);
}

void GDScriptCache::remove_script(const String &p_path) {
	if (singleton == nullptr) {
		return;
	}

	MutexLock lock(singleton->mutex);

	if (singleton->cleared) {
		return;
	}

	if (HashMap<String, Vector<ObjectID>>::Iterator E = singleton->abandoned_parser_map.find(p_path)) {
		for (ObjectID parser_ref_id : E->value) {
			Ref<GDScriptParserRef> parser_ref = { ObjectDB::get_instance(parser_ref_id) };
			if (parser_ref.is_valid()) {
				parser_ref->clear();
			}
		}
	}

	singleton->abandoned_parser_map.erase(p_path);

	if (singleton->parser_map.has(p_path)) {
		singleton->parser_map[p_path]->clear();
	}

	remove_parser(p_path);

	singleton->dependencies.erase(p_path);
	singleton->shallow_gdscript_cache.erase(p_path);
	singleton->full_gdscript_cache.erase(p_path);
}

Ref<GDScriptParserRef> GDScriptCache::get_parser(const String &p_path, GDScriptParserRef::Status p_status, Error &r_error, const String &p_owner) {
	MutexLock lock(singleton->mutex);
	Ref<GDScriptParserRef> ref;
	if (!p_owner.is_empty()) {
		singleton->dependencies[p_owner].insert(p_path);
		singleton->parser_inverse_dependencies[p_path].insert(p_owner);
	}
	if (singleton->parser_map.has(p_path)) {
		ref = Ref<GDScriptParserRef>(singleton->parser_map[p_path]);
		if (ref.is_null()) {
			r_error = ERR_INVALID_DATA;
			return ref;
		}
	} else {
		String remapped_path = ResourceLoader::path_remap(p_path);
		if (!FileAccess::exists(remapped_path)) {
			r_error = ERR_FILE_NOT_FOUND;
			return ref;
		}
		ref.instantiate();
		ref->path = p_path;
		singleton->parser_map[p_path] = ref.ptr();
	}
	r_error = ref->raise_status(p_status);

	return ref;
}

bool GDScriptCache::has_parser(const String &p_path) {
	MutexLock lock(singleton->mutex);
	return singleton->parser_map.has(p_path);
}

void GDScriptCache::remove_parser(const String &p_path) {
	MutexLock lock(singleton->mutex);

	if (singleton->parser_map.has(p_path)) {
		GDScriptParserRef *parser_ref = singleton->parser_map[p_path];
		parser_ref->abandoned = true;
		singleton->abandoned_parser_map[p_path].push_back(parser_ref->get_instance_id());
	}

	// Can't clear the parser because some other parser might be currently using it in the chain of calls.
	singleton->parser_map.erase(p_path);

	// Have to copy while iterating, because parser_inverse_dependencies is modified.
	HashSet<String> ideps = singleton->parser_inverse_dependencies[p_path];
	singleton->parser_inverse_dependencies.erase(p_path);
	for (String idep_path : ideps) {
		remove_parser(idep_path);
	}
}

String GDScriptCache::get_source_code(const String &p_path) {
	Vector<uint8_t> source_file;
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V(err, "");

	uint64_t len = f->get_length();
	source_file.resize(len + 1);
	uint64_t r = f->get_buffer(source_file.ptrw(), len);
	ERR_FAIL_COND_V(r != len, "");
	source_file.write[len] = 0;

	String source;
	if (source.append_utf8((const char *)source_file.ptr(), len) != OK) {
		ERR_FAIL_V_MSG("", "Script '" + p_path + "' contains invalid unicode (UTF-8), so it was not loaded. Please ensure that scripts are saved in valid UTF-8 unicode.");
	}
	return source;
}

Vector<uint8_t> GDScriptCache::get_binary_tokens(const String &p_path) {
	Vector<uint8_t> buffer;
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err != OK, buffer, "Failed to open binary GDScript file '" + p_path + "'.");

	uint64_t len = f->get_length();
	buffer.resize(len);
	uint64_t read = f->get_buffer(buffer.ptrw(), buffer.size());
	ERR_FAIL_COND_V_MSG(read != len, Vector<uint8_t>(), "Failed to read binary GDScript file '" + p_path + "'.");

	// Check if the file is encrypted
	if (buffer.size() >= 4 && buffer[0] == 'G' && buffer[1] == 'D' && buffer[2] == 'S' && buffer[3] == 'E') {
		// This is an encrypted GDScript file
		// Try to decrypt it using the global script encryption key
		Vector<uint8_t> decrypted_buffer;
		
		// Check if we have a valid encryption key
		bool has_key = false;
		for (int i = 0; i < 32; i++) {
			if (script_encryption_key[i] != 0) {
				has_key = true;
				break;
			}
		}
		
		if (has_key) {
			// First try AES256 decryption
		// Create a temporary file to store the encrypted data
		String temp_path = OS::get_singleton()->get_cache_path().path_join("temp_gdc_" + itos(OS::get_singleton()->get_unix_time()));
		Ref<FileAccess> temp_file = FileAccess::open(temp_path, FileAccess::WRITE);
		if (temp_file.is_valid()) {
			// Write the encrypted data to the temp file (skip the 'GDSE' header)
			temp_file->store_buffer(buffer.ptr() + 4, buffer.size() - 4);
			temp_file->close();
			
			// Try to decrypt the file with AES256
			Ref<FileAccessEncrypted> fae;
			fae.instantiate();
			Vector<uint8_t> key;
			key.resize(32);
			for (int i = 0; i < 32; i++) {
				key.write[i] = script_encryption_key[i];
			}
			
			Ref<FileAccess> f_temp = FileAccess::open(temp_path, FileAccess::READ);
			if (f_temp.is_valid()) {
				err = fae->open_and_parse(f_temp, key, FileAccessEncrypted::MODE_READ, false);
				if (err == OK) {
					decrypted_buffer.resize(fae->get_length());
					fae->get_buffer(decrypted_buffer.ptrw(), decrypted_buffer.size());
					
					// Verify integrity using a simple checksum
					if (decrypted_buffer.size() >= 8) {
						// The last 4 bytes should contain a checksum of the data
						uint32_t expected_checksum = decode_uint32(&decrypted_buffer[decrypted_buffer.size() - 4]);
						
						// Calculate checksum of all data except the checksum itself
						uint32_t calculated_checksum = hash_djb2_buffer(decrypted_buffer.ptr(), decrypted_buffer.size() - 4);
						
						if (expected_checksum == calculated_checksum) {
							// Remove the checksum from the buffer
							decrypted_buffer.resize(decrypted_buffer.size() - 4);
							
							// Replace the 'GDSE' header with 'GDSC' to make it compatible with the standard tokenizer
							if (decrypted_buffer.size() >= 4) {
								decrypted_buffer.write[0] = 'G';
								decrypted_buffer.write[1] = 'D';
								decrypted_buffer.write[2] = 'S';
								decrypted_buffer.write[3] = 'C';
							}
							
							buffer = decrypted_buffer;
						} else {
							// Checksum doesn't match, try XOR decryption
							decrypted_buffer.clear();
						}
					} else {
						// Buffer too small, try XOR decryption
						decrypted_buffer.clear();
					}
				}
				f_temp->close();
			}
			
			// Clean up the temporary file
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			da->remove(temp_path);
		}
			
			// If AES256 decryption failed, try XOR decryption
			if (decrypted_buffer.is_empty()) {
				// Get the key as a string for XOR decryption
				String key_str;
				for (int i = 0; i < 32; i++) {
					if (script_encryption_key[i] == 0) {
						break;
					}
					key_str += (char)script_encryption_key[i];
				}
				
				// Skip the 'GDSE' header and decrypt the rest with XOR
				const uint8_t *src = buffer.ptr() + 4;
				int data_len = buffer.size() - 4;
				
				decrypted_buffer.resize(data_len);
				uint8_t *dst = decrypted_buffer.ptrw();
				
				if (key_str.is_empty()) {
					// Use default XOR key if none provided
					for (int i = 0; i < data_len; i++) {
						dst[i] = src[i] ^ 0xb6;
					}
				} else {
					// Use provided key for XOR
					int key_len = key_str.length();
					for (int i = 0; i < data_len; i++) {
						dst[i] = src[i] ^ key_str[i % key_len];
					}
				}
				
				// Verify integrity using a simple checksum
				if (decrypted_buffer.size() >= 8) {
					// The last 4 bytes should contain a checksum of the data
					uint32_t expected_checksum = decode_uint32(&decrypted_buffer[decrypted_buffer.size() - 4]);
					
					// Calculate checksum of all data except the checksum itself
					uint32_t calculated_checksum = hash_djb2_buffer(decrypted_buffer.ptr(), decrypted_buffer.size() - 4);
					
					if (expected_checksum == calculated_checksum) {
						// Remove the checksum from the buffer
						decrypted_buffer.resize(decrypted_buffer.size() - 4);
						
						// Replace the 'GDSE' header with 'GDSC' to make it compatible with the standard tokenizer
						if (decrypted_buffer.size() >= 4) {
							decrypted_buffer.write[0] = 'G';
							decrypted_buffer.write[1] = 'D';
							decrypted_buffer.write[2] = 'S';
							decrypted_buffer.write[3] = 'C';
						}
						
						buffer = decrypted_buffer;
					} else {
						ERR_FAIL_V_MSG(Vector<uint8_t>(), "GDScript file '" + p_path + "' appears to be corrupted or tampered with.");
					}
				} else {
					ERR_FAIL_V_MSG(Vector<uint8_t>(), "GDScript file '" + p_path + "' is too small to contain valid encrypted data.");
				}
			}
		}
		
		if (decrypted_buffer.is_empty() && has_key) {
			ERR_FAIL_V_MSG(Vector<uint8_t>(), "Failed to decrypt GDScript file '" + p_path + "'. Invalid encryption key or corrupted data.");
		}
	}

	return buffer;
}

Ref<GDScript> GDScriptCache::get_shallow_script(const String &p_path, Error &r_error, const String &p_owner) {
	MutexLock lock(singleton->mutex);

	if (!p_owner.is_empty()) {
		singleton->dependencies[p_owner].insert(p_path);
	}
	if (singleton->full_gdscript_cache.has(p_path)) {
		return singleton->full_gdscript_cache[p_path];
	}
	if (singleton->shallow_gdscript_cache.has(p_path)) {
		return singleton->shallow_gdscript_cache[p_path];
	}

	const String remapped_path = ResourceLoader::path_remap(p_path);

	Ref<GDScript> script;
	script.instantiate();
	script->set_path(p_path, true);
	if (remapped_path.has_extension("gdc")) {
		Vector<uint8_t> buffer = get_binary_tokens(remapped_path);
		if (buffer.is_empty()) {
			r_error = ERR_FILE_CANT_READ;
		}
		script->set_binary_tokens_source(buffer);
	} else {
		r_error = script->load_source_code(remapped_path);
	}

	if (r_error) {
		return Ref<GDScript>(); // Returns null and does not cache when the script fails to load.
	}

	Ref<GDScriptParserRef> parser_ref = get_parser(p_path, GDScriptParserRef::PARSED, r_error);
	if (r_error == OK) {
		GDScriptCompiler::make_scripts(script.ptr(), parser_ref->get_parser()->get_tree(), true);
	}

	singleton->shallow_gdscript_cache[p_path] = script;

	return script;
}

Ref<GDScript> GDScriptCache::get_full_script(const String &p_path, Error &r_error, const String &p_owner, bool p_update_from_disk) {
	MutexLock lock(singleton->mutex);

	if (!p_owner.is_empty()) {
		singleton->dependencies[p_owner].insert(p_path);
	}

	Ref<GDScript> script;
	r_error = OK;
	if (singleton->full_gdscript_cache.has(p_path)) {
		script = singleton->full_gdscript_cache[p_path];
		if (!p_update_from_disk) {
			return script;
		}
	}

	if (script.is_null()) {
		script = get_shallow_script(p_path, r_error);
		// Only exit early if script failed to load, otherwise let reload report errors.
		if (script.is_null()) {
			return script;
		}
	}

	const String remapped_path = ResourceLoader::path_remap(p_path);

	if (p_update_from_disk) {
		if (remapped_path.has_extension("gdc")) {
			Vector<uint8_t> buffer = get_binary_tokens(remapped_path);
			if (buffer.is_empty()) {
				r_error = ERR_FILE_CANT_READ;
				return script;
			}
			script->set_binary_tokens_source(buffer);
		} else {
			r_error = script->load_source_code(remapped_path);
			if (r_error) {
				return script;
			}
		}
	}

	// Allowing lifting the lock might cause a script to be reloaded multiple times,
	// which, as a last resort deadlock prevention strategy, is a good tradeoff.
	uint32_t allowance_id = WorkerThreadPool::thread_enter_unlock_allowance_zone(singleton->mutex);
	r_error = script->reload(true);
	WorkerThreadPool::thread_exit_unlock_allowance_zone(allowance_id);
	if (r_error) {
		return script;
	}

	singleton->full_gdscript_cache[p_path] = script;
	singleton->shallow_gdscript_cache.erase(p_path);

	return script;
}

Ref<GDScript> GDScriptCache::get_cached_script(const String &p_path) {
	MutexLock lock(singleton->mutex);

	if (singleton->full_gdscript_cache.has(p_path)) {
		return singleton->full_gdscript_cache[p_path];
	}

	if (singleton->shallow_gdscript_cache.has(p_path)) {
		return singleton->shallow_gdscript_cache[p_path];
	}

	return Ref<GDScript>();
}

Error GDScriptCache::finish_compiling(const String &p_owner) {
	MutexLock lock(singleton->mutex);

	// Mark this as compiled.
	Ref<GDScript> script = get_cached_script(p_owner);
	singleton->full_gdscript_cache[p_owner] = script;
	singleton->shallow_gdscript_cache.erase(p_owner);

	HashSet<String> depends = singleton->dependencies[p_owner];

	Error err = OK;
	for (const String &E : depends) {
		Error this_err = OK;
		// No need to save the script. We assume it's already referenced in the owner.
		get_full_script(E, this_err);

		if (this_err != OK) {
			err = this_err;
		}
	}

	singleton->dependencies.erase(p_owner);

	return err;
}

void GDScriptCache::add_static_script(Ref<GDScript> p_script) {
	ERR_FAIL_COND_MSG(p_script.is_null(), "Trying to cache empty script as static.");
	ERR_FAIL_COND_MSG(!p_script->is_valid(), "Trying to cache non-compiled script as static.");
	singleton->static_gdscript_cache[p_script->get_fully_qualified_name()] = p_script;
}

void GDScriptCache::remove_static_script(const String &p_fqcn) {
	singleton->static_gdscript_cache.erase(p_fqcn);
}

void GDScriptCache::clear() {
	if (singleton == nullptr) {
		return;
	}

	MutexLock lock(singleton->mutex);

	if (singleton->cleared) {
		return;
	}
	singleton->cleared = true;

	singleton->parser_inverse_dependencies.clear();

	for (const KeyValue<String, Vector<ObjectID>> &KV : singleton->abandoned_parser_map) {
		for (ObjectID parser_ref_id : KV.value) {
			Ref<GDScriptParserRef> parser_ref = { ObjectDB::get_instance(parser_ref_id) };
			if (parser_ref.is_valid()) {
				parser_ref->clear();
			}
		}
	}

	singleton->abandoned_parser_map.clear();

	RBSet<Ref<GDScriptParserRef>> parser_map_refs;
	for (KeyValue<String, GDScriptParserRef *> &E : singleton->parser_map) {
		parser_map_refs.insert(E.value);
	}

	singleton->parser_map.clear();

	for (Ref<GDScriptParserRef> &E : parser_map_refs) {
		if (E.is_valid()) {
			E->clear();
		}
	}

	parser_map_refs.clear();
	singleton->shallow_gdscript_cache.clear();
	singleton->full_gdscript_cache.clear();
	singleton->static_gdscript_cache.clear();
}

GDScriptCache::GDScriptCache() {
	singleton = this;
}

GDScriptCache::~GDScriptCache() {
	if (!cleared) {
		clear();
	}
	singleton = nullptr;
}

void GDScriptCache::set_script_encryption_key(const String &p_key) {
	// Clear the current key first
	for (int i = 0; i < 32; i++) {
		script_encryption_key[i] = 0;
	}
	
	// Set the new key
	int key_len = p_key.length();
	for (int i = 0; i < MIN(32, key_len); i++) {
		script_encryption_key[i] = p_key[i];
	}
}

String GDScriptCache::get_script_encryption_key() {
	// Convert the key back to a string, stopping at the first null byte
	String key;
	for (int i = 0; i < 32; i++) {
		if (script_encryption_key[i] == 0) {
			break;
		}
		key += (char)script_encryption_key[i];
	}
	return key;
}
