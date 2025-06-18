#ifndef CUSTOM_CRYPTO_H
#define CUSTOM_CRYPTO_H

#include "core/templates/vector.h"

// 一个简单的静态类，用于我们的自定义XOR加密
class CustomCrypto {
public:
    // 我们的自定义XOR密钥
    static const uint8_t XOR_KEY = 0xAE;

    // 对数据进行XOR操作（加密和解密是同一个操作）
    static void xor_data(Vector<uint8_t> &p_data) {
        if (p_data.is_empty()) {
            return;
        }
        uint8_t *data_ptr = p_data.ptrw();
        for (int i = 0; i < p_data.size(); i++) {
            data_ptr[i] = data_ptr[i] ^ XOR_KEY;
        }
    }
};

#endif // CUSTOM_CRYPTO_H 