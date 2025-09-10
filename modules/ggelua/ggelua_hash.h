/**************************************************************************/
/*  ggelua_hash.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"

class GGELUAHash : public RefCounted {
	GDCLASS(GGELUAHash, RefCounted);

protected:
	static void _bind_methods();
	
	static void string_adjust(const String &path, char *output);

public:
	GGELUAHash();
	~GGELUAHash();

	static uint32_t calculate_hash(const String &path);
};