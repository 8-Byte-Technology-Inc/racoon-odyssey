#pragma once

#include <atomic>

#include "common/basic_types.h"

namespace TB8
{

class ref_count
{
public:
	ref_count();

	s32 addref();
	s32 release();

private:
	virtual void __free() = 0;

	std::atomic<s32>	m_count;
};

}
