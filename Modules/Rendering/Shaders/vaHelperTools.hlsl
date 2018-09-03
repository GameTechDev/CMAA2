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

#include "vaShared.hlsl"

#include "vaSharedTypes_HelperTools.h"

cbuffer ImageCompareToolConstantsBuffer         : register( B_CONCATENATER( IMAGE_COMPARE_TOOL_BUFFERSLOT ) )
{
    ImageCompareToolShaderConstants              g_ImageCompareToolConstants;
}

Texture2D           g_ReferenceImage            : register( T_CONCATENATER( IMAGE_COMPARE_TOOL_TEXTURE_SLOT0 ) );
Texture2D           g_CurrentImage              : register( T_CONCATENATER( IMAGE_COMPARE_TOOL_TEXTURE_SLOT1 ) );

float4 ImageCompareToolVisualizationPS( const float4 svPos : SV_Position ) : SV_Target
{
    int mode = g_ImageCompareToolConstants.VisType;

    float4 referenceCol = g_ReferenceImage.Load( int3( svPos.xy, 0 ) );
    float4 currentCol = g_CurrentImage.Load( int3( svPos.xy, 0 ) );

    if( mode == 1 )
    {
        return referenceCol;
    }
    else
    {
        float mult = pow( 10, max( 0, (float)mode-2.0 ) );

        float4 retVal = abs( referenceCol - currentCol ) * mult;
        return retVal;
    }
}

cbuffer UITextureDrawShaderConstantsBuffer         : register( B_CONCATENATER( TEXTURE_UI_DRAW_TOOL_BUFFERSLOT ) )
{
    UITextureDrawShaderConstants              g_UITextureDrawShaderConstants;
}

Texture2D           g_UIDrawTexture2D           : register( T_CONCATENATER( TEXTURE_UI_DRAW_TOOL_TEXTURE_SLOT0 ) );
Texture2DArray      g_UIDrawTexture2DArray      : register( T_CONCATENATER( TEXTURE_UI_DRAW_TOOL_TEXTURE_SLOT0 ) );
TextureCube         g_UIDrawTextureCube         : register( T_CONCATENATER( TEXTURE_UI_DRAW_TOOL_TEXTURE_SLOT0 ) );

float4 ConvertToUIColor( float4 value, int contentsType, float alpha )
{
    switch( contentsType )
    {
        case( 7 ) : // linear depth
        {
            //value.x += +0.17;
            return float4( frac(value.x * 2.0), frac(value.x/10.0), value.x/100.0, alpha );
        }
        break;
        default:
            return float4( value.rgb, alpha );
    }
}

float4 UIDrawTexture2DPS( const float4 svPos : SV_Position ) : SV_Target
{
    float2 clip = (svPos.xy - g_UITextureDrawShaderConstants.ClipRect.xy)/g_UITextureDrawShaderConstants.ClipRect.zw;
    if( clip.x < 0 || clip.y < 0 || clip.x > 1 || clip.y > 1 )
        discard;

    const float4 dstRect = g_UITextureDrawShaderConstants.DestinationRect;
    float2 uv = (svPos.xy - dstRect.xy) / dstRect.zw;
    if( uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1 )
        discard;

    float4 value = g_UIDrawTexture2D.Sample( g_samplerLinearClamp, uv );

    return ConvertToUIColor( value, g_UITextureDrawShaderConstants.ContentsType, g_UITextureDrawShaderConstants.Alpha );
}

float4 UIDrawTexture2DArrayPS( const float4 svPos : SV_Position ) : SV_Target
{
    float2 clip = (svPos.xy - g_UITextureDrawShaderConstants.ClipRect.xy)/g_UITextureDrawShaderConstants.ClipRect.zw;
    if( clip.x < 0 || clip.y < 0 || clip.x > 1 || clip.y > 1 )
        discard;

    const float4 dstRect = g_UITextureDrawShaderConstants.DestinationRect;
    float2 uv = (svPos.xy - dstRect.xy) / dstRect.zw;
    if( uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1 )
        discard;

    float4 value = g_UIDrawTexture2DArray.Sample( g_samplerLinearClamp, float3( uv, g_UITextureDrawShaderConstants.TextureArrayIndex ) );
    
    return ConvertToUIColor( value, g_UITextureDrawShaderConstants.ContentsType, g_UITextureDrawShaderConstants.Alpha );
}

float4 UIDrawTextureCubePS( const float4 svPos : SV_Position ) : SV_Target
{
    float2 clip = (svPos.xy - g_UITextureDrawShaderConstants.ClipRect.xy)/g_UITextureDrawShaderConstants.ClipRect.zw;
    if( clip.x < 0 || clip.y < 0 || clip.x > 1 || clip.y > 1 )
        discard;

    const float4 dstRect = g_UITextureDrawShaderConstants.DestinationRect;
    float2 uv = (svPos.xy - dstRect.xy) / dstRect.zw;
    if( uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1 )
        discard;

    // not correct
    float3 norm = normalize( float3( uv * 2.0 - 1.0, 1 ) );

    float4 value = g_UIDrawTextureCube.Sample( g_samplerLinearClamp, norm );
    
    // debug
    value.xyz = norm.rgb;

    return ConvertToUIColor( value, g_UITextureDrawShaderConstants.ContentsType, g_UITextureDrawShaderConstants.Alpha );
}

#ifdef VA_UPDATE_3D_CURSOR_SPECIFIC
Texture2D<float>    g_Update3DCursorDepthSource                 : register( t0 );
Texture2DMS<float>  g_Update3DCursorDepthSourceMS               : register( t1 );
RWTexture1D<float4> g_Update3DCursorDestination                 : register( u0 );

[numthreads(1, 1, 1)]
void CursorCaptureCS( uint3 groupID : SV_GroupID )
{
    float depth = 0;

    // activated once on Dispatch( 2, 1, 1 ) - no MSAA
    if( groupID.x == 1 )        
    {
        depth = g_Update3DCursorDepthSource.Load( int3( g_Global.CursorScreenPosition, 0 ) );
    }
    // activated once on Dispatch( 1, 2, 1 ) - MSAA
    else if( groupID.y == 1 )
    {
        int width, height, samples;
        g_Update3DCursorDepthSourceMS.GetDimensions( width, height, samples );

        // get max depth from all MSAA samples
        depth = 0;
        for( int i = 0; i < samples; i++ )
            depth = max( depth, g_Update3DCursorDepthSourceMS.Load( int2( g_Global.CursorScreenPosition ), i ) );
    }

    if( groupID.x == 1 || groupID.y == 1 )
    {
        float4 pos;
        pos.xy = NDCToClipSpacePositionXY( g_Global.CursorScreenPosition.xy );
        pos.z = depth;
        pos.w = 1.0;

        pos = mul( g_Global.ViewProjInv, pos );
        pos /= pos.w;
    
        g_Update3DCursorDestination[ 0 ] = float4( pos.xyz, depth );
    }
}
#endif

#ifdef VA_RENDERING_GLOBALS_DEBUG_DRAW_SPECIFIC

Texture2D<float>    g_DepthSource               : register( t0 );

float4 DebugDrawDepthPS( in float4 inPos : SV_Position ) : SV_Target0
{
    int2 screenPos = (int2)inPos.xy;

    float viewspaceDepth = NDCToViewDepth( g_DepthSource.Load( int3( screenPos, 0 ) ) );
    
    return float4( frac(viewspaceDepth / 800.0), frac(viewspaceDepth / 40.0), frac(viewspaceDepth / 2.0), 1.0 );
}

// Inputs are viewspace depth
float4 ComputeEdgesFromDepth( const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ )
{
    // slope-sensitive depth-based edge detection
    float4 edgesLRTB = float4( leftZ, rightZ, topZ, bottomZ ) - centerZ;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min( abs( edgesLRTB ), abs( edgesLRTBSlopeAdjusted ) );
    return saturate( ( 1.3 - edgesLRTB / (centerZ * 0.040) ) );
}
// Compute center pixel's viewspace normal from neighbours
float3 ComputeNormal( const float4 edgesLRTB, float3 pixCenterPos, float3 pixLPos, float3 pixRPos, float3 pixTPos, float3 pixBPos )
{
    float4 acceptedNormals  = float4( edgesLRTB.x*edgesLRTB.z, edgesLRTB.z*edgesLRTB.y, edgesLRTB.y*edgesLRTB.w, edgesLRTB.w*edgesLRTB.x );

    pixLPos = normalize(pixLPos - pixCenterPos);
    pixRPos = normalize(pixRPos - pixCenterPos);
    pixTPos = normalize(pixTPos - pixCenterPos);
    pixBPos = normalize(pixBPos - pixCenterPos);

    float3 pixelNormal = float3( 0, 0, -0.0005 );
    pixelNormal += ( acceptedNormals.x ) * cross( pixLPos, pixTPos );
    pixelNormal += ( acceptedNormals.y ) * cross( pixTPos, pixRPos );
    pixelNormal += ( acceptedNormals.z ) * cross( pixRPos, pixBPos );
    pixelNormal += ( acceptedNormals.w ) * cross( pixBPos, pixLPos );
    pixelNormal = normalize( pixelNormal );
    
    return pixelNormal;
}
float4 DebugDrawNormalsFromDepthPS( in float4 inPos : SV_Position ) : SV_Target0
{
    float viewspaceDepthC = NDCToViewDepth( g_DepthSource.Load( int3( inPos.xy, 0 ) ) );
    float viewspaceDepthL = NDCToViewDepth( g_DepthSource.Load( int3( inPos.xy, 0 ), int2( -1, 0 ) ) );
    float viewspaceDepthR = NDCToViewDepth( g_DepthSource.Load( int3( inPos.xy, 0 ), int2( 1,  0 ) ) );
    float viewspaceDepthT = NDCToViewDepth( g_DepthSource.Load( int3( inPos.xy, 0 ), int2( 0, -1 ) ) );
    float viewspaceDepthB = NDCToViewDepth( g_DepthSource.Load( int3( inPos.xy, 0 ), int2( 0,  1 ) ) );

    float4 edgesLRTB = ComputeEdgesFromDepth( viewspaceDepthC, viewspaceDepthL, viewspaceDepthR, viewspaceDepthT, viewspaceDepthB );

    float3 viewspacePosC  = NDCToViewspacePosition( inPos.xy + float2(  0.0,  0.0 ), viewspaceDepthC );
    float3 viewspacePosL  = NDCToViewspacePosition( inPos.xy + float2( -1.0,  0.0 ), viewspaceDepthL );
    float3 viewspacePosR  = NDCToViewspacePosition( inPos.xy + float2(  1.0,  0.0 ), viewspaceDepthR );
    float3 viewspacePosT  = NDCToViewspacePosition( inPos.xy + float2(  0.0, -1.0 ), viewspaceDepthT );
    float3 viewspacePosB  = NDCToViewspacePosition( inPos.xy + float2(  0.0,  1.0 ), viewspaceDepthB );

    float3 normal = ComputeNormal( edgesLRTB, viewspacePosC, viewspacePosL, viewspacePosR, viewspacePosT, viewspacePosB );

    return float4( DisplayNormalSRGB( normal ), 1.0 );
}

#endif

#ifdef VA_ZOOM_TOOL_SPECIFIC

cbuffer ZoomToolConstantsBuffer                      : register( B_CONCATENATER( ZOOMTOOL_CONSTANTS_BUFFERSLOT ) )
{
    ZoomToolShaderConstants         g_zoomToolConstants;
}

#ifdef VA_ZOOM_TOOL_USE_UNORM_FLOAT
RWTexture2D<unorm float4>           g_screenTexture             : register( u0 );
#else
RWTexture2D<float4>                 g_screenTexture             : register( u0 );
#endif

bool IsInRect( float2 pt, float4 rect )
{
    return ( (pt.x >= rect.x) && (pt.x <= rect.z) && (pt.y >= rect.y) && (pt.y <= rect.w) );
}

void DistToClosestRectEdge( float2 pt, float4 rect, out float dist, out int edge )
{
    edge = 0;
    dist = 1e20;

    float distTmp;
    distTmp = abs( pt.x - rect.x );
    if( distTmp <= dist ) { dist = distTmp; edge = 2; }  // left

    distTmp = abs( pt.y - rect.y );
    if( distTmp <= dist ) { dist = distTmp; edge = 3; }  // top

    distTmp = abs( pt.x - rect.z ); 
    if( distTmp <= dist ) { dist = distTmp; edge = 0; }  // right

    distTmp = abs( pt.y - rect.w );
    if( distTmp <= dist ) { dist = distTmp; edge = 1; }  // bottom
}

float2 RectToRect( float2 pt, float2 srcRCentre, float2 srcRSize, float2 dstRCentre, float2 dstRSize )
{
    pt -= srcRCentre;
    pt /= srcRSize;

    pt *= dstRSize;
    pt += dstRCentre;
    
    return pt;
}

[numthreads(16, 16, 1)]
void ZoomToolCS( uint2 dispatchThreadID : SV_DispatchThreadID )
{
    const float2 screenPos = float2( dispatchThreadID ) + float2( 0.5, 0.5 ); // same as SV_Position

    const float zoomFactor = g_zoomToolConstants.ZoomFactor;

    float4 srcRect  = g_zoomToolConstants.SourceRectangle;
    float4 srcColor = g_screenTexture[dispatchThreadID];

    uint2 screenSizeUI;
    g_screenTexture.GetDimensions( screenSizeUI.x, screenSizeUI.y );

    const float2 screenSize     = float2(screenSizeUI);
    const float2 screenCenter   = float2(screenSizeUI) * 0.5;

    float2 srcRectSize = float2( srcRect.z - srcRect.x, srcRect.w - srcRect.y );
    float2 srcRectCenter = srcRect.xy + srcRectSize.xy * 0.5;

    float2 displayRectSize = srcRectSize * zoomFactor.xx;
    float2 displayRectCenter; 
    displayRectCenter.x = (srcRectCenter.x > screenCenter.x)?(srcRectCenter.x - srcRectSize.x * 0.5 - displayRectSize.x * 0.5 - 100):(srcRectCenter.x + srcRectSize.x * 0.5 + displayRectSize.x * 0.5 + 100);
    
    //displayRectCenter.y = (srcRectCenter.y > screenCenter.y)?(srcRectCenter.y - srcRectSize.y * 0.5 - displayRectSize.y * 0.5 - 50):(srcRectCenter.y + srcRectSize.y * 0.5 + displayRectSize.y * 0.5 + 50);
    displayRectCenter.y = lerp( displayRectSize.y/2, screenSize.y - displayRectSize.y/2, srcRectCenter.y / screenSize.y );
    
    float4 displayRect = float4( displayRectCenter.xy - displayRectSize.xy * 0.5, displayRectCenter.xy + displayRectSize.xy * 0.5 );

    bool chessPattern = (((uint)screenPos.x + (uint)screenPos.y) % 2) == 0;

    if( IsInRect(screenPos.xy, displayRect ) )
    {
        {
            // draw destination box frame
            float dist; int edge;
            DistToClosestRectEdge( screenPos.xy, displayRect, dist, edge );

            if( dist < 1.1 )
            {
                g_screenTexture[dispatchThreadID] = float4( 1.0, 0.8, 0.8, dist < 1.1 );
                return;
            }
        }

        float2 texCoord = RectToRect( screenPos.xy, displayRectCenter, displayRectSize, srcRectCenter, srcRectSize );
        float3 colour = g_screenTexture.Load( int2( texCoord ) ).rgb;
        
        g_screenTexture[dispatchThreadID] = float4( colour, 1 );
        return;
    }

    srcRect.xy -= 1.0;
    srcRect.zw += 1.0;
    if( IsInRect(screenPos.xy, srcRect ) )
    {
        // draw source box frame
        float dist; int edge;
        DistToClosestRectEdge( screenPos.xy, srcRect, dist, edge );

        if( dist < 1.1 )
        {
            g_screenTexture[dispatchThreadID] = float4( 0.8, 1, 0.8, 1 ); // lerp( srcColor, float4( 0.8, 1, 0.8, 1 ) );
            return;
        }
    }

}
#endif
