/**************************************************************************/
/*  map_loader.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "map_loader.h"
#include "core/error/error_macros.h"
#include "core/io/image.h"
#include "core/string/print_string.h"

void MapLoader::_bind_methods() {
    ClassDB::bind_method(D_METHOD("open", "path"), &MapLoader::open);
    ClassDB::bind_method(D_METHOD("close"), &MapLoader::close);
    ClassDB::bind_method(D_METHOD("get_map_tile", "map_id"), &MapLoader::get_map_tile);
    ClassDB::bind_method(D_METHOD("get_mask_info", "map_id"), &MapLoader::get_mask_info);
    ClassDB::bind_method(D_METHOD("get_mask_image", "map_id", "mask_index"), &MapLoader::get_mask_image);
    ClassDB::bind_method(D_METHOD("get_cell_data"), &MapLoader::get_cell_data);
    ClassDB::bind_method(D_METHOD("get_map_block_data", "map_id"), &MapLoader::get_map_block_data);
    ClassDB::bind_method(D_METHOD("get_row_count"), &MapLoader::get_row_count);
    ClassDB::bind_method(D_METHOD("get_col_count"), &MapLoader::get_col_count);
    ClassDB::bind_method(D_METHOD("get_map_count"), &MapLoader::get_map_count);
    ClassDB::bind_method(D_METHOD("get_mask_count"), &MapLoader::get_mask_count);
    ClassDB::bind_method(D_METHOD("get_map_size"), &MapLoader::get_map_size);
    ClassDB::bind_method(D_METHOD("get_map_format"), &MapLoader::get_map_format);
    ClassDB::bind_method(D_METHOD("get_file_path"), &MapLoader::get_file_path);
}

MapLoader::MapLoader() {
}

MapLoader::~MapLoader() {
    close();
}

Error MapLoader::open(const String &p_path) {
    close();
    
    file = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_OPEN, "Cannot open map file: " + p_path);
    
    // 读取头部
    Vector<uint8_t> header_data = file->get_buffer(sizeof(MapHeader));
    if (header_data.size() != sizeof(MapHeader)) {
        close();
        ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Cannot read map header");
    }
    memcpy(&header, header_data.ptr(), sizeof(MapHeader));
    
    // 头部信息解析（调试打印已移除）
    
    if (header.flag != 0x302E314D && header.flag != 0x5850414D) { // 'M1.0' 或 'MAPX'
        // 尝试字节序转换
        uint32_t swapped_flag = ((header.flag & 0xFF) << 24) | 
                               (((header.flag >> 8) & 0xFF) << 16) | 
                               (((header.flag >> 16) & 0xFF) << 8) | 
                               ((header.flag >> 24) & 0xFF);
        // Trying byte-swapped flag
        
        if (swapped_flag == 0x302E314D || swapped_flag == 0x5850414D) {
            header.flag = swapped_flag;
            // Using byte-swapped format
        } else {
            close();
            ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid map file format. Got: 0x" + String::num_uint64(header.flag, 16));
        }
    }
    
    // 计算地图分块数
    row_count = (header.height + 239) / 240; // 向上取整
    col_count = (header.width + 319) / 320;  // 向上取整
    map_count = row_count * col_count;
    
    // 读取地图偏移表
    map_offsets.resize(map_count);
    for (uint32_t i = 0; i < map_count; i++) {
        map_offsets.write[i] = file->get_32();
    }
    
    if (header.flag == 0x302E314D) { // M1.0格式
        // 读取遮罩偏移
        uint32_t mask_offset = file->get_32();
        if (mask_offset > 0) {
            file->seek(mask_offset);
            mask_count = file->get_32();
            
            if (mask_count > 0) {
                mask_offsets.resize(mask_count);
                for (uint32_t i = 0; i < mask_count; i++) {
                    mask_offsets.write[i] = file->get_32();
                }
            }
        }
    } else { // MAPX格式
        // 跳过文件大小
        file->get_32();
        
        // 读取JPEG头部
        MapBlockInfo block_info;
        Vector<uint8_t> block_data = file->get_buffer(sizeof(MapBlockInfo));
        if (block_data.size() != sizeof(MapBlockInfo)) {
            close();
            ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Cannot read MAPX block header");
        }
        memcpy(&block_info, block_data.ptr(), sizeof(MapBlockInfo));
        
        jpeg_header.resize(block_info.size);
        file->get_buffer(jpeg_header.ptrw(), block_info.size);
    }
    
    file_path = p_path;
    is_loaded = true;
    return OK;
}

void MapLoader::close() {
    if (file.is_valid()) {
        file->close();
        file.unref();
    }
    map_offsets.clear();
    mask_offsets.clear();
    jpeg_header.clear();
    is_loaded = false;
}

Ref<Image> MapLoader::get_map_tile(int map_id) {
    ERR_FAIL_COND_V(!is_loaded, Ref<Image>());
    ERR_FAIL_COND_V(map_id < 0 || map_id >= (int)map_count, Ref<Image>());
    
    return decode_map_block(map_id);
}

Ref<Image> MapLoader::decode_map_block(uint32_t map_id) {
    file->seek(map_offsets[map_id]);
    
    // 跳过遮罩数量
    uint32_t mask_num = file->get_32();
    // Removed debug print for mask_num
    
    if (header.flag == 0x302E314D && mask_num > 0) { // M1.0
        file->seek(file->get_position() + mask_num * sizeof(uint32_t));
    }
    
    // 读取图像块
    while (true) {
        MapBlockInfo block_info;
        Vector<uint8_t> block_header = file->get_buffer(sizeof(MapBlockInfo));
        if (block_header.size() != sizeof(MapBlockInfo)) {
            print_line("Failed to read block header, got " + String::num_uint64(block_header.size()) + " bytes");
            break;
        }
        memcpy(&block_info, block_header.ptr(), sizeof(MapBlockInfo));
        
        // Removed debug print for block flag
        
        if (block_info.flag == 0x4A504732) { // 'JPG2' - 梦幻普通JPG
            PackedByteArray jpeg_data = file->get_buffer(block_info.size);
            // Loading JPG2 block
            
            // 使用SDL_image加载JPEG数据
            Ref<Image> img = memnew(Image);
            Error err = img->load_jpg_from_buffer(jpeg_data);
            if (err == OK) {
                // JPG2 block loaded
                return img;
            } else {
                print_line("Failed to load JPG2 block, error: " + String::num_uint64(err));
                return Ref<Image>();
            }
        }
        else if (block_info.flag == 0x31474E50) { // 'PNG1'
            PackedByteArray png_data = file->get_buffer(block_info.size);
            print_line("Found PNG1 block");
            Ref<Image> img = memnew(Image);
            img->load_png_from_buffer(png_data);
            return img;
        }
        else if (block_info.flag == 0x50424557) { // 'WEBP'  
            PackedByteArray webp_data = file->get_buffer(block_info.size);
            print_line("Found WEBP block");
            Ref<Image> img = memnew(Image);
            img->load_webp_from_buffer(webp_data);
            return img;
        }
        else if (block_info.flag == 0x47454A50) { // 'JPEG'
            PackedByteArray jpeg_data = file->get_buffer(block_info.size);
            print_line("Found JPEG block");
            
            if (header.flag == 0x302E314D) { // M1.0格式
                // 检查是否为云风格式
                if (jpeg_data.size() >= 4) {
                    const uint16_t *data16 = (const uint16_t *)jpeg_data.ptr();
                    if (data16[1] == 0xA0FF) {
                        print_line("Fixing cloud JPEG format");
                        // 修复云风JPEG格式
                        jpeg_data = fix_jpeg_format(jpeg_data);
                    }
                }
                Ref<Image> img = memnew(Image);
                img->load_jpg_from_buffer(jpeg_data);
                return img;
            } else { // MAPX格式
                // 合并JPEG头部和数据
                PackedByteArray full_jpeg;
                full_jpeg.resize(jpeg_header.size() + jpeg_data.size());
                memcpy(full_jpeg.ptrw(), jpeg_header.ptr(), jpeg_header.size());
                memcpy(full_jpeg.ptrw() + jpeg_header.size(), jpeg_data.ptr(), jpeg_data.size());
                
                Ref<Image> img = memnew(Image);
                img->load_jpg_from_buffer(full_jpeg);
                return img;
            }
        }
        else if (block_info.flag == 0x43454C4C) { // 'CELL' - 跳过障碍数据
            // Found CELL block (collision data), skipping
            file->seek(file->get_position() + block_info.size);
        }
        else if (block_info.flag == 0x42524947) { // 'BRIG' - 跳过亮度数据  
            print_line("Found BRIG block (brightness data), skipping");
            file->seek(file->get_position() + block_info.size);
        }
        else if (block_info.flag == 0) { // 结束标记
            print_line("Found end marker, skipping " + String::num_uint64(block_info.size) + " bytes");
            file->seek(file->get_position() + block_info.size);
            break;
        }
        else {
            // 跳过未知块
            print_line("Unknown block type: 0x" + String::num_uint64(block_info.flag, 16) + ", skipping " + String::num_uint64(block_info.size) + " bytes");
            file->seek(file->get_position() + block_info.size);
        }
    }
    
    print_line("No valid image block found for map " + String::num_uint64(map_id));
    return Ref<Image>();
}

PackedByteArray MapLoader::fix_jpeg_format(const PackedByteArray &input_data) {
    PackedByteArray output;
    output.resize(input_data.size() + 3); // 预留额外空间
    
    uint32_t result_size = GGELUACore::fix_jpeg_format(
        input_data.ptr(), input_data.size(), output.ptrw());
    
    output.resize(result_size);
    return output;
}

Array MapLoader::get_mask_info(int map_id) {
    Array result;
    ERR_FAIL_COND_V(!is_loaded, result);
    ERR_FAIL_COND_V(map_id < 0 || map_id >= (int)map_count, result);
    
    // 跳转到地图块位置
    file->seek(map_offsets[map_id]);
    
    // 读取遮罩数量
    uint32_t mask_num = file->get_32();
    // Map mask count info removed
    
    if (mask_num == 0) {
        return result;
    }
    
    if (header.flag == 0x302E314D) { // M1.0格式
        // 读取遮罩ID列表
        for (uint32_t i = 0; i < mask_num; i++) {
            uint32_t mask_id = file->get_32();
            if (mask_id < mask_count) {
                Dictionary mask_info;
                mask_info["id"] = mask_id;
                mask_info["offset"] = mask_offsets[mask_id];
                
                // 读取遮罩详细信息
                int64_t current_pos = file->get_position();
                file->seek(mask_offsets[mask_id]);
                
                // M1.0格式：先读矩形，再读大小
                int32_t x = file->get_32();
                int32_t y = file->get_32();
                int32_t w = file->get_32();
                int32_t h = file->get_32();
                uint32_t size = file->get_32();
                
                mask_info["x"] = x;
                mask_info["y"] = y;
                mask_info["w"] = w;
                mask_info["h"] = h;
                mask_info["size"] = size;
                
                result.push_back(mask_info);
                
                // 恢复位置
                file->seek(current_pos);
            }
        }
    } else { // MAPX格式
        // 解析MASK块
        while (true) {
            MapBlockInfo block_info;
            Vector<uint8_t> block_header = file->get_buffer(sizeof(MapBlockInfo));
            if (block_header.size() != sizeof(MapBlockInfo)) {
                break;
            }
            memcpy(&block_info, block_header.ptr(), sizeof(MapBlockInfo));
            
            if (block_info.flag == 0x4B53414D) { // 'MASK'
                // MAPX格式：先读大小，再读矩形
                uint32_t size = file->get_32();
                int32_t x = file->get_32();
                int32_t y = file->get_32();
                int32_t w = file->get_32();
                int32_t h = file->get_32();
                
                size -= 16; // 减去矩形结构的大小
                
                // 调整坐标（旧地图坐标是相对于地表）
                x += (map_id % col_count) * 320;
                y += (map_id / col_count) * 240;
                
                Dictionary mask_info;
                mask_info["id"] = map_id;
                mask_info["offset"] = file->get_position() - 4; // 回退到size位置
                mask_info["x"] = x;
                mask_info["y"] = y;
                mask_info["w"] = w;
                mask_info["h"] = h;
                mask_info["size"] = size;
                
                result.push_back(mask_info);
                
                // 跳过遮罩数据
                file->seek(file->get_position() + size);
            }
            else if (block_info.flag == 0) { // 结束标记
                break;
            }
            else {
                // 跳过其他块
                file->seek(file->get_position() + block_info.size);
            }
        }
    }
    
    // Found masks info removed
    return result;
}

Ref<Image> MapLoader::get_mask_image(int map_id, int mask_index) {
    ERR_FAIL_COND_V(!is_loaded, Ref<Image>());
    ERR_FAIL_COND_V(map_id < 0 || map_id >= (int)map_count, Ref<Image>());
    
    // 获取遮罩信息
    Array mask_info_array = get_mask_info(map_id);
    ERR_FAIL_COND_V(mask_index < 0 || mask_index >= mask_info_array.size(), Ref<Image>());
    
    Dictionary mask_info = mask_info_array[mask_index];
    uint32_t offset = mask_info["offset"];
    int32_t x = mask_info["x"];
    int32_t y = mask_info["y"];
    int32_t w = mask_info["w"];
    int32_t h = mask_info["h"];
    uint32_t size = mask_info["size"];
    
    // 遮罩图像解析信息已移除调试打印
    
    // 跳转到遮罩数据位置
    file->seek(offset + 20); // 跳过头部
    
    // 读取压缩数据
    PackedByteArray compressed_data = file->get_buffer(size);
    
    // 计算解压后的大小（2bit像素，4对齐）
    int aligned_width = ((w + 3) >> 2) * 4; // 4对齐
    int decompressed_size = ((w + 3) >> 2) * h; // 每4像素一个字节
    
    PackedByteArray decompressed_data;
    decompressed_data.resize(decompressed_size);
    
    // 使用LZO解压
    uint32_t result_size = GGELUACore::lzo_decompress(
        compressed_data.ptr(), compressed_data.size(),
        decompressed_data.ptrw(), decompressed_data.size()
    );
    
    if (result_size != decompressed_size) {
        // 遮罩数据解压失败信息已移除调试打印
        return Ref<Image>();
    }
    
    // 将解压数据转换为像素数据（每像素用2bit表示）
    PackedByteArray pixel_data;
    pixel_data.resize(w * h);
    
    const uint8_t* compressed_ptr = decompressed_data.ptr();
    uint8_t* pixel_ptr = pixel_data.ptrw();
    
    for (int y_pos = 0; y_pos < h; y_pos++) {
        int bit_idx = 0;
        const uint8_t* row_ptr = compressed_ptr;
        
        for (int x_pos = 0; x_pos < w; x_pos++) {
            // 提取2bit数据（原始代码：(*data >> bitidx) & 3）
            uint8_t alpha = (*row_ptr >> bit_idx) & 3;
            *pixel_ptr++ = alpha;
            
            bit_idx += 2;
            if (bit_idx == 8) {
                bit_idx = 0;
                row_ptr++;
            }
        }
        
        // 数据4对齐，有剩余就跳过
        if (bit_idx != 0) {
            compressed_ptr = row_ptr + 1;
        } else {
            compressed_ptr = row_ptr;
        }
    }
    
    // 创建图像RGBA格式，alpha值作为透明度
    PackedByteArray rgba_data;
    rgba_data.resize(w * h * 2); // LA8格式，每像素两个字节
    
    for (int i = 0; i < w * h; i++) {
        uint8_t alpha = pixel_data[i];
        rgba_data.write[i * 2] = 255;     // 亮度（白色）
        rgba_data.write[i * 2 + 1] = alpha * 85; // 透明度（0-3 -> 0-255）
    }
    
    Ref<Image> img = Image::create_from_data(w, h, false, Image::FORMAT_LA8, rgba_data);
    
    // 遮罩图像解析完成信息已移除调试打印
    return img;
}

PackedByteArray MapLoader::get_cell_data() {
    ERR_FAIL_COND_V(!is_loaded, PackedByteArray());
    
    // 开始解析障碍数据...
    // 按照 liblxy-master 的正确算法
    
    int ow = col_count * 16;  // 障碍物网格宽度
    int oh = row_count * 12;  // 障碍物网格高度
    int cell_len = ow * oh;   // 总大小
    
    print_line("障碍物网格: " + String::num(ow) + "x" + String::num(oh) + " = " + String::num(cell_len) + " 个格子");
    
    PackedByteArray result;
    result.resize(cell_len);
    memset(result.ptrw(), 1, result.size());  // 初始化为障碍物(1)
    
    uint8_t* obstacles = result.ptrw();
    int parsed_cells = 0;
    int total_walkable_count = 0;
    
    // 按照 liblxy-master 算法处理每个地图块
    for (uint32_t k = 0; k < row_count; k++) {  // k = 块行索引
        int offy = k * 12 * ow;  // 当前块行在障碍物数组中的偏移
        
        for (uint32_t i = 0; i < col_count; i++) {  // i = 块列索引
            uint32_t map_id = k * col_count + i;
            
            // 跳转到地图块位置
            file->seek(map_offsets[map_id]);
            
            // 读取遮罩数量
            uint32_t mask_num = file->get_32();
            
            // M1.0格式需要跳过遮罩ID列表
            if (header.flag == 0x302E314D && mask_num > 0) {
                file->seek(file->get_position() + mask_num * sizeof(uint32_t));
            }
            
            // 解析块数据寻找CELL
            bool found_cell = false;
            while (true) {
                MapBlockInfo block_info;
                Vector<uint8_t> block_header = file->get_buffer(sizeof(MapBlockInfo));
                if (block_header.size() != sizeof(MapBlockInfo)) {
                    break;
                }
                memcpy(&block_info, block_header.ptr(), sizeof(MapBlockInfo));
                
                if (block_info.flag == 0x43454C4C) { // 'CELL' - LLEC
                    // 按照 liblxy-master 算法处理
                    PackedByteArray cell_data = file->get_buffer(block_info.size);
                    uint8_t* _ptr = cell_data.ptrw();
                    
                    // 计算当前块在障碍物数组中的起始位置
                    uint8_t* obs = obstacles + offy + i * 16;
                    
                    // 处理 LLEC 数据
                    for (uint32_t flag = 0; flag < block_info.size; flag++) {
                        if (*_ptr == 0) {
                            // 按照 liblxy-master 公式计算位置
                            *(obs + (flag / 16 * ow) + (flag % 16)) = 0;
                            total_walkable_count++;
                        }
                        _ptr++;
                    }
                    
                    parsed_cells++;
                    found_cell = true;
                    break;
                }
                else if (block_info.flag == 0) { // 结束标记
                    file->seek(file->get_position() + block_info.size);
                    break;
                }
                else {
                    // 跳过其他块
                    file->seek(file->get_position() + block_info.size);
                }
            }
            
            // 进度显示
            if ((map_id + 1) % 50 == 0 || map_id == map_count - 1) {
                print_line("处理进度: " + String::num(map_id + 1) + "/" + String::num(map_count) + 
                          " (已解析CELL: " + String::num(parsed_cells) + ")");
            }
        }
    }
    
    // 统计结果
    int total_cells = ow * oh;
    int obstacle_count = total_cells - total_walkable_count;
    float walkable_ratio = (float)total_walkable_count / total_cells * 100.0f;
    float obstacle_ratio = (float)obstacle_count / total_cells * 100.0f;
    
    print_line("=== 障碍数据解析完成 ===");
    print_line("总格子数: " + String::num(total_cells));
    print_line("可通行: " + String::num(total_walkable_count) + " (" + String::num(walkable_ratio, 1) + "%)");
    print_line("障碍物: " + String::num(obstacle_count) + " (" + String::num(obstacle_ratio, 1) + "%)");
    print_line("已处理CELL块: " + String::num(parsed_cells) + "/" + String::num(map_count));
    
    return result;
}

Dictionary MapLoader::get_map_block_data(int map_id) {
    Dictionary result;
    ERR_FAIL_COND_V(!is_loaded, result);
    ERR_FAIL_COND_V(map_id < 0 || map_id >= (int)map_count, result);
    
    // 返回原始块数据用于调试
    file->seek(map_offsets[map_id]);
    
    uint32_t mask_num = file->get_32();
    result["mask_count"] = mask_num;
    
    Array blocks;
    while (true) {
        MapBlockInfo block_info;
        Vector<uint8_t> block_header = file->get_buffer(sizeof(MapBlockInfo));
        if (block_header.size() != sizeof(MapBlockInfo)) {
            break;
        }
        memcpy(&block_info, block_header.ptr(), sizeof(MapBlockInfo));
        
        Dictionary block;
        block["flag"] = String::num_uint64(block_info.flag, 16);
        block["size"] = block_info.size;
        
        if (block_info.flag == 0) {
            break;
        }
        
        // 读取块数据
        PackedByteArray data = file->get_buffer(block_info.size);
        block["data"] = data;
        
        blocks.push_back(block);
    }
    
    result["blocks"] = blocks;
    return result;
}

int MapLoader::get_row_count() const {
    return is_loaded ? row_count : 0;
}

int MapLoader::get_col_count() const {
    return is_loaded ? col_count : 0;
}

int MapLoader::get_map_count() const {
    return is_loaded ? map_count : 0;
}

int MapLoader::get_mask_count() const {
    return is_loaded ? mask_count : 0;
}

Vector2i MapLoader::get_map_size() const {
    return is_loaded ? Vector2i(header.width, header.height) : Vector2i();
}

String MapLoader::get_map_format() const {
    if (!is_loaded) {
        return "";
    }
    
    if (header.flag == 0x302E314D) {
        return "M1.0";
    } else if (header.flag == 0x5850414D) {
        return "MAPX";
    }
    
    return "Unknown";
}

String MapLoader::get_file_path() const {
    return file_path;
}