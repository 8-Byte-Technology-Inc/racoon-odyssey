#pragma once

namespace TB8
{
namespace memory
{

void* malloc(size_t size, const char* pszFile, int line);
void* realloc(void* ptr, size_t size, const char* pszFile, int line);
void free(void* ptr);

struct sig
{
	// signature to avoid conflicting new operators.
};

extern struct sig g_sig;

}
}

#define TB8_MALLOC(s) \
	TB8::memory::malloc(s, __FILE__, __LINE__)

#define TB8_REALLOC(p, s) \
	TB8::memory::realloc(p, s, __FILE__, __LINE__)

#define TB8_FREE(p) \
	TB8::memory::free(p)

void* operator new(size_t s, TB8::memory::sig& sig, const char* pszFile, int line);
void operator delete(void* p, TB8::memory::sig& sig, const char* pszFile, int line);

#define TB8_NEW(x) \
	new(TB8::memory::g_sig, __FILE__, __LINE__) (x)

template<class T> void TB8_DEL(T* p)
{
	if (p)
	{
		p->~T();
		TB8_FREE(p);
	}
}
