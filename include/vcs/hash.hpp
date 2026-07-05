#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace vcs {

inline uint32_t rotl(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

class SHA1 {
public:
    SHA1() { reset(); }

    void reset() {
        state_[0] = 0x67452301;
        state_[1] = 0xEFCDAB89;
        state_[2] = 0x98BADCFE;
        state_[3] = 0x10325476;
        state_[4] = 0xC3D2E1F0;
        count_ = 0;
        buffer_.clear();
    }

    void update(const std::vector<uint8_t>& data) {
        for (auto byte : data) {
            buffer_.push_back(byte);
            if (buffer_.size() == 64) {
                processBlock(buffer_.data());
                count_ += 512;
                buffer_.clear();
            }
        }
    }

    void update(const std::string& data) {
        update(std::vector<uint8_t>(data.begin(), data.end()));
    }

    std::string digest() {
        uint64_t bits = count_ + buffer_.size() * 8;
        buffer_.push_back(0x80);
        while (buffer_.size() != 56) {
            if (buffer_.size() > 56) {
                while (buffer_.size() < 64) buffer_.push_back(0);
                processBlock(buffer_.data());
                buffer_.clear();
            } else {
                buffer_.push_back(0);
            }
        }
        for (int i = 7; i >= 0; --i) {
            buffer_.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
        processBlock(buffer_.data());

        std::ostringstream oss;
        for (int i = 0; i < 5; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(8) << state_[i];
        }
        return oss.str();
    }

    static std::string hash(const std::vector<uint8_t>& data) {
        SHA1 sha;
        sha.update(data);
        return sha.digest();
    }

    static std::string hash(const std::string& data) {
        return hash(std::vector<uint8_t>(data.begin(), data.end()));
    }

private:
    uint32_t state_[5];
    uint64_t count_;
    std::vector<uint8_t> buffer_;

    void processBlock(const uint8_t* block) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t a = state_[0], b = state_[1], c = state_[2];
        uint32_t d = state_[3], e = state_[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = temp;
        }
        state_[0] += a; state_[1] += b; state_[2] += c;
        state_[3] += d; state_[4] += e;
    }
};

} // namespace vcs
