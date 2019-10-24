#include "pch.h"

#include <stack>

#include "file_io.h"

namespace TB8
{

File::File()
{
	m_hFile = INVALID_HANDLE_VALUE;
}

File::~File()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

File* File::AllocOpen(const char* path, bool readOnly)
{
	File* pObj = new File();
	WCHAR wPath[MAX_PATH];
	mbstowcs_s(nullptr, wPath, path, ARRAYSIZE(wPath));

	pObj->m_hFile = CreateFile(wPath,
		readOnly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (pObj->m_hFile == INVALID_HANDLE_VALUE)
	{
		delete pObj;
		return nullptr;
	}
	return pObj;
}

File* File::AllocCreate(const char* path)
{
	File* pObj = new File();
	WCHAR wPath[MAX_PATH];
	mbstowcs_s(nullptr, wPath, path, ARRAYSIZE(wPath));

	pObj->m_hFile = CreateFile(
		wPath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (pObj->m_hFile == INVALID_HANDLE_VALUE)
	{
		delete pObj;
		return nullptr;
	}
	return pObj;
}

u32 File::Read(u8* pBuffer, u32 bytesToRead)
{
	DWORD bytesRead = 0;
	ReadFile(m_hFile, pBuffer, bytesToRead, &bytesRead, nullptr);
	return bytesRead;
}

u32 File::Read(std::vector<u8>* dst)
{
	u32 fileSize = 0;
	// Get the file size
	FILE_STANDARD_INFO fileInfo;
	if (!GetFileInformationByHandleEx(m_hFile, FileStandardInfo, &fileInfo, sizeof(fileInfo)))
	{
		return 0;
	}

	assert(fileInfo.EndOfFile.QuadPart < 0x100000000);
	fileSize = static_cast<u32>(fileInfo.EndOfFile.QuadPart);
	
	dst->resize(fileSize);
	const u32 bytesRead = Read(dst->data(), fileSize);
	dst->resize(bytesRead);
	return bytesRead;
}

u32 File::Write(const u8* pBuffer, u32 bytesToWrite)
{
	u32 bytesWritten = 0;
	WriteFile(m_hFile, pBuffer, bytesToWrite, reinterpret_cast<LPDWORD>(&bytesWritten), nullptr);
	return bytesWritten;
}

void File::WriteText(const char* fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	int used = vsprintf_s(buffer, fmt, args);
	va_end(args);
	Write(reinterpret_cast<u8*>(buffer), used);
}

void File::Flush()
{
	FlushFileBuffers(m_hFile);
}

void File::Free()
{
	delete this;
}

void File::CreateDir(const char* path)
{
	WCHAR wPath[MAX_PATH];
	mbstowcs_s(nullptr, wPath, path, ARRAYSIZE(wPath));
	if (CreateDirectoryW(wPath, nullptr))
		return;
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		return;

	std::stack<std::string> paths;
	std::string tempPath = path;
	while (!tempPath.empty() && tempPath.back() != '.')
	{
		std::string path1;
		std::string file1;
		paths.push(tempPath);
		ParsePath(tempPath.c_str(), &path1, &file1);
		tempPath = path1;
	}

	while (!paths.empty())
	{
		std::string curPath = paths.top();
		paths.pop();

		mbstowcs_s(nullptr, wPath, curPath.c_str(), ARRAYSIZE(wPath));
		CreateDirectoryW(wPath, nullptr);
	}
}

void File::AppendToPath(std::string& path, const char* file)
{
	std::string tempPath = path;
	while (true)
	{
		if (file[0] == '.' && file[1] == '.')
		{
			std::string path1;
			std::string file1;
			ParsePath(tempPath.c_str(), &path1, &file1);
			if (path1.empty())
				break;

			tempPath = path1;
			file += 2;
			if (*file == '\\' || *file == '/')
				++file;
		}
		else if ((file[0] == '.') && ((file[1] == '\\') || (file[1] == '/')))
		{
			file += 2;
		}
		else if ((file[0] == '\\') || (file[0] == '/'))
		{
			++file;
		}
		else
		{
			break;
		}
	}

	if (!*file)
		return;

	path = tempPath;

	if (!path.empty() && (path[path.length() - 1] != '\\'))
	{
		path += "\\";
	}

	path.append(file);
}

void File::SetExtension(std::string& path, const char* extension)
{
	while (!path.empty() && (path[path.length() - 1] != '.'))
		path.pop_back();
	if (path[path.length() - 1] == '.')
		path.pop_back();
	path += extension;
}

void File::StripFilePathFromPath(std::string& path)
{
	for (s32 i = 0; i <  static_cast<s32>(path.size()); ++i)
	{
		if (path[i] == '\\' || path[i] == '/')
		{
			path.erase(path.begin(), path.begin() + i + 1);
			i = -1;
		}
	}
}

void File::StripFileNameFromPath(std::string& path)
{
	for (s32 i = static_cast<s32>(path.size()) - 1; i >= 0; --i)
	{
		if (path[i] == '\\' || path[i] == '/')
		{
			path.erase(path.begin() + i, path.end());
			return;
		}
	}
	path.clear();
}

void File::StripFileExtFromName(std::string& name)
{
	for (s32 i = static_cast<s32>(name.size()) - 1; i >= 0; --i)
	{
		if (name[i] == '.')
		{
			name.erase(name.begin() + i, name.end());
			return;
		}
	}
}

void File::StripFileNameFromName(std::string& name)
{
	for (s32 i = 0; i < static_cast<s32>(name.size()); ++i)
	{
		if (name[i] == '.')
		{
			name.erase(name.begin(), name.begin() + i + 1);
			i = -1;
		}
	}
}

void File::ParsePath(const char* fullPathAndFile, std::string* path, std::string* fileName)
{
	const s32 len = static_cast<s32>(strlen(fullPathAndFile));

	for (s32 i = len - 1; i >= 0; --i)
	{
		if (fullPathAndFile[i] == '\\' || fullPathAndFile[i] == '/')
		{
			*path = fullPathAndFile;
			path->resize(i);

			*fileName = &(fullPathAndFile[i + 1]);
			return;
		}
	}

	*fileName = fullPathAndFile;
}

}; // namespace TB8
