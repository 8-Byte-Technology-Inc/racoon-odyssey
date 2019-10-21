#pragma once

namespace TB8
{

void* alloc(size_t size, const char* pszFile, int line);
void* realloc(void* ptr, size_t size, const char* pszFile, int line);
void free(void* ptr);

}

#define TB8_ALLOC(s) \
	TB8::alloc(s, __FILE__, __LINE__)

#define TB8_REALLOC(p, s) \
	TB8::realloc(p, s, __FILE__, __LINE__)

#define TB8_FREE(p) \
	TB8::free(p)

void* operator new(size_t s, const char* pszFile, int line);
void operator delete(void* p, const char* pszFile, int line);

#define TB8_NEW(x) \
	new(__FILE__, __LINE__) (x)

template<class T> void TB8_DEL(T* p)
{
	if (p)
	{
		p->~T();
		TB8_FREE(p);
	}
}
