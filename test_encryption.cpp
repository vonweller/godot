#include <godot.h>
#include <editor/export/editor_export.h>
#include <editor/export/project_export.h>

using namespace godot;

void test_script_encryption_mode() {
    // Create a test preset
    Ref<EditorExportPreset> preset;
    preset.instantiate();
    
    // Test AES256 mode
    preset->set_script_encryption_mode(EditorExportPreset::MODE_SCRIPT_ENCRYPTION_AES256);
    int aes256_mode = preset->get_script_encryption_mode();
    print_line("AES256 mode: ", aes256_mode);
    
    // Test XOR mode
    preset->set_script_encryption_mode(EditorExportPreset::MODE_SCRIPT_ENCRYPTION_XOR);
    int xor_mode = preset->get_script_encryption_mode();
    print_line("XOR mode: ", xor_mode);
    
    // Test None mode
    preset->set_script_encryption_mode(EditorExportPreset::MODE_SCRIPT_ENCRYPTION_NONE);
    int none_mode = preset->get_script_encryption_mode();
    print_line("None mode: ", none_mode);
}