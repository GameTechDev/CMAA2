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

#include "vaShared.hlsl"

#include "vaSharedTypes_PostProcessBlur.h"

Texture2D            g_sourceTexure0               : register( T_CONCATENATER( POSTPROCESS_BLUR_TEXTURE_SLOT0 ) );
Texture2D            g_sourceTexure1               : register( T_CONCATENATER( POSTPROCESS_BLUR_TEXTURE_SLOT1 ) );

/*
float4 PSBlurA( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    float2 halfPixel = g_PostProcessBlurConsts.PixelSize * 0.5f;

    // scale by blur factor
    halfPixel *= g_PostProcessBlurConsts.Factor0;

    float4 vals[4];
    vals[0] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( -halfPixel.x, -halfPixel.y * 3 ), 0.0 );
    vals[1] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( +halfPixel.x * 3, -halfPixel.y ), 0.0 );
    vals[2] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( +halfPixel.x, +halfPixel.y * 3 ), 0.0 );
    vals[3] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( -halfPixel.x * 3, +halfPixel.y ), 0.0 );

    float4 avgVal = (vals[0] + vals[1] + vals[2] + vals[3]) * 0.25;

    // avgVal = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV, 0 );

    return avgVal;
}

float4 PSBlurB( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    float2 halfPixel = g_PostProcessBlurConsts.PixelSize * 0.5f;

    // scale by blur factor
    halfPixel *= g_PostProcessBlurConsts.Factor0;

    float4 vals[4];
    vals[0] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( -halfPixel.x * 3, -halfPixel.y ), 0.0 );
    vals[1] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( +halfPixel.x, -halfPixel.y * 3 ), 0.0 );
    vals[2] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( +halfPixel.x * 3, +halfPixel.y ), 0.0 );
    vals[3] = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV + float2( -halfPixel.x, +halfPixel.y * 3 ), 0.0 );

    float4 avgVal = (vals[0] + vals[1] + vals[2] + vals[3]) * 0.25;

    // avgVal = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, inUV, 0 );

    return avgVal;
}
*/

float3 GaussianBlur( float2 centreUV, float2 pixelOffset )
{
    float3 colOut = float3( 0, 0, 0 );

    const int stepCount = g_PostProcessBlurConsts.GaussIterationCount;

    for( int i = 0; i < stepCount; i++ )                                                                                                                             
    {
        float offset = g_PostProcessBlurConsts.GaussOffsetsWeights[i].x;
        float weight = g_PostProcessBlurConsts.GaussOffsetsWeights[i].y;

        float2 texCoordOffset = offset.xx * pixelOffset;

        float3 col = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, centreUV + texCoordOffset, 0.0 ).xyz
                   + g_sourceTexure0.SampleLevel( g_samplerLinearClamp, centreUV - texCoordOffset, 0.0 ).xyz;

        colOut += weight.xxx * col;                                                                                                                               
    }                                                                                                                                                                

    return colOut;                                                                                                                                                   
}

float4 PSGaussHorizontal( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    float3 col = GaussianBlur( inUV, float2( g_PostProcessBlurConsts.PixelSize.x, 0.0 ) );

    return float4( col, 1.0 );
}

float4 PSGaussVertical( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    float3 col = GaussianBlur( inUV, float2( 0.0, g_PostProcessBlurConsts.PixelSize.y ) );

    return float4( col, 1.0 );
}