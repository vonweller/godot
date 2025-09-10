/**************************************************************************/
/*  ggelua_hash.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "ggelua_hash.h"
#include "ggelua_core.h"

void GGELUAHash::_bind_methods() {
	ClassDB::bind_static_method("GGELUAHash", D_METHOD("calculate_hash", "path"), &GGELUAHash::calculate_hash);
}

GGELUAHash::GGELUAHash() {
}

GGELUAHash::~GGELUAHash() {
}

uint32_t GGELUAHash::calculate_hash(const String &path) {
	CharString cs = path.utf8();
	return GGELUACore::calculate_hash(cs.get_data());
}