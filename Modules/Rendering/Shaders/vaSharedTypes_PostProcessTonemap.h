///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VA_SHADER_SHARED_TYPES_POSTPROCESS_TONEMAP_HLSL
#define VA_SHADER_SHARED_TYPES_POSTPROCESS_TONEMAP_HLSL

#include "vaShaderCore.h"

#ifndef VA_COMPILED_AS_SHADER_CODE
namespace VertexAsylum
{
#endif

    struct PostProcessTonemapConstants
    {
        float                   AverageLuminanceMIP;
        float                   Exposure;
        float                   WhiteLevel;
        float                   Saturation;

        float                   BloomThreshold;
        float                   BloomMultiplier;

        // just above values, pre-calculated for faster shader math
        float                   Exp2Exposure;
        float                   WhiteLevelSquared;

        vaVector2               FullResPixelSize;
        vaVector2               BloomSampleUVMul;
    };

#ifndef VA_COMPILED_AS_SHADER_CODE
} // namespace VertexAsylum
#endif

#define POSTPROCESS_TONEMAP_CONSTANTSBUFFERSLOT    1

#define POSTPROCESS_TONEMAP_TEXTURE_SLOT0           0
#define POSTPROCESS_TONEMAP_TEXTURE_SLOT1           1


#ifdef VA_COMPILED_AS_SHADER_CODE

cbuffer PostProcessTonemapConstantsBuffer : register( B_CONCATENATER( POSTPROCESS_TONEMAP_CONSTANTSBUFFERSLOT ) )
{
    PostProcessTonemapConstants              g_PostProcessTonemapConsts;
}

#endif

#endif // VA_SHADER_SHARED_TYPES_POSTPROCESS_TONEMAP_HLSL