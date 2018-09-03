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

#ifndef VA_SHADER_SHARED_TYPES_HLSL
#define VA_SHADER_SHARED_TYPES_HLSL

#include "vaShaderCore.h"

#include "vaSharedTypes_PostProcess.h"

#ifndef VA_COMPILED_AS_SHADER_CODE
namespace VertexAsylum
{
#endif

#define SHADERGLOBAL_CONSTANTSBUFFERSLOT                10
#define LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT              11
#define LIGHTINGGLOBAL_DIRLIGHTS_CONSTANTSBUFFERSLOT    12
#define LIGHTINGGLOBAL_PTLIGHTS_CONSTANTSBUFFERSLOT     13

#define SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT             10
#define SHADERGLOBAL_POINTWRAP_SAMPLERSLOT              11
#define SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT            12
#define SHADERGLOBAL_LINEARWRAP_SAMPLERSLOT             13
#define SHADERGLOBAL_ANISOTROPICCLAMP_SAMPLERSLOT       14
#define SHADERGLOBAL_ANISOTROPICWRAP_SAMPLERSLOT        15
#define SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT               9


#define SHADERGLOBAL_DEBUG_FLOAT_OUTPUT_COUNT       4096

// TODO: move elsewhere?
#define POSTPROCESS_CONSTANTS_BUFFERSLOT                    1
#define RENDERMESHMATERIAL_CONSTANTS_BUFFERSLOT             1
#define RENDERMESH_CONSTANTS_BUFFERSLOT                     2
#define SIMPLEPARTICLESYSTEM_CONSTANTS_BUFFERSLOT           2
#define SKYBOX_CONSTANTS_BUFFERSLOT                         4
#define ZOOMTOOL_CONSTANTS_BUFFERSLOT                       4
// #define GBUFFER_CONSTANTS_BUFFERSLOT                        5


// TODO: delete?
#define RENDERMESH_TEXTURE_SLOT0                            0
#define RENDERMESH_TEXTURE_SLOT1                            1
#define RENDERMESH_TEXTURE_SLOT2                            2
#define RENDERMESH_TEXTURE_SLOT3                            3
#define RENDERMESH_TEXTURE_SLOT4                            4
#define RENDERMESH_TEXTURE_SLOT5                            5


#define SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT            20
#define SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT       21

struct ShaderLightDirectional
{
    static const int    MaxLights                       = 16;

    vaVector3           Intensity;
    float               IntensityLength;

    vaVector3           Direction;
    float               Dummy1;
};
//struct ShaderLightPoint - no longer used, just using spot instead
//{
//    static const int    MaxLights                       = 16;
//
//    vaVector3           Intensity;
//    float               Size;                           // distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
//    vaVector3           Position;
//    float               EffectiveRadius;                // distance at which it is considered that it cannot effectively contribute any light and can be culled
//};
struct ShaderLightSpot
{
    static const int    MaxLights                       = 32;

    vaVector3           Intensity;
    float               IntensityLength;
    vaVector3           Position;
    float               EffectiveRadius;                // distance at which it is considered that it cannot effectively contribute any light and can be culled
    vaVector3           Direction;
    float               Size;                           // distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
    float               SpotInnerAngle;                 // angle from Direction below which the spot light has the full intensity (a.k.a. inner cone angle)
    float               SpotOuterAngle;                 // angle from Direction below which the spot light intensity starts dropping (a.k.a. outer cone angle)
    
    float               CubeShadowIndex;                // if used, index of cubemap shadow in the cubemap array texture; otherwise -1
    float               Dummy1;
};

struct LightingShaderConstants
{
    static const int        MaxShadowCubes                  = 8;   // so the number of cube faces is x6 this - lots of RAM

    vaVector4               AmbientLightIntensity;

    // See vaFogSphere for descriptions
    vaVector3               FogCenter;
    int                     FogEnabled;

    vaVector3               FogColor;
    float                   FogRadiusInner;

    float                   FogRadiusOuter;
    float                   FogBlendCurvePow;
    float                   FogBlendMultiplier;
    float                   FogRange;                   // FogRadiusOuter - FogRadiusInner

    vaMatrix4x4             EnvmapRotation;             // ideally we shouldn't need this but at the moment we support this to simplify asset side...
    
    int                     EnvmapEnabled;
    float                   EnvmapMultiplier;
    int                     LightCountDirectional;
    int                     LightCountSpotAndPoint;

    int                     LightCountSpotOnly;
    float                   CubeShadowDirJitterOffset;          // 'sideways' offset when doing PCF jitter for cubemap shadows
    float                   CubeShadowViewDistJitterOffset;     // multiplication offset along cube sampling dir - similar to ViewspaceDepthOffsetFlatMul but only identical at the cube face centers!!
    float                   CubeShadowViewDistJitterOffsetSqrt; // same as above but for use with sqrt-packed cube shadowmaps

    ShaderLightDirectional  LightsDirectional[ShaderLightDirectional::MaxLights];
    //ShaderLightPoint        LightsPoint[ShaderLightPoint::MaxLights];
    ShaderLightSpot         LightsSpotAndPoint[ShaderLightSpot::MaxLights];

    //
    // vaVector4               ShadowCubes[MaxShadowCubes];    // .xyz is cube center and .w is unused at the moment
};

struct ShaderGlobalConstants
{
    vaMatrix4x4             View;
    vaMatrix4x4             ViewInv;
    vaMatrix4x4             Proj;
    vaMatrix4x4             ViewProj;
    vaMatrix4x4             ViewProjInv;

    vaVector4               ViewDirection;
    vaVector4               CameraWorldPosition;
    vaVector4               CameraSubpixelOffset;   // .xy contains value of subpixel offset used for supersampling/TAA (otheriwse knows as jitter) or (0,0) if no jitter enabled; .zw are 0

    vaVector2               ViewportSize;           // ViewportSize.x, ViewportSize.y
    vaVector2               ViewportPixelSize;      // 1.0 / ViewportSize.x, 1.0 / ViewportSize.y
    vaVector2               ViewportHalfSize;       // ViewportSize.x * 0.5, ViewportSize.y * 0.5
    vaVector2               ViewportPixel2xSize;    // 2.0 / ViewportSize.x, 2.0 / ViewportSize.y

    vaVector2               DepthUnpackConsts;
    vaVector2               CameraTanHalfFOV;
    vaVector2               CameraNearFar;
    vaVector2               CursorScreenPosition;

    float                   TransparencyPass;
    float                   WireframePass;
    int                     CustomShadowmapShaderType;      // 0 - disabled; 1 - output linear distance to center   (only enabled when drawing into shadow maps)
    int                     Dummy0;

    vaVector2               GlobalPixelScale;
    float                   GlobalMIPOffset;
    int                     Dummy1;

    float                   TimeFract;              // fractional part of total time from starting the app
    float                   TimeFmod3600;           // remainder of total time from starting the app divided by an hour (3600 seconds) - leaves you with at least 2 fractional part decimals of precision
    float                   SinTime2Pi;             // sine of total time from starting the app times 2*PI
    float                   SinTime1Pi;             // sine of total time from starting the app times PI

    // push (or pull) vertices by this amount away (towards) camera; to avoid z fighting for debug wireframe or for the shadow drawing offset
    float                   ViewspaceDepthOffsetFlatAdd;
    float                   ViewspaceDepthOffsetFlatMul;
    float                   ViewspaceDepthOffsetSlopeAdd;
    float                   ViewspaceDepthOffsetSlopeMul;
};

struct ShaderSimpleMeshPerInstanceConstants
{
    vaMatrix4x4         World;
    vaMatrix4x4         WorldView;
    vaMatrix4x4         ShadowWorldViewProj;
};

struct ShaderSimpleParticleSystemConstants
{
    vaMatrix4x4         World;
    vaMatrix4x4         WorldView;
    vaMatrix4x4         Proj;
};

struct SimpleSkyConstants
{
    vaMatrix4x4          ProjToWorld;

    vaVector4            SunDir;

    vaVector4            SkyColorLow;
    vaVector4            SkyColorHigh;

    vaVector4            SunColorPrimary;
    vaVector4            SunColorSecondary;

    float                SkyColorLowPow;
    float                SkyColorLowMul;

    float                SunColorPrimaryPow;
    float                SunColorPrimaryMul;
    float                SunColorSecondaryPow;
    float                SunColorSecondaryMul;

    float               Dummy0;
    float               Dummy1;
};

struct RenderMeshMaterialConstants
{
    vaVector4           ColorMultAlbedo;
    vaVector4           ColorMultSpecular;
};

struct RenderMeshConstants
{
    vaMatrix4x4         World;
    vaMatrix4x4         WorldView;
    vaMatrix4x4         ShadowWorldViewProj;
    
    // since we now support non-uniform scale, we need the 'normal matrix' to keep normals correct 
    // (for more info see : https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals or http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/ )
    vaMatrix4x4         NormalWorld;
    vaMatrix4x4         NormalWorldView;    

    vaVector4           ColorOverride;          // used for highlights, wireframe, etc - finalColor = finalColor * ColorOverride.aaa + ColorOverride.rgb
};

// struct GBufferConstants
// {
//     float               Dummy0;
//     float               Dummy1;
//     float               Dummy2;
//     float               Dummy3;
// };

struct ZoomToolShaderConstants
{
    vaVector4           SourceRectangle;

    int                 ZoomFactor;
    float               Dummy1;
    float               Dummy2;
    float               Dummy3;
};


#ifndef VA_COMPILED_AS_SHADER_CODE
} // namespace VertexAsylum
#endif

#ifdef VA_COMPILED_AS_SHADER_CODE


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global buffers
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer GlobalConstantsBuffer                       : register( B_CONCATENATER( SHADERGLOBAL_CONSTANTSBUFFERSLOT ) )
{
    ShaderGlobalConstants                   g_Global;
}


//cbuffer ShaderSimpleShadowsGlobalConstantsBuffer    : register( B_CONCATENATER( SHADERSIMPLESHADOWSGLOBAL_CONSTANTSBUFFERSLOT ) )
//{
//    ShaderSimpleShadowsGlobalConstants      g_SimpleShadowsGlobal;
//}

cbuffer RenderMeshMaterialConstantsBuffer : register( B_CONCATENATER( RENDERMESHMATERIAL_CONSTANTS_BUFFERSLOT ) )
{
    RenderMeshMaterialConstants             g_RenderMeshMaterialGlobal;
}

cbuffer RenderMeshConstantsBuffer                   : register( B_CONCATENATER( RENDERMESH_CONSTANTS_BUFFERSLOT ) )
{
    RenderMeshConstants                     g_RenderMeshGlobal;
}

cbuffer ShaderSimpleParticleSystemConstantsBuffer   : register( B_CONCATENATER( SIMPLEPARTICLESYSTEM_CONSTANTS_BUFFERSLOT ) )
{
    ShaderSimpleParticleSystemConstants     g_ParticleSystemConstants;
}

// cbuffer GBufferConstantsBuffer                      : register( B_CONCATENATER( GBUFFER_CONSTANTS_BUFFERSLOT ) )
// {
//     GBufferConstants                        g_GBufferConstants;
// }



#endif

#endif // VA_SHADER_SHARED_TYPES_HLSL