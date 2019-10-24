#pragma once

#include <vector>
#include <string>

#include "basic_types.h"

namespace TB8
{
	
class File
{
public:
	static File* AllocOpen(const char* path, bool readOnly);
	static File* AllocCreate(const char* path);
	u32 Read(u8* pBuffer, u32 bytesToRead);
	u32 Write(const u8* pBuffer, u32 bytesToWrite);
	void WriteText(const char* fmt, ...);
	u32 Read(std::vector<u8>* dst);
	void Flush();
	void Free();

	static void CreateDir(const char* path);

	static void AppendToPath(std::string& path, const char* pathOrFile);
	static void SetExtension(std::string& path, const char* extension);
	static void StripFileNameFromPath(std::string& path);
	static void StripFilePathFromPath(std::string& path);
	static void StripFileExtFromName(std::string& path);
	static void StripFileNameFromName(std::string& path);
	static void ParsePath(const char* fullPathAndFile, std::string* path, std::string* fileName);
	
private:
	File();
	~File();

	HANDLE m_hFile;
};
	
};
