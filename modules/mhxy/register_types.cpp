#include "register_types.h"

#include "core/object/class_db.h"
#include "mhxy.h"

void initialize_mhxy_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// 常驻堆损坏/访问违例取证（见 mhxy.cpp 顶部注释）
	MHXY::install_crash_diag();
	GDREGISTER_CLASS(MHXY);
}

void uninitialize_mhxy_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// 先于 ObjectDB::cleanup（core 级）释放缓存里的 Ref，否则 static 析构会在
	// ObjectDB 拆表后触发，退出必段错误（2026-10-23 定案，见 mhxy.h 注释）。
	MHXY::shutdown_cache();
}
