///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
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

#include "Rendering/vaRenderGlobals.h"

#include "Rendering/vaRenderDeviceContext.h"

#include "Rendering/vaTexture.h"

#include "IntegratedExternals/vaImguiIntegration.h"


using namespace VertexAsylum;

vaRenderGlobals::vaRenderGlobals( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ),
    vaUIPanel( "RenderGlobals", 0, false, vaUIPanel::DockLocation::DockedLeftBottom ),
    m_constantsBuffer( params ),
    m_cursorCaptureCS( params ),
    m_cursorCaptureMSCS( params )
{ 
//    assert( vaRenderingCore::IsInitialized() );


    for( int i = 0; i < c_shaderDebugOutputSyncDelay; i++ )
    {
        m_shaderDebugOutputGPUTextures[i] = vaTexture::Create1D( GetRenderDevice(), vaResourceFormat::R32_FLOAT, c_shaderDebugFloatOutputCount, 1, 1, vaResourceBindSupportFlags::UnorderedAccess, vaResourceAccessFlags::Default );
        m_shaderDebugOutputCPUTextures[i] = vaTexture::Create1D( GetRenderDevice(), vaResourceFormat::R32_FLOAT, c_shaderDebugFloatOutputCount, 1, 1, vaResourceBindSupportFlags::None, vaResourceAccessFlags::CPURead );
    }

    for( int i = 0; i < _countof(m_cursorCaptureGPUTextures); i++ )
    {
        m_cursorCaptureGPUTextures[i] = vaTexture::Create1D( GetRenderDevice(), vaResourceFormat::R32G32B32A32_FLOAT, 1, 1, 1, vaResourceBindSupportFlags::UnorderedAccess );
        m_cursorCaptureCPUTextures[i] = vaTexture::Create1D( GetRenderDevice(), vaResourceFormat::R32G32B32A32_FLOAT, 1, 1, 1, vaResourceBindSupportFlags::None, vaResourceAccessFlags::CPURead | vaResourceAccessFlags::CPUReadManuallySynced );
        m_cursorCaptureCPUTexturesHasData[i] = false;
    }

    m_cursorCapture = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f );

    for( int i = 0; i < c_shaderDebugFloatOutputCount; i++ )
    {
        m_shaderDebugFloats[i] = 0.0f;
    }

    m_cursorCaptureLastFrame = -1;

    m_cursorLastScreenPosition = vaVector2( 0, 0 );

    m_debugDrawDepth = false;
    m_debugDrawNormalsFromDepth = false;

    m_cursorCaptureCS->CreateShaderFromFile( L"vaHelperTools.hlsl", "cs_5_0", "CursorCaptureCS", { pair< string, string >( "VA_UPDATE_3D_CURSOR_SPECIFIC", "" ) }, false );
    m_cursorCaptureMSCS->CreateShaderFromFile( L"vaHelperTools.hlsl", "cs_5_0", "CursorCaptureMSCS", { pair< string, string >( "VA_UPDATE_3D_CURSOR_SPECIFIC", "" ) }, false );
}

vaRenderGlobals::~vaRenderGlobals( )
{
}

void vaRenderGlobals::UpdateShaderConstants( vaSceneDrawContext & drawContext )
{
    ShaderGlobalConstants consts;
    memset( &consts, 0, sizeof(consts) );

    const vaViewport & viewport = drawContext.RenderDeviceContext.GetViewport();

    //consts.View                             = drawContext.Camera.GetViewMatrix( );
    consts.ViewInv                          = drawContext.Camera.GetInvViewMatrix( );
    consts.Proj                             = drawContext.Camera.GetProjMatrix( );

    if( (drawContext.RenderFlags & vaDrawContextFlags::SetZOffsettedProjMatrix) != 0 )
        drawContext.Camera.GetZOffsettedProjMatrix( consts.Proj, 1.0001f, 0.0001f );

    consts.ViewProj                         = drawContext.Camera.GetViewMatrix( ) * consts.Proj;
    //consts.ProjInv                          = consts.Proj.Inverse();
    consts.ViewProjInv                      = consts.ViewProj.Inverse();

    //consts.ViewDirection                    = vaVector4( drawContext.Camera.GetDirection(), 0.0f );
    //consts.CameraWorldPosition              = vaVector4( drawContext.Camera.GetPosition(), 0.0f );
    consts.CameraSubpixelOffset             = vaVector4( drawContext.Camera.GetSubpixelOffset(), 0.0f, 0.0f );

    {
        consts.ViewportSize                 = vaVector2( (float)viewport.Width, (float)viewport.Height );
        consts.ViewportPixelSize            = vaVector2( 1.0f / (float)viewport.Width, 1.0f / (float)viewport.Height );
        consts.ViewportHalfSize             = vaVector2( (float)viewport.Width*0.5f, (float)viewport.Height*0.5f );
        consts.ViewportPixel2xSize          = vaVector2( 2.0f / (float)viewport.Width, 2.0f / (float)viewport.Height );

        float depthLinearizeMul             = -consts.Proj.m[3][2];         // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
        float depthLinearizeAdd             = consts.Proj.m[2][2];          // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
        // correct the handedness issue. need to make sure this below is correct, but I think it is.
        if( depthLinearizeMul * depthLinearizeAdd < 0 )
            depthLinearizeAdd = -depthLinearizeAdd;
        consts.DepthUnpackConsts            = vaVector2( depthLinearizeMul, depthLinearizeAdd );

        float tanHalfFOVY                   = 1.0f / consts.Proj.m[1][1];    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
        float tanHalfFOVX                   = 1.0F / consts.Proj.m[0][0];    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
        consts.CameraTanHalfFOV     = vaVector2( tanHalfFOVX, tanHalfFOVY );

        float clipNear  = drawContext.Camera.GetNearPlane();
        float clipFar   = drawContext.Camera.GetFarPlane();
        consts.CameraNearFar        = vaVector2( clipNear, clipFar );

        consts.CursorScreenPosition = m_cursorLastScreenPosition;
    }

    consts.TransparencyPass                 = 0.0f; //( ( flags & vaDrawContextFlags::TransparencyPass) != 0 )?( 1.0f ) : ( 0.0f );
    consts.WireframePass                    = ( ( drawContext.RenderFlags & vaDrawContextFlags::DebugWireframePass) != 0 )?( 1.0f ) : ( 0.0f );
    
    if( (drawContext.RenderFlags & vaDrawContextFlags::CustomShadowmapShaderLinearDistanceToCenter) != 0 )
    {
        assert( drawContext.OutputType == vaDrawContextOutputType::CustomShadow );
        consts.CustomShadowmapShaderType = 1;
    }
    else
    {
        assert( drawContext.OutputType != vaDrawContextOutputType::CustomShadow );
        consts.CustomShadowmapShaderType = 0;
    }
    consts.GlobalMIPOffset                  = drawContext.GlobalMIPOffset;
    consts.GlobalPixelScale                 = drawContext.GlobalPixelScale;
    
    consts.Dummy0 = 0;
    consts.Dummy1 = 0;

    double totalTime = GetRenderDevice().GetTotalTime();
    consts.TimeFract                        = (float)fmod( totalTime, 1.0 );
    consts.TimeFmod3600                     = (float)fmod( totalTime, 360.0 );
    consts.SinTime2Pi                       = (float)sin( totalTime * 2.0 * VA_PI );
    consts.SinTime1Pi                       = (float)sin( totalTime * VA_PI );
    //consts.ProjToWorld = viewProj.Inverse( );

    consts.ViewspaceDepthOffsetFlatAdd      = drawContext.ViewspaceDepthOffsets.x;
    consts.ViewspaceDepthOffsetFlatMul      = drawContext.ViewspaceDepthOffsets.y;
    consts.ViewspaceDepthOffsetSlopeAdd     = drawContext.ViewspaceDepthOffsets.z;
    consts.ViewspaceDepthOffsetSlopeMul     = drawContext.ViewspaceDepthOffsets.w;

    m_constantsBuffer.Update( drawContext.RenderDeviceContext, consts );
}

void vaRenderGlobals::Update3DCursor( vaSceneDrawContext & drawContext, const shared_ptr<vaTexture> & depthSource, const vaVector2 & cursorScreenPosition )
{
    VA_SCOPE_CPUGPU_TIMER( Update3DCursor, drawContext.RenderDeviceContext );

    m_cursorLastScreenPosition = cursorScreenPosition;

    // 1.) get data from first ready CPU resource
    int oldestWriteIndex = GetRenderDevice().GetCurrentFrameIndex() % _countof(m_cursorCaptureGPUTextures);
    const shared_ptr<vaTexture> & readTex = m_cursorCaptureCPUTextures[oldestWriteIndex];
    if( m_cursorCaptureCPUTexturesHasData[oldestWriteIndex] )
    {
        if( readTex->TryMap( drawContext.RenderDeviceContext, vaResourceMapType::Read, false ) )
        {
            memcpy( &m_cursorCapture, readTex->GetMappedData()[0].Buffer, sizeof(m_cursorCapture) );

            readTex->Unmap( drawContext.RenderDeviceContext );
        }
        else
        {
            VA_LOG_ERROR( "Couldn't read 3d cursor buffer info!" );
        }
    }

    // Got to back up as depth might be selected - there's overhead but this happens just once per frame so 'meh'
    vaRenderDeviceContext::RenderOutputsState rtState = drawContext.RenderDeviceContext.GetOutputs( );
    drawContext.RenderDeviceContext.SetRenderTarget( nullptr, nullptr, false );

    assert( m_cursorCaptureLastFrame != GetRenderDevice().GetCurrentFrameIndex() );
    m_cursorCaptureLastFrame = GetRenderDevice().GetCurrentFrameIndex();

    // same as oldest heh!
    int currentWriteIndex = GetRenderDevice().GetCurrentFrameIndex() % _countof(m_cursorCaptureGPUTextures);

    vaComputeItem computeItem;
    computeItem.SceneDrawContext = &drawContext;
    computeItem.UnorderedAccessViews[0] = m_cursorCaptureGPUTextures[currentWriteIndex];
    if( depthSource->GetSampleCount() == 1 )
    {
        m_cursorCaptureCS->WaitFinishIfBackgroundCreateActive();
        computeItem.ComputeShader = m_cursorCaptureCS;
        computeItem.ShaderResourceViews[ 0 ] = depthSource;
        computeItem.SetDispatch( 1, 1, 1 );
    }
    else
    {
        m_cursorCaptureMSCS->WaitFinishIfBackgroundCreateActive();
        computeItem.ComputeShader = m_cursorCaptureMSCS;
        computeItem.ShaderResourceViews[ 1 ] = depthSource;
        computeItem.SetDispatch( 1, 1, 1 );
    }
    
    drawContext.RenderDeviceContext.ExecuteSingleItem( computeItem );
    
    // restore previous RTs
    drawContext.RenderDeviceContext.SetOutputs( rtState );

    // 2.) enqueue resource GPU->CPU copy
    const shared_ptr<vaTexture> & src = m_cursorCaptureGPUTextures[ currentWriteIndex ];
    const shared_ptr<vaTexture> & dst = m_cursorCaptureCPUTextures[ currentWriteIndex ];
    dst->CopyFrom( drawContext.RenderDeviceContext, src );
    m_cursorCaptureCPUTexturesHasData[currentWriteIndex] = true;
    
    // we must work on the main context due to mapping limitations
    assert( &drawContext.RenderDeviceContext == GetRenderDevice().GetMainContext() );
}

void vaRenderGlobals::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::Checkbox( "Debug draw depth", &m_debugDrawDepth );
    if( m_debugDrawDepth )
        m_debugDrawNormalsFromDepth = false;
    
    ImGui::Checkbox( "Debug draw normals from depth", &m_debugDrawNormalsFromDepth );
    if( m_debugDrawNormalsFromDepth )
        m_debugDrawDepth = false;
#endif
}

void vaRenderGlobals::DebugDraw( vaSceneDrawContext & drawContext )
{
    vaRenderDeviceContext & renderContext = drawContext.RenderDeviceContext;

    if( m_debugDrawDepth || m_debugDrawNormalsFromDepth )
    {

        vaRenderDeviceContext::RenderOutputsState outState = renderContext.GetOutputs();

        vaRenderDeviceContext::RenderOutputsState outStateCopy = outState;
        outStateCopy.DepthStencil = nullptr;
        renderContext.SetOutputs( outStateCopy );

        assert( false ); // switch to vaGraphicsItem platform independent path

        //outState.DepthStencil->SetToAPISlotSRV( renderContext, 0 );

        //if( m_debugDrawDepth )
        //{
        // lazy create - update the code
        //    if( m_debugDrawDepthPS == nullptr ) 
        //{ 
        //    m_debugDrawDepthPS = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, renderDevice );
        //    m_debugDrawDepthPS->CreateShaderFromFile( L"vaHelperTools.hlsl", "ps_5_0", "DebugDrawDepthPS", vector< pair< string, string > > ( 1, pair< string, string >( "VA_RENDERING_GLOBALS_DEBUG_DRAW_SPECIFIC", "" ) ) );
        //    renderContext.FullscreenPassDraw( *m_debugDrawDepthPS );
        //}
        //else if( m_debugDrawNormalsFromDepth )
        //{
        //    m_debugDrawNormalsFromDepthPS = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, renderDevice );
        //    m_debugDrawNormalsFromDepthPS->CreateShaderFromFile( L"vaHelperTools.hlsl", "ps_5_0", "DebugDrawNormalsFromDepthPS", vector< pair< string, string > > ( 1, pair< string, string >( "VA_RENDERING_GLOBALS_DEBUG_DRAW_SPECIFIC", "" ) ) );
        //    renderContext.FullscreenPassDraw( *m_debugDrawNormalsFromDepthPS );
        //}

        //outState.DepthStencil->UnsetFromAPISlotSRV( renderContext, 0 );

        renderContext.SetOutputs( outState );
    }
}


