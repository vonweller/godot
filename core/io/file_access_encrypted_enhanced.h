/**************************************************************************/
/*  file_access_encrypted_enhanced.h                                      */
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

#pragma once

#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/pck_key_derivation.h"

#define ENHANCED_ENCRYPTED_HEADER_MAGIC 0x43454748  // CEGH - Enhanced encryption magic

class FileAccessEncryptedEnhanced : public FileAccess {
	GDSOFTCLASS(FileAccessEncryptedEnhanced, FileAccess);

public:
	enum Mode : int32_t {
		MODE_READ,
		MODE_WRITE_AES256_ENHANCED,
		MODE_MAX
	};

	typedef PCKKeyDerivation::SecurityParameters EnhancedSecurityParams;

private:
	Vector<uint8_t> iv;
	Vector<uint8_t> user_key;              // Original user key
	Vector<uint8_t> master_key;            // Derived master key
	Vector<uint8_t> file_key;              // File-specific key
	Vector<uint8_t> final_key;             // Final encryption key
	
	bool writing = false;
	Ref<FileAccess> file;
	uint64_t base = 0;
	uint64_t length = 0;
	Vector<uint8_t> data;
	mutable uint64_t pos = 0;
	mutable bool eofed = false;
	bool use_magic = true;
	
	String file_path;                      // For key derivation context
	EnhancedSecurityParams security_params;

	void _close();
	Error _derive_master_key();
	Error _derive_file_key(const String &p_context);
	Error _finalize_encryption_key(uint64_t p_file_size, const Vector<uint8_t> &p_file_md5);
	Vector<uint8_t> _generate_file_iv();

	static CryptoCore::RandomGenerator *_enhanced_static_rng;

public:
	Error open_and_parse(Ref<FileAccess> p_base, const Vector<uint8_t> &p_key, Mode p_mode, 
						const String &p_file_path = "", bool p_with_magic = true, 
						const EnhancedSecurityParams &p_security_params = EnhancedSecurityParams());
	
	Error open_and_parse_password(Ref<FileAccess> p_base, const String &p_key, Mode p_mode, const String &p_file_path = "");

	Vector<uint8_t> get_iv() const { return iv; }
	EnhancedSecurityParams get_security_params() const { return security_params; }

	virtual Error open_internal(const String &p_path, int p_mode_flags) override; ///< open a file
	virtual bool is_open() const override; ///< true when file is open

	virtual String get_path() const override; /// returns the path for the current open file
	virtual String get_path_absolute() const override; /// returns the absolute path for the current open file

	virtual void seek(uint64_t p_position) override; ///< seek to a given position
	virtual void seek_end(int64_t p_position = 0) override; ///< seek from the end of file
	virtual uint64_t get_position() const override; ///< get position in the file
	virtual uint64_t get_length() const override; ///< get size of the file

	virtual bool eof_reached() const override; ///< reading passed EOF

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

	virtual Error get_error() const override; ///< get last error

	virtual Error resize(int64_t p_length) override { return ERR_UNAVAILABLE; }
	virtual void flush() override;
	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) override; ///< store an array of bytes

	virtual bool file_exists(const String &p_name) override; ///< return true if a file exists

	virtual uint64_t _get_modified_time(const String &p_file) override;
	virtual uint64_t _get_access_time(const String &p_file) override;
	virtual int64_t _get_size(const String &p_file) override;
	virtual BitField<FileAccess::UnixPermissionFlags> _get_unix_permissions(const String &p_file) override;
	virtual Error _set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) override;

	virtual bool _get_hidden_attribute(const String &p_file) override;
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) override;
	virtual bool _get_read_only_attribute(const String &p_file) override;
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) override;

	virtual void close() override;

	static void deinitialize();

	FileAccessEncryptedEnhanced() { memset(&security_params, 0, sizeof(EnhancedSecurityParams)); }
	~FileAccessEncryptedEnhanced();
};