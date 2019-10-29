#pragma once

#include <string>
#include <vector>
#include <stdarg.h>

#include "basic_types.h"

namespace TB8
{
	
char ValToHexAscii(u8 val);
u8 HexAsciiToVal(char ch);
void BytesToHexAscii(const u8* pSrc, u32 srcSize, std::string* pDst);
void StrTok(const char* psz, const char* pszDelimit, std::vector<std::string>* values);
	
}

#if !defined(WIN32)

int vsprintf_s(char* buffer, size_t size, const char* format, va_list& argptr);
template <size_t size> int vsprintf_s(char (&buffer)[size], const char* format, va_list& argptr)
{
	return vsprintf_s(buffer, size, format, argptr);
}

int sprintf_s(char* buffer, size_t sizeOfBuffer, const char* format, ... );
template <size_t size> int sprintf_s(char (&buffer)[size], const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	int r = vsprintf_s(buffer, size, format, argptr);
	va_end(argptr);
	return r;
}

int _strcmpi(const char* lhs, const char* rhs);
int _stricmp(const char* lhs, const char* rhs);
int _strnicmp(const char* lhs, const char* rhs, size_t count);

#endif
