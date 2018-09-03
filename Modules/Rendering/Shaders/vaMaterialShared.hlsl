///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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



#ifndef VA_MATERIALSHARED_SH_INCLUDED
#define VA_MATERIALSHARED_SH_INCLUDED

#include "vaShared.hlsl"

struct RenderMaterialShaderInterpolants
{
             float4 Position            : SV_Position;
    /*centroid*/ float4 Color               : COLOR;
    /*centroid*/ float4 ViewspacePos        : TEXCOORD0;
    /*centroid*/ float4 ViewspaceNormal     : NORMAL0;
    /*centroid*/ float4 Texcoord01          : TEXCOORD1;
//  /*centroid*/ float4 Texcoord23          : TEXCOORD2;      // currently commented out - texcoords 2 and 3 are probably never going to be needed but just in case (maybe lightmaps + object space shading or something?)
};


//cbuffer B2 : register(b2)
//{
//   MaterialConstants    g_material;
//};

// this will declare textures for the current material
VA_RM_TEXTURE_DECLARATIONS;


// predefined input macros:
// VA_RM_INPUT_LOAD_#name( interpolants )           : if texture, will use interpolants to load; if value, will ignore interpolants
// VA_RM_INPUT_#name#_TYPE                          : int, float, float4
// VA_RM_INPUT_#name#_IS_TEXTURE                    : 0 or 1
//
// defined only if type is bool and it is not dynamic:
// VA_RM_INPUT_#name#_CONST_BOOLEAN                 : 0 or 1; this is for use by macros, bool value can still be read the normal way with VA_RM_LOAD_INPUT( name, interpolants ) )
//
// defined only if type is texture:
// VA_RM_INPUT_#name#_TEXTURE                       : name of the Texture2D<TYPE> global variable with correct ":register( tx )" 
// VA_RM_INPUT_#name#_TEXTURE_CONTENTS_TYPE_XXX     : where XXX is GENERIC_COLOR, GENERIC_LINEAR, NORMAL_XYZ_UNORM, etc and value is '1' if defined
// VA_RM_INPUT_#name#_TEXTURE_UV_INDEX              : 0, 1, ... - index of the (vertex) UV stream in the interpolants
// VA_RM_INPUT_#name#_TEXTURE_SAMPLER               : name of one of the predefined samplers used for sampling
// VA_RM_INPUT_#name#_TEXTURE_UV( __interpolants )  : actual resolved texture coordinates (looks something like this after resolved: __interpolants.Texcoord01.xy)


#endif // VA_MATERIALSHARED_SH_INCLUDED
