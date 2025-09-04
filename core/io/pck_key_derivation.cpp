/**************************************************************************/
/*  pck_key_derivation.cpp                                                */
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

#include "pck_key_derivation.h"

#include "core/io/file_access_pack.h"

static CryptoCore::RandomGenerator *_key_derivation_rng = nullptr;

void PCKKeyDerivation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("derive_master_key"), &PCKKeyDerivation::derive_master_key);
	ClassDB::bind_method(D_METHOD("derive_file_key", "file_path", "context"), &PCKKeyDerivation::derive_file_key, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("finalize_encryption_key", "file_size", "file_md5"), &PCKKeyDerivation::finalize_encryption_key);
	ClassDB::bind_method(D_METHOD("generate_file_iv"), &PCKKeyDerivation::generate_file_iv);
	ClassDB::bind_method(D_METHOD("get_final_key"), &PCKKeyDerivation::get_final_key);
	ClassDB::bind_method(D_METHOD("clear_keys"), &PCKKeyDerivation::clear_keys);
	
	ClassDB::bind_static_method("PCKKeyDerivation", D_METHOD("hex_string_to_key", "hex_string"), &PCKKeyDerivation::hex_string_to_key);
}

Error PCKKeyDerivation::initialize(const Vector<uint8_t> &p_user_key, const SecurityParameters &p_params) {
	ERR_FAIL_COND_V(p_user_key.size() != 32, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!validate_security_parameters(p_params), ERR_INVALID_PARAMETER);
	
	user_key = p_user_key;
	security_params = p_params;
	initialized = true;
	
	return OK;
}

PCKKeyDerivation::SecurityParameters PCKKeyDerivation::generate_security_parameters(uint32_t p_iterations) {
	SecurityParameters params;
	params.kdf_iterations = p_iterations;
	params.security_version = 1;
	
	// Generate random salt
	if (unlikely(!_key_derivation_rng)) {
		_key_derivation_rng = memnew(CryptoCore::RandomGenerator);
		if (_key_derivation_rng->init() != OK) {
			memdelete(_key_derivation_rng);
			_key_derivation_rng = nullptr;
			ERR_FAIL_V_MSG(params, "Failed to initialize random number generator for key derivation.");
		}
	}
	
	Error err = _key_derivation_rng->get_random_bytes(params.master_salt, 32);
	if (err != OK) {
		ERR_PRINT("Failed to generate random salt for key derivation");
		// Fill with zeros as fallback (not ideal but prevents crashes)
		memset(params.master_salt, 0, 32);
	}
	
	return params;
}

Error PCKKeyDerivation::derive_master_key() {
	ERR_FAIL_COND_V(!initialized, ERR_UNCONFIGURED);
	
	master_key.resize(32);
	
	// Layer 1: Use PBKDF2-HMAC-SHA256 to derive master key from user key
	Error err = CryptoCore::pbkdf2_hmac_sha256(
		user_key.ptr(), user_key.size(),
		security_params.master_salt, 32,
		security_params.kdf_iterations,
		master_key.ptrw(), 32
	);
	
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to derive master key using PBKDF2.");
	
	return OK;
}

Error PCKKeyDerivation::derive_file_key(const String &p_file_path, const String &p_context) {
	ERR_FAIL_COND_V(master_key.size() != 32, ERR_UNCONFIGURED);
	
	file_key.resize(32);
	file_path = p_file_path;
	
	// Layer 2: Build context information for HKDF
	// Format: file_path + "|" + PCK_version + "|" + context + "|" + security_version
	String context_str = p_file_path + "|" + String::num(PACK_FORMAT_VERSION) + "|" + p_context + "|" + String::num(security_params.security_version);
	CharString cs = context_str.utf8();
	
	// Use HKDF-SHA256 to derive file-specific key
	Error err = CryptoCore::hkdf_sha256(
		nullptr, 0,                           // No additional salt (salt was already used in PBKDF2)
		master_key.ptr(), master_key.size(),
		(const uint8_t *)cs.get_data(), cs.length(),
		file_key.ptrw(), 32
	);
	
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to derive file key using HKDF.");
	
	return OK;
}

Error PCKKeyDerivation::finalize_encryption_key(uint64_t p_file_size, const Vector<uint8_t> &p_file_md5) {
	ERR_FAIL_COND_V(file_key.size() != 32, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_file_md5.size() != 16, ERR_INVALID_PARAMETER);
	
	final_key.resize(32);
	
	// Layer 3: Create HMAC input by combining file metadata
	Vector<uint8_t> hmac_input;
	hmac_input.resize(8 + 16 + 32 + 1);  // file_size + MD5 + file_key + security_version
	
	// Add file size (8 bytes, big-endian format for consistency across platforms)
	for (int i = 0; i < 8; i++) {
		hmac_input.write[i] = (p_file_size >> (56 - i * 8)) & 0xFF;
	}
	
	// Add MD5 checksum
	memcpy(hmac_input.ptrw() + 8, p_file_md5.ptr(), 16);
	
	// Add file key
	memcpy(hmac_input.ptrw() + 24, file_key.ptr(), 32);
	
	// Add security version for forward compatibility
	hmac_input.write[56] = security_params.security_version;
	
	// Use HMAC-SHA256 to generate final encryption key
	Error err = CryptoCore::hmac_sha256(
		file_key.ptr(), file_key.size(),
		hmac_input.ptr(), hmac_input.size(),
		final_key.ptrw()
	);
	
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to finalize encryption key using HMAC.");
	
	return OK;
}

Vector<uint8_t> PCKKeyDerivation::generate_file_iv() {
	Vector<uint8_t> file_iv;
	file_iv.resize(16);
	
	ERR_FAIL_COND_V(final_key.size() != 32, file_iv);
	ERR_FAIL_COND_V(file_path.is_empty(), file_iv);
	
	// Use final key and file path to generate deterministic but unpredictable IV
	String iv_input = file_path + "|IV_GENERATION|" + String::num(security_params.security_version);
	CharString cs = iv_input.utf8();
	
	uint8_t full_hash[32];
	Error err = CryptoCore::hmac_sha256(
		final_key.ptr(), final_key.size(),
		(const uint8_t *)cs.get_data(), cs.length(),
		full_hash
	);
	
	if (err == OK) {
		// Use first 16 bytes of HMAC as IV
		memcpy(file_iv.ptrw(), full_hash, 16);
	} else {
		ERR_PRINT("Failed to generate file IV, using zero IV as fallback");
		memset(file_iv.ptrw(), 0, 16);
	}
	
	return file_iv;
}

void PCKKeyDerivation::clear_keys() {
	// Clear all sensitive key material
	if (user_key.size() > 0) {
		memset(user_key.ptrw(), 0, user_key.size());
		user_key.clear();
	}
	if (master_key.size() > 0) {
		memset(master_key.ptrw(), 0, master_key.size());
		master_key.clear();
	}
	if (file_key.size() > 0) {
		memset(file_key.ptrw(), 0, file_key.size());
		file_key.clear();
	}
	if (final_key.size() > 0) {
		memset(final_key.ptrw(), 0, final_key.size());
		final_key.clear();
	}
	
	// Clear file path and reset state
	file_path = "";
	initialized = false;
}

Vector<uint8_t> PCKKeyDerivation::hex_string_to_key(const String &p_hex_string) {
	Vector<uint8_t> key;
	
	// Validate hex string format
	if (p_hex_string.length() != 64 || !p_hex_string.is_valid_hex_number(false)) {
		ERR_PRINT("Invalid encryption key format. Expected 64 hexadecimal characters.");
		return key;
	}
	
	key.resize(32);
	String _key = p_hex_string.to_lower();
	
	for (int i = 0; i < 32; i++) {
		int v = 0;
		if (i * 2 < _key.length()) {
			char32_t ct = _key[i * 2];
			if (is_digit(ct)) {
				ct = ct - '0';
			} else if (ct >= 'a' && ct <= 'f') {
				ct = 10 + ct - 'a';
			}
			v |= ct << 4;
		}

		if (i * 2 + 1 < _key.length()) {
			char32_t ct = _key[i * 2 + 1];
			if (is_digit(ct)) {
				ct = ct - '0';
			} else if (ct >= 'a' && ct <= 'f') {
				ct = 10 + ct - 'a';
			}
			v |= ct;
		}
		key.write[i] = v;
	}
	
	return key;
}

bool PCKKeyDerivation::validate_security_parameters(const SecurityParameters &p_params) {
	// Validate iteration count (minimum 10,000 for security, maximum 1,000,000 for performance)
	if (p_params.kdf_iterations < 10000 || p_params.kdf_iterations > 1000000) {
		ERR_PRINT("Invalid KDF iteration count. Must be between 10,000 and 1,000,000.");
		return false;
	}
	
	// Validate security version
	if (p_params.security_version == 0 || p_params.security_version > 255) {
		ERR_PRINT("Invalid security version. Must be between 1 and 255.");
		return false;
	}
	
	// Check if salt is all zeros (indicates uninitialized)
	bool salt_all_zeros = true;
	for (int i = 0; i < 32; i++) {
		if (p_params.master_salt[i] != 0) {
			salt_all_zeros = false;
			break;
		}
	}
	if (salt_all_zeros) {
		ERR_PRINT("Security parameters contain empty salt. This may indicate uninitialized parameters.");
		return false;
	}
	
	return true;
}