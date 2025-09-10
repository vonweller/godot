/**************************************************************************/
/*  ggelua_core.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>

// 定义基本类型，避免依赖SDL
using Uint8 = uint8_t;
using Uint16 = uint16_t;
using Uint32 = uint32_t;
using Sint16 = int16_t;
using Sint32 = int32_t;

// TCP格式结构定义
struct TCPHead {
    Uint16 flag;   // 'PS' 0x5053 or 'PR' 0x5052
    Uint16 len;    // 文件头长度
    Uint16 group;  // 组数
    Uint16 frame;  // 帧数
    Uint16 width;  // 宽度
    Uint16 height; // 高度
    Sint16 x;      // 关键位X
    Sint16 y;      // 关键位Y
};

struct TCPFrameInfo {
    Sint32 x;      // 图片关键位X
    Sint32 y;      // 图片关键位Y
    Uint32 width;  // 图片宽度
    Uint32 height; // 图片高度
};

// WDF格式结构定义
struct WDFHead {
    Uint32 flag;   // 'WDFP' 0x50464457
    Uint32 number; // 文件数量
    Uint32 offset; // 文件列表偏移
};

struct WDFFileInfo {
    Uint32 hash;   // 文件名哈希
    Uint32 offset; // 文件偏移
    Uint32 size;   // 文件大小
    Uint32 unused; // 未使用空间
};

// 地图格式结构定义
struct MapHeader {
    Uint32 flag;   // 'M1.0' 0x302E314D or 'MAPX' 0x5850414D
    Uint32 width;  // 地图宽度
    Uint32 height; // 地图高度
};

struct MapBlockInfo {
    Uint32 flag; // 块类型标识
    Uint32 size; // 块大小
};

// 核心工具函数
namespace GGELUACore {
    // 颜色转换
    Uint32 rgb565_to_888(Uint16 color, Uint8 alpha);
    Uint32 rgb565_to_888_transform(Uint16 color16, 
        Uint32 r1, Uint32 g1, Uint32 b1,
        Uint32 r2, Uint32 g2, Uint32 b2,
        Uint32 r3, Uint32 g3, Uint32 b3);
    
    // 哈希计算
    Uint32 calculate_hash(const char* path);
    
    // JPEG修复
    Uint32 fix_jpeg_format(const Uint8* input, Uint32 input_size, Uint8* output);
    
    // LZO解压
    int lzo_decompress(const void* input, Uint32 input_size, void* output, Uint32 output_size);
    
    // TCP解码
    bool decode_tcp_frame(const Uint8* data, Uint32 data_size, 
        const Uint32* palette, TCPFrameInfo& info, Uint8* output);
}