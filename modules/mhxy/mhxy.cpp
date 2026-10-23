#include "mhxy.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

#include <cstring>

#ifdef _WIN32
// 堆损坏（0xC0000374）等 fail-fast 绕过 SEH 过滤器与 Godot 崩溃处理器，看不到栈。
// VEH 先于一切用户态过滤器收到异常；这里对致命码抓栈并用 dbghelp 符号化打到
// stderr，逐行 fflush（2026-10-24 --bagshot 退出堆损坏排查，保留作常驻取证）。
#include <windows.h>
#include <dbghelp.h>

namespace {
LONG WINAPI mhxy_crash_diag(PEXCEPTION_POINTERS info) {
	static thread_local bool in_handler = false;
	if (in_handler) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	const DWORD code = info->ExceptionRecord->ExceptionCode;
	if (code != EXCEPTION_ACCESS_VIOLATION && code != 0xC0000374 && code != 0xC0000409 && code != 0xC0000417) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	in_handler = true;

	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
	SymInitializeW(process, nullptr, TRUE);

	fprintf(stderr, "\n=== MHXY diag: exception 0x%08lX at %p ===\n",
			(unsigned long)code, info->ExceptionRecord->ExceptionAddress);
	fflush(stderr);

	void *frames[64];
	USHORT count = CaptureStackBackTrace(0, 64, frames, nullptr);
	for (USHORT i = 0; i < count; i++) {
		DWORD64 addr = (DWORD64)frames[i];
		char buffer[sizeof(SYMBOL_INFOW) + 512] = {};
		auto *sym = (SYMBOL_INFOW *)buffer;
		sym->SizeOfStruct = sizeof(SYMBOL_INFOW);
		sym->MaxNameLen = 511;
		DWORD64 displacement = 0;
		if (SymFromAddrW(process, addr, &displacement, sym)) {
			IMAGEHLP_LINEW64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
			DWORD line_disp = 0;
			if (SymGetLineFromAddrW64(process, addr, &line_disp, &line)) {
				fwprintf(stderr, L"[%2d] %s+%llu (%s:%lu)\n", (int)i,
						sym->Name, (unsigned long long)displacement,
						line.FileName, (unsigned long)line.LineNumber);
			} else {
				fwprintf(stderr, L"[%2d] %s+%llu\n", (int)i,
						sym->Name, (unsigned long long)displacement);
			}
		} else {
			fprintf(stderr, "[%2d] %p\n", (int)i, frames[i]);
		}
		fflush(stderr);
	}
	fprintf(stderr, "=== MHXY diag end ===\n");
	fflush(stderr);

	SymCleanup(process);
	return EXCEPTION_CONTINUE_SEARCH;
}
} // namespace
#endif // _WIN32

void MHXY::install_crash_diag() {
#ifdef _WIN32
	AddVectoredExceptionHandler(0, mhxy_crash_diag);
#endif
}

namespace {
const uint8_t KEY2[] = { 0xBE,0xB4,0xF9,0x81,0x05,0xFD,0x6F,0x2D,0x64,0x62,0xEB,0x7E,0xCA,0x64,0x96,0xB2,0x19,0x15,0x42,0x9A,0xC8,0xB3,0xD0,0x73,0x25,0x2A,0x83,0x85,0xA2,0xB1,0x07,0xCF,0x98,0xC5,0x15,0xFC,0x10,0xE5,0x19,0xFA,0xEE,0x6E,0x35,0x40,0x71,0xF4,0xA7,0xA7,0xD3,0x13,0x04,0xFE,0x55,0x87,0x30,0x0B,0xE3,0x86,0x5C,0xD9,0x7A,0xE9,0x8A,0xB1,0x5B,0x1C,0xE4,0x05,0x4F,0x98,0xA3,0x32,0xFA,0x47,0xAA,0x5A,0x7F,0x90,0x03,0x30,0x81,0xDB,0xB0,0x8F,0xAD,0x08,0x7B,0x12,0x96,0x0B,0x1A,0x88,0x8A,0x3E,0x80,0x06,0xE3,0x82,0xE3,0x55,0x10,0xA3,0x58,0x3B,0x6F,0xD1,0x04,0x0E,0xDC,0xD4,0x8B,0x53,0x83,0x2D,0x59,0x9E,0x39,0xF9,0x04,0x54,0x9F,0xD3,0x6D,0x45,0x7F,0x1B,0xA6,0xBC,0xC5,0xA4,0x76,0xFB,0xAC,0xAD,0x20,0xDD,0x83,0x76,0x21,0xDB,0x04,0x6D,0x34,0xC1,0x7E,0xC6,0x05,0x5E,0xE4,0xE1,0xCC,0xE8,0x7E,0x8F,0xB4,0x3B,0x36,0x14,0xBB,0x54,0x2B,0x46,0x48,0x38,0x5F,0x8E,0xA6,0xD5,0x0D,0x7B,0x1B,0xC1,0xD9,0x28,0x50,0x33,0x9F,0xAB,0xC5,0xB8,0x17,0x49,0xD7,0xE2,0xD7,0x8C,0x7E,0x1D,0xF7,0xE3,0xA7,0x70,0xB9,0xCB,0xB1,0xBD,0x26,0xFB,0x63,0xEF,0x21,0x2E,0x51,0x69,0xC7,0x0B,0x0F,0x57,0x3F,0x82,0x9B,0x89,0x4D,0x04,0x3E,0xE0,0x6D,0x64,0x60,0xD1,0x11,0x89,0xDD,0x28,0xD2,0xA9,0x09,0x0F,0x76,0x92,0x0D,0xA3,0x49,0xD5,0x97,0xE7,0xF7,0x6E,0x1C,0xCC,0x7D,0x27,0xDC,0x76,0x49,0x1E,0x24,0xB6,0x8D,0xCF,0x50,0xC1,0x9C,0x90,0xDE,0x96,0x50 };

void decrypt_rxpk(uint8_t *data, size_t size) {
	int step = 1;
	for (size_t i = 0; i < size; i++) {
		data[i] ^= KEY2[step - 1];
		step++;
		if (step == 34) {
			step++;
		}
		if (step == 258) {
			step = 1;
		}
	}
}

void decrypt_xxge(uint8_t *data, size_t size) {
	const uint8_t key4[] = { 0x48, 0x75, 0x35, 0x35, 0x75, 0x6F, 0x50, 0x55, 0x45, 0x4F, 0x70, 0x33, 0x32, 0x51, 0x31, 0x77, 0x39, 0x53, 0x6F, 0x69 };
	int step = 0;
	for (size_t i = 0; i < size; i++) {
		data[i] ^= key4[step];
		step++;
		if (step == 20) {
			step = 0;
		}
	}
}

uint32_t rgb565_to_argb(uint16_t color, uint8_t alpha) {
	uint32_t r = (color >> 11) & 0x1f;
	uint32_t g = (color >> 5) & 0x3f;
	uint32_t b = color & 0x1f;
	uint32_t R = (r << 3) | (r >> 2);
	uint32_t G = (g << 2) | (g >> 4);
	uint32_t B = (b << 3) | (b >> 2);
	return (uint32_t(alpha) << 24) | (R << 16) | (G << 8) | B;
}

struct Entry {
	uint32_t hash = 0;
	uint32_t offset = 0;
	uint32_t size = 0;
};

// 一个 WDF/RPK 只打开一次，索引建成哈希表。以前每次取帧都重开文件、解密整张表再线性扫。
struct Archive {
	Ref<FileAccess> file;
	HashMap<uint32_t, Entry> index;
};

struct MapKeep {
	Ref<FileAccess> file;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t cols = 0;
	uint32_t rows = 0;
	bool ok = false;
};

static HashMap<String, Archive> g_archives;
static HashMap<String, PackedByteArray> g_blobs;
static uint64_t g_blob_bytes = 0;
static HashMap<String, Ref<Image>> g_frames;
static HashSet<String> g_frame_miss;
static HashMap<String, MapKeep> g_maps;
static HashMap<String, Ref<Image>> g_tiles;

static const uint64_t BLOB_CAP = 64ull * 1024ull * 1024ull;
static const int FRAME_CAP = 4096;
static const int TILE_CAP = 1024;

static String blob_key(const String &path, uint32_t hash) {
	return path + String("|") + String::num_uint64(hash, 16);
}

static String frame_key(const String &path, uint32_t hash, int frame) {
	return blob_key(path, hash) + String("#") + itos(frame);
}

static void store_blob(const String &key, const PackedByteArray &blob) {
	if (blob.is_empty() || blob.size() > 4 * 1024 * 1024 || g_blobs.has(key)) {
		return;
	}
	if (g_blob_bytes + (uint64_t)blob.size() > BLOB_CAP) {
		g_blobs.clear();
		g_blob_bytes = 0;
	}
	g_blobs.insert(key, blob);
	g_blob_bytes += (uint64_t)blob.size();
}

static bool load_archive(const String &path, Archive &arch) {
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	uint8_t head[12];
	if (f->get_buffer(head, 12) != 12) {
		return false;
	}
	char flag[5] = { (char)head[0], (char)head[1], (char)head[2], (char)head[3], 0 };
	uint32_t number = head[4] | (head[5] << 8) | (head[6] << 16) | (head[7] << 24);
	uint32_t list_off = head[8] | (head[9] << 8) | (head[10] << 16) | (head[11] << 24);
	if (number == 0 || number > 200000) {
		return false;
	}
	f->seek(list_off);
	PackedByteArray list = f->get_buffer(number * 16);
	if (list.size() != (int)(number * 16)) {
		return false;
	}
	if (strcmp(flag, "RXPK") == 0) {
		decrypt_rxpk(list.ptrw(), list.size());
	} else if (strcmp(flag, "XXGE") == 0) {
		decrypt_xxge(list.ptrw(), list.size());
	}
	const uint8_t *p = list.ptr();
	for (uint32_t i = 0; i < number; i++) {
		Entry e;
		e.hash = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
		e.offset = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
		e.size = p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24);
		arch.index.insert(e.hash, e);
		p += 16;
	}
	arch.file = f;
	return true;
}

static Archive *archive_of(const String &path) {
	if (Archive *found = g_archives.getptr(path)) {
		if (found->file.is_valid()) {
			return found;
		}
	}
	Archive created;
	if (!load_archive(path, created)) {
		return nullptr;
	}
	g_archives.insert(path, created);
	return g_archives.getptr(path);
}

bool find_entry(const String &path, uint32_t hash, PackedByteArray &out) {
	String key = blob_key(path, hash);
	if (PackedByteArray *cached = g_blobs.getptr(key)) {
		out = *cached;
		return true;
	}
	Archive *arch = archive_of(path);
	if (arch == nullptr) {
		return false;
	}
	Entry *entry = arch->index.getptr(hash);
	if (entry == nullptr || arch->file.is_null()) {
		return false;
	}
	arch->file->seek(entry->offset);
	out = arch->file->get_buffer(entry->size);
	if (out.size() != (int)entry->size) {
		return false;
	}
	store_blob(key, out);
	return true;
}

Ref<Image> decode_sp_frame(const PackedByteArray &blob, int frame_index);

static Ref<Image> cached_sp_frame(const String &path, uint32_t hash, int frame, const PackedByteArray &blob) {
	String key = frame_key(path, hash, frame);
	if (Ref<Image> *found = g_frames.getptr(key)) {
		return *found;
	}
	if (g_frame_miss.has(key)) {
		return Ref<Image>();
	}
	Ref<Image> image = decode_sp_frame(blob, frame);
	if (image.is_null()) {
		g_frame_miss.insert(key);
		return image;
	}
	if (g_frames.size() >= FRAME_CAP) {
		g_frames.clear();
		g_frame_miss.clear();
	}
	g_frames.insert(key, image);
	return image;
}

Ref<Image> decode_sp_frame(const PackedByteArray &blob, int frame_index) {
	if (blob.size() < 16) {
		return Ref<Image>();
	}
	const uint8_t *raw = blob.ptr();
	uint16_t flag = raw[0] | (raw[1] << 8);
	if (flag != 0x5053) {
		return Ref<Image>();
	}
	uint16_t head_len = raw[2] | (raw[3] << 8);
	uint16_t group = raw[4] | (raw[5] << 8);
	uint16_t frames = raw[6] | (raw[7] << 8);
	int number = group * frames;
	if (number <= 0 || frame_index < 0 || frame_index >= number) {
		return Ref<Image>();
	}
	int extra = head_len - 12;
	if (extra < 0) {
		extra = 0;
	}
	int pos = 16 + extra;
	if (blob.size() < pos + 512 + number * 4) {
		return Ref<Image>();
	}
	uint32_t pal[256];
	for (int i = 0; i < 256; i++) {
		uint16_t c = raw[pos] | (raw[pos + 1] << 8);
		pal[i] = rgb565_to_argb(c, 255);
		pos += 2;
	}
	uint32_t frame_off = raw[pos + frame_index * 4] | (raw[pos + frame_index * 4 + 1] << 8) | (raw[pos + frame_index * 4 + 2] << 16) | (raw[pos + frame_index * 4 + 3] << 24);
	if (frame_off == 0) {
		return Ref<Image>();
	}
	frame_off += head_len + 4;
	if ((int)frame_off + 16 > blob.size()) {
		return Ref<Image>();
	}
	const uint8_t *frame = raw + frame_off;
	uint32_t width = frame[8] | (frame[9] << 8) | (frame[10] << 16) | (frame[11] << 24);
	uint32_t height = frame[12] | (frame[13] << 8) | (frame[14] << 16) | (frame[15] << 24);
	if (width == 0 || height == 0 || width > 4096 || height > 4096) {
		return Ref<Image>();
	}
	Vector<uint8_t> pixels;
	pixels.resize((int)(width * height * 4));
	uint8_t *pix = pixels.ptrw();
	memset(pix, 0, pixels.size());
	const uint8_t *line_base = frame + 16;
	for (uint32_t y = 0; y < height; y++) {
		uint32_t line_rel = line_base[y * 4] | (line_base[y * 4 + 1] << 8) | (line_base[y * 4 + 2] << 16) | (line_base[y * 4 + 3] << 24);
		const uint8_t *rdata = frame + line_rel;
		uint8_t *wdata = pix + y * width * 4;
		if (*rdata == 0) {
			if (y > 0) {
				memcpy(wdata, wdata - width * 4, width * 4);
			}
			continue;
		}
		uint32_t x = 0;
		while (*rdata && x < width) {
			uint8_t style = (*rdata & 0xC0) >> 6;
			if (style == 0) {
				if (*rdata & 0x20) {
					uint8_t alpha = ((*rdata++) & 0x1F) << 3;
					uint8_t idx = *rdata++;
					uint32_t c = (pal[idx] & 0xFFFFFF) | (uint32_t(alpha) << 24);
					if (x < width) {
						wdata[0] = (c >> 16) & 255;
						wdata[1] = (c >> 8) & 255;
						wdata[2] = c & 255;
						wdata[3] = (c >> 24) & 255;
						wdata += 4;
						x++;
					}
				} else {
					uint8_t repeat = (*rdata++) & 0x1F;
					uint8_t alpha = (*rdata++) << 3;
					uint8_t idx = *rdata++;
					uint32_t c = (pal[idx] & 0xFFFFFF) | (uint32_t(alpha) << 24);
					while (repeat && x < width) {
						wdata[0] = (c >> 16) & 255;
						wdata[1] = (c >> 8) & 255;
						wdata[2] = c & 255;
						wdata[3] = (c >> 24) & 255;
						wdata += 4;
						x++;
						repeat--;
					}
				}
			} else if (style == 1) {
				uint8_t repeat = (*rdata++) & 0x3F;
				while (repeat && x < width) {
					uint32_t c = pal[*rdata++];
					wdata[0] = (c >> 16) & 255;
					wdata[1] = (c >> 8) & 255;
					wdata[2] = c & 255;
					wdata[3] = (c >> 24) & 255;
					wdata += 4;
					x++;
					repeat--;
				}
			} else if (style == 2) {
				uint8_t repeat = (*rdata++) & 0x3F;
				uint32_t c = pal[*rdata++];
				while (repeat && x < width) {
					wdata[0] = (c >> 16) & 255;
					wdata[1] = (c >> 8) & 255;
					wdata[2] = c & 255;
					wdata[3] = (c >> 24) & 255;
					wdata += 4;
					x++;
					repeat--;
				}
			} else {
				// {11******} 跳过 n 个透明像素（tcp.c case 3 为准）
				uint8_t repeat = (*rdata++) & 0x3F;
				if (!repeat) {
					// 非常规处理：不结束本行（旧代码 break 会把整行截断成竖条）；
					// 修正前一像素——RGB 全黑且非首行 → 取上一行同列，否则强制不透明（边缘）
					if (x > 0) {
						if ((wdata[-4] | wdata[-3] | wdata[-2]) == 0 && y > 0) {
							memcpy(wdata - 4, wdata - 4 - width * 4, 4);
						} else {
							wdata[-1] = 255;
						}
					}
					rdata += 2;
					continue;
				}
				uint32_t skip = repeat;
				if (x + skip > width) {
					skip = width - x;
				}
				wdata += skip * 4;
				x += skip;
			}
		}
	}
	return Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, pixels);
}

Dictionary decode_sp_sprite(const PackedByteArray &blob, int direction) {
	Dictionary info;
	if (blob.size() < 16) {
		return info;
	}
	const uint8_t *raw = blob.ptr();
	uint16_t flag = raw[0] | (raw[1] << 8);
	if (flag != 0x5053) {
		return info;
	}
	uint16_t head_len = raw[2] | (raw[3] << 8);
	uint16_t group = raw[4] | (raw[5] << 8);
	uint16_t frames = raw[6] | (raw[7] << 8);
	if (group == 0 || frames == 0) {
		return info;
	}
	int dir = direction < 0 ? 0 : direction % group;
	int frame_index = dir * frames;
	Ref<Image> image = decode_sp_frame(blob, frame_index);
	if (image.is_null()) {
		return info;
	}
	int extra = head_len > 12 ? head_len - 12 : 0;
	int pos = 16 + extra;
	if (blob.size() < pos + 512 + (frame_index + 1) * 4) {
		return info;
	}
	auto u32 = [](const uint8_t *p) -> uint32_t {
		return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	};
	uint32_t frame_off = u32(raw + pos + 512 + frame_index * 4);
	if (frame_off == 0) {
		return info;
	}
	frame_off += head_len + 4;
	if ((int)frame_off + 16 > blob.size()) {
		return info;
	}
	info["image"] = image;
	info["key_x"] = (int)u32(raw + frame_off);
	info["key_y"] = (int)u32(raw + frame_off + 4);
	info["width"] = (int)image->get_width();
	info["height"] = (int)image->get_height();
	info["group"] = (int)group;
	info["frames"] = (int)frames;
	return info;
}

uint32_t read_u32(const uint8_t *p) {
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

bool map_header(const String &path, uint32_t &width, uint32_t &height, uint32_t &cols, uint32_t &rows) {
	if (MapKeep *kept = g_maps.getptr(path)) {
		if (!kept->ok) {
			return false;
		}
		width = kept->width;
		height = kept->height;
		cols = kept->cols;
		rows = kept->rows;
		return true;
	}
	MapKeep kept;
	kept.file = FileAccess::open(path, FileAccess::READ);
	if (kept.file.is_null()) {
		g_maps.insert(path, kept);
		return false;
	}
	uint8_t head[12];
	if (kept.file->get_buffer(head, 12) != 12) {
		kept.file.unref();
		g_maps.insert(path, kept);
		return false;
	}
	if (!(head[0] == 0x30 && head[1] == 0x2E && head[2] == 0x31 && head[3] == 0x4D)) {
		kept.file.unref();
		g_maps.insert(path, kept);
		return false;
	}
	kept.width = read_u32(head + 4);
	kept.height = read_u32(head + 8);
	kept.cols = (kept.width + 319) / 320;
	kept.rows = (kept.height + 239) / 240;
	kept.ok = kept.cols > 0 && kept.rows > 0 && kept.cols * kept.rows < 200000;
	if (!kept.ok) {
		kept.file.unref();
	}
	g_maps.insert(path, kept);
	if (!kept.ok) {
		return false;
	}
	width = kept.width;
	height = kept.height;
	cols = kept.cols;
	rows = kept.rows;
	return true;
}

static Ref<FileAccess> map_file(const String &path) {
	uint32_t width, height, cols, rows;
	if (!map_header(path, width, height, cols, rows)) {
		return Ref<FileAccess>();
	}
	MapKeep *kept = g_maps.getptr(path);
	if (kept == nullptr) {
		return Ref<FileAccess>();
	}
	return kept->file;
}

Ref<Image> decode_map_tile(const String &path, int tile_id);

static Ref<Image> cached_map_tile(const String &path, int tile_id) {
	String key = path + String("#") + itos(tile_id);
	if (Ref<Image> *found = g_tiles.getptr(key)) {
		return *found;
	}
	Ref<Image> image = decode_map_tile(path, tile_id);
	if (g_tiles.size() >= TILE_CAP) {
		g_tiles.clear();
	}
	g_tiles.insert(key, image);
	return image;
}

Ref<Image> decode_map_tile(const String &path, int tile_id) {
	uint32_t width, height, cols, rows;
	if (!map_header(path, width, height, cols, rows)) {
		return Ref<Image>();
	}
	uint32_t count = cols * rows;
	if (tile_id < 0 || (uint32_t)tile_id >= count) {
		return Ref<Image>();
	}
	Ref<FileAccess> f = map_file(path);
	if (f.is_null()) {
		return Ref<Image>();
	}
	f->seek(12 + (uint64_t)tile_id * 4);
	uint8_t offb[4];
	f->get_buffer(offb, 4);
	uint32_t offset = read_u32(offb);
	f->seek(offset);
	uint8_t nb[4];
	if (f->get_buffer(nb, 4) != 4) {
		return Ref<Image>();
	}
	uint32_t masknum = read_u32(nb);
	if (masknum > 0 && masknum < 100000) {
		f->seek(f->get_position() + (uint64_t)masknum * 4);
	}
	for (int guard = 0; guard < 16; guard++) {
		uint8_t info[8];
		if (f->get_buffer(info, 8) != 8) {
			return Ref<Image>();
		}
		uint32_t size = read_u32(info + 4);
		if (size == 0 || size > 8 * 1024 * 1024) {
			return Ref<Image>();
		}
		PackedByteArray blob = f->get_buffer(size);
		if (blob.size() != (int)size) {
			return Ref<Image>();
		}
		Ref<Image> image;
		image.instantiate();
		if (image->load_jpg_from_buffer(blob) == OK || image->load_png_from_buffer(blob) == OK || image->load_webp_from_buffer(blob) == OK) {
			if (image->get_width() > 0) {
				return image;
			}
		}
	}
	return Ref<Image>();
}

bool cell_flag(const uint8_t *p) {
	return (p[0] == 'L' && p[1] == 'L' && p[2] == 'E' && p[3] == 'C') || (p[0] == 'C' && p[1] == 'E' && p[2] == 'L' && p[3] == 'L');
}

PackedByteArray decode_map_cell(const String &path, int tile_id) {
	PackedByteArray out;
	uint32_t width, height, cols, rows;
	if (!map_header(path, width, height, cols, rows)) {
		return out;
	}
	uint32_t count = cols * rows;
	if (tile_id < 0 || (uint32_t)tile_id >= count) {
		return out;
	}
	Ref<FileAccess> f = map_file(path);
	if (f.is_null()) {
		return out;
	}
	f->seek(12 + (uint64_t)tile_id * 4);
	uint8_t offb[4];
	if (f->get_buffer(offb, 4) != 4) {
		return out;
	}
	f->seek(read_u32(offb));
	uint8_t nb[4];
	if (f->get_buffer(nb, 4) != 4) {
		return out;
	}
	uint32_t masknum = read_u32(nb);
	if (masknum > 0 && masknum < 100000) {
		f->seek(f->get_position() + (uint64_t)masknum * 4);
	}
	for (int guard = 0; guard < 16; guard++) {
		uint8_t info[8];
		if (f->get_buffer(info, 8) != 8) {
			return out;
		}
		uint32_t size = read_u32(info + 4);
		if (info[0] == 0 && info[1] == 0 && info[2] == 0 && info[3] == 0) {
			return out;
		}
		if (cell_flag(info)) {
			if (size != 192) {
				return out;
			}
			out.resize(192);
			uint8_t *bytes = out.ptrw();
			if (f->get_buffer(bytes, 192) != 192) {
				return PackedByteArray();
			}
			for (int i = 0; i < 192; i++) {
				if (bytes[i] == 2) {
					bytes[i] = 0;
				}
			}
			return out;
		}
		if (size == 0 || size > 8 * 1024 * 1024) {
			return out;
		}
		f->seek(f->get_position() + size);
	}
	return out;
}

int floor_div(int value, int size) {
	int q = value / size;
	if (value < 0 && value % size != 0) {
		q -= 1;
	}
	return q;
}

bool copy_lit(uint8_t *&op, uint8_t *op_end, const uint8_t *&ip, const uint8_t *ip_end, unsigned n) {
	if (n > (unsigned)(ip_end - ip) || n > (unsigned)(op_end - op)) {
		return false;
	}
	memcpy(op, ip, n);
	op += n;
	ip += n;
	return true;
}

bool copy_match(uint8_t *&op, uint8_t *op_end, const uint8_t *out, int back, unsigned n) {
	if (back <= 0 || back > (int)(op - out) || n > (unsigned)(op_end - op)) {
		return false;
	}
	const uint8_t *src = op - back;
	for (unsigned i = 0; i < n; i++) {
		*op++ = *src++;
	}
	return true;
}

int decompress_mask(const uint8_t *in, int in_len, uint8_t *out, int out_cap) {
	const uint8_t *ip = in;
	const uint8_t *ip_end = in + in_len;
	uint8_t *op = out;
	uint8_t *op_end = out + out_cap;
	unsigned t = 0;
	unsigned dist = 0;
	int guard = in_len * 8 + 16;
	if (in_len < 1 || out_cap < 1) {
		return -1;
	}
	if (*ip > 17) {
		t = *ip++ - 17;
		if (t < 4) {
			if (!copy_lit(op, op_end, ip, ip_end, t) || ip >= ip_end) {
				return -1;
			}
			t = *ip++;
			goto match;
		}
		if (!copy_lit(op, op_end, ip, ip_end, t)) {
			return -1;
		}
		goto first_literal_run;
	}
	for (;;) {
		if (--guard <= 0 || ip >= ip_end) {
			return -1;
		}
		t = *ip++;
		if (t < 16) {
			if (t == 0) {
				while (ip < ip_end && *ip == 0) {
					t += 255;
					ip++;
				}
				if (ip >= ip_end) {
					return -1;
				}
				t += 15 + *ip++;
			}
			if (!copy_lit(op, op_end, ip, ip_end, t + 3)) {
				return -1;
			}
		first_literal_run:
			if (--guard <= 0 || ip >= ip_end) {
				return -1;
			}
			t = *ip++;
			if (t < 16) {
				if (ip >= ip_end) {
					return -1;
				}
				int back = 0x0801 + (int)(t >> 2) + ((int)(*ip++) << 2);
				if (!copy_match(op, op_end, out, back, 3)) {
					return -1;
				}
				goto match_done;
			}
		}
		for (;;) {
		match:
			if (--guard <= 0) {
				return -1;
			}
			if (t >= 64) {
				if (ip >= ip_end) {
					return -1;
				}
				int back = 1 + (int)((t >> 2) & 7) + ((int)(*ip++) << 3);
				if (!copy_match(op, op_end, out, back, (t >> 5) + 1)) {
					return -1;
				}
				goto match_done;
			} else if (t >= 32) {
				t &= 31;
				if (t == 0) {
					while (ip < ip_end && *ip == 0) {
						t += 255;
						ip++;
					}
					if (ip >= ip_end) {
						return -1;
					}
					t += 31 + *ip++;
				}
				if (ip + 2 > ip_end) {
					return -1;
				}
				dist = (unsigned)ip[0] | ((unsigned)ip[1] << 8);
				ip += 2;
				if (!copy_match(op, op_end, out, 1 + (int)(dist >> 2), t + 2)) {
					return -1;
				}
			} else if (t >= 16) {
				int back = (int)((t & 8) << 11);
				t &= 7;
				if (t == 0) {
					while (ip < ip_end && *ip == 0) {
						t += 255;
						ip++;
					}
					if (ip >= ip_end) {
						return -1;
					}
					t += 7 + *ip++;
				}
				if (ip + 2 > ip_end) {
					return -1;
				}
				dist = (unsigned)ip[0] | ((unsigned)ip[1] << 8);
				ip += 2;
				back += (int)(dist >> 2);
				if (back == 0) {
					return (int)(op - out);
				}
				back += 0x4000;
				if (!copy_match(op, op_end, out, back, t + 2)) {
					return -1;
				}
			} else {
				if (ip >= ip_end) {
					return -1;
				}
				int back = 1 + (int)(t >> 2) + ((int)(*ip++) << 2);
				if (!copy_match(op, op_end, out, back, 2)) {
					return -1;
				}
			}
		match_done:
			if (ip < in + 2) {
				return -1;
			}
			t = ip[-2] & 3;
			if (t == 0) {
				break;
			}
			if (!copy_lit(op, op_end, ip, ip_end, t) || ip >= ip_end) {
				return -1;
			}
			t = *ip++;
		}
	}
}

void unpack_mask(const uint8_t *data, int width, int height, uint8_t *alpha) {
	int bitidx = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			*alpha++ = (*data >> bitidx) & 3;
			bitidx += 2;
			if (bitidx == 8) {
				bitidx = 0;
				data++;
			}
		}
		if (bitidx != 0) {
			bitidx = 0;
			data++;
		}
	}
}

Ref<Image> cached_tile(HashMap<uint32_t, Ref<Image>> &cache, const String &path, uint32_t tile_id, uint32_t tile_count) {
	if (tile_id >= tile_count) {
		return Ref<Image>();
	}
	if (Ref<Image> *found = cache.getptr(tile_id)) {
		return *found;
	}
	Ref<Image> image = cached_map_tile(path, (int)tile_id);
	if (image.is_valid() && image->get_format() != Image::FORMAT_RGBA8) {
		image->convert(Image::FORMAT_RGBA8);
	}
	cache.insert(tile_id, image);
	return image;
}

Ref<Image> compose_mask(const String &path, uint32_t cols, uint32_t rows, int mx, int my, int mw, int mh, const uint8_t *alpha, HashMap<uint32_t, Ref<Image>> &cache) {
	Ref<Image> image = Image::create_empty(mw, mh, false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 0, 0, 0));
	uint8_t *dst = image->ptrw();
	uint32_t tile_count = cols * rows;
	for (int y = 0; y < mh; y++) {
		int map_y = my + y;
		int tile_row = floor_div(map_y, 240);
		int local_y = map_y - tile_row * 240;
		for (int x = 0; x < mw; x++) {
			uint8_t a = alpha[y * mw + x];
			if (a == 3) {
				a = 150;
			}
			int pixel = (y * mw + x) * 4;
			int map_x = mx + x;
			int tile_col = floor_div(map_x, 320);
			int local_x = map_x - tile_col * 320;
			if (tile_col < 0 || tile_row < 0 || (uint32_t)tile_col >= cols || (uint32_t)tile_row >= rows) {
				continue;
			}
			Ref<Image> tile = cached_tile(cache, path, (uint32_t)tile_row * cols + (uint32_t)tile_col, tile_count);
			if (tile.is_null() || local_x < 0 || local_y < 0 || local_x >= tile->get_width() || local_y >= tile->get_height()) {
				continue;
			}
			const uint8_t *src = tile->ptr();
			int sp = (local_y * tile->get_width() + local_x) * 4;
			dst[pixel] = src[sp];
			dst[pixel + 1] = src[sp + 1];
			dst[pixel + 2] = src[sp + 2];
			dst[pixel + 3] = a;
		}
	}
	return image;
}

Array decode_map_masks(const String &path, int tile_id) {
	Array result;
	uint32_t width, height, cols, rows;
	if (!map_header(path, width, height, cols, rows)) {
		return result;
	}
	uint32_t count = cols * rows;
	if (tile_id < 0 || (uint32_t)tile_id >= count) {
		return result;
	}
	Ref<FileAccess> f = map_file(path);
	if (f.is_null()) {
		return result;
	}
	f->seek(12 + (uint64_t)tile_id * 4);
	uint8_t b[4];
	if (f->get_buffer(b, 4) != 4) {
		return result;
	}
	f->seek(read_u32(b));
	if (f->get_buffer(b, 4) != 4) {
		return result;
	}
	uint32_t near_count = read_u32(b);
	if (near_count == 0 || near_count > 4096) {
		return result;
	}
	PackedByteArray ids;
	ids.resize(near_count * 4);
	if (f->get_buffer(ids.ptrw(), near_count * 4) != near_count * 4) {
		return result;
	}
	f->seek(12 + (uint64_t)count * 4);
	if (f->get_buffer(b, 4) != 4) {
		return result;
	}
	uint32_t mask_table = read_u32(b);
	f->seek(mask_table);
	if (f->get_buffer(b, 4) != 4) {
		return result;
	}
	uint32_t mask_count = read_u32(b);
	if (mask_count == 0 || mask_count > 200000) {
		return result;
	}
	HashMap<uint32_t, Ref<Image>> cache;
	const uint8_t *id_bytes = ids.ptr();
	for (uint32_t i = 0; i < near_count; i++) {
		uint32_t mid = read_u32(id_bytes + i * 4);
		if (mid >= mask_count) {
			continue;
		}
		f->seek(mask_table + 4 + (uint64_t)mid * 4);
		if (f->get_buffer(b, 4) != 4) {
			continue;
		}
		uint32_t moff = read_u32(b);
		f->seek(moff);
		uint8_t head[20];
		if (f->get_buffer(head, 20) != 20) {
			continue;
		}
		int mx = (int)read_u32(head);
		int my = (int)read_u32(head + 4);
		int mw = (int)read_u32(head + 8);
		int mh = (int)read_u32(head + 12);
		uint32_t msize = read_u32(head + 16);
		if (mw <= 0 || mh <= 0 || mw > 4096 || mh > 4096 || (int64_t)mw * mh > 2000000) {
			continue;
		}
		if (msize == 0 || msize > 512 * 1024) {
			continue;
		}
		PackedByteArray compressed;
		compressed.resize(msize);
		if (f->get_buffer(compressed.ptrw(), msize) != msize) {
			continue;
		}
		int raw_len = ((mw + 3) >> 2) * mh;
		Vector<uint8_t> raw;
		raw.resize(raw_len);
		int got = decompress_mask(compressed.ptr(), (int)msize, raw.ptrw(), raw_len);
		if (got != raw_len) {
			continue;
		}
		Vector<uint8_t> alpha;
		alpha.resize(mw * mh);
		unpack_mask(raw.ptr(), mw, mh, alpha.ptrw());
		Ref<Image> image = compose_mask(path, cols, rows, mx, my, mw, mh, alpha.ptr(), cache);
		Dictionary item;
		item["id"] = mid;
		item["x"] = mx;
		item["y"] = my;
		item["w"] = mw;
		item["h"] = mh;
		item["image"] = image;
		result.push_back(item);
	}
	return result;
}
}

void MHXY::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_was_frame", "path", "hash", "frame"), &MHXY::load_was_frame, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("load_was_sprite", "path", "hash", "direction"), &MHXY::load_was_sprite, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("load_was_frame_info", "path", "hash", "frame"), &MHXY::load_was_frame_info);
	ClassDB::bind_method(D_METHOD("load_was_direction", "path", "hash", "direction"), &MHXY::load_was_direction, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("load_map_info", "path"), &MHXY::load_map_info);
	ClassDB::bind_method(D_METHOD("load_map_tile", "path", "tile"), &MHXY::load_map_tile);
	ClassDB::bind_method(D_METHOD("load_map_cell", "path", "tile"), &MHXY::load_map_cell);
	ClassDB::bind_method(D_METHOD("load_map_masks", "path", "tile"), &MHXY::load_map_masks);
	ClassDB::bind_method(D_METHOD("map_tiles_in_view", "path", "origin", "margin"), &MHXY::map_tiles_in_view, DEFVAL(4));
	ClassDB::bind_method(D_METHOD("query_aoi", "points", "view"), &MHXY::query_aoi);
	ClassDB::bind_method(D_METHOD("cache_stats"), &MHXY::cache_stats);
	ClassDB::bind_method(D_METHOD("clear_resource_cache"), &MHXY::clear_resource_cache);
}

Dictionary MHXY::load_map_info(const String &p_path) const {
	Dictionary info;
	uint32_t width, height, cols, rows;
	if (!map_header(p_path, width, height, cols, rows)) {
		return info;
	}
	info["width"] = width;
	info["height"] = height;
	info["cols"] = cols;
	info["rows"] = rows;
	return info;
}

Ref<Image> MHXY::load_map_tile(const String &p_path, int p_tile) const {
	return cached_map_tile(p_path, p_tile);
}

PackedByteArray MHXY::load_map_cell(const String &p_path, int p_tile) const {
	return decode_map_cell(p_path, p_tile);
}

Array MHXY::load_map_masks(const String &p_path, int p_tile) const {
	return decode_map_masks(p_path, p_tile);
}

String MHXY::get_module_name() const {
	return "MHXY";
}

Dictionary MHXY::load_was_sprite(const String &p_path, int64_t p_hash, int p_direction) const {
	PackedByteArray blob;
	if (!find_entry(p_path, (uint32_t)p_hash, blob)) {
		return Dictionary();
	}
	return decode_sp_sprite(blob, p_direction);
}

Ref<Image> MHXY::load_was_frame(const String &p_path, int64_t p_hash, int p_frame) const {
	String key = frame_key(p_path, (uint32_t)p_hash, p_frame);
	if (Ref<Image> *found = g_frames.getptr(key)) {
		return *found;
	}
	if (g_frame_miss.has(key)) {
		return Ref<Image>();
	}
	PackedByteArray blob;
	if (!find_entry(p_path, (uint32_t)p_hash, blob)) {
		print_line(vformat("MHXY missing hash %x in %s", (uint32_t)p_hash, p_path));
		g_frame_miss.insert(key);
		return Ref<Image>();
	}
	Ref<Image> image = cached_sp_frame(p_path, (uint32_t)p_hash, p_frame, blob);
	if (image.is_null()) {
		print_line(vformat("MHXY hash %x is not an SP frame, size %d flag %02x %02x", (uint32_t)p_hash, blob.size(), blob.size() > 0 ? blob[0] : 0, blob.size() > 1 ? blob[1] : 0));
	}
	return image;
}

/// 逐帧 key 锚点。load_was_sprite 的第三参数是方向，单方向动画（group=1）永远只取第 0 帧；
/// 动画换帧必须用本接口按帧序号取 key 与帧尺寸。
Dictionary MHXY::load_was_frame_info(const String &p_path, int64_t p_hash, int p_frame) const {
	Dictionary info;
	PackedByteArray blob;
	if (!find_entry(p_path, (uint32_t)p_hash, blob)) {
		return info;
	}
	if (blob.size() < 16) {
		return info;
	}
	const uint8_t *raw = blob.ptr();
	uint16_t flag = raw[0] | (raw[1] << 8);
	if (flag != 0x5053) {
		return info;
	}
	uint16_t head_len = raw[2] | (raw[3] << 8);
	uint16_t group = raw[4] | (raw[5] << 8);
	uint16_t frames = raw[6] | (raw[7] << 8);
	int number = group * frames;
	if (number <= 0 || p_frame < 0 || p_frame >= number) {
		return info;
	}
	Ref<Image> image = cached_sp_frame(p_path, (uint32_t)p_hash, p_frame, blob);
	if (image.is_null()) {
		return info;
	}
	int extra = head_len > 12 ? head_len - 12 : 0;
	int pos = 16 + extra;
	if (blob.size() < pos + 512 + (p_frame + 1) * 4) {
		return info;
	}
	auto u32 = [](const uint8_t *p) -> uint32_t {
		return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	};
	uint32_t frame_off = u32(raw + pos + 512 + p_frame * 4);
	if (frame_off == 0) {
		return info;
	}
	frame_off += head_len + 4;
	if ((int)frame_off + 16 > blob.size()) {
		return info;
	}
	info["image"] = image;
	info["key_x"] = (int)u32(raw + frame_off);
	info["key_y"] = (int)u32(raw + frame_off + 4);
	info["width"] = (int)image->get_width();
	info["height"] = (int)image->get_height();
	info["group"] = (int)group;
	info["frames"] = (int)frames;
	return info;
}

Array MHXY::load_was_direction(const String &p_path, int64_t p_hash, int p_direction) const {
	Array out;
	Dictionary head = load_was_sprite(p_path, p_hash, 0);
	int group = int(head.get("group", 0));
	int frames = int(head.get("frames", 0));
	if (group <= 0 || frames <= 0) {
		return out;
	}
	int dir = p_direction < 0 ? 0 : p_direction % group;
	int base = dir * frames;
	for (int frame = 0; frame < frames; frame++) {
		Dictionary info = load_was_frame_info(p_path, p_hash, base + frame);
		if (!info.has("image")) {
			break;
		}
		out.push_back(info);
	}
	return out;
}

PackedInt32Array MHXY::map_tiles_in_view(const String &p_path, const Vector2 &p_origin, int p_margin) const {
	PackedInt32Array ids;
	uint32_t width, height, cols, rows;
	if (!map_header(p_path, width, height, cols, rows)) {
		return ids;
	}
	int margin = p_margin < 0 ? 0 : p_margin;
	int tile_c = (int)p_origin.x / 320;
	int tile_r = (int)p_origin.y / 240;
	int c0 = MAX(0, tile_c - margin);
	int r0 = MAX(0, tile_r - margin);
	int c1 = MIN((int)cols - 1, tile_c + margin);
	int r1 = MIN((int)rows - 1, tile_r + margin);
	for (int row = r0; row <= r1; row++) {
		for (int col = c0; col <= c1; col++) {
			ids.push_back(row * (int)cols + col);
		}
	}
	return ids;
}

PackedInt32Array MHXY::query_aoi(const PackedVector2Array &p_points, const Rect2 &p_view) const {
	PackedInt32Array hit;
	for (int i = 0; i < p_points.size(); i++) {
		if (p_view.has_point(p_points[i])) {
			hit.push_back(i);
		}
	}
	return hit;
}

Dictionary MHXY::cache_stats() const {
	Dictionary info;
	info["archives"] = (int)g_archives.size();
	info["blobs"] = (int)g_blobs.size();
	info["blob_bytes"] = (int64_t)g_blob_bytes;
	info["frames"] = (int)g_frames.size();
	info["frame_misses"] = (int)g_frame_miss.size();
	info["maps"] = (int)g_maps.size();
	info["tiles"] = (int)g_tiles.size();
	return info;
}

void MHXY::clear_resource_cache() {
	shutdown_cache();
}

void MHXY::shutdown_cache() {
	g_archives.clear();
	g_blobs.clear();
	g_blob_bytes = 0;
	g_frames.clear();
	g_frame_miss.clear();
	g_maps.clear();
	g_tiles.clear();
}
