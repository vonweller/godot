/**************************************************************************/
/*  gltf_accessor.cpp                                                     */
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

#include "gltf_accessor.h"
#include "gltf_accessor.compat.inc"

void GLTFAccessor::_bind_methods() {
	BIND_ENUM_CONSTANT(TYPE_SCALAR);
	BIND_ENUM_CONSTANT(TYPE_VEC2);
	BIND_ENUM_CONSTANT(TYPE_VEC3);
	BIND_ENUM_CONSTANT(TYPE_VEC4);
	BIND_ENUM_CONSTANT(TYPE_MAT2);
	BIND_ENUM_CONSTANT(TYPE_MAT3);
	BIND_ENUM_CONSTANT(TYPE_MAT4);

	BIND_ENUM_CONSTANT(COMPONENT_TYPE_NONE);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_SIGNED_BYTE);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_UNSIGNED_BYTE);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_SIGNED_SHORT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_UNSIGNED_SHORT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_SIGNED_INT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_UNSIGNED_INT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_SINGLE_FLOAT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_DOUBLE_FLOAT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_HALF_FLOAT);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_SIGNED_LONG);
	BIND_ENUM_CONSTANT(COMPONENT_TYPE_UNSIGNED_LONG);

	ClassDB::bind_method(D_METHOD("get_buffer_view"), &GLTFAccessor::get_buffer_view);
	ClassDB::bind_method(D_METHOD("set_buffer_view", "buffer_view"), &GLTFAccessor::set_buffer_view);
	ClassDB::bind_method(D_METHOD("get_byte_offset"), &GLTFAccessor::get_byte_offset);
	ClassDB::bind_method(D_METHOD("set_byte_offset", "byte_offset"), &GLTFAccessor::set_byte_offset);
	ClassDB::bind_method(D_METHOD("get_component_type"), &GLTFAccessor::get_component_type);
	ClassDB::bind_method(D_METHOD("set_component_type", "component_type"), &GLTFAccessor::set_component_type);
	ClassDB::bind_method(D_METHOD("get_normalized"), &GLTFAccessor::get_normalized);
	ClassDB::bind_method(D_METHOD("set_normalized", "normalized"), &GLTFAccessor::set_normalized);
	ClassDB::bind_method(D_METHOD("get_count"), &GLTFAccessor::get_count);
	ClassDB::bind_method(D_METHOD("set_count", "count"), &GLTFAccessor::set_count);
	ClassDB::bind_method(D_METHOD("get_accessor_type"), &GLTFAccessor::get_accessor_type);
	ClassDB::bind_method(D_METHOD("set_accessor_type", "accessor_type"), &GLTFAccessor::set_accessor_type);
	ClassDB::bind_method(D_METHOD("get_type"), &GLTFAccessor::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &GLTFAccessor::set_type);
	ClassDB::bind_method(D_METHOD("get_min"), &GLTFAccessor::get_min);
	ClassDB::bind_method(D_METHOD("set_min", "min"), &GLTFAccessor::set_min);
	ClassDB::bind_method(D_METHOD("get_max"), &GLTFAccessor::get_max);
	ClassDB::bind_method(D_METHOD("set_max", "max"), &GLTFAccessor::set_max);
	ClassDB::bind_method(D_METHOD("get_sparse_count"), &GLTFAccessor::get_sparse_count);
	ClassDB::bind_method(D_METHOD("set_sparse_count", "sparse_count"), &GLTFAccessor::set_sparse_count);
	ClassDB::bind_method(D_METHOD("get_sparse_indices_buffer_view"), &GLTFAccessor::get_sparse_indices_buffer_view);
	ClassDB::bind_method(D_METHOD("set_sparse_indices_buffer_view", "sparse_indices_buffer_view"), &GLTFAccessor::set_sparse_indices_buffer_view);
	ClassDB::bind_method(D_METHOD("get_sparse_indices_byte_offset"), &GLTFAccessor::get_sparse_indices_byte_offset);
	ClassDB::bind_method(D_METHOD("set_sparse_indices_byte_offset", "sparse_indices_byte_offset"), &GLTFAccessor::set_sparse_indices_byte_offset);
	ClassDB::bind_method(D_METHOD("get_sparse_indices_component_type"), &GLTFAccessor::get_sparse_indices_component_type);
	ClassDB::bind_method(D_METHOD("set_sparse_indices_component_type", "sparse_indices_component_type"), &GLTFAccessor::set_sparse_indices_component_type);
	ClassDB::bind_method(D_METHOD("get_sparse_values_buffer_view"), &GLTFAccessor::get_sparse_values_buffer_view);
	ClassDB::bind_method(D_METHOD("set_sparse_values_buffer_view", "sparse_values_buffer_view"), &GLTFAccessor::set_sparse_values_buffer_view);
	ClassDB::bind_method(D_METHOD("get_sparse_values_byte_offset"), &GLTFAccessor::get_sparse_values_byte_offset);
	ClassDB::bind_method(D_METHOD("set_sparse_values_byte_offset", "sparse_values_byte_offset"), &GLTFAccessor::set_sparse_values_byte_offset);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffer_view"), "set_buffer_view", "get_buffer_view"); // GLTFBufferViewIndex
	ADD_PROPERTY(PropertyInfo(Variant::INT, "byte_offset"), "set_byte_offset", "get_byte_offset"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "component_type"), "set_component_type", "get_component_type"); // int
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalized"), "set_normalized", "get_normalized"); // bool
	ADD_PROPERTY(PropertyInfo(Variant::INT, "count"), "set_count", "get_count"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "accessor_type"), "set_accessor_type", "get_accessor_type"); // GLTFAccessor::GLTFAccessorType
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_type", "get_type"); // Deprecated, int for GLTFAccessor::GLTFAccessorType
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "min"), "set_min", "get_min"); // Vector<real_t>
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "max"), "set_max", "get_max"); // Vector<real_t>
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sparse_count"), "set_sparse_count", "get_sparse_count"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sparse_indices_buffer_view"), "set_sparse_indices_buffer_view", "get_sparse_indices_buffer_view"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sparse_indices_byte_offset"), "set_sparse_indices_byte_offset", "get_sparse_indices_byte_offset"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sparse_indices_component_type"), "set_sparse_indices_component_type", "get_sparse_indices_component_type"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sparse_values_buffer_view"), "set_sparse_values_buffer_view", "get_sparse_values_buffer_view"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sparse_values_byte_offset"), "set_sparse_values_byte_offset", "get_sparse_values_byte_offset"); // int
}

GLTFBufferViewIndex GLTFAccessor::get_buffer_view() const {
	return buffer_view;
}

void GLTFAccessor::set_buffer_view(GLTFBufferViewIndex p_buffer_view) {
	buffer_view = p_buffer_view;
}

int64_t GLTFAccessor::get_byte_offset() const {
	return byte_offset;
}

void GLTFAccessor::set_byte_offset(int64_t p_byte_offset) {
	byte_offset = p_byte_offset;
}

GLTFAccessor::GLTFComponentType GLTFAccessor::get_component_type() const {
	return component_type;
}

void GLTFAccessor::set_component_type(GLTFComponentType p_component_type) {
	component_type = (GLTFComponentType)p_component_type;
}

bool GLTFAccessor::get_normalized() const {
	return normalized;
}

void GLTFAccessor::set_normalized(bool p_normalized) {
	normalized = p_normalized;
}

int64_t GLTFAccessor::get_count() const {
	return count;
}

void GLTFAccessor::set_count(int64_t p_count) {
	count = p_count;
}

GLTFAccessor::GLTFAccessorType GLTFAccessor::get_accessor_type() const {
	return accessor_type;
}

void GLTFAccessor::set_accessor_type(GLTFAccessorType p_accessor_type) {
	accessor_type = p_accessor_type;
}

int GLTFAccessor::get_type() const {
	return (int)accessor_type;
}

void GLTFAccessor::set_type(int p_accessor_type) {
	accessor_type = (GLTFAccessorType)p_accessor_type; // TODO: Register enum
}

Vector<double> GLTFAccessor::get_min() const {
	return min;
}

void GLTFAccessor::set_min(Vector<double> p_min) {
	min = p_min;
}

Vector<double> GLTFAccessor::get_max() const {
	return max;
}

void GLTFAccessor::set_max(Vector<double> p_max) {
	max = p_max;
}

int64_t GLTFAccessor::get_sparse_count() const {
	return sparse_count;
}

void GLTFAccessor::set_sparse_count(int64_t p_sparse_count) {
	sparse_count = p_sparse_count;
}

GLTFBufferViewIndex GLTFAccessor::get_sparse_indices_buffer_view() const {
	return sparse_indices_buffer_view;
}

void GLTFAccessor::set_sparse_indices_buffer_view(GLTFBufferViewIndex p_sparse_indices_buffer_view) {
	sparse_indices_buffer_view = p_sparse_indices_buffer_view;
}

int64_t GLTFAccessor::get_sparse_indices_byte_offset() const {
	return sparse_indices_byte_offset;
}

void GLTFAccessor::set_sparse_indices_byte_offset(int64_t p_sparse_indices_byte_offset) {
	sparse_indices_byte_offset = p_sparse_indices_byte_offset;
}

GLTFAccessor::GLTFComponentType GLTFAccessor::get_sparse_indices_component_type() const {
	return sparse_indices_component_type;
}

void GLTFAccessor::set_sparse_indices_component_type(GLTFComponentType p_sparse_indices_component_type) {
	sparse_indices_component_type = (GLTFComponentType)p_sparse_indices_component_type;
}

GLTFBufferViewIndex GLTFAccessor::get_sparse_values_buffer_view() const {
	return sparse_values_buffer_view;
}

void GLTFAccessor::set_sparse_values_buffer_view(GLTFBufferViewIndex p_sparse_values_buffer_view) {
	sparse_values_buffer_view = p_sparse_values_buffer_view;
}

int64_t GLTFAccessor::get_sparse_values_byte_offset() const {
	return sparse_values_byte_offset;
}

void GLTFAccessor::set_sparse_values_byte_offset(int64_t p_sparse_values_byte_offset) {
	sparse_values_byte_offset = p_sparse_values_byte_offset;
}
