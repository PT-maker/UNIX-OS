#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Sha1 {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buf[64];
    size_t blen = 0;
    uint64_t total = 0;

    static uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    void block(const uint8_t* p) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 |
                   (uint32_t)p[i * 4 + 2] << 8 | (uint32_t)p[i * 4 + 3];
        for (int i = 16; i < 80; i++)
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    void update(const uint8_t* data, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = std::min(n, 64 - blen);
            std::memcpy(buf + blen, data, take);
            blen += take; data += take; n -= take;
            if (blen == 64) { block(buf); blen = 0; }
        }
    }

    std::string finish() {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (blen != 56) update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
        update(len, 8);
        char out[41];
        for (int i = 0; i < 5; i++) std::snprintf(out + i * 8, 9, "%08x", h[i]);
        return std::string(out, 40);
    }
};

static bool hash_file(const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    Sha1 s;
    std::vector<char> buf(1 << 16);
    while (in) {
        in.read(buf.data(), buf.size());
        s.update(reinterpret_cast<uint8_t*>(buf.data()), (size_t)in.gcount());
    }
    out = s.finish();
    return true;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <каталог>\n";
        return 1;
    }
    fs::path root = argv[1];
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        std::cerr << "Не каталог: " << root << "\n";
        return 2;
    }

    std::map<std::string, std::vector<fs::path>> groups;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        std::string h;
        if (!hash_file(entry.path(), h)) {
            std::cerr << "Не удалось прочитать: " << entry.path() << "\n";
            continue;
        }
        std::cout << h << "  " << entry.path().string() << "\n";
        groups[h].push_back(entry.path());
    }

    for (auto& [h, list] : groups) {
        const fs::path& keep = list[0];
        for (size_t i = 1; i < list.size(); i++) {
            const fs::path& dup = list[i];
            if (fs::equivalent(keep, dup)) continue;
            fs::path tmp = dup;
            tmp += ".tmp_link";
            fs::create_hard_link(keep, tmp, ec);
            if (ec) { std::cerr << "Ошибка: " << ec.message() << "\n"; continue; }
            fs::rename(tmp, dup, ec);
            if (ec) { std::cerr << "Ошибка: " << ec.message() << "\n"; fs::remove(tmp); continue; }
            std::cout << dup.string() << " -> " << keep.string() << "\n";
        }
    }
    return 0;
}