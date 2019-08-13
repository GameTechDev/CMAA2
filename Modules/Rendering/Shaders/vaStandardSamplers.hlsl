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

#ifndef VA_STANDARD_SAMPLERS_HLSL_INCLUDED
#define VA_STANDARD_SAMPLERS_HLSL_INCLUDED

#ifndef VA_COMPILED_AS_SHADER_CODE
#error not intended to be included outside of HLSL!
#endif

#include "vaSharedTypes.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global sampler slots

SamplerComparisonState                  g_samplerLinearCmpSampler               : register( S_CONCATENATER( SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT ) ); 

SamplerState                            g_samplerPointClamp                     : register( S_CONCATENATER( SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT         ) ); 
SamplerState                            g_samplerPointWrap                      : register( S_CONCATENATER( SHADERGLOBAL_POINTWRAP_SAMPLERSLOT          ) ); 
SamplerState                            g_samplerLinearClamp                    : register( S_CONCATENATER( SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT        ) ); 
SamplerState                            g_samplerLinearWrap                     : register( S_CONCATENATER( SHADERGLOBAL_LINEARWRAP_SAMPLERSLOT         ) ); 
SamplerState                            g_samplerAnisotropicClamp               : register( S_CONCATENATER( SHADERGLOBAL_ANISOTROPICCLAMP_SAMPLERSLOT   ) ); 
SamplerState                            g_samplerAnisotropicWrap                : register( S_CONCATENATER( SHADERGLOBAL_ANISOTROPICWRAP_SAMPLERSLOT    ) ); 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif // VA_STANDARD_SAMPLERS_HLSL_INCLUDED