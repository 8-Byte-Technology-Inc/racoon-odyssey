#pragma once

#include "common/basic_types.h"

namespace TB8
{

enum RenderScaleSize : u32
{
	RenderScaleSize_Invalid = 0,
	RenderScaleSize_100,
	RenderScaleSize_125,
	RenderScaleSize_150,
	RenderScaleSize_200,
	RenderScaleSize_250,
};

s32 ComputeViewPixelsFromHardPixels(s32 hardPixels, RenderScaleSize scaleSize);
s32 GetScalePercent(RenderScaleSize scaleSize);
RenderScaleSize GetRenderScaleFromDPI(s32 dpi);

}
