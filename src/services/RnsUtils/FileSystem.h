#pragma once


#include <cstddef>
#include <FileSystem.h>
#include <FileStream.h>
#include <Bytes.h>
#include <FS.h>

class FileSystem : public RNS::FileSystemImpl {

public:
	FileSystem()  {}

	bool init();

public:
	static void listDir(const char* dir, int levels =3);

public:
	virtual bool file_exists(const char* file_path);
	virtual size_t read_file(const char* file_path, RNS::Bytes& data);
	virtual size_t write_file(const char* file_path, const RNS::Bytes& data);
	virtual RNS::FileStream open_file(const char* file_path, RNS::FileStream::MODE file_mode);
	virtual bool remove_file(const char* file_path);
	virtual bool rename_file(const char* from_file_path, const char* to_file_path);
	virtual bool directory_exists(const char* directory_path);
	virtual bool create_directory(const char* directory_path);
	virtual bool remove_directory(const char* directory_path);
	virtual std::list<std::string> list_directory(const char* directory_path);
	virtual size_t storage_size();
	virtual size_t storage_available();

protected:
	class FileStreamImpl : public RNS::FileStreamImpl {

	private:
		std::unique_ptr<File> _file;
		bool _closed = false;

	public:
		FileStreamImpl(File* file) : RNS::FileStreamImpl(), _file(file) {}
		virtual ~FileStreamImpl() { if (!_closed) close(); }

	public:
		inline virtual const char* name() { return _file->name(); }
		inline virtual size_t size() { return _file->size(); }
		inline virtual void close() { _closed = true; _file->close(); }

		// Print overrides
		inline virtual size_t write(uint8_t byte) { return _file->write(byte); }
		inline virtual size_t write(const uint8_t *buffer, size_t size) { return _file->write(buffer, size); }

		// Stream overrides
		inline virtual int available() { return _file->available(); }
		inline virtual int read() { return _file->read(); }
		inline virtual int peek() { return _file->peek(); }
		inline virtual void flush() { _file->flush(); }

	};

};
