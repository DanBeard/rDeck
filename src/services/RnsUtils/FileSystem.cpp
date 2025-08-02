#include "FileSystem.h"

#include <Utilities/OS.h>
#include <Log.h>

// TODO Abstract away SD to a generic file system retHal
// RIght now we need an SD though. It's just too much space
#include <SD.h>

bool FileSystem::init() {
	INFO("INIT SD CARD FS");

	return true;
}


void FileSystem::listDir(const char* dir, int levels) {
	Serial.print("DIR: ");
	Serial.println(dir);

   File root = SD.open(dir);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

/*virtual*/ bool FileSystem::file_exists(const char* file_path) {

	return SD.exists(file_path);
}

/*virtual*/ size_t FileSystem::read_file(const char* file_path, RNS::Bytes& data) {
	size_t read = 0;

	File file = SD.open(file_path, FILE_READ);
	if (file) {
		size_t size = file.size();
		read = file.readBytes((char*)data.writable(size), size);

		file.close();
	}
	else {
		ERROR("read_file: failed to open input file " + std::string(file_path));
	}
    return read;
}

/*virtual*/ size_t FileSystem::write_file(const char* file_path, const RNS::Bytes& data) {
	// CBA TODO Replace remove with working truncation
	remove_file(file_path);
    size_t wrote = 0;

	File file = SD.open(file_path, FILE_WRITE);
	if (file) {
		wrote = file.write(data.data(), data.size());
        TRACE("write_file: wrote " + std::to_string(wrote) + " bytes to file " + std::string(file_path));
        if (wrote < data.size()) {
			WARNING("write_file: not all data was written to file " + std::string(file_path));
		}
		//TRACE("write_file: closing output file");

		file.close();

	}
	else {
		ERROR("write_file: failed to open output file " + std::string(file_path));
	}
    return wrote;
}

/*virtual*/ RNS::FileStream FileSystem::open_file(const char* file_path, RNS::FileStream::MODE file_mode) {
	TRACEF("open_file: opening file %s", file_path);
	const char* mode;
	if (file_mode == RNS::FileStream::MODE_READ) {
		mode = FILE_READ;
	}
	else if (file_mode == RNS::FileStream::MODE_WRITE) {
		mode = FILE_WRITE;
	}
	else if (file_mode == RNS::FileStream::MODE_APPEND) {
		mode = FILE_APPEND;
	}
	else {
		ERRORF("open_file: unsupported mode %d", file_mode);
		return {RNS::Type::NONE};
	}
	TRACEF("open_file: opening file %s in mode %s", file_path, mode);
	//// Using copy constructor to create a File* instead of local
	//File file = SPIFFS.open(file_path, mode);
	//if (!file) {
	File* file = new File(SD.open(file_path, mode));
	if (file == NULL || !(*file)) {
		ERRORF("open_file: failed to open output file %s", file_path);
		return {RNS::Type::NONE};
	}
	TRACEF("open_file: successfully opened file %s", file_path);
	return RNS::FileStream(new FileStreamImpl(file));
}

/*virtual*/ bool FileSystem::remove_file(const char* file_path) {

	return SD.remove(file_path);

}

/*virtual*/ bool FileSystem::rename_file(const char* from_file_path, const char* to_file_path) {
	return SD.rename(from_file_path, to_file_path);

}

/*virtua*/ bool FileSystem::directory_exists(const char* directory_path) {
	TRACE("directory_exists: checking for existence of directory " + std::string(directory_path));
	File file = SD.open(directory_path, FILE_READ);
	if (file) {
		bool is_directory = file.isDirectory();
		file.close();
		return is_directory;
	}
    return false;
}

/*virtual*/ bool FileSystem::create_directory(const char* directory_path) {
	if (!SD.mkdir(directory_path)) {
		ERROR("create_directory: failed to create directorty " + std::string(directory_path));
		return false;
	}
	return true;

}

/*virtua*/ bool FileSystem::remove_directory(const char* directory_path) {
	TRACE("remove_directory: removing directory " + std::string(directory_path));
	//if (!LittleFS.rmdir_r(directory_path)) {
	if (!SD.rmdir(directory_path)) {
		ERROR("remove_directory: failed to remove directorty " + std::string(directory_path));
		return false;
	}
	return true;
}

/*virtua*/ std::list<std::string> FileSystem::list_directory(const char* directory_path) {
	TRACE("list_directory: listing directory " + std::string(directory_path));
	std::list<std::string> files;

	File root = SD.open(directory_path);

	if (!root) {
		ERROR("list_directory: failed to open directory " + std::string(directory_path));
		return files;
	}
	File file = root.openNextFile();
	while (file) {
		if (!file.isDirectory()) {
			char* name = (char*)file.name();
			files.push_back(name);
		}
		// CBA Following close required to avoid leaking memory
		file.close();
		file = root.openNextFile();
	}
	TRACE("list_directory: returning directory listing");
	root.close();
	return files;
}


/*virtual*/ size_t FileSystem::storage_size() {
	return SD.totalBytes();
}

/*virtual*/ size_t FileSystem::storage_available() {
	return (SD.totalBytes() - SD.usedBytes());
}

