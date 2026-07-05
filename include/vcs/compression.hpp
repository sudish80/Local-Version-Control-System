#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <zlib.h>

namespace vcs {

class Compression {
public:
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};

        uLong srcLen = static_cast<uLong>(data.size());
        uLong destLen = compressBound(srcLen);
        std::vector<uint8_t> compressed(destLen);

        auto result = compress2(compressed.data(), &destLen,
                                data.data(), srcLen, Z_BEST_SPEED);
        if (result != Z_OK) {
            throw std::runtime_error("Compression failed");
        }
        compressed.resize(destLen);
        return compressed;
    }

    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = static_cast<uInt>(data.size());
        strm.next_in = const_cast<Bytef*>(data.data());

        if (inflateInit2(&strm, 15 + 32) != Z_OK) {
            throw std::runtime_error("Decompression init failed");
        }

        std::vector<uint8_t> decompressed;
        decompressed.reserve(data.size() * 2);
        uint8_t buffer[8192];

        while (true) {
            strm.avail_out = sizeof(buffer);
            strm.next_out = buffer;
            auto ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&strm);
                throw std::runtime_error("Decompression failed");
            }
            auto have = sizeof(buffer) - strm.avail_out;
            decompressed.insert(decompressed.end(), buffer, buffer + have);
            if (ret == Z_STREAM_END) break;
        }

        inflateEnd(&strm);
        return decompressed;
    }

    static uint32_t crc32(const std::vector<uint8_t>& data) {
        return static_cast<uint32_t>(::crc32(0, data.data(), static_cast<uInt>(data.size())));
    }

    static std::vector<uint8_t> deflate(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        uLong srcLen = static_cast<uLong>(data.size());
        uLong destLen = compressBound(srcLen);
        std::vector<uint8_t> result(destLen);
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.next_in = const_cast<Bytef*>(data.data());
        strm.avail_in = srcLen;
        strm.next_out = result.data();
        strm.avail_out = destLen;
        deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        auto ret = ::deflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END) { deflateEnd(&strm); throw std::runtime_error("Deflate failed"); }
        result.resize(strm.total_out);
        deflateEnd(&strm);
        return result;
    }
};

} // namespace vcs
