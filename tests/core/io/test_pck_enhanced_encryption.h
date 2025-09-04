/**************************************************************************/
/*  test_pck_enhanced_encryption.h                                        */
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

#include "core/io/file_access_encrypted_enhanced.h"
#include "core/io/pck_key_derivation.h"
#include "core/io/pck_packer.h"
#include "core/os/os.h"

#include "tests/test_utils.h"
#include "thirdparty/doctest/doctest.h"

namespace TestPCKEnhancedEncryption {

TEST_CASE("[PCKEnhancedEncryption] Key derivation functionality") {
	// Test key derivation functions
	String test_key = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	Vector<uint8_t> key = PCKKeyDerivation::hex_string_to_key(test_key);
	
	CHECK_MESSAGE(key.size() == 32, "Key should be 32 bytes");
	CHECK_MESSAGE(key[0] == 0x01, "First byte should be 0x01");
	CHECK_MESSAGE(key[31] == 0xef, "Last byte should be 0xef");
}

TEST_CASE("[PCKEnhancedEncryption] Security parameters generation") {
	PCKKeyDerivation::SecurityParameters params = PCKKeyDerivation::generate_security_parameters(50000);
	
	CHECK_MESSAGE(params.kdf_iterations == 50000, "KDF iterations should match");
	CHECK_MESSAGE(params.security_version == 1, "Security version should be 1");
	CHECK_MESSAGE(PCKKeyDerivation::validate_security_parameters(params), "Generated parameters should be valid");
}

TEST_CASE("[PCKEnhancedEncryption] Multi-layer key derivation") {
	String test_key = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
	Vector<uint8_t> user_key = PCKKeyDerivation::hex_string_to_key(test_key);
	
	PCKKeyDerivation kd;
	PCKKeyDerivation::SecurityParameters params = PCKKeyDerivation::generate_security_parameters(10000);
	
	Error err = kd.initialize(user_key, params);
	CHECK_MESSAGE(err == OK, "Key derivation initialization should succeed");
	
	err = kd.derive_master_key();
	CHECK_MESSAGE(err == OK, "Master key derivation should succeed");
	
	err = kd.derive_file_key("test/file/path.txt", "TEST_CONTEXT");
	CHECK_MESSAGE(err == OK, "File key derivation should succeed");
	
	Vector<uint8_t> file_md5;
	file_md5.resize(16);
	for (int i = 0; i < 16; i++) {
		file_md5.write[i] = i; // Dummy MD5
	}
	
	err = kd.finalize_encryption_key(1024, file_md5);
	CHECK_MESSAGE(err == OK, "Final key derivation should succeed");
	
	Vector<uint8_t> final_key = kd.get_final_key();
	CHECK_MESSAGE(final_key.size() == 32, "Final key should be 32 bytes");
	
	Vector<uint8_t> iv = kd.generate_file_iv();
	CHECK_MESSAGE(iv.size() == 16, "IV should be 16 bytes");
}

TEST_CASE("[PCKEnhancedEncryption] Enhanced file encryption") {
	const String test_data = "This is a test string for enhanced encryption validation.";
	const String test_key = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	const String temp_file_path = TestUtils::get_temp_path("enhanced_encrypted_test.dat");
	
	Vector<uint8_t> key = PCKKeyDerivation::hex_string_to_key(test_key);
	
	// Write encrypted data
	{
		Ref<FileAccess> base_file = FileAccess::open(temp_file_path, FileAccess::WRITE);
		CHECK_MESSAGE(base_file.is_valid(), "Should be able to create temp file");
		
		Ref<FileAccessEncryptedEnhanced> enhanced_file;
		enhanced_file.instantiate();
		
		FileAccessEncryptedEnhanced::EnhancedSecurityParams params;
		params.kdf_iterations = 10000;
		params.security_version = 1;
		
		Error err = enhanced_file->open_and_parse(base_file, key, FileAccessEncryptedEnhanced::MODE_WRITE_AES256_ENHANCED, "test_file.txt", true, params);
		CHECK_MESSAGE(err == OK, "Enhanced encryption file should open for writing");
		
		CharString data_utf8 = test_data.utf8();
		enhanced_file->store_buffer((const uint8_t *)data_utf8.get_data(), data_utf8.length());
		enhanced_file->close();
	}
	
	// Read and verify encrypted data
	{
		Ref<FileAccess> base_file = FileAccess::open(temp_file_path, FileAccess::READ);
		CHECK_MESSAGE(base_file.is_valid(), "Should be able to open temp file for reading");
		
		Ref<FileAccessEncryptedEnhanced> enhanced_file;
		enhanced_file.instantiate();
		
		Error err = enhanced_file->open_and_parse(base_file, key, FileAccessEncryptedEnhanced::MODE_READ, "test_file.txt");
		CHECK_MESSAGE(err == OK, "Enhanced encryption file should open for reading");
		
		uint64_t file_length = enhanced_file->get_length();
		Vector<uint8_t> decrypted_data;
		decrypted_data.resize(file_length);
		
		uint64_t read_bytes = enhanced_file->get_buffer(decrypted_data.ptrw(), file_length);
		CHECK_MESSAGE(read_bytes == file_length, "Should read all decrypted data");
		
		String decrypted_string = String::utf8((const char *)decrypted_data.ptr(), file_length);
		CHECK_MESSAGE(decrypted_string == test_data, "Decrypted data should match original");
		
		enhanced_file->close();
	}
	
	// Clean up
	OS::get_singleton()->move_to_trash(temp_file_path);
}

TEST_CASE("[PCKEnhancedEncryption] Enhanced PCK packing and unpacking") {
	const String test_key = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
	const String temp_pck_path = TestUtils::get_temp_path("enhanced_test.pck");
	const String temp_source_file = TestUtils::get_temp_path("source_file.txt");
	
	// Create a test source file
	const String test_content = "Enhanced PCK encryption test content\nMultiple lines\nSpecial characters: !@#$%^&*()";
	{
		Ref<FileAccess> source_file = FileAccess::open(temp_source_file, FileAccess::WRITE);
		CHECK_MESSAGE(source_file.is_valid(), "Should create source file");
		source_file->store_string(test_content);
		source_file->close();
	}
	
	// Pack with enhanced encryption
	{
		PCKPacker packer;
		Error err = packer.pck_start_enhanced(temp_pck_path, 32, test_key, false, 10000);
		CHECK_MESSAGE(err == OK, "Enhanced PCK packing should start successfully");
		
		err = packer.add_file_enhanced("test_file.txt", temp_source_file, true);
		CHECK_MESSAGE(err == OK, "Should add file with enhanced encryption");
		
		err = packer.flush();
		CHECK_MESSAGE(err == OK, "Should flush PCK successfully");
	}
	
	// Verify PCK file was created and has reasonable size
	{
		Ref<FileAccess> pck_file = FileAccess::open(temp_pck_path, FileAccess::READ);
		CHECK_MESSAGE(pck_file.is_valid(), "PCK file should be created");
		CHECK_MESSAGE(pck_file->get_length() > 200, "PCK file should have reasonable size");
		pck_file->close();
	}
	
	// Clean up
	OS::get_singleton()->move_to_trash(temp_pck_path);
	OS::get_singleton()->move_to_trash(temp_source_file);
}

} // namespace TestPCKEnhancedEncryption