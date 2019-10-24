#pragma once

#include <atomic>

#include "common/basic_types.h"

namespace TB8
{

class ref_count
{
public:
	ref_count();

	s32 AddRef();
	s32 Release();

private:
	virtual void __Free() = 0;

	std::atomic<s32>	m_count;
};

}
