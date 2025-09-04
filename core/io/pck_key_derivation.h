/**************************************************************************/
/*  pck_key_derivation.h                                                  */
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
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

// Multi-layer key derivation manager for enhanced PCK encryption
class PCKKeyDerivation : public RefCounted {
	GDCLASS(PCKKeyDerivation, RefCounted);
public:
	struct SecurityParameters {
		uint32_t kdf_iterations = 100000;      // PBKDF2 iterations (configurable)
		uint8_t master_salt[32];               // Master key salt (random)
		uint8_t security_version = 1;          // Security version for future upgrades
		uint8_t reserved[15];                  // Reserved for future use
		
		SecurityParameters() {
			memset(master_salt, 0, sizeof(master_salt));
			memset(reserved, 0, sizeof(reserved));
		}
	};

private:
	Vector<uint8_t> user_key;          // Original user input key
	Vector<uint8_t> master_key;        // Layer 1: PBKDF2 derived master key
	Vector<uint8_t> file_key;          // Layer 2: HKDF derived file-specific key
	Vector<uint8_t> final_key;         // Layer 3: Final encryption key with metadata
	
	SecurityParameters security_params;
	String file_path;                  // Current file path for context
	bool initialized = false;

	static void _bind_methods();

public:
	// Initialize with user key and security parameters
	Error initialize(const Vector<uint8_t> &p_user_key, const SecurityParameters &p_params);
	
	// Generate new security parameters with random salt
	static SecurityParameters generate_security_parameters(uint32_t p_iterations = 100000);
	
	// Layer 1: Derive master key from user key using PBKDF2
	Error derive_master_key();
	
	// Layer 2: Derive file-specific key using HKDF with context
	Error derive_file_key(const String &p_file_path, const String &p_context = "");
	
	// Layer 3: Finalize encryption key by mixing with file metadata
	Error finalize_encryption_key(uint64_t p_file_size, const Vector<uint8_t> &p_file_md5);
	
	// Generate deterministic but unpredictable IV for the file
	Vector<uint8_t> generate_file_iv();
	
	// Get the final encryption key
	Vector<uint8_t> get_final_key() const { return final_key; }
	
	// Get security parameters
	SecurityParameters get_security_parameters() const { return security_params; }
	
	// Clear all sensitive key data
	void clear_keys();
	
	// Utility function to convert hex string to key vector
	static Vector<uint8_t> hex_string_to_key(const String &p_hex_string);
	
	// Validate security parameters
	static bool validate_security_parameters(const SecurityParameters &p_params);
	
	PCKKeyDerivation() {}
	~PCKKeyDerivation() { clear_keys(); }
};