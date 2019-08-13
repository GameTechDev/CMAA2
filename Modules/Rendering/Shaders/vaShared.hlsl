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

#ifndef VASHARED_HLSL_INCLUDED
#define VASHARED_HLSL_INCLUDED

#ifndef VA_COMPILED_AS_SHADER_CODE
#error not intended to be included outside of HLSL!
#endif

#include "vaSharedTypes.h"

#include "vaStandardSamplers.hlsl"


///////////////////////////////////////////////////////////////////////////////////////////////////
// Global utility functions (some might be using constants above, some not)

#ifndef VA_PI
#define VA_PI               	(3.1415926535897932384626433832795)
#endif

// for Texel to Vertex mapping (3x3 kernel, the texel centers are at quad vertices)
float3  ComputeHeightmapNormal( float h00, float h10, float h20, float h01, float h11, float h21, float h02, float h12, float h22, const float3 pixelWorldSize )
{
    // Sobel 3x3
	//    0,0 | 1,0 | 2,0
	//    ----+-----+----
	//    0,1 | 1,1 | 2,1
	//    ----+-----+----
	//    0,2 | 1,2 | 2,2

    h00 -= h11;
    h10 -= h11;
    h20 -= h11;
    h01 -= h11;
    h21 -= h11;
    h02 -= h11;
    h12 -= h11;
    h22 -= h11;
   
	// The Sobel X kernel is:
	//
	// [ 1.0  0.0  -1.0 ]
	// [ 2.0  0.0  -2.0 ]
	// [ 1.0  0.0  -1.0 ]
	
	float Gx = h00 - h20 + 2.0 * h01 - 2.0 * h21 + h02 - h22;
				
	// The Sobel Y kernel is:
	//
	// [  1.0    2.0    1.0 ]
	// [  0.0    0.0    0.0 ]
	// [ -1.0   -2.0   -1.0 ]
	
	float Gy = h00 + 2.0 * h10 + h20 - h02 - 2.0 * h12 - h22;
	
	// The 0.5f leading coefficient can be used to control
	// how pronounced the bumps are - less than 1.0 enhances
	// and greater than 1.0 smoothes.
	
	//return float4( 0, 0, 0, 0 );
	
   float stepX = pixelWorldSize.x;
   float stepY = pixelWorldSize.y;
   float sizeZ = pixelWorldSize.z;
   
	Gx = Gx * stepY * sizeZ;
	Gy = Gy * stepX * sizeZ;
	
	float Gz = stepX * stepY * 8;
	
    return normalize( float3( Gx, Gy, Gz ) );
}

float3 normalize_safe( float3 val )
{
    float len = max( 0.0001, length( val ) );
    return val / len;
}

float3 normalize_safe( float3 val, float threshold )
{
    float len = max( threshold, length( val ) );
    return val / len;
}

float GLSL_mod( float x, float y )
{
    return x - y * floor( x / y );
}
float2 GLSL_mod( float2 x, float2 y )
{
    return x - y * floor( x / y );
}
float3 GLSL_mod( float3 x, float3 y )
{
    return x - y * floor( x / y );
}

// from https://www.shadertoy.com/view/lt2GDc - New Gradients from (0-1 float) Created by ChocoboBreeder in 2015-Jun-3
float3 GradientPalette( in float t, in float3 a, in float3 b, in float3 c, in float3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}
// rainbow gradient
float3 GradientRainbow( in float t )
{
    return GradientPalette( t, float3(0.55,0.4,0.3), float3(0.50,0.51,0.35)+0.1, float3(0.8,0.75,0.8), float3(0.075,0.33,0.67)+0.21 );
}
// from https://www.shadertoy.com/view/llKGWG - Heat map, Created by joshliebe in 2016-Oct-15
float3 GradientHeatMap( in float greyValue )
{
    float3 heat;      
    heat.r = smoothstep(0.5, 0.8, greyValue);
    if(greyValue >= 0.90) {
    	heat.r *= (1.1 - greyValue) * 5.0;
    }
	if(greyValue > 0.7) {
		heat.g = smoothstep(1.0, 0.7, greyValue);
	} else {
		heat.g = smoothstep(0.0, 0.7, greyValue);
    }    
	heat.b = smoothstep(1.0, 0.0, greyValue);          
    if(greyValue <= 0.3) {
    	heat.b *= greyValue / 0.3;     
    }
    return heat;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Various filters
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manual bilinear filter: input 'coords' is standard [0, 1] texture uv coords multiplied by [textureWidth, textureHeight]
float      BilinearFilter( float c00, float c10, float c01, float c11, float2 coords )
{
    float2 intPt    = floor(coords);
    float2 fractPt  = frac(coords);
    float top       = lerp( c00, c10, fractPt.x );
    float bottom    = lerp( c01, c11, fractPt.x );
    return lerp( top, bottom, fractPt.y );
}
float3      BilinearFilter( float3 c00, float3 c10, float3 c01, float3 c11, float2 coords )
{
    float2 intPt    = floor(coords);
    float2 fractPt  = frac(coords);
    float3 top      = lerp( c00, c10, fractPt.x );
    float3 bottom   = lerp( c01, c11, fractPt.x );
    return lerp( top, bottom, fractPt.y );
}
float4      BilinearFilter( float4 c00, float4 c10, float4 c01, float4 c11, float2 coords )
{
    float2 intPt    = floor(coords);
    float2 fractPt  = frac(coords);
    float4 top      = lerp( c00, c10, fractPt.x );
    float4 bottom   = lerp( c01, c11, fractPt.x );
    return lerp( top, bottom, fractPt.y );
}
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sRGB <-> linear conversions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float FLOAT_to_SRGB( float val )
{
    if( val < 0.0031308 )
        val *= float( 12.92 );
    else
        val = float( 1.055 ) * pow( abs( val ), float( 1.0 ) / float( 2.4 ) ) - float( 0.055 );
    return val;
}
float3 FLOAT3_to_SRGB( float3 val )
{
    float3 outVal;
    outVal.x = FLOAT_to_SRGB( val.x );
    outVal.y = FLOAT_to_SRGB( val.y );
    outVal.z = FLOAT_to_SRGB( val.z );
    return outVal;
}
float SRGB_to_FLOAT( float val )
{
    if( val < 0.04045 )
        val /= float( 12.92 );
    else
        val = pow( abs( val + float( 0.055 ) ) / float( 1.055 ), float( 2.4 ) );
    return val;
}
float3 SRGB_to_FLOAT3( float3 val )
{
    float3 outVal;
    outVal.x = SRGB_to_FLOAT( val.x );
    outVal.y = SRGB_to_FLOAT( val.y );
    outVal.z = SRGB_to_FLOAT( val.z );
    return outVal;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Perlin simplex noise
//
// Description : Array and textureless GLSL 2D simplex noise function.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110409 (stegu)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
float3 permute(float3 x) { return GLSL_mod(((x*34.0)+1.0)*x, 289.0); }
//
float snoise(float2 v)
{
   const float4 C = float4( 0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439 );
   float2 i  = floor(v + dot(v, C.yy) );
   float2 x0 = v -   i + dot(i, C.xx);
   float2 i1;
   i1 = (x0.x > x0.y) ? float2(1.0, 0.0) : float2(0.0, 1.0);
   float4 x12 = x0.xyxy + C.xxzz;
   x12.xy -= i1;
   i = GLSL_mod(i, 289.0);
   float3 p = permute( permute( i.y + float3( 0.0, i1.y, 1.0 ) ) + i.x + float3( 0.0, i1.x, 1.0 ) );
   float3 m = max(0.5 - float3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
   m = m*m ;
   m = m*m ;
   float3 x = 2.0 * frac(p * C.www) - 1.0;
   float3 h = abs(x) - 0.5;
   float3 ox = floor(x + 0.5);
   float3 a0 = x - ox;
   m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
   float3 g;
   g.x  = a0.x  * x0.x  + h.x  * x0.y;
   g.yz = a0.yz * x12.xz + h.yz * x12.yw;
   return 130.0 * dot(m, g);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hash functions from https://www.shadertoy.com/view/lt2yDm / https://www.shadertoy.com/view/4djSRW
// #pragma warning( disable : 3557 ) // error X3557: loop only executes for 0 iteration(s), forcing loop to unroll
//
float Hash2D( float2 uv )
{
    const float HASHSCALE1 = 0.1031;    // use 443.8975 for [0, 1] inputs
	float3 p3  = frac(float3(uv.xyx) * HASHSCALE1);
    p3 += dot(p3, p3.yzx + 19.19);
    return frac((p3.x + p3.y) * p3.z);
}
//
float Hash3D( float3 p3 )
{
    const float HASHSCALE1 = 0.1031;    // use 443.8975 for [0, 1] inputs
	p3  = frac( p3 * HASHSCALE1 );
    p3 += dot( p3, p3.yzx + 19.19 );
    return frac( (p3.x + p3.y) * p3. z);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// From DXT5_NM standard
float3 UnpackNormalDXT5_NM( float4 packedNormal )
{
    float3 normal;
    normal.xy = packedNormal.wy * 2.0 - 1.0;
    normal.z = sqrt( 1.0 - normal.x*normal.x - normal.y * normal.y );
    return normal;
}

float3 DisplayNormal( float3 normal )
{
    return normal * 0.5 + 0.5;
}

float3 DisplayNormalSRGB( float3 normal )
{
    return pow( abs( normal * 0.5 + 0.5 ), 2.2 );
}

// this codepath is disabled - here's one simple idea for future improvements: http://iquilezles.org/www/articles/fog/fog.htm
//float3 FogForwardApply( float3 color, float viewspaceDistance )
//{
//    //return frac( viewspaceDistance / 10.0 );
//    float d = max(0.0, viewspaceDistance - g_Lighting.FogDistanceMin);
//    float fogStrength = exp( - d * g_Lighting.FogDensity ); 
//    return lerp( g_Lighting.FogColor.rgb, color.rgb, fogStrength );
//}

// float3 DebugViewGenericSceneVertexTransformed( in float3 inColor, const in GenericSceneVertexTransformed input )
// {
// //    inColor.x = 1.0;
// 
//     return inColor;
// }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Normals encode/decode
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float3 GBufferEncodeNormal( float3 normal )
{
    float3 encoded = normal * 0.5 + 0.5;

    return encoded;
}

float3 GBufferDecodeNormal( float3 encoded )
{
    float3 normal = encoded * 2.0 - 1.0;
    return normalize( normal );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Space conversions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// normalized device coordinates (SV_Position from PS) to viewspace depth
float NDCToViewDepth( float screenDepth )
{
    float depthHackMul = g_Global.DepthUnpackConsts.x;
    float depthHackAdd = g_Global.DepthUnpackConsts.y;

    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"

    // Set your depthHackMul and depthHackAdd to:
    // depthHackMul = ( cameraClipFar * cameraClipNear) / ( cameraClipFar - cameraClipNear );
    // depthHackAdd = cameraClipFar / ( cameraClipFar - cameraClipNear );

    return depthHackMul / ( depthHackAdd - screenDepth );
}

// from [0, width], [0, height] to [-1, 1], [-1, 1]
float2 NDCToClipSpacePositionXY( float2 SVPos )
{
    return SVPos * float2( g_Global.ViewportPixel2xSize.x, -g_Global.ViewportPixel2xSize.y ) + float2( -1.0f, 1.0f );
}

float3 NDCToViewspacePosition( float2 SVPos, float viewspaceDepth )
{
    return float3( g_Global.CameraTanHalfFOV.xy * viewspaceDepth * NDCToClipSpacePositionXY( SVPos ), viewspaceDepth );
}

float3 ClipSpaceToViewspacePosition( float2 clipPos, float viewspaceDepth )
{
    return float3( g_Global.CameraTanHalfFOV.xy * viewspaceDepth * clipPos, viewspaceDepth );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stuff
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// color -> log luma conversion used for edge detection
float RGBToLumaForEdges( float3 linearRGB )
{
#if 0
    // this matches Miniengine luma path
    float Luma = dot( linearRGB, float3(0.212671, 0.715160, 0.072169) );
    return log2(1 + Luma * 15) / 4;
#else
    // this is what original FXAA (and consequently CMAA2) use by default - these coefficients correspond to Rec. 601 and those should be
    // used on gamma-compressed components (see https://en.wikipedia.org/wiki/Luma_(video)#Rec._601_luma_versus_Rec._709_luma_coefficients), 
    float luma = dot( sqrt( linearRGB.rgb ), float3( 0.299, 0.587, 0.114 ) );  // http://en.wikipedia.org/wiki/CCIR_601
    // using sqrt luma for now but log luma like in miniengine provides a nicer curve on the low-end
    return luma;
#endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Normalmap encode/decode 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// http://aras-p.info/texts/CompactNormalStorage.html

// #define NORMAL_COMPRESSION 2 		// 2 is LAEA
//
// // Spheremap Transform
// #if (NORMAL_COMPRESSION == 1)
// 
// float3 NormalmapEncode(float3 n)    // Spheremap Transform: http://www.crytek.com/sites/default/files/A_bit_more_deferred_-_CryEngine3.ppt
// {
//     //n.xyz = n.xzy;                  // swizzle for y up
// 
//     this needs testing
// 
//     n.z = 1-n.z;                    // positive z towards camera
// 
//     n.rgb = n.rgb * 2 - 1;          // [0, 1] to [-1, 1]
//     n.rgb = normalize( n.rgb );
// 
//     float2 enc = normalize(n.xy) * sqrt( n.z * 0.5 + 0.5 );
//     
//     enc = enc * 0.5 + 0.5;          // [-1, 1] to [0, 1]
//     
//     return float3( enc, 0 );
// }
// 
// float3 NormalmapDecode(float3 enc)
// {
//     enc.rg = enc.rg * 2 - 1;        // [0, 1] to [-1, 1]
// 
//     float3 ret;
//     ret.z = length( enc.xy ) * 2 - 1;
//     ret.xy = normalize( enc.xy ) * sqrt(1-ret.z*ret.z);
// 
//     ret = ret * 0.5 + 0.5;          // [-1, 1] to [0, 1]
// 
//     ret.z = 1 - ret.z;              // positive z towards camera
// 
//     //ret.xzy = ret.xyz;            // swizzle for y up
// 
//     return ret;
// }
//
//#elif (NORMAL_COMPRESSION == 2)

// LAEA - Lambert Azimuthal Equal-Area projection (http://en.wikipedia.org/wiki/Lambert_azimuthal_equal-area_projection)
float2 NormalmapEncodeLAEA(float3 n)
{
    //n.xyz = n.xzy;                  // swizzle for y up

    float f = sqrt(8*n.z+8);
    float2 ret = n.xy / f + 0.5;

    ret = ret * 0.5 + 0.5;          // [-1, 1] to [0, 1]

    return ret;
}
float3 NormalmapDecodeLAEA(float2 enc)
{
    enc.rg = enc.rg * 2 - 1;        // [0, 1] to [-1, 1]

    float2 fenc = enc.xy*4-2;
    float f = dot(fenc,fenc);
    float g = sqrt(1-f/4);
    float3 ret;
    ret.xy = fenc*g;
    ret.z = 1-f/2;

    return ret;
}

//#else   // no compression, just normalize
//
//float2 NormalmapEncode(float3 n)
//{
//    n = n * 0.5 + 0.5;              // [-1, 1] to [0, 1]
//    return n;
//}
//
//float3 NormalmapDecode(float3 enc)
//{
//    n.rgb = n.rgb * 2 - 1;          // [0, 1] to [-1, 1]
//    normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);   // unpack
//    //n.rgb = normalize( n.rgb );
//    return enc;
//}
//
//#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pixel packing helpers
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color conversion functions below come mostly from: https://github.com/apitrace/dxsdk/blob/master/Include/d3dx_dxgiformatconvert.inl
// For additional future formats, refer to https://github.com/GPUOpen-LibrariesAndSDKs/nBodyD3D12/blob/master/MiniEngine/Core/Shaders/PixelPacking.hlsli 
// (and there's an excellent blogpost here: https://bartwronski.com/2017/04/02/small-float-formats-r11g11b10f-precision/)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sRGB <-> linear conversions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float LINEAR_to_SRGB( float val )
{
    if( val < 0.0031308 )
        val *= float( 12.92 );
    else
        val = float( 1.055 ) * pow( abs( val ), float( 1.0 ) / float( 2.4 ) ) - float( 0.055 );
    return val;
}
float3 LINEAR_to_SRGB( float3 val )
{
    return float3( LINEAR_to_SRGB( val.x ), LINEAR_to_SRGB( val.y ), LINEAR_to_SRGB( val.z ) );
}
float SRGB_to_LINEAR( float val )
{
    if( val < 0.04045 )
        val /= float( 12.92 );
    else
        val = pow( abs( val + float( 0.055 ) ) / float( 1.055 ), float( 2.4 ) );
    return val;
}
float3 SRGB_to_LINEAR( float3 val )
{
    return float3( SRGB_to_LINEAR( val.x ), SRGB_to_LINEAR( val.y ), SRGB_to_LINEAR( val.z ) );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// B8G8R8A8_UNORM <-> float4
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 R8G8B8A8_UNORM_to_FLOAT4( uint packedInput )
{
    float4 unpackedOutput;
    unpackedOutput.x = (float)( packedInput & 0x000000ff ) / 255;
    unpackedOutput.y = (float)( ( ( packedInput >> 8 ) & 0x000000ff ) ) / 255;
    unpackedOutput.z = (float)( ( ( packedInput >> 16 ) & 0x000000ff ) ) / 255;
    unpackedOutput.w = (float)( packedInput >> 24 ) / 255;
    return unpackedOutput;
}
uint FLOAT4_to_R8G8B8A8_UNORM( float4 unpackedInput )
{
    return ( ( uint( saturate( unpackedInput.x ) * 255 + 0.5 ) ) |
             ( uint( saturate( unpackedInput.y ) * 255 + 0.5 ) << 8 ) |
             ( uint( saturate( unpackedInput.z ) * 255 + 0.5 ) << 16 ) |
             ( uint( saturate( unpackedInput.w ) * 255 + 0.5 ) << 24 ) );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// R11G11B10_UNORM <-> float3
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float3 R11G11B10_UNORM_to_FLOAT3( uint packedInput )
{
    float3 unpackedOutput;
    unpackedOutput.x = (float)( ( packedInput       ) & 0x000007ff ) / 2047.0;
    unpackedOutput.y = (float)( ( packedInput >> 11 ) & 0x000007ff ) / 2047.0;
    unpackedOutput.z = (float)( ( packedInput >> 22 ) & 0x000003ff ) / 1023.0;
    return unpackedOutput;
}
// 'unpackedInput' is float3 and not float3 on purpose as half float lacks precision for below!
uint FLOAT3_to_R11G11B10_UNORM( float3 unpackedInput )
{
    uint packedOutput;
    packedOutput =( ( uint( saturate( unpackedInput.x ) * 2047 + 0.5 ) ) |
                    ( uint( saturate( unpackedInput.y ) * 2047 + 0.5 ) << 11 ) |
                    ( uint( saturate( unpackedInput.z ) * 1023 + 0.5 ) << 22 ) );
    return packedOutput;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Following R11G11B10 conversions taken from 
// https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/PixelPacking_R11G11B10.hlsli 
// Original license included:
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

// #include "ColorSpaceUtility.hlsli"

// The standard 32-bit HDR color format.  Each float has a 5-bit exponent and no sign bit.
uint Pack_R11G11B10_FLOAT( float3 rgb )
{
    // Clamp upper bound so that it doesn't accidentally round up to INF 
    // Exponent=15, Mantissa=1.11111
    rgb = min(rgb, asfloat(0x477C0000));  
    uint r = ((f32tof16(rgb.x) + 8) >> 4) & 0x000007FF;
    uint g = ((f32tof16(rgb.y) + 8) << 7) & 0x003FF800;
    uint b = ((f32tof16(rgb.z) + 16) << 17) & 0xFFC00000;
    return r | g | b;
}

float3 Unpack_R11G11B10_FLOAT( uint rgb )
{
    float r = f16tof32((rgb << 4 ) & 0x7FF0);
    float g = f16tof32((rgb >> 7 ) & 0x7FF0);
    float b = f16tof32((rgb >> 17) & 0x7FE0);
    return float3(r, g, b);
}

// An improvement to float is to store the mantissa in logarithmic form.  This causes a
// smooth and continuous change in precision rather than having jumps in precision every
// time the exponent increases by whole amounts.
uint Pack_R11G11B10_FLOAT_LOG( float3 rgb )
{
    float3 flat_mantissa = asfloat(asuint(rgb) & 0x7FFFFF | 0x3F800000);
    float3 curved_mantissa = min(log2(flat_mantissa) + 1.0, asfloat(0x3FFFFFFF));
    rgb = asfloat(asuint(rgb) & 0xFF800000 | asuint(curved_mantissa) & 0x7FFFFF);

    uint r = ((f32tof16(rgb.x) + 8) >>  4) & 0x000007FF;
    uint g = ((f32tof16(rgb.y) + 8) <<  7) & 0x003FF800;
    uint b = ((f32tof16(rgb.z) + 16) << 17) & 0xFFC00000;
    return r | g | b;
}

float3 Unpack_R11G11B10_FLOAT_LOG( uint p )
{
    float3 rgb = f16tof32(uint3(p << 4, p >> 7, p >> 17) & uint3(0x7FF0, 0x7FF0, 0x7FE0));
    float3 curved_mantissa = asfloat(asuint(rgb) & 0x7FFFFF | 0x3F800000);
    float3 flat_mantissa = exp2(curved_mantissa - 1.0);
    return asfloat(asuint(rgb) & 0xFF800000 | asuint(flat_mantissa) & 0x7FFFFF);
}

// As an alternative to floating point, we can store the log2 of a value in fixed point notation.
// The 11-bit fields store 5.6 fixed point notation for log2(x) with an exponent bias of 15.  The
// 10-bit field uses 5.5 fixed point.  The disadvantage here is we don't handle underflow.  Instead
// we use the extra two exponent values to extend the range down through two more exponents.
// Range = [2^-16, 2^16)
uint Pack_R11G11B10_FIXED_LOG(float3 rgb)
{
    uint3 p = clamp((log2(rgb) + 16.0) * float3(64, 64, 32) + 0.5, 0.0, float3(2047, 2047, 1023));
    return p.b << 22 | p.g << 11 | p.r;
}

float3 Unpack_R11G11B10_FIXED_LOG(uint p)
{
    return exp2((uint3(p, p >> 11, p >> 21) & uint3(2047, 2047, 2046)) / 64.0 - 16.0);
}

// These next two encodings are great for LDR data.  By knowing that our values are [0.0, 1.0]
// (or [0.0, 2.0), incidentally), we can reduce how many bits we need in the exponent.  We can
// immediately eliminate all postive exponents.  By giving more bits to the mantissa, we can
// improve precision at the expense of range.  The 8E3 format goes one bit further, quadrupling
// mantissa precision but increasing smallest exponent from -14 to -6.  The smallest value of 8E3
// is 2^-14, while the smallest value of 7E4 is 2^-21.  Both are smaller than the smallest 8-bit
// sRGB value, which is close to 2^-12.

// This is like R11G11B10_FLOAT except that it moves one bit from each exponent to each mantissa.
uint Pack_R11G11B10_E4_FLOAT( float3 rgb )
{
    // Clamp to [0.0, 2.0).  The magic number is 1.FFFFF x 2^0.  (We can't represent hex floats in HLSL.)
    // This trick works because clamping your exponent to 0 reduces the number of bits needed by 1.
    rgb = clamp( rgb, 0.0, asfloat(0x3FFFFFFF) );
    uint r = ((f32tof16(rgb.r) + 4) >> 3 ) & 0x000007FF;
    uint g = ((f32tof16(rgb.g) + 4) << 8 ) & 0x003FF800;
    uint b = ((f32tof16(rgb.b) + 8) << 18) & 0xFFC00000;
    return r | g | b;
}

float3 Unpack_R11G11B10_E4_FLOAT( uint rgb )
{
    float r = f16tof32((rgb << 3 ) & 0x3FF8);
    float g = f16tof32((rgb >> 8 ) & 0x3FF8);
    float b = f16tof32((rgb >> 18) & 0x3FF0);
    return float3(r, g, b);
}

// This is like R11G11B10_FLOAT except that it moves two bits from each exponent to each mantissa.
uint Pack_R11G11B10_E3_FLOAT( float3 rgb )
{
    // Clamp to [0.0, 2.0).  Divide by 256 to bias the exponent by -8.  This shifts it down to use one
    // fewer bit while still taking advantage of the denormalization hardware.  In half precision,
    // the exponent of 0 is 0xF.  Dividing by 256 makes the max exponent 0x7--one fewer bit.
    rgb = clamp( rgb, 0.0, asfloat(0x3FFFFFFF) ) / 256.0;
    uint r = ((f32tof16(rgb.r) + 2) >> 2 ) & 0x000007FF;
    uint g = ((f32tof16(rgb.g) + 2) << 9 ) & 0x003FF800;
    uint b = ((f32tof16(rgb.b) + 4) << 19) & 0xFFC00000;
    return r | g | b;
}

float3 Unpack_R11G11B10_E3_FLOAT( uint rgb )
{
    float r = f16tof32((rgb << 2 ) & 0x1FFC);
    float g = f16tof32((rgb >> 9 ) & 0x1FFC);
    float b = f16tof32((rgb >> 19) & 0x1FF8);
    return float3(r, g, b) * 256.0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RGB <-> HSV/HSL/HCY/HCL
// Code borrowed from here http://www.chilliant.com/rgb2hsv.html
// (c) by Chilli Ant aka IAN TAYLOR
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Converting pure hue to RGB
float3 HUEtoRGB( in float H )
{
    float R = abs( H * 6 - 3 ) - 1;
    float G = 2 - abs( H * 6 - 2 );
    float B = 2 - abs( H * 6 - 4 );
    return saturate( float3( R, G, B ) );
}
//
// Converting RGB to hue/chroma/value
static const float ccEpsilon = 1e-10;
float3 RGBtoHCV( in float3 RGB )
{
    // Based on work by Sam Hocevar and Emil Persson
    float4 P = ( RGB.g < RGB.b ) ? float4( RGB.bg, -1.0, 2.0 / 3.0 ) : float4( RGB.gb, 0.0, -1.0 / 3.0 );
    float4 Q = ( RGB.r < P.x ) ? float4( P.xyw, RGB.r ) : float4( RGB.r, P.yzx );
    float C = Q.x - min( Q.w, Q.y );
    float H = abs( ( Q.w - Q.y ) / ( 6 * C + ccEpsilon ) + Q.z );
    return float3( H, C, Q.x );
}
//
// Converting HSV to RGB
float3 HSVtoRGB( in float3 HSV )
{
    float3 RGB = HUEtoRGB( HSV.x );
    return ( ( RGB - 1 ) * HSV.y + 1 ) * HSV.z;
}
//
// Converting HSL to RGB
float3 HSLtoRGB( in float3 HSL )
{
    float3 RGB = HUEtoRGB( HSL.x );
    float C = ( 1 - abs( 2 * HSL.z - 1 ) ) * HSL.y;
    return ( RGB - 0.5 ) * C + HSL.z;
}
//
// Converting HCY to RGB
static const float3 ccHCYwts = float3( 0.299, 0.587, 0.114 ); // The weights of RGB contributions to luminance.
float3 HCYtoRGB( in float3 HCY )
{
    float3 RGB = HUEtoRGB( HCY.x );
    float Z = dot( RGB, ccHCYwts );
    if( HCY.z < Z )
    {
        HCY.y *= HCY.z / Z;
    }
    else if( Z < 1 )
    {
        HCY.y *= ( 1 - HCY.z ) / ( 1 - Z );
    }
    return ( RGB - Z ) * HCY.y + HCY.z;
}
//
// Converting HCL to RGB
static const float ccHCLgamma = 3;
static const float ccHCLy0 = 100;
static const float ccHCLmaxL = 0.530454533953517; // == exp(ccHCLgamma / ccHCLy0) - 0.5
static const float ccPI = 3.1415926536;
float3 HCLtoRGB( in float3 HCL )
{
    float3 RGB = 0;
    if( HCL.z != 0 )
    {
        float H = HCL.x;
        float C = HCL.y;
        float L = HCL.z * ccHCLmaxL;
        float Q = exp( ( 1 - C / ( 2 * L ) ) * ( ccHCLgamma / ccHCLy0 ) );
        float U = ( 2 * L - C ) / ( 2 * Q - 1 );
        float V = C / Q;
        float T = tan( ( H + min( frac( 2 * H ) / 4, frac( -2 * H ) / 8 ) ) * ccPI * 2 );
        H *= 6;
        if( H <= 1 )
        {
            RGB.r = 1;
            RGB.g = T / ( 1 + T );
        }
        else if( H <= 2 )
        {
            RGB.r = ( 1 + T ) / T;
            RGB.g = 1;
        }
        else if( H <= 3 )
        {
            RGB.g = 1;
            RGB.b = 1 + T;
        }
        else if( H <= 4 )
        {
            RGB.g = 1 / ( 1 + T );
            RGB.b = 1;
        }
        else if( H <= 5 )
        {
            RGB.r = -1 / T;
            RGB.b = 1;
        }
        else
        {
            RGB.r = 1;
            RGB.b = -T;
        }
        RGB = RGB * V + U;
    }
    return RGB;
}
//
// Converting RGB to HSV
float3 RGBtoHSV( in float3 RGB )
{
    float3 HCV = RGBtoHCV( RGB );
    float S = HCV.y / ( HCV.z + ccEpsilon );
    return float3( HCV.x, S, HCV.z );
}
//
// Converting RGB to HSL
float3 RGBtoHSL( in float3 RGB )
{
    float3 HCV = RGBtoHCV( RGB );
    float L = HCV.z - HCV.y * 0.5;
    float S = HCV.y / ( 1 - abs( L * 2 - 1 ) + ccEpsilon );
    return float3( HCV.x, S, L );
}
//
// Converting RGB to HCY
float3 RGBtoHCY( in float3 RGB )
{
    float3 HCV = RGBtoHCV( RGB );
    float Y = dot( RGB, ccHCYwts );
    if( HCV.y != 0 )
    {
        float Z = dot( HUEtoRGB( HCV.x ), ccHCYwts );
        if( Y > Z )
        {
            Y = 1 - Y;
            Z = 1 - Z;
        }
        HCV.y *= Z / Y;
    }
    return float3( HCV.x, HCV.y, Y );
}
//
// Converting RGB to HCL
float3 RGBtoHCL( in float3 RGB )
{
    float3 HCL;
    float H = 0;
    float U = min( RGB.r, min( RGB.g, RGB.b ) );
    float V = max( RGB.r, max( RGB.g, RGB.b ) );
    float Q = ccHCLgamma / ccHCLy0;
    HCL.y = V - U;
    if( HCL.y != 0 )
    {
        H = atan2( RGB.g - RGB.b, RGB.r - RGB.g ) / ccPI;
        Q *= U / V;
    }
    Q = exp( Q );
    HCL.x = frac( H / 2 - min( frac( H ), frac( -H ) ) / 6 );
    HCL.y *= Q;
    HCL.z = lerp( -U, V, Q ) / ( ccHCLmaxL * 2 );
    return HCL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // VASHARED_HLSL_INCLUDED