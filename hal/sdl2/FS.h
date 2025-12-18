/**
 * FS.h - Arduino Filesystem stub for emulator.
 * Provides minimal File and FS classes for compilation.
 * In the emulator, filesystem operations are no-ops or use POSIX.
 */
#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

class File {
public:
    File() : _file(nullptr), _isDir(false) {}
    File(FILE* f, bool isDir = false) : _file(f), _isDir(isDir) {}

    ~File() {
        close();
    }

    // Copy/move constructors
    File(File&& other) : _file(other._file), _isDir(other._isDir), _path(std::move(other._path)) {
        other._file = nullptr;
    }

    File& operator=(File&& other) {
        if (this != &other) {
            close();
            _file = other._file;
            _isDir = other._isDir;
            _path = std::move(other._path);
            other._file = nullptr;
        }
        return *this;
    }

    operator bool() const { return _file != nullptr; }

    size_t write(uint8_t c) {
        if (!_file) return 0;
        return fwrite(&c, 1, 1, _file);
    }

    size_t write(const uint8_t* buf, size_t size) {
        if (!_file) return 0;
        return fwrite(buf, 1, size, _file);
    }

    size_t write(const char* str) {
        if (!_file || !str) return 0;
        return fwrite(str, 1, strlen(str), _file);
    }

    int read() {
        if (!_file) return -1;
        return fgetc(_file);
    }

    size_t read(uint8_t* buf, size_t size) {
        if (!_file) return 0;
        return fread(buf, 1, size, _file);
    }

    int available() {
        if (!_file) return 0;
        long pos = ftell(_file);
        fseek(_file, 0, SEEK_END);
        long end = ftell(_file);
        fseek(_file, pos, SEEK_SET);
        return (int)(end - pos);
    }

    int peek() {
        if (!_file) return -1;
        int c = fgetc(_file);
        if (c != EOF) ungetc(c, _file);
        return c;
    }

    void flush() {
        if (_file) fflush(_file);
    }

    bool seek(uint32_t pos) {
        if (!_file) return false;
        return fseek(_file, pos, SEEK_SET) == 0;
    }

    size_t position() {
        if (!_file) return 0;
        return ftell(_file);
    }

    size_t size() {
        if (!_file) return 0;
        long pos = ftell(_file);
        fseek(_file, 0, SEEK_END);
        long sz = ftell(_file);
        fseek(_file, pos, SEEK_SET);
        return sz;
    }

    void close() {
        if (_file) {
            fclose(_file);
            _file = nullptr;
        }
    }

    const char* name() const {
        return _path.c_str();
    }

    bool isDirectory() const {
        return _isDir;
    }

    // For compatibility with JSON serialization
    size_t print(const char* str) { return write(str); }
    size_t println(const char* str) {
        size_t n = write(str);
        n += write("\n");
        return n;
    }

    // Read entire file as string
    std::string readString() {
        if (!_file) return "";
        long pos = ftell(_file);
        fseek(_file, 0, SEEK_END);
        long sz = ftell(_file);
        fseek(_file, 0, SEEK_SET);
        std::string content;
        content.resize(sz);
        fread(&content[0], 1, sz, _file);
        fseek(_file, pos, SEEK_SET);
        return content;
    }

    void setPath(const std::string& path) { _path = path; }

    // Get path
    const char* path() const { return _path.c_str(); }

    // Read bytes (Arduino compatible)
    size_t readBytes(char* buf, size_t len) {
        if (!_file) return 0;
        return fread(buf, 1, len, _file);
    }

    size_t readBytes(uint8_t* buf, size_t len) {
        return readBytes((char*)buf, len);
    }

    // Directory iteration support
    File openNextFile() {
        // This is a stub - real implementation would iterate directory
        // For now, just return invalid File to signal end of directory
        return File();
    }

private:
    FILE* _file;
    bool _isDir;
    std::string _path;

    // Disable copy
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};

class FS {
public:
    FS() : _basePath(".") {}
    explicit FS(const char* basePath) : _basePath(basePath ? basePath : ".") {}

    File open(const char* path, const char* mode = FILE_READ, bool create = false) {
        std::string fullPath = _basePath + path;

        // Create parent directories if needed
        if (create) {
            std::string dir = fullPath.substr(0, fullPath.rfind('/'));
            mkdir_p(dir.c_str());
        }

        FILE* f = fopen(fullPath.c_str(), mode);
        File file(f);
        file.setPath(path);
        return file;
    }

    bool exists(const char* path) {
        std::string fullPath = _basePath + path;
        struct stat st;
        return stat(fullPath.c_str(), &st) == 0;
    }

    bool mkdir(const char* path) {
        std::string fullPath = _basePath + path;
        return ::mkdir(fullPath.c_str(), 0755) == 0 || errno == EEXIST;
    }

    bool remove(const char* path) {
        std::string fullPath = _basePath + path;
        return ::remove(fullPath.c_str()) == 0;
    }

    bool rmdir(const char* path) {
        std::string fullPath = _basePath + path;
        return ::rmdir(fullPath.c_str()) == 0;
    }

    bool rename(const char* from, const char* to) {
        std::string fullFrom = _basePath + from;
        std::string fullTo = _basePath + to;
        return ::rename(fullFrom.c_str(), fullTo.c_str()) == 0;
    }

private:
    std::string _basePath;

    void mkdir_p(const char* path) {
        char tmp[256];
        strncpy(tmp, path, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';

        for (char* p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                ::mkdir(tmp, 0755);
                *p = '/';
            }
        }
        ::mkdir(tmp, 0755);
    }
};

// Global filesystem instance (for SPIFFS replacement)
extern FS SPIFFS;
// Note: SD is declared in SD.h as SDClass type
