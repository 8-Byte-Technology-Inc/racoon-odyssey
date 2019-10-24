#include "pch.h"

#include "RenderScale.h"

namespace TB8
{

s32 ComputeViewPixelsFromHardPixels(s32 hardPixels, RenderScaleSize scaleSize)
{
	switch (scaleSize)
	{
		default:
		case RenderScaleSize_100:
			return hardPixels;
		case RenderScaleSize_125:
			return (hardPixels * 125) / 100;
		case RenderScaleSize_150:
			return (hardPixels * 150) / 100;
		case RenderScaleSize_200:
			return (hardPixels * 200) / 100;
		case RenderScaleSize_250:
			return (hardPixels * 250) / 100;
	}
}

s32 GetScalePercent(RenderScaleSize scaleSize)
{
	switch (scaleSize)
	{
	default:
	case RenderScaleSize_100:
		return 100;
	case RenderScaleSize_125:
		return 125;
	case RenderScaleSize_150:
		return 150;
	case RenderScaleSize_200:
		return 200;
	case RenderScaleSize_250:
		return 250;
	}
}

static struct DPI_Map
{
	s32 m_dpi;
	RenderScaleSize m_scaleSize;
} s_DPI_Map[] =
{
	{ 96, RenderScaleSize_100 },
	{ 120, RenderScaleSize_125 },
	{ 144, RenderScaleSize_150 },
	{ 196, RenderScaleSize_200 },
	{ 240, RenderScaleSize_250 },
};

RenderScaleSize GetRenderScaleFromDPI(s32 dpi)
{
	for (s32 i = 0; i < ARRAYSIZE(s_DPI_Map); ++i)
	{
		if (dpi <= s_DPI_Map[i].m_dpi)
		{
			return s_DPI_Map[i].m_scaleSize;
		}
	}
	return s_DPI_Map[ARRAYSIZE(s_DPI_Map) - 1].m_scaleSize;
}

}
