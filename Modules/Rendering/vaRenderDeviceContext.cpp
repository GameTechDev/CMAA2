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

#pragma once

#include "vaRenderDeviceContext.h"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Core/System/vaFileTools.h"

#include "Rendering/vaShader.h"

#include "Rendering/vaRenderBuffers.h"

using namespace VertexAsylum;


vaRenderDeviceContext::vaRenderDeviceContext( const vaRenderingModuleParams & params ) : vaRenderingModule( params ), m_outputsState(), 
    m_fsVertexShader( params ), m_fsVertexBufferZ0( params ), m_fsVertexBufferZ1( params )
{ 
    std::vector<vaVertexInputElementDesc> inputElements;
    inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32A32_FLOAT,    0,  0, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    inputElements.push_back( { "TEXCOORD", 0,       vaResourceFormat::R32G32_FLOAT,          0, 16, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

    //ID3DBlob *pVSBlob = NULL;
    const char * pVSString = "void main( inout const float4 xPos : SV_Position, inout float2 UV : TEXCOORD0 ) { }";
    m_fsVertexShader->CreateShaderAndILFromBuffer( pVSString, "vs_5_0", "main", inputElements, vaShaderMacroContaner(), true );

    {
        // using one big triangle
        SimpleVertex fsVertices[3];
        float z = 0.0f;
        fsVertices[0] = SimpleVertex( -1.0f,  1.0f, z, 1.0f, 0.0f, 0.0f );
        fsVertices[1] = SimpleVertex( 3.0f,   1.0f, z, 1.0f, 2.0f, 0.0f );
        fsVertices[2] = SimpleVertex( -1.0f, -3.0f, z, 1.0f, 0.0f, 2.0f );

        m_fsVertexBufferZ0->Create( _countof(fsVertices), sizeof(SimpleVertex), fsVertices, false );
    }

    {
        // using one big triangle
        SimpleVertex fsVertices[3];
        float z = 1.0f;
        fsVertices[0] = SimpleVertex( -1.0f,  1.0f, z, 1.0f, 0.0f, 0.0f );
        fsVertices[1] = SimpleVertex( 3.0f,   1.0f, z, 1.0f, 2.0f, 0.0f );
        fsVertices[2] = SimpleVertex( -1.0f, -3.0f, z, 1.0f, 0.0f, 2.0f );

        m_fsVertexBufferZ1->Create( _countof(fsVertices), sizeof(SimpleVertex), fsVertices, false );
    }
}

vaRenderDeviceContext::~vaRenderDeviceContext( )
{

}

vaRenderDeviceContext::RenderOutputsState::RenderOutputsState( )
{
    Viewport            = vaViewport( 0, 0 );
    ScissorRect         = vaRecti( 0, 0, 0, 0 );
    ScissorRectEnabled  = false;
    
    for( int i = 0; i < _countof(UAVInitialCounts); i++ )
        UAVInitialCounts[i] = 0;
    
    RenderTargetCount   = 0;
    UAVsStartSlot       = 0;
    UAVCount            = 0;
}

void vaRenderDeviceContext::SetRenderTarget( const std::shared_ptr<vaTexture> & renderTarget, const std::shared_ptr<vaTexture> & depthStencil, bool updateViewport )
{
    for( int i = 0; i < c_maxRTs; i++ )     m_outputsState.RenderTargets[i]      = nullptr;
    for( int i = 0; i < c_maxUAVs; i++ )    m_outputsState.UAVs[i]               = nullptr;
    for( int i = 0; i < c_maxUAVs; i++ )    m_outputsState.UAVInitialCounts[i]   = (uint32)-1;
    m_outputsState.RenderTargets[0]     = renderTarget;
    m_outputsState.DepthStencil         = depthStencil;
    m_outputsState.RenderTargetCount    = 1;
    m_outputsState.UAVsStartSlot        = 0;
    m_outputsState.UAVCount             = 0;
    m_outputsState.ScissorRect          = vaRecti( 0, 0, 0, 0 );
    m_outputsState.ScissorRectEnabled   = false;

    const std::shared_ptr<vaTexture> & anyRT = ( renderTarget != NULL ) ? ( renderTarget ) : ( depthStencil );

    vaViewport vp = m_outputsState.Viewport;

    if( anyRT != NULL )
    {
        assert( ( anyRT->GetType( ) == vaTextureType::Texture2D ) || ( anyRT->GetType( ) == vaTextureType::Texture2DMS || ( anyRT->GetType( ) == vaTextureType::Texture2DArray ) || ( anyRT->GetType( ) == vaTextureType::Texture2DMSArray ) ) );   // others not supported yet
        vp.X = 0;
        vp.Y = 0;
        vp.Width = anyRT->GetViewedSliceSizeX( );
        vp.Height = anyRT->GetViewedSliceSizeY( );
    }

    if( renderTarget != NULL )
    {
        assert( ( renderTarget->GetBindSupportFlags( ) & vaResourceBindSupportFlags::RenderTarget ) != 0 );
    }
    if( depthStencil != NULL )
    {
        assert( ( depthStencil->GetBindSupportFlags( ) & vaResourceBindSupportFlags::DepthStencil ) != 0 );
    }

    UpdateRenderTargetsDepthStencilUAVs( );

    if( updateViewport )
        SetViewport( vp );
}

void vaRenderDeviceContext::SetRenderTargetsAndUnorderedAccessViews( uint32 numRTs, const std::shared_ptr<vaTexture> * renderTargets, const std::shared_ptr<vaTexture> & depthStencil,
    uint32 UAVStartSlot, uint32 numUAVs, const std::shared_ptr<vaTexture> * UAVs, bool updateViewport, const uint32 * UAVInitialCounts )
{
    assert( numRTs <= c_maxRTs );
    assert( numUAVs <= c_maxUAVs );
    m_outputsState.RenderTargetCount = numRTs  = vaMath::Min( numRTs , c_maxRTs  );
    m_outputsState.UAVCount          = numUAVs = vaMath::Min( numUAVs, c_maxUAVs );

    for( size_t i = 0; i < c_maxRTs; i++ )     
        m_outputsState.RenderTargets[i]      = (i < m_outputsState.RenderTargetCount)?(renderTargets[i]):(nullptr);
    for( size_t i = 0; i < c_maxUAVs; i++ )    
    {
        m_outputsState.UAVs[i]               = (i < m_outputsState.UAVCount)?(UAVs[i]):(nullptr);
        m_outputsState.UAVInitialCounts[i]   = ( (i < m_outputsState.UAVCount) && (UAVInitialCounts != nullptr) )?( UAVInitialCounts[i] ):( -1 );
    }
    m_outputsState.DepthStencil = depthStencil;
    m_outputsState.UAVsStartSlot = UAVStartSlot;

    const std::shared_ptr<vaTexture> & anyRT = ( m_outputsState.RenderTargets[0] != NULL ) ? ( m_outputsState.RenderTargets[0] ) : ( depthStencil );

    vaViewport vp = m_outputsState.Viewport;

    if( anyRT != NULL )
    {
        assert( ( anyRT->GetType( ) == vaTextureType::Texture2D ) || ( anyRT->GetType( ) == vaTextureType::Texture2DMS ) || ( anyRT->GetType( ) == vaTextureType::Texture2DArray ) || ( anyRT->GetType( ) == vaTextureType::Texture2DMSArray ) );   // others not supported yet
        vp.X = 0;
        vp.Y = 0;
        vp.Width = anyRT->GetViewedSliceSizeX( );
        vp.Height = anyRT->GetViewedSliceSizeY( );
    }

    for( size_t i = 0; i < m_outputsState.RenderTargetCount; i++ )
    {
        if( m_outputsState.RenderTargets[i] != nullptr )
        {
            assert( ( m_outputsState.RenderTargets[i]->GetBindSupportFlags( ) & vaResourceBindSupportFlags::RenderTarget ) != 0 );
        }
    }
    for( size_t i = 0; i < m_outputsState.UAVCount; i++ )
    {
        if( m_outputsState.UAVs[i] != nullptr )
        {
            assert( ( m_outputsState.UAVs[i]->GetBindSupportFlags( ) & vaResourceBindSupportFlags::UnorderedAccess ) != 0 );
        }
    }
    if( depthStencil != NULL )
    {
        assert( ( depthStencil->GetBindSupportFlags( ) & vaResourceBindSupportFlags::DepthStencil ) != 0 );
    }

    UpdateRenderTargetsDepthStencilUAVs( );

    if( updateViewport )
    {
        // can't update viewport if no RTs
        assert(anyRT != NULL);
        if( anyRT != NULL )
            SetViewport( vp );
    }
}

void vaRenderDeviceContext::SetRenderTargets( uint32 numRTs, const std::shared_ptr<vaTexture> * renderTargets, const std::shared_ptr<vaTexture> & depthStencil, bool updateViewport )
{
    SetRenderTargetsAndUnorderedAccessViews( numRTs, renderTargets, depthStencil, 0, 0, nullptr, updateViewport );
}

namespace VertexAsylum
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    void ImSetBigClearSansRegular( ImFont * font );
    void ImSetBigClearSansBold(    ImFont * font );
#endif`
}

void vaRenderDeviceContext::ImGuiCreate( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
        
    io.Fonts->AddFontDefault();

    // this would be a good place for DPI scaling.
    float bigSizePixels = 26.0f;
    float displayOffset = -1.0f;

    ImFontConfig fontConfig;

    vaFileTools::EmbeddedFileData fontFileData;

    fontFileData = vaFileTools::EmbeddedFilesFind( wstring( L"fonts:\\ClearSans-Regular.ttf" ) );
    if( fontFileData.HasContents( ) )
    {
        void * imguiBuffer = ImGui::MemAlloc( (int)fontFileData.MemStream->GetLength() );
        memcpy( imguiBuffer, fontFileData.MemStream->GetBuffer(), (int)fontFileData.MemStream->GetLength() );

        ImFont * font = io.Fonts->AddFontFromMemoryTTF( imguiBuffer, (int)fontFileData.MemStream->GetLength(), bigSizePixels, &fontConfig );
        font->DisplayOffset.y += displayOffset;   // Render 1 pixel down
        ImSetBigClearSansRegular( font );
    }

    fontFileData = vaFileTools::EmbeddedFilesFind( wstring( L"fonts:\\ClearSans-Bold.ttf" ) );
    if( fontFileData.HasContents( ) )
    {
        void * imguiBuffer = ImGui::MemAlloc( (int)fontFileData.MemStream->GetLength() );
        memcpy( imguiBuffer, fontFileData.MemStream->GetBuffer(), (int)fontFileData.MemStream->GetLength() );

        ImFont * font = io.Fonts->AddFontFromMemoryTTF( imguiBuffer, (int)fontFileData.MemStream->GetLength(), bigSizePixels, &fontConfig );
        font->DisplayOffset.y += displayOffset;   // Render 1 pixel down
        ImSetBigClearSansBold( font );
    }

#endif
}

void vaRenderDeviceContext::ImGuiDestroy( )
{
    ImGui::DestroyContext();
}

void vaRenderDeviceContext::FillFullscreenPassRenderItem( vaRenderItem & renderItem, bool zIs0 )
{
    renderItem.Topology         = vaPrimitiveTopology::TriangleList;
    renderItem.VertexShader     = m_fsVertexShader.get();
    renderItem.VertexBuffer     = (zIs0)?(m_fsVertexBufferZ0.get()):(m_fsVertexBufferZ1.get());
    renderItem.DrawType         = vaRenderItem::DrawSimple;
    renderItem.DrawSimpleParams.VertexCount = 3;
}
