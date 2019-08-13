/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VertexAsylum Codebase, Copyright (c) Filip Strugar.
// Contents of this file are distributed under MIT license (https://en.wikipedia.org/wiki/MIT_License)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __VA_SKYBOX_HLSL__
#define __VA_SKYBOX_HLSL__

#include "vaSharedTypes.h"

#define SIMPLESKY_CONSTANTSBUFFERSLOT                      4
#define SKYBOX_TEXTURE_SLOT0                                0

#ifdef __cplusplus
namespace VertexAsylum
{
#endif

struct SkyboxConstants
{
    vaMatrix4x4         ProjToWorld;
    vaMatrix4x4         CubemapRotate;
    vaVector4           ColorMul;
};

#ifdef __cplusplus
}
#endif

// The rest below is shader only code
#ifndef __cplusplus

#include "vaShared.hlsl"

cbuffer SkyboxConstantsBuffer                    : register( B_CONCATENATER( SIMPLESKY_CONSTANTSBUFFERSLOT ) )
{
    SkyboxConstants                      g_SkyboxGlobal;
}

TextureCube           g_Cubemap           : register( T_CONCATENATER( SKYBOX_TEXTURE_SLOT0 ) );

struct VSOutput
{
   float4 Position      : SV_Position;
   float4 PositionProj  : TEXCOORD0;
};


VSOutput SkyboxVS( const float4 inPos : SV_Position )
{
   VSOutput ret;

   ret.Position = inPos;
   ret.PositionProj = inPos; //-mul( ProjToWorld, xOut.m_xPosition ).xyz;	

   return ret;
}

float4 SkyboxPS( const VSOutput input ) : SV_Target
{
   float4 skyDirection = mul( g_SkyboxGlobal.ProjToWorld, input.PositionProj ).xyzw;
   skyDirection *= 1.0 / skyDirection.wwww;
   skyDirection.xyz = normalize( skyDirection.xyz );

   // yes, this could be premultiplied on the C++ side but leave it like this for a while
   skyDirection.xyz = normalize( mul( (float3x3)g_SkyboxGlobal.CubemapRotate, skyDirection.xyz ) );

   float3 cubeCol = g_Cubemap.Sample( g_samplerLinearWrap, skyDirection.xyz ).xyz;
   
   //return float4( frac( skyDirection.xyz ), 1 );
   return float4( cubeCol.xyz * g_SkyboxGlobal.ColorMul.xyz, g_SkyboxGlobal.ColorMul.w );
}

#endif // #ifndef __cplusplus

#endif // #ifndef __VA_SKYBOX_HLSL__