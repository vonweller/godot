/**************************************************************************/
/*  test_gdscript_encryption.cpp                                         */
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

#include "test_gdscript_encryption.h"

#include "../gdscript_cache.h"
#include "../gdscript_tokenizer_buffer.h"

#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

namespace GDScriptEncryptionTests {

// Global encryption key for testing
static uint8_t script_encryption_key[32];

// Test helper functions
static Vector<uint8_t> _encrypt_file_aes256(const Vector<uint8_t> &p_data, const String &p_key) {
	// Calculate checksum of the original data
	uint32_t checksum = hash_djb2_buffer(p_data.ptr(), p_data.size());
	
	// Append checksum to the data
	Vector<uint8_t> data_with_checksum;
	data_with_checksum.resize(p_data.size() + 4);
	memcpy(data_with_checksum.ptrw(), p_data.ptr(), p_data.size());
	encode_uint32(checksum, &data_with_checksum.write[p_data.size()]);
	
	// Convert key to bytes
	Vector<uint8_t> key;
	key.resize(32); // AES256 uses 32-byte keys
	for (int i = 0; i < 32; i++) {
		if (i < p_key.length()) {
			key.write[i] = p_key[i];
		} else {
			key.write[i] = 0; // Pad with zeros if key is shorter
		}
	}

	// Create a temporary file to hold the encrypted data
	String temp_path = OS::get_singleton()->get_cache_dir().plus_file("temp_encrypted_" + itos(Math::rand()));
	Ref<FileAccess> temp_file = FileAccess::open(temp_path, FileAccess::WRITE);
	if (temp_file.is_null()) {
		return Vector<uint8_t>();
	}
	
	// Create encrypted file access
	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	
	// Generate random IV
	Vector<uint8_t> iv;
	iv.resize(16);
	CryptoCore::RandomGenerator rng;
	rng.init();
	for (int i = 0; i < 16; i++) {
		iv.write[i] = rng.get_random_byte();
	}
	
	// Open for writing with AES256
	Error err = fae->open_and_parse(temp_file, key, FileAccessEncrypted::MODE_WRITE_AES256, true, iv);
	if (err != OK) {
		temp_file->close();
		FileAccess::remove_file_or_error(temp_path);
		return Vector<uint8_t>();
	}
	
	// Store the data with checksum
	fae->store_buffer(data_with_checksum.ptr(), data_with_checksum.size());
	fae->close();
	temp_file->close();
	
	// Read the encrypted data back
	Vector<uint8_t> encrypted_data = FileAccess::get_file_as_bytes(temp_path);
	FileAccess::remove_file_or_error(temp_path);
	
	return encrypted_data;
}

static Vector<uint8_t> _encrypt_file_xor(const Vector<uint8_t> &p_data, const String &p_key) {
	// Calculate checksum of the original data
	uint32_t checksum = hash_djb2_buffer(p_data.ptr(), p_data.size());
	
	// Append checksum to the data
	Vector<uint8_t> data_with_checksum;
	data_with_checksum.resize(p_data.size() + 4);
	memcpy(data_with_checksum.ptrw(), p_data.ptr(), p_data.size());
	encode_uint32(checksum, &data_with_checksum.write[p_data.size()]);
	
	if (p_key.is_empty()) {
		// Default XOR key if none provided
		Vector<uint8_t> result;
		result.resize(data_with_checksum.size());
		
		const uint8_t *src = data_with_checksum.ptr();
		uint8_t *dst = result.ptrw();
		
		for (int i = 0; i < data_with_checksum.size(); i++) {
			dst[i] = src[i] ^ 0xb6;
		}
		
		return result;
	}
	
	// Use provided key for XOR
	Vector<uint8_t> result;
	result.resize(data_with_checksum.size());
	
	const uint8_t *src = data_with_checksum.ptr();
	uint8_t *dst = result.ptrw();
	int key_len = p_key.length();
	
	for (int i = 0; i < data_with_checksum.size(); i++) {
		dst[i] = src[i] ^ p_key[i % key_len];
	}
	
	return result;
}

// Test functions
static bool test_aes256_encryption() {
	print_line("Testing AES256 encryption...");
	
	// Create a simple GDScript code
	String test_code = "extends Node\n\nfunc _ready():\n    print(\"Hello, World!\")\n";
	
	// Convert to binary format
	Vector<uint8_t> binary_data = GDScriptTokenizerBuffer::parse_code_string(test_code, GDScriptTokenizerBuffer::COMPRESS_NONE);
	ERR_FAIL_COND_V_MSG(binary_data.is_empty(), false, "Failed to generate binary data from GDScript code");
	
	// Encrypt with AES256
	String encryption_key = "my_test_encryption_key_123456789012"; // 32 bytes
	Vector<uint8_t> encrypted_data = _encrypt_file_aes256(binary_data, encryption_key);
	ERR_FAIL_COND_V_MSG(encrypted_data.is_empty(), false, "Failed to encrypt data with AES256");
	
	// Create a temporary file with the encrypted data
	String temp_path = OS::get_singleton()->get_cache_dir().plus_file("test_aes256.gdc");
	Ref<FileAccess> temp_file = FileAccess::open(temp_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(temp_file.is_null(), false, "Failed to create temporary file");
	
	// Write "GDSE" header followed by encrypted data
	temp_file->store_buffer((const uint8_t *)"GDSE", 4);
	temp_file->store_buffer(encrypted_data.ptr(), encrypted_data.size());
	temp_file->close();
	
	// Set the global encryption key
	for (int i = 0; i < 32; i++) {
		if (i < encryption_key.length()) {
			script_encryption_key[i] = encryption_key[i];
		} else {
			script_encryption_key[i] = 0;
		}
	}
	
	// Try to decrypt using GDScriptCache
	Vector<uint8_t> decrypted_data = GDScriptCache::get_binary_tokens(temp_path);
	ERR_FAIL_COND_V_MSG(decrypted_data.is_empty(), false, "Failed to decrypt AES256 encrypted file");
	
	// Verify that decrypted data matches original binary data
	ERR_FAIL_COND_V_MSG(decrypted_data.size() != binary_data.size(), false, "Decrypted data size doesn't match original");
	
	for (int i = 0; i < binary_data.size(); i++) {
		ERR_FAIL_COND_V_MSG(decrypted_data[i] != binary_data[i], false, "Decrypted data doesn't match original");
	}
	
	// Clean up
	FileAccess::remove_file_or_error(temp_path);
	
	print_line("AES256 encryption test passed!");
	return true;
}

static bool test_xor_encryption() {
	print_line("Testing XOR encryption...");
	
	// Create a simple GDScript code
	String test_code = "extends Node\n\nfunc _ready():\n    print(\"Hello, XOR!\")\n";
	
	// Convert to binary format
	Vector<uint8_t> binary_data = GDScriptTokenizerBuffer::parse_code_string(test_code, GDScriptTokenizerBuffer::COMPRESS_NONE);
	ERR_FAIL_COND_V_MSG(binary_data.is_empty(), false, "Failed to generate binary data from GDScript code");
	
	// Encrypt with XOR
	String encryption_key = "my_xor_key";
	Vector<uint8_t> encrypted_data = _encrypt_file_xor(binary_data, encryption_key);
	ERR_FAIL_COND_V_MSG(encrypted_data.is_empty(), false, "Failed to encrypt data with XOR");
	
	// Create a temporary file with the encrypted data
	String temp_path = OS::get_singleton()->get_cache_dir().plus_file("test_xor.gdc");
	Ref<FileAccess> temp_file = FileAccess::open(temp_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(temp_file.is_null(), false, "Failed to create temporary file");
	
	// Write "GDSE" header followed by encrypted data
	temp_file->store_buffer((const uint8_t *)"GDSE", 4);
	temp_file->store_buffer(encrypted_data.ptr(), encrypted_data.size());
	temp_file->close();
	
	// Set the global encryption key
	for (int i = 0; i < 32; i++) {
		if (i < encryption_key.length()) {
			script_encryption_key[i] = encryption_key[i];
		} else {
			script_encryption_key[i] = 0;
		}
	}
	
	// Try to decrypt using GDScriptCache
	Vector<uint8_t> decrypted_data = GDScriptCache::get_binary_tokens(temp_path);
	ERR_FAIL_COND_V_MSG(decrypted_data.is_empty(), false, "Failed to decrypt XOR encrypted file");
	
	// Verify that decrypted data matches original binary data
	ERR_FAIL_COND_V_MSG(decrypted_data.size() != binary_data.size(), false, "Decrypted data size doesn't match original");
	
	for (int i = 0; i < binary_data.size(); i++) {
		ERR_FAIL_COND_V_MSG(decrypted_data[i] != binary_data[i], false, "Decrypted data doesn't match original");
	}
	
	// Clean up
	FileAccess::remove_file_or_error(temp_path);
	
	print_line("XOR encryption test passed!");
	return true;
}

static bool test_tampering_detection() {
	print_line("Testing tampering detection...");
	
	// Create a simple GDScript code
	String test_code = "extends Node\n\nfunc _ready():\n    print(\"Tampering test\")\n";
	
	// Convert to binary format
	Vector<uint8_t> binary_data = GDScriptTokenizerBuffer::parse_code_string(test_code, GDScriptTokenizerBuffer::COMPRESS_NONE);
	ERR_FAIL_COND_V_MSG(binary_data.is_empty(), false, "Failed to generate binary data from GDScript code");
	
	// Encrypt with XOR
	String encryption_key = "tamper_test_key";
	Vector<uint8_t> encrypted_data = _encrypt_file_xor(binary_data, encryption_key);
	ERR_FAIL_COND_V_MSG(encrypted_data.is_empty(), false, "Failed to encrypt data with XOR");
	
	// Create a temporary file with the encrypted data
	String temp_path = OS::get_singleton()->get_cache_dir().plus_file("test_tamper.gdc");
	Ref<FileAccess> temp_file = FileAccess::open(temp_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(temp_file.is_null(), false, "Failed to create temporary file");
	
	// Write "GDSE" header followed by encrypted data
	temp_file->store_buffer((const uint8_t *)"GDSE", 4);
	temp_file->store_buffer(encrypted_data.ptr(), encrypted_data.size());
	temp_file->close();
	
	// Tamper with the encrypted file by modifying a byte in the middle
	temp_file = FileAccess::open(temp_path, FileAccess::READ_WRITE);
	ERR_FAIL_COND_V_MSG(temp_file.is_null(), false, "Failed to open temporary file for tampering");
	
	temp_file->seek(10); // Seek to some position in the encrypted data
	uint8_t original_byte = temp_file->get_8();
	temp_file->seek(10);
	temp_file->store_8(original_byte ^ 0xFF); // Flip all bits
	temp_file->close();
	
	// Set the global encryption key
	for (int i = 0; i < 32; i++) {
		if (i < encryption_key.length()) {
			script_encryption_key[i] = encryption_key[i];
		} else {
			script_encryption_key[i] = 0;
		}
	}
	
	// Try to decrypt using GDScriptCache - should fail due to tampering
	Vector<uint8_t> decrypted_data = GDScriptCache::get_binary_tokens(temp_path);
	
	// Clean up
	FileAccess::remove_file_or_error(temp_path);
	
	// If decryption succeeded, tampering detection failed
	ERR_FAIL_COND_V_MSG(!decrypted_data.is_empty(), false, "Tampering detection failed - corrupted file was accepted");
	
	print_line("Tampering detection test passed!");
	return true;
}

static bool test_default_xor_encryption() {
	print_line("Testing default XOR encryption (no key provided)...");
	
	// Create a simple GDScript code
	String test_code = "extends Node\n\nfunc _ready():\n    print(\"Default XOR test\")\n";
	
	// Convert to binary format
	Vector<uint8_t> binary_data = GDScriptTokenizerBuffer::parse_code_string(test_code, GDScriptTokenizerBuffer::COMPRESS_NONE);
	ERR_FAIL_COND_V_MSG(binary_data.is_empty(), false, "Failed to generate binary data from GDScript code");
	
	// Encrypt with default XOR (empty key)
	Vector<uint8_t> encrypted_data = _encrypt_file_xor(binary_data, "");
	ERR_FAIL_COND_V_MSG(encrypted_data.is_empty(), false, "Failed to encrypt data with default XOR");
	
	// Create a temporary file with the encrypted data
	String temp_path = OS::get_singleton()->get_cache_dir().plus_file("test_default_xor.gdc");
	Ref<FileAccess> temp_file = FileAccess::open(temp_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(temp_file.is_null(), false, "Failed to create temporary file");
	
	// Write "GDSE" header followed by encrypted data
	temp_file->store_buffer((const uint8_t *)"GDSE", 4);
	temp_file->store_buffer(encrypted_data.ptr(), encrypted_data.size());
	temp_file->close();
	
	// Set the global encryption key to zeros (simulating default key)
	for (int i = 0; i < 32; i++) {
		script_encryption_key[i] = 0;
	}
	
	// Try to decrypt using GDScriptCache - should fail because we don't have the right key
	Vector<uint8_t> decrypted_data = GDScriptCache::get_binary_tokens(temp_path);
	
	// Clean up
	FileAccess::remove_file_or_error(temp_path);
	
	// If decryption succeeded, something's wrong
	ERR_FAIL_COND_V_MSG(!decrypted_data.is_empty(), false, "Default XOR encryption test failed - decryption succeeded with wrong key");
	
	print_line("Default XOR encryption test passed!");
	return true;
}

bool run_all_tests() {
	print_line("Running GDScript encryption tests...");
	print_line("===================================");
	
	bool all_passed = true;
	
	all_passed &= test_aes256_encryption();
	print_line("");
	
	all_passed &= test_xor_encryption();
	print_line("");
	
	all_passed &= test_tampering_detection();
	print_line("");
	
	all_passed &= test_default_xor_encryption();
	print_line("");
	
	if (all_passed) {
		print_line("All encryption tests passed!");
	} else {
		print_line("Some encryption tests failed!");
	}
	
	print_line("===================================");
	
	return all_passed;
}

} // namespace GDScriptEncryptionTests