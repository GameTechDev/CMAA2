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

#include "vaMaterialShared.hlsl"

struct RenderMeshStandardVertexInput
{
    float4 Position             : SV_Position;
    float4 Color                : COLOR;
    float4 Normal               : NORMAL;
    float2 Texcoord0            : TEXCOORD0;
    float2 Texcoord1            : TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// float4 VS_PosOnly( const in RenderMeshStandardVertexInput input ) : SV_Position
// {
//     return mul( g_RenderMeshGlobal.ShadowWorldViewProj, float4( input.Position.xyz, 1) );
// }

RenderMaterialShaderInterpolants VS_Standard( const in RenderMeshStandardVertexInput input )
{
    RenderMaterialShaderInterpolants ret;

    //ret.Color                   = input.Color;
    ret.Texcoord01          = float4( input.Texcoord0, input.Texcoord1 );
    //ret.Texcoord23          = float4( 0, 0, 0, 0 );

    ret.ViewspacePos        = mul( g_RenderMeshGlobal.WorldView, float4( input.Position.xyz, 1) );
    ret.ViewspaceNormal.xyz = normalize( mul( (float3x3)g_RenderMeshGlobal.NormalWorldView, input.Normal.xyz ).xyz );

    {
        float viewspaceLength = length( ret.ViewspacePos.xyz );
        float slope = saturate( 1 - abs( dot( ret.ViewspacePos.xyz / viewspaceLength, ret.ViewspaceNormal.xyz ) ) );
        float scale = lerp( viewspaceLength * g_Global.ViewspaceDepthOffsetFlatMul + g_Global.ViewspaceDepthOffsetFlatAdd,
                                viewspaceLength * g_Global.ViewspaceDepthOffsetSlopeMul + g_Global.ViewspaceDepthOffsetSlopeAdd, 
                                slope ) / viewspaceLength;
        ret.ViewspacePos.xyz *= scale;
    }

    ret.Position            = mul( g_Global.Proj, float4( ret.ViewspacePos.xyz, 1.0 ) );

//    // this is unused but hey, copy it anyway
//    ret.ViewspaceNormal.w   = input.Normal.w;

    // hijack this for highlighting and similar stuff
    ret.Color               = g_RenderMeshGlobal.ColorOverride;

    // For normals that are facing back (or close to facing back, slightly skew them back towards the eye to 
    // reduce (mostly specular) aliasing artifacts on edges; do it separately for potential forward or back
    // looking vectors because facing is determinted later, and the correct one is picked in the shader.
    float3 viewDir          = normalize( ret.ViewspacePos.xyz );
    float normalAwayFront   = saturate( ( dot( viewDir,  ret.ViewspaceNormal.xyz ) + 0.08 ) * 1.0 );
    float normalAwayBack    = saturate( ( dot( viewDir, -ret.ViewspaceNormal.xyz ) + 0.08 ) * 1.0 );

    ret.ViewspaceNormal.w   = normalAwayFront;
    ret.ViewspacePos.w      = normalAwayBack;

    return ret;
}
