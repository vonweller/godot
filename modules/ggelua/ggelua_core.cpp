/**************************************************************************/
/*  ggelua_core.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "ggelua_core.h"
#include <algorithm>

namespace GGELUACore {

// RGB565到RGB888颜色转换
Uint32 rgb565_to_888(Uint16 color, Uint8 alpha) {
    Uint32 r = (color >> 11) & 0x1f;
    Uint32 g = (color >> 5) & 0x3f;
    Uint32 b = (color) & 0x1f;

    Uint32 a = alpha << 24;
    Uint32 r32 = ((r << 3) | (r >> 2)) << 16;
    Uint32 g32 = ((g << 2) | (g >> 4)) << 8;
    Uint32 b32 = (b << 3) | (b >> 2);

    return a | r32 | g32 | b32;
}

// 带调色板变换的颜色转换
Uint32 rgb565_to_888_transform(Uint16 color16, 
    Uint32 r1, Uint32 g1, Uint32 b1,
    Uint32 r2, Uint32 g2, Uint32 b2,
    Uint32 r3, Uint32 g3, Uint32 b3) {
    
    Uint32 r = (color16 >> 11) & 0x1f;
    Uint32 g = (color16 >> 5) & 0x3f;
    Uint32 b = (color16) & 0x1f;

    Uint32 r2_calc = r * r1 + g * r2 + b * r3;
    Uint32 g2_calc = r * g1 + g * g2 + b * g3;
    Uint32 b2_calc = r * b1 + g * b2 + b * b3;
    
    r = std::min(r2_calc >> 8, 0x1fU);
    g = std::min(g2_calc >> 8, 0x3fU);
    b = std::min(b2_calc >> 8, 0x1fU);

    Uint32 a = 255 << 24;
    Uint32 r32 = ((r << 3) | (r >> 2)) << 16;
    Uint32 g32 = ((g << 2) | (g >> 4)) << 8;
    Uint32 b32 = (b << 3) | (b >> 2);

    return a | r32 | g32 | b32;
}

// 哈希计算（兼容原游戏算法）
static void string_adjust(const char* path, char* output) {
    int i;
    strncpy(output, path, 260);
    output[259] = '\0';

    for (i = 0; output[i]; i++) {
        if (output[i] >= 'A' && output[i] <= 'Z') {
            output[i] += 'a' - 'A';
        } else if (output[i] == '/') {
            output[i] = '\\';
        }
    }
}

Uint32 calculate_hash(const char* path) {
    Uint32 m[70];
    memset(m, 0, sizeof(m));
    
    string_adjust(path, (char*)m);
    
    Uint32 i;
    for (i = 0; i < 256 / 4 && ((char*)m)[i * 4]; i++);
    
    m[i++] = 0x9BE74448;
    m[i++] = 0x66F42C48;
    
    Uint32 v = 0xF4FA8928;
    Uint32 edi = 0x7758B42B;
    Uint32 esi = 0x37A8470E;
    
    for (Uint32 ecx = 0; ecx < i; ecx++) {
        Uint32 ebx = 0x267B0B11;
        v = (v << 1) | (v >> 0x1F);
        ebx ^= v;
        Uint32 eax = m[ecx];
        esi ^= eax;
        edi ^= eax;
        Uint32 edx = ebx;
        edx += edi;
        edx |= 0x02040801;
        edx &= 0xBFEF7FDF;
        uint64_t num = (uint64_t)edx * esi;
        eax = (Uint32)num;
        edx = (Uint32)(num >> 32);
        if (edx != 0) {
            eax++;
        }
        num = (uint64_t)eax + edx;
        eax = (Uint32)num;
        if ((num >> 32) != 0) {
            eax++;
        }
        edx = ebx;
        edx += esi;
        edx |= 0x00804021;
        edx &= 0x7DFEFBFF;
        esi = eax;
        num = (uint64_t)edi * edx;
        eax = (Uint32)num;
        edx = (Uint32)(num >> 32);
        num = (uint64_t)edx + edx;
        edx = (Uint32)num;
        if ((num >> 32) != 0) {
            eax++;
        }
        num = (uint64_t)eax + edx;
        eax = (Uint32)num;
        if ((num >> 32) != 0) {
            eax += 2;
        }
        edi = eax;
    }
    esi ^= edi;
    return esi;
}

// JPEG格式修复（云风格式）
Uint32 fix_jpeg_format(const Uint8* input, Uint32 input_size, Uint8* output) {
    const Uint8* inbuf = input;
    Uint8* outbuf = output;
    Uint32 out_pos = 0;
    Uint32 in_pos = 0;
    
    while (in_pos < input_size && inbuf[in_pos] == 0xFF) {
        outbuf[out_pos++] = 0xFF;
        in_pos++;
        
        switch (inbuf[in_pos]) {
            case 0xD8: // SOI
                outbuf[out_pos++] = 0xD8;
                in_pos++;
                break;
                
            case 0xA0: // 云风格式标记 - 跳过
                in_pos++;
                out_pos--; // 移除前面的FF
                break;
                
            case 0xDA: // SOS
                outbuf[out_pos++] = 0xDA;
                outbuf[out_pos++] = 0x00;
                outbuf[out_pos++] = 0x0C;
                in_pos++;
                
                // 跳过原长度字段
                in_pos += 2;
                
                // 处理扫描数据
                while (in_pos < input_size - 2 && out_pos < input_size * 2) {
                    if (inbuf[in_pos] == 0xFF) {
                        outbuf[out_pos++] = 0xFF;
                        outbuf[out_pos++] = 0x00; // FF转义
                        in_pos++;
                    } else {
                        outbuf[out_pos++] = inbuf[in_pos++];
                    }
                }
                
                // 添加结束标记
                outbuf[out_pos++] = 0xFF;
                outbuf[out_pos++] = 0xD9;
                return out_pos;
                
            default: {
                // 复制其他段
                outbuf[out_pos++] = inbuf[in_pos++];
                if (in_pos < input_size - 1) {
                    Uint16 len = (inbuf[in_pos] << 8) | inbuf[in_pos + 1];
                    for (int i = 0; i < len && in_pos < input_size; i++) {
                        outbuf[out_pos++] = inbuf[in_pos++];
                    }
                }
                break;
            }
        }
    }
    
    return out_pos;
}

// 简化的LZO解压实现
int lzo_decompress(const void* input, Uint32 input_size, void* output, Uint32 output_size) {
    const Uint8* ip = (const Uint8*)input;
    Uint8* op = (Uint8*)output;
    const Uint8* const ip_end = ip + input_size;
    const Uint8* const op_end = op + output_size;
    const Uint8* start_op = op;
    
    Uint32 t;
    const Uint8* m_pos;

    if (ip >= ip_end) return 0;
    
    if (*ip > 17) {
        t = *ip++ - 17;
        if (t < 4) {
            goto match_next;
        }
        if (op + t > op_end || ip + t > ip_end) return -1;
        do {
            *op++ = *ip++;
        } while (--t > 0);
        goto first_literal_run;
    }

    while (ip < ip_end && op < op_end) {
        t = *ip++;
        if (t >= 16) {
            goto match;
        }
        
        if (t == 0) {
            while (ip < ip_end && *ip == 0) {
                t += 255;
                ip++;
            }
            if (ip >= ip_end) break;
            t += 15 + *ip++;
        }

        // 复制字面量
        if (op + t + 4 > op_end || ip + t + 4 > ip_end) break;
        for (Uint32 i = 0; i < t + 4; i++) {
            *op++ = *ip++;
        }

    first_literal_run:
        if (ip >= ip_end) break;
        t = *ip++;
        if (t >= 16) {
            goto match;
        }

        m_pos = op - 0x0801;
        m_pos -= t >> 2;
        if (ip >= ip_end) break;
        m_pos -= *ip++ << 2;

        if (m_pos < start_op || op + 3 > op_end) break;
        *op++ = *m_pos++;
        *op++ = *m_pos++;
        *op++ = *m_pos;
        goto match_done;

        while (ip < ip_end && op < op_end) {
        match:
            if (t >= 64) {
                m_pos = op - 1;
                m_pos -= (t >> 2) & 7;
                if (ip >= ip_end) break;
                m_pos -= *ip++ << 3;
                t = (t >> 5) - 1;
                goto copy_match;
            } else if (t >= 32) {
                t &= 31;
                if (t == 0) {
                    while (ip < ip_end && *ip == 0) {
                        t += 255;
                        ip++;
                    }
                    if (ip >= ip_end) break;
                    t += 31 + *ip++;
                }
                m_pos = op - 1;
                if (ip + 1 >= ip_end) break;
                m_pos -= (ip[0] | (ip[1] << 8)) >> 2;
                ip += 2;
            } else if (t >= 16) {
                m_pos = op;
                m_pos -= (t & 8) << 11;
                t &= 7;
                if (t == 0) {
                    while (ip < ip_end && *ip == 0) {
                        t += 255;
                        ip++;
                    }
                    if (ip >= ip_end) break;
                    t += 7 + *ip++;
                }
                if (ip + 1 >= ip_end) break;
                m_pos -= (ip[0] | (ip[1] << 8)) >> 2;
                ip += 2;
                if (m_pos == op) {
                    goto eof_found;
                }
                m_pos -= 0x4000;
            } else {
                m_pos = op - 1;
                m_pos -= t >> 2;
                if (ip >= ip_end) break;
                m_pos -= *ip++ << 2;
                if (m_pos < start_op || op + 2 > op_end) break;
                *op++ = *m_pos++;
                *op++ = *m_pos;
                goto match_done;
            }

        copy_match:
            if (m_pos < start_op || op + t + 2 > op_end) break;
            *op++ = *m_pos++;
            *op++ = *m_pos++;
            do {
                *op++ = *m_pos++;
            } while (--t > 0);

        match_done:
            t = ip[-2] & 3;
            if (t == 0) {
                break;
            }

        match_next:
            if (op + t > op_end || ip + t > ip_end) break;
            do {
                *op++ = *ip++;
            } while (--t > 0);
            if (ip >= ip_end) break;
            t = *ip++;
        }
    }

eof_found:
    return (int)(op - start_op);
}

// TCP帧解码
bool decode_tcp_frame(const Uint8* data, Uint32 data_size, 
    const Uint32* palette, TCPFrameInfo& info, Uint8* output) {
    
    if (data_size < sizeof(TCPFrameInfo)) {
        return false;
    }
    
    // 读取帧信息
    memcpy(&info, data, sizeof(TCPFrameInfo));
    
    if (info.width == 0 || info.height == 0) {
        return false;
    }
    
    const Uint8* frame_data = data + sizeof(TCPFrameInfo);
    const Uint32* line_offsets = (const Uint32*)frame_data;
    
    Uint32* pixels = (Uint32*)output;
    
    // 解码每一行
    for (Uint32 h = 0; h < info.height; h++) {
        Uint32* line_pixels = pixels + h * info.width;
        const Uint8* line_data = data + line_offsets[h];
        
        if (*line_data == 0) {
            // 隔行处理
            if (h > 0) {
                memcpy(line_pixels, line_pixels - info.width, info.width * 4);
            }
            continue;
        }
        
        Uint32 x = 0;
        while (*line_data && x < info.width) {
            Uint8 style = (*line_data & 0xC0) >> 6;
            
            switch (style) {
                case 0: { // 透明像素
                    if (*line_data & 0x20) {
                        // 单个带Alpha像素
                        Uint8 alpha = ((*line_data++) & 0x1F) << 3;
                        Uint32 color = (palette[*line_data++] & 0xFFFFFF) | (alpha << 24);
                        if (x < info.width) {
                            line_pixels[x++] = color;
                        }
                    } else {
                        // 重复带Alpha像素
                        Uint8 repeat = (*line_data++) & 0x1F;
                        Uint8 alpha = (*line_data++) << 3;
                        Uint32 color = (palette[*line_data++] & 0xFFFFFF) | (alpha << 24);
                        while (repeat-- && x < info.width) {
                            line_pixels[x++] = color;
                        }
                    }
                    break;
                }
                case 1: { // 不重复像素段
                    Uint8 count = (*line_data++) & 0x3F;
                    while (count-- && x < info.width) {
                        line_pixels[x++] = palette[*line_data++];
                    }
                    break;
                }
                case 2: { // 重复像素
                    Uint8 repeat = (*line_data++) & 0x3F;
                    Uint32 color = palette[*line_data++];
                    while (repeat-- && x < info.width) {
                        line_pixels[x++] = color;
                    }
                    break;
                }
                case 3: { // 跳过像素
                    Uint8 skip = (*line_data++) & 0x3F;
                    if (skip == 0) {
                        // 边缘处理
                        if (x > 0) {
                            line_pixels[x-1] |= 0xFF000000;
                        }
                        line_data += 2;
                        break;
                    }
                    x += skip; // 跳过透明像素
                    break;
                }
            }
        }
    }
    
    return true;
}

} // namespace GGELUACore