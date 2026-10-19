#include "register_types.h"

#include "core/object/class_db.h"
#include "mhxy.h"

void initialize_mhxy_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(MHXY);
}

void uninitialize_mhxy_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
