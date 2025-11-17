/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "gdscript.h"
#include "gdscript_cache.h"
#include "gdscript_parser.h"
#include "gdscript_tokenizer_buffer.h"
#include "gdscript_utility_functions.h"

#ifdef TOOLS_ENABLED
#include "editor/gdscript_highlighter.h"
#include "editor/gdscript_translation_parser_plugin.h"

#ifndef GDSCRIPT_NO_LSP
#include "language_server/gdscript_language_server.h"
#endif
#endif // TOOLS_ENABLED

#ifdef TESTS_ENABLED
#include "tests/test_gdscript.h"
#endif

#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/crypto/crypto_core.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "core/io/marshalls.h"
#include "core/math/math_funcs.h"
#include "core/io/dir_access.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/translations/editor_translation_parser.h"

#ifndef GDSCRIPT_NO_LSP
#include "core/config/engine.h"
#endif
#endif // TOOLS_ENABLED

#ifdef TESTS_ENABLED
#include "tests/test_macros.h"
#endif

GDScriptLanguage *script_language_gd = nullptr;
Ref<ResourceFormatLoaderGDScript> resource_loader_gd;
Ref<ResourceFormatSaverGDScript> resource_saver_gd;
GDScriptCache *gdscript_cache = nullptr;

#ifdef TOOLS_ENABLED

Ref<GDScriptEditorTranslationParserPlugin> gdscript_translation_parser_plugin;

class EditorExportGDScript : public EditorExportPlugin {
	GDCLASS(EditorExportGDScript, EditorExportPlugin);

	static constexpr int DEFAULT_SCRIPT_MODE = EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;
	int script_mode = DEFAULT_SCRIPT_MODE;
	int script_encryption_mode = EditorExportPreset::MODE_SCRIPT_ENCRYPTION_NONE;

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override {
		script_mode = DEFAULT_SCRIPT_MODE;
		script_encryption_mode = EditorExportPreset::MODE_SCRIPT_ENCRYPTION_NONE;

		const Ref<EditorExportPreset> &preset = get_export_preset();
		if (preset.is_valid()) {
			script_mode = preset->get_script_export_mode();
			script_encryption_mode = preset->get_script_encryption_mode();
		}
	}

	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override {
		if (p_path.get_extension() != "gd" || script_mode == EditorExportPreset::MODE_SCRIPT_TEXT) {
			return;
		}

		Vector<uint8_t> file = FileAccess::get_file_as_bytes(p_path);
		if (file.is_empty()) {
			return;
		}

		String source = String::utf8(reinterpret_cast<const char *>(file.ptr()), file.size());
		GDScriptTokenizerBuffer::CompressMode compress_mode = script_mode == EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED ? GDScriptTokenizerBuffer::COMPRESS_ZSTD : GDScriptTokenizerBuffer::COMPRESS_NONE;
		file = GDScriptTokenizerBuffer::parse_code_string(source, compress_mode);
		if (file.is_empty()) {
			return;
		}

		// Apply encryption if needed
		if (script_encryption_mode != EditorExportPreset::MODE_SCRIPT_ENCRYPTION_NONE) {
			const Ref<EditorExportPreset> &preset = get_export_preset();
			if (preset.is_valid()) {
				String encryption_key = preset->get_script_encryption_key();
				
				if (script_encryption_mode == EditorExportPreset::MODE_SCRIPT_ENCRYPTION_AES256) {
					// AES256 encryption
					if (encryption_key.is_empty()) {
						WARN_PRINT("AES256 encryption requested but no key provided. Skipping encryption.");
					} else {
						file = _encrypt_file_aes256(file, encryption_key);
						if (file.is_empty()) {
							WARN_PRINT("Failed to encrypt file with AES256: " + p_path);
							return;
						}
					}
				} else if (script_encryption_mode == EditorExportPreset::MODE_SCRIPT_ENCRYPTION_XOR) {
					// XOR encryption
					file = _encrypt_file_xor(file, encryption_key);
					if (file.is_empty()) {
						WARN_PRINT("Failed to encrypt file with XOR: " + p_path);
						return;
					}
				}
			}
		}

		add_file(p_path.get_basename() + ".gdc", file, true);
	}

private:
	Vector<uint8_t> _encrypt_file_aes256(const Vector<uint8_t> &p_data, const String &p_key) {
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
		String temp_path = OS::get_singleton()->get_cache_path().path_join("temp_encrypted_" + itos(Math::rand()));
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
		rng.get_random_bytes(iv.ptrw(), 16);
		
		// Open for writing with AES256
		Error err = fae->open_and_parse(temp_file, key, FileAccessEncrypted::MODE_WRITE_AES256, true, iv);
		if (err != OK) {
			temp_file->close();
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			da->remove(temp_path);
			return Vector<uint8_t>();
		}
		
		// Store the data with checksum
		fae->store_buffer(data_with_checksum.ptr(), data_with_checksum.size());
		fae->close();
		temp_file->close();
		
		// Read the encrypted data back
		Vector<uint8_t> encrypted_data = FileAccess::get_file_as_bytes(temp_path);
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->remove(temp_path);
		
		return encrypted_data;
	}

	Vector<uint8_t> _encrypt_file_xor(const Vector<uint8_t> &p_data, const String &p_key) {
		// Calculate checksum of the original data
		uint32_t checksum = hash_djb2_buffer(p_data.ptr(), p_data.size());
		
		// Append checksum to the data
		Vector<uint8_t> data_with_checksum;
		data_with_checksum.resize(p_data.size() + 4);
		memcpy(data_with_checksum.ptrw(), p_data.ptr(), p_data.size());
		encode_uint32(checksum, &data_with_checksum.write[p_data.size()]);
		
		if (p_key.is_empty()) {
			// Default XOR key if none provided
			return _xor_data(data_with_checksum, 0xb6);
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
	
	Vector<uint8_t> _xor_data(const Vector<uint8_t> &p_data, uint8_t p_key) {
		Vector<uint8_t> result;
		result.resize(p_data.size());
		
		const uint8_t *src = p_data.ptr();
		uint8_t *dst = result.ptrw();
		
		for (int i = 0; i < p_data.size(); i++) {
			dst[i] = src[i] ^ p_key;
		}
		
		return result;
	}

public:
	virtual String get_name() const override { return "GDScript"; }
};

static void _editor_init() {
	Ref<EditorExportGDScript> gd_export;
	gd_export.instantiate();
	EditorExport::get_singleton()->add_export_plugin(gd_export);

#ifdef TOOLS_ENABLED
	Ref<GDScriptSyntaxHighlighter> gdscript_syntax_highlighter;
	gdscript_syntax_highlighter.instantiate();
	ScriptEditor::get_singleton()->register_syntax_highlighter(gdscript_syntax_highlighter);
#endif

#ifndef GDSCRIPT_NO_LSP
	register_lsp_types();
	GDScriptLanguageServer *lsp_plugin = memnew(GDScriptLanguageServer);
	EditorNode::get_singleton()->add_editor_plugin(lsp_plugin);
#endif // !GDSCRIPT_NO_LSP
}

#endif // TOOLS_ENABLED

void initialize_gdscript_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(GDScript);

		script_language_gd = memnew(GDScriptLanguage);
		ScriptServer::register_language(script_language_gd);

		resource_loader_gd.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_gd);

		resource_saver_gd.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_gd);

		gdscript_cache = memnew(GDScriptCache);

		GDScriptUtilityFunctions::register_functions();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		EditorNode::add_init_callback(_editor_init);

		gdscript_translation_parser_plugin.instantiate();
		EditorTranslationParser::get_singleton()->add_parser(gdscript_translation_parser_plugin, EditorTranslationParser::STANDARD);
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(GDScriptSyntaxHighlighter);
	}
#endif // TOOLS_ENABLED
}

void uninitialize_gdscript_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ScriptServer::unregister_language(script_language_gd);

		if (gdscript_cache) {
			memdelete(gdscript_cache);
		}

		if (script_language_gd) {
			memdelete(script_language_gd);
		}

		ResourceLoader::remove_resource_format_loader(resource_loader_gd);
		resource_loader_gd.unref();

		ResourceSaver::remove_resource_format_saver(resource_saver_gd);
		resource_saver_gd.unref();

		GDScriptParser::cleanup();
		GDScriptUtilityFunctions::unregister_functions();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
#ifndef GDSCRIPT_NO_LSP
		// Find and remove the GDScriptLanguageServer plugin
		// Since GDScriptLanguageServer doesn't have get_singleton(), we need to find it in the editor
		EditorData &editor_data = EditorNode::get_singleton()->get_editor_data();
		for (int i = 0; i < editor_data.get_editor_plugin_count(); i++) {
			EditorPlugin *plugin = editor_data.get_editor_plugin(i);
			GDScriptLanguageServer *lsp = Object::cast_to<GDScriptLanguageServer>(plugin);
			if (lsp) {
				EditorNode::get_singleton()->remove_editor_plugin(lsp);
				break;
			}
		}
#endif // !GDSCRIPT_NO_LSP
		EditorTranslationParser::get_singleton()->remove_parser(gdscript_translation_parser_plugin, EditorTranslationParser::STANDARD);
		gdscript_translation_parser_plugin.unref();
	}
#endif // TOOLS_ENABLED
}

#ifdef TESTS_ENABLED
void test_tokenizer() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_TOKENIZER);
}

void test_tokenizer_buffer() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_TOKENIZER_BUFFER);
}

void test_parser() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_PARSER);
}

void test_compiler() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_COMPILER);
}

void test_bytecode() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_BYTECODE);
}

void test_encryption() {
	GDScriptTests::GDScriptEncryptionTests::run_all_tests();
}

REGISTER_TEST_COMMAND("gdscript-tokenizer", &test_tokenizer);
REGISTER_TEST_COMMAND("gdscript-tokenizer-buffer", &test_tokenizer_buffer);
REGISTER_TEST_COMMAND("gdscript-parser", &test_parser);
REGISTER_TEST_COMMAND("gdscript-compiler", &test_compiler);
REGISTER_TEST_COMMAND("gdscript-bytecode", &test_bytecode);
REGISTER_TEST_COMMAND("gdscript-encryption", &test_encryption);
#endif
