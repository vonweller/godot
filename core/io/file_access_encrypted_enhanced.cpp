/**************************************************************************/
/*  file_access_encrypted_enhanced.cpp                                    */
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

#include "file_access_encrypted_enhanced.h"

#include "core/variant/variant.h"

CryptoCore::RandomGenerator *FileAccessEncryptedEnhanced::_enhanced_static_rng = nullptr;

void FileAccessEncryptedEnhanced::deinitialize() {
	if (_enhanced_static_rng) {
		memdelete(_enhanced_static_rng);
		_enhanced_static_rng = nullptr;
	}
}

Error FileAccessEncryptedEnhanced::_derive_master_key() {
	master_key.resize(32);
	
	// Use PBKDF2 to derive master key from user key
	Error err = CryptoCore::pbkdf2_hmac_sha256(
		user_key.ptr(), user_key.size(),
		security_params.master_salt, 32,
		security_params.kdf_iterations,
		master_key.ptrw(), 32
	);
	
	return err;
}

Error FileAccessEncryptedEnhanced::_derive_file_key(const String &p_context) {
	file_key.resize(32);
	
	// Build context information: file_path + PCK version + custom context
	String context_str = file_path + "|" + String::num(3) + "|" + p_context;  // PCK version 3
	CharString cs = context_str.utf8();
	
	// Use HKDF to derive file-specific key
	Error err = CryptoCore::hkdf_sha256(
		nullptr, 0,                    // No additional salt
		master_key.ptr(), master_key.size(),
		(const uint8_t *)cs.get_data(), cs.length(),
		file_key.ptrw(), 32
	);
	
	return err;
}

Error FileAccessEncryptedEnhanced::_finalize_encryption_key(uint64_t p_file_size, const Vector<uint8_t> &p_file_md5) {
	final_key.resize(32);
	
	// Create HMAC input: file_size + MD5 + file_key
	Vector<uint8_t> hmac_input;
	hmac_input.resize(8 + 16 + 32);
	
	// Add file size (8 bytes, big-endian)
	for (int i = 0; i < 8; i++) {
		hmac_input.write[i] = (p_file_size >> (56 - i * 8)) & 0xFF;
	}
	
	// Add MD5 checksum
	memcpy(hmac_input.ptrw() + 8, p_file_md5.ptr(), 16);
	
	// Add file key
	memcpy(hmac_input.ptrw() + 24, file_key.ptr(), 32);
	
	// Use HMAC-SHA256 to generate final key
	Error err = CryptoCore::hmac_sha256(
		file_key.ptr(), file_key.size(),
		hmac_input.ptr(), hmac_input.size(),
		final_key.ptrw()
	);
	
	return err;
}

Vector<uint8_t> FileAccessEncryptedEnhanced::_generate_file_iv() {
	Vector<uint8_t> file_iv;
	file_iv.resize(16);
	
	// Use file key and path to generate deterministic but unpredictable IV
	String iv_input = file_path + "|IV_GENERATION";
	CharString cs = iv_input.utf8();
	
	uint8_t full_hash[32];
	Error err = CryptoCore::hmac_sha256(
		final_key.ptr(), final_key.size(),
		(const uint8_t *)cs.get_data(), cs.length(),
		full_hash
	);
	
	if (err == OK) {
		// Use first 16 bytes as IV
		memcpy(file_iv.ptrw(), full_hash, 16);
	} else {
		// Fallback to zeros if HMAC fails (should not happen)
		memset(file_iv.ptrw(), 0, 16);
	}
	
	return file_iv;
}

Error FileAccessEncryptedEnhanced::open_and_parse(Ref<FileAccess> p_base, const Vector<uint8_t> &p_key, Mode p_mode, 
													const String &p_file_path, bool p_with_magic, 
													const EnhancedSecurityParams &p_security_params) {
	ERR_FAIL_COND_V_MSG(file.is_valid(), ERR_ALREADY_IN_USE, vformat("Can't open file while another file from path '%s' is open.", file->get_path_absolute()));
	ERR_FAIL_COND_V(p_key.size() != 32, ERR_INVALID_PARAMETER);

	pos = 0;
	eofed = false;
	use_magic = p_with_magic;
	user_key = p_key;
	file_path = p_file_path;
	security_params = p_security_params;

	if (p_mode == MODE_WRITE_AES256_ENHANCED) {
		data.clear();
		writing = true;
		file = p_base;
		
		// Generate random salt if not provided
		if (unlikely(!_enhanced_static_rng)) {
			_enhanced_static_rng = memnew(CryptoCore::RandomGenerator);
			if (_enhanced_static_rng->init() != OK) {
				memdelete(_enhanced_static_rng);
				_enhanced_static_rng = nullptr;
				ERR_FAIL_V_MSG(FAILED, "Failed to initialize random number generator.");
			}
		}
		
		Error err = _enhanced_static_rng->get_random_bytes(security_params.master_salt, 32);
		ERR_FAIL_COND_V(err != OK, err);
		
		// Start key derivation process
		err = _derive_master_key();
		ERR_FAIL_COND_V(err != OK, err);
		
		err = _derive_file_key("WRITE");
		ERR_FAIL_COND_V(err != OK, err);

	} else if (p_mode == MODE_READ) {
		writing = false;
		
		if (use_magic) {
			uint32_t magic = p_base->get_32();
			ERR_FAIL_COND_V(magic != ENHANCED_ENCRYPTED_HEADER_MAGIC, ERR_FILE_UNRECOGNIZED);
		}
		
		// Read security parameters
		p_base->get_buffer((uint8_t *)&security_params, sizeof(EnhancedSecurityParams));
		
		unsigned char md5d[16];
		p_base->get_buffer(md5d, 16);
		length = p_base->get_64();

		// Start key derivation process
		Error err = _derive_master_key();
		ERR_FAIL_COND_V(err != OK, err);
		
		err = _derive_file_key("READ");
		ERR_FAIL_COND_V(err != OK, err);
		
		// For reading, we need the file MD5 to finalize the key
		Vector<uint8_t> file_md5;
		file_md5.resize(16);
		memcpy(file_md5.ptrw(), md5d, 16);
		
		err = _finalize_encryption_key(length, file_md5);
		ERR_FAIL_COND_V(err != OK, err);
		
		// Generate IV for this file
		iv = _generate_file_iv();

		base = p_base->get_position();
		ERR_FAIL_COND_V(p_base->get_length() < base + length, ERR_FILE_CORRUPT);
		uint64_t ds = length;
		if (ds % 16) {
			ds += 16 - (ds % 16);
		}
		data.resize(ds);

		uint64_t blen = p_base->get_buffer(data.ptrw(), ds);
		ERR_FAIL_COND_V(blen != ds, ERR_FILE_CORRUPT);

		{
			CryptoCore::AESContext ctx;
			ctx.set_encode_key(final_key.ptrw(), 256);
			ctx.decrypt_cfb(ds, iv.ptrw(), data.ptrw(), data.ptrw());
		}

		data.resize(length);

		// Verify MD5
		unsigned char hash[16];
		ERR_FAIL_COND_V(CryptoCore::md5(data.ptr(), data.size(), hash) != OK, ERR_BUG);
		ERR_FAIL_COND_V_MSG(String::md5(hash) != String::md5(md5d), ERR_FILE_CORRUPT, "The MD5 sum of the decrypted file does not match the expected value. Enhanced encryption integrity check failed.");

		file = p_base;
	}

	return OK;
}

Error FileAccessEncryptedEnhanced::open_and_parse_password(Ref<FileAccess> p_base, const String &p_key, Mode p_mode, const String &p_file_path) {
	String cs = p_key.md5_text();
	ERR_FAIL_COND_V(cs.length() != 32, ERR_INVALID_PARAMETER);
	Vector<uint8_t> key_md5;
	key_md5.resize(32);
	for (int i = 0; i < 32; i++) {
		key_md5.write[i] = cs[i];
	}

	return open_and_parse(p_base, key_md5, p_mode, p_file_path);
}

Error FileAccessEncryptedEnhanced::open_internal(const String &p_path, int p_mode_flags) {
	return OK;
}

void FileAccessEncryptedEnhanced::_close() {
	if (file.is_null()) {
		return;
	}

	if (writing) {
		Vector<uint8_t> compressed;
		uint64_t len = data.size();
		if (len % 16) {
			len += 16 - (len % 16);
		}

		unsigned char hash[16];
		ERR_FAIL_COND(CryptoCore::md5(data.ptr(), data.size(), hash) != OK);

		compressed.resize(len);
		memset(compressed.ptrw(), 0, len);
		for (int i = 0; i < data.size(); i++) {
			compressed.write[i] = data[i];
		}
		
		// Finalize encryption key with file metadata
		Vector<uint8_t> file_md5;
		file_md5.resize(16);
		memcpy(file_md5.ptrw(), hash, 16);
		
		Error err = _finalize_encryption_key(data.size(), file_md5);
		ERR_FAIL_COND(err != OK);
		
		// Generate IV
		iv = _generate_file_iv();

		CryptoCore::AESContext ctx;
		ctx.set_encode_key(final_key.ptrw(), 256);

		if (use_magic) {
			file->store_32(ENHANCED_ENCRYPTED_HEADER_MAGIC);
		}

		// Store security parameters
		file->store_buffer((const uint8_t *)&security_params, sizeof(EnhancedSecurityParams));
		
		file->store_buffer(hash, 16);
		file->store_64(data.size());

		ctx.encrypt_cfb(len, iv.ptrw(), compressed.ptrw(), compressed.ptrw());

		file->store_buffer(compressed.ptr(), compressed.size());
		data.clear();
	}

	file.unref();
}

// Implement remaining virtual functions similar to original FileAccessEncrypted
bool FileAccessEncryptedEnhanced::is_open() const {
	return file.is_valid();
}

String FileAccessEncryptedEnhanced::get_path() const {
	if (file.is_valid()) {
		return file->get_path();
	} else {
		return "";
	}
}

String FileAccessEncryptedEnhanced::get_path_absolute() const {
	if (file.is_valid()) {
		return file->get_path_absolute();
	} else {
		return "";
	}
}

void FileAccessEncryptedEnhanced::seek(uint64_t p_position) {
	if (p_position > get_length()) {
		p_position = get_length();
	}

	pos = p_position;
	eofed = false;
}

void FileAccessEncryptedEnhanced::seek_end(int64_t p_position) {
	seek(get_length() + p_position);
}

uint64_t FileAccessEncryptedEnhanced::get_position() const {
	return pos;
}

uint64_t FileAccessEncryptedEnhanced::get_length() const {
	return data.size();
}

bool FileAccessEncryptedEnhanced::eof_reached() const {
	return eofed;
}

uint64_t FileAccessEncryptedEnhanced::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V_MSG(writing, -1, "File has not been opened in read mode.");

	if (!p_length) {
		return 0;
	}

	ERR_FAIL_NULL_V(p_dst, -1);

	uint64_t to_copy = MIN(p_length, get_length() - pos);

	memcpy(p_dst, data.ptr() + pos, to_copy);
	pos += to_copy;

	if (to_copy < p_length) {
		eofed = true;
	}

	return to_copy;
}

Error FileAccessEncryptedEnhanced::get_error() const {
	return eofed ? ERR_FILE_EOF : OK;
}

bool FileAccessEncryptedEnhanced::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V_MSG(!writing, false, "File has not been opened in write mode.");

	if (!p_length) {
		return true;
	}

	ERR_FAIL_NULL_V(p_src, false);

	if (pos + p_length >= get_length()) {
		ERR_FAIL_COND_V(data.resize(pos + p_length) != OK, false);
	}

	memcpy(data.ptrw() + pos, p_src, p_length);
	pos += p_length;

	return true;
}

void FileAccessEncryptedEnhanced::flush() {
	ERR_FAIL_COND_MSG(!writing, "File has not been opened in write mode.");
	// Enhanced encrypted files keep data in memory till close()
}

bool FileAccessEncryptedEnhanced::file_exists(const String &p_name) {
	Ref<FileAccess> fa = FileAccess::open(p_name, FileAccess::READ);
	if (fa.is_null()) {
		return false;
	}
	return true;
}

// Implement remaining file attribute functions by delegating to underlying file
uint64_t FileAccessEncryptedEnhanced::_get_modified_time(const String &p_file) {
	if (file.is_valid()) {
		return file->get_modified_time(p_file);
	} else {
		return 0;
	}
}

uint64_t FileAccessEncryptedEnhanced::_get_access_time(const String &p_file) {
	if (file.is_valid()) {
		return file->get_access_time(p_file);
	} else {
		return 0;
	}
}

int64_t FileAccessEncryptedEnhanced::_get_size(const String &p_file) {
	if (file.is_valid()) {
		return file->get_size(p_file);
	} else {
		return -1;
	}
}

BitField<FileAccess::UnixPermissionFlags> FileAccessEncryptedEnhanced::_get_unix_permissions(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_unix_permissions(p_file);
	}
	return 0;
}

Error FileAccessEncryptedEnhanced::_set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	if (file.is_valid()) {
		return file->_set_unix_permissions(p_file, p_permissions);
	}
	return FAILED;
}

bool FileAccessEncryptedEnhanced::_get_hidden_attribute(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_hidden_attribute(p_file);
	}
	return false;
}

Error FileAccessEncryptedEnhanced::_set_hidden_attribute(const String &p_file, bool p_hidden) {
	if (file.is_valid()) {
		return file->_set_hidden_attribute(p_file, p_hidden);
	}
	return FAILED;
}

bool FileAccessEncryptedEnhanced::_get_read_only_attribute(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_read_only_attribute(p_file);
	}
	return false;
}

Error FileAccessEncryptedEnhanced::_set_read_only_attribute(const String &p_file, bool p_ro) {
	if (file.is_valid()) {
		return file->_set_read_only_attribute(p_file, p_ro);
	}
	return FAILED;
}

void FileAccessEncryptedEnhanced::close() {
	_close();
}

FileAccessEncryptedEnhanced::~FileAccessEncryptedEnhanced() {
	_close();
}