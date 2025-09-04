/**************************************************************************/
/*  enhanced_pck_encryption_example.h                                     */
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

#include "core/io/pck_packer.h"

/*
 * Enhanced PCK Encryption Usage Example
 * 
 * This demonstrates how to use the new enhanced PCK encryption system
 * which provides multi-layer key derivation for significantly improved security.
 * 
 * Key Features:
 * - PBKDF2-based master key derivation (configurable iterations)
 * - HKDF file-specific key derivation with context
 * - HMAC-based final key mixing with file metadata
 * - Deterministic but unpredictable IV generation
 * - Backward compatible with existing PCK encryption
 */

class EnhancedPCKEncryptionExample {
public:
    // Example 1: Basic enhanced encryption
    static Error create_enhanced_pck_basic() {
        PCKPacker packer;
        
        // Start enhanced PCK with custom iteration count
        // Higher iterations = more secure but slower
        Error err = packer.pck_start_enhanced(
            "my_game_enhanced.pck",        // Output PCK file
            32,                            // Alignment
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", // 64-char hex key
            false,                         // Don't encrypt directory (yet)
            100000                         // PBKDF2 iterations (100k recommended)
        );
        
        if (err != OK) {
            print_error("Failed to start enhanced PCK");
            return err;
        }
        
        // Add files with enhanced encryption
        err = packer.add_file_enhanced("scripts/player.gd", "source/scripts/player.gd", true);
        if (err != OK) {
            print_error("Failed to add enhanced encrypted file");
            return err;
        }
        
        // Add unencrypted files (still works)
        err = packer.add_file_enhanced("textures/background.png", "source/textures/background.png", false);
        if (err != OK) {
            print_error("Failed to add unencrypted file");
            return err;
        }
        
        // Flush to finalize PCK
        err = packer.flush(true); // verbose output
        if (err != OK) {
            print_error("Failed to flush enhanced PCK");
            return err;
        }
        
        print_line("Enhanced PCK created successfully!");
        return OK;
    }
    
    // Example 2: High-security configuration
    static Error create_high_security_pck() {
        PCKPacker packer;
        
        // High-security configuration
        Error err = packer.pck_start_enhanced(
            "my_game_high_security.pck",
            32,
            "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210", // Different key
            true,                          // Encrypt directory structure too
            500000                         // Higher iterations for maximum security
        );
        
        if (err != OK) {
            return err;
        }
        
        // Encrypt all critical game files
        const char* critical_files[] = {
            "scripts/game_logic.gd",
            "scripts/player_controller.gd", 
            "scripts/enemy_ai.gd",
            "data/game_config.json",
            "data/level_data.dat"
        };
        
        for (int i = 0; i < 5; i++) {
            err = packer.add_file_enhanced(critical_files[i], String("source/") + critical_files[i], true);
            if (err != OK) {
                print_error(String("Failed to add: ") + critical_files[i]);
                return err;
            }
        }
        
        err = packer.flush(true);
        return err;
    }
    
    // Example 3: Mixed encryption modes (backward compatibility)
    static Error create_mixed_encryption_pck() {
        PCKPacker packer;
        
        Error err = packer.pck_start_enhanced(
            "my_game_mixed.pck",
            32,
            "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
            false,
            75000
        );
        
        if (err != OK) {
            return err;
        }
        
        // Use enhanced encryption for sensitive files
        packer.add_file_enhanced("scripts/core/game_state.gd", "source/scripts/core/game_state.gd", true);
        packer.add_file_enhanced("data/save_data.dat", "source/data/save_data.dat", true);
        
        // Use traditional encryption for some files (if needed for compatibility)
        // Note: This would require switching back to regular mode, so not recommended
        
        // Leave public assets unencrypted
        packer.add_file_enhanced("images/ui/button.png", "source/images/ui/button.png", false);
        packer.add_file_enhanced("sounds/music/theme.ogg", "source/sounds/music/theme.ogg", false);
        
        return packer.flush(true);
    }
    
    // Security recommendations
    static void print_security_recommendations() {
        print_line("=== Enhanced PCK Encryption Security Recommendations ===");
        print_line("");
        print_line("1. Key Management:");
        print_line("   - Use a strong 64-character hexadecimal key");
        print_line("   - Generate keys using cryptographically secure random generators");
        print_line("   - Never hardcode keys in source code");
        print_line("   - Use environment variables during build: SCRIPT_AES256_ENCRYPTION_KEY");
        print_line("");
        print_line("2. Iteration Count:");
        print_line("   - Minimum: 50,000 iterations (development)");
        print_line("   - Recommended: 100,000 iterations (production)");
        print_line("   - High security: 500,000+ iterations (critical applications)");
        print_line("   - Consider build time vs security trade-off");
        print_line("");
        print_line("3. Encryption Strategy:");
        print_line("   - Encrypt sensitive game logic and data files");
        print_line("   - Consider encrypting directory structure for additional obfuscation");
        print_line("   - Leave large assets (textures, audio) unencrypted for performance");
        print_line("");
        print_line("4. Build Process:");
        print_line("   - Use enhanced encryption for release builds");
        print_line("   - Test thoroughly with encrypted builds");
        print_line("   - Ensure export templates are compiled with the same key");
        print_line("");
        print_line("5. Transparency:");
        print_line("   - Enhanced encryption is completely transparent to your game");
        print_line("   - No code changes needed in your game scripts");
        print_line("   - Files are automatically decrypted during loading");
        print_line("");
    }
};

/*
 * Compilation Instructions:
 * 
 * To compile Godot with enhanced PCK encryption support:
 * 
 * 1. Set your encryption key as environment variable:
 *    set SCRIPT_AES256_ENCRYPTION_KEY=your64characterhexkey
 *    
 * 2. Compile Godot:
 *    scons platform=windows target=release
 *    
 * 3. Your game will automatically use the enhanced encryption when:
 *    - Exporting with "Encrypt PCK" enabled
 *    - The export template was compiled with enhanced encryption support
 *    
 * Usage in Editor:
 * 
 * 1. Open Project Settings -> Export
 * 2. Select your platform preset
 * 3. Enable "Encrypt PCK" 
 * 4. Enter your 64-character hex encryption key
 * 5. Export your project
 * 
 * The exported game will use enhanced encryption automatically!
 */