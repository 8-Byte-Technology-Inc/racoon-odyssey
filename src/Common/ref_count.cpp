/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "ref_count.h"

namespace TB8
{

ref_count::ref_count()
	: m_count(1)
{
}

s32 ref_count::AddRef()
{
	return m_count.fetch_add(1) + 1;
}

s32 ref_count::Release()
{
	s32 count = m_count.fetch_sub(1) - 1;
	if (0 == count)
	{
		__Free();
	}
	return count;
}

}
