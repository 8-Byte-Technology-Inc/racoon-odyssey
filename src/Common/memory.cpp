/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "memory.h"

namespace TB8
{
namespace memory
{

void* malloc(size_t size, const char* pszFile, int line)
{
	return ::malloc(size);
}

void* realloc(void* ptr, size_t size, const char* pszFile, int line)
{
	return ::realloc(ptr, size);
}

void free(void* ptr)
{
	::free(ptr);
}

struct sig g_sig;

}
}

void* operator new(size_t s, TB8::memory::sig& sig, const char* pszFile, int line)
{
	return TB8::memory::malloc(s, pszFile, line);
}

void operator delete(void* p, TB8::memory::sig& sig, const char* pszFile, int line)
{
	TB8::memory::free(p);
}

