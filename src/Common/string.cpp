#include "pch.h"

#include "string.h"

namespace TB8
{

char ValToHexAscii(u8 val)
{
	return (val < 10) ? ('0' + val) : ('a' + val - 10);
}

u8 HexAsciiToVal(char ch)
{
	if ('0' <= ch && ch <= '9')
		return ch - '0';
	else if ('A' <= ch && ch <= 'F')
		return ch - 'A' + 10;
	else if ('a' <= ch && ch <= 'f')
		return ch - 'a' + 10;
	return 0;
}

void BytesToHexAscii(const u8* pSrc, u32 srcSize, std::string* pDst)
{
	pDst->reserve(srcSize * 2);
	for (u32 i = 0; i < srcSize; ++i)
	{
		pDst->push_back(TB8::ValToHexAscii((pSrc[i] & 0xf0) >> 4));
		pDst->push_back(TB8::ValToHexAscii((pSrc[i] & 0x0f) >> 0));
	}
}

void StrTok(const char* psz, const char* pszDelimit, std::vector<std::string>* pValues)
{
	const char* pszStart = psz;
	const char* pszCursor = pszStart;
	while (true)
	{
		// check for delimiter.
		bool foundDelimiter = (*pszCursor == 0);
		for (const char* pszDelimitCursor = pszDelimit; *pszDelimitCursor && !foundDelimiter; ++pszDelimitCursor)
		{
			if (*pszDelimitCursor == *pszCursor)
				foundDelimiter = true;
		}

		if (!foundDelimiter)
		{
			++pszCursor;
			continue;
		}

		if (pszStart == pszCursor)
		{
			if (!*pszCursor)
				break;
			++pszStart;
			pszCursor = pszStart;
			continue;
		}

		pValues->emplace_back();
		std::string& value = pValues->back();
		value.assign(pszStart, pszCursor - pszStart);

		if (!*pszCursor)
			break;
		
		pszStart = pszCursor + 1;
		pszCursor = pszStart;
	}
}

}

#if !defined(WIN32)

int vsprintf_s(char* buffer, size_t size, const char* format, va_list& argptr)
{
	return vsprintf(buffer, format, argptr);
}

int sprintf_s(char* buffer, size_t size, const char* format, ... )
{
	va_list argptr;
	va_start(argptr, format);
	int r = vsprintf_s(buffer, size, format, argptr);
	va_end(argptr);
	return r;
}

int _stricmp(const char* lhs, const char* rhs)
{
	return strcasecmp(lhs, rhs);
}

int _strcmpi(const char* lhs, const char* rhs)
{
	return strcasecmp(lhs, rhs);
}

int _strnicmp(const char* lhs, const char* rhs, size_t count)
{
	return strncasecmp(lhs, rhs, count);
}

#endif
