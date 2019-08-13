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

#ifndef VA_SHADER_SHARED_TYPES_POSTPROCESS_BLUR_HLSL
#define VA_SHADER_SHARED_TYPES_POSTPROCESS_BLUR_HLSL

#include "vaShaderCore.h"

#ifndef VA_COMPILED_AS_SHADER_CODE
namespace VertexAsylum
{
#endif

    // all of this is unused at the moment
    struct PostProcessBlurConstants
    {
        vaVector2               PixelSize;
        float                   Factor0;
        float                   Dummy0;

        int                     GaussIterationCount;
        int                     Dummy1;
        int                     Dummy2;
        int                     Dummy3;
        vaVector4               GaussOffsetsWeights[1024];
    };

#ifndef VA_COMPILED_AS_SHADER_CODE
} // namespace VertexAsylum
#endif

#define POSTPROCESS_BLUR_CONSTANTSBUFFERSLOT    1

#define POSTPROCESS_BLUR_TEXTURE_SLOT0           0
#define POSTPROCESS_BLUR_TEXTURE_SLOT1           1


#ifdef VA_COMPILED_AS_SHADER_CODE

cbuffer PostProcessBlurConstantsBuffer : register( B_CONCATENATER( POSTPROCESS_BLUR_CONSTANTSBUFFERSLOT ) )
{
    PostProcessBlurConstants              g_PostProcessBlurConsts;
}

#endif

#endif // VA_SHADER_SHARED_TYPES_POSTPROCESS_BLUR_HLSL