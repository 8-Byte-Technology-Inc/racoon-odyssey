/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "memory.h"

namespace TB8
{

void* alloc(size_t size, const char* pszFile, int line)
{
	return malloc(size);
}

void* realloc(void* ptr, size_t size, const char* pszFile, int line)
{
	return ::realloc(ptr, size);
}

void free(void* ptr)
{
	::free(ptr);
}

}

void* operator new(size_t s, const char* pszFile, int line)
{
	return TB8::alloc(s, pszFile, line);
}

void operator delete(void* p, const char* pszFile, int line)
{
	TB8::free(p);
}

