#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <chrono>

namespace vcs {
namespace fs = std::filesystem;

class FileSystem {
public:
    static bool exists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static bool isRegularFile(const std::string& path);

    static bool createDirectory(const std::string& path);
    static bool createDirectories(const std::string& path);
    static bool remove(const std::string& path);
    static bool removeAll(const std::string& path);
    static bool rename(const std::string& from, const std::string& to);

    static std::vector<uint8_t> readFile(const std::string& path);
    static std::string readTextFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::vector<uint8_t>& data);
    static bool writeTextFile(const std::string& path, const std::string& text);
    static bool appendTextFile(const std::string& path, const std::string& text);

    static std::vector<std::string> listDirectory(const std::string& path);
    static std::vector<std::string> listFiles(const std::string& path);
    static std::vector<std::string> listDirectories(const std::string& path);

    static std::string getCurrentWorkingDirectory();
    static bool setCurrentWorkingDirectory(const std::string& path);

    static uint64_t getFileSize(const std::string& path);
    static uint64_t getLastModifiedTime(const std::string& path);

    static std::string absolute(const std::string& path);
    static std::string normalize(const std::string& path);

    static std::string readLink(const std::string& path);
    static bool isSymlink(const std::string& path);

    static std::vector<uint8_t> readFileAtomically(const std::string& path);
    static bool writeFileAtomically(const std::string& path, const std::vector<uint8_t>& data);
    static bool writeTextFileAtomically(const std::string& path, const std::string& text);
};

inline bool FileSystem::exists(const std::string& path) { return fs::exists(path); }
inline bool FileSystem::isDirectory(const std::string& path) { return fs::is_directory(path); }
inline bool FileSystem::isRegularFile(const std::string& path) { return fs::is_regular_file(path); }
inline bool FileSystem::createDirectory(const std::string& path) { return fs::create_directory(path); }

inline bool FileSystem::createDirectories(const std::string& path) {
    std::error_code ec; fs::create_directories(path, ec); return !ec;
}

inline bool FileSystem::remove(const std::string& path) { return fs::remove(path); }

inline bool FileSystem::removeAll(const std::string& path) {
    std::error_code ec; auto count = fs::remove_all(path, ec); return count > 0 && !ec;
}

inline bool FileSystem::rename(const std::string& from, const std::string& to) {
    std::error_code ec; fs::rename(from, to, ec); return !ec;
}

inline std::vector<uint8_t> FileSystem::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg(); f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

inline std::string FileSystem::readTextFile(const std::string& path) {
    auto data = readFile(path); return std::string(data.begin(), data.end());
}

inline bool FileSystem::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return f.good();
}

inline bool FileSystem::writeTextFile(const std::string& path, const std::string& text) {
    return writeFile(path, std::vector<uint8_t>(text.begin(), text.end()));
}

inline bool FileSystem::appendTextFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::app);
    if (!f) return false; f << text; return f.good();
}

inline std::vector<std::string> FileSystem::listDirectory(const std::string& path) {
    std::vector<std::string> entries;
    if (!exists(path)) return entries;
    for (auto& entry : fs::directory_iterator(path))
        entries.push_back(entry.path().filename().string());
    return entries;
}

inline std::vector<std::string> FileSystem::listFiles(const std::string& path) {
    std::vector<std::string> entries;
    if (!exists(path)) return entries;
    for (auto& entry : fs::directory_iterator(path))
        if (entry.is_regular_file() || entry.is_symlink())
            entries.push_back(entry.path().filename().string());
    return entries;
}

inline std::vector<std::string> FileSystem::listDirectories(const std::string& path) {
    std::vector<std::string> entries;
    if (!exists(path)) return entries;
    for (auto& entry : fs::directory_iterator(path))
        if (entry.is_directory()) entries.push_back(entry.path().filename().string());
    return entries;
}

inline std::string FileSystem::getCurrentWorkingDirectory() { return fs::current_path().string(); }

inline bool FileSystem::setCurrentWorkingDirectory(const std::string& path) {
    std::error_code ec; fs::current_path(path, ec); return !ec;
}

inline uint64_t FileSystem::getFileSize(const std::string& path) {
    std::error_code ec; auto size = fs::file_size(path, ec); return ec ? 0 : size;
}

inline uint64_t FileSystem::getLastModifiedTime(const std::string& path) {
    std::error_code ec; auto ftime = fs::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
}

inline std::string FileSystem::absolute(const std::string& path) { return fs::absolute(path).string(); }
inline std::string FileSystem::normalize(const std::string& path) { return fs::weakly_canonical(path).string(); }
inline std::string FileSystem::readLink(const std::string& path) {
    std::error_code ec; auto target = fs::read_symlink(path, ec); return ec ? "" : target.string();
}
inline bool FileSystem::isSymlink(const std::string& path) {
    std::error_code ec; return fs::is_symlink(path, ec);
}
inline std::vector<uint8_t> FileSystem::readFileAtomically(const std::string& path) { return readFile(path); }

inline bool FileSystem::writeFileAtomically(const std::string& path, const std::vector<uint8_t>& data) {
    auto tmpPath = path + ".tmp";
    if (!writeFile(tmpPath, data)) return false;
    std::error_code ec; fs::rename(tmpPath, path, ec); return !ec;
}

inline bool FileSystem::writeTextFileAtomically(const std::string& path, const std::string& text) {
    return writeFileAtomically(path, std::vector<uint8_t>(text.begin(), text.end()));
}

} // namespace vcs
