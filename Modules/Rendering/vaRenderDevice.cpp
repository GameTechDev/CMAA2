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

#include "Rendering/vaRenderDevice.h"
#include "Rendering/vaTextureHelpers.h"

#include "vaAssetPack.h"

#include "vaRenderMesh.h"

#include "Rendering/vaDebugCanvas.h"

#include "Rendering/vaTexture.h"

#include "Core/System/vaFileTools.h"

#include "EmbeddedRenderingMedia.cpp"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Rendering/vaRenderGlobals.h"

#include "Core/Misc/vaProfiler.h"


using namespace VertexAsylum;

vaRenderDevice::vaRenderDevice( )
{ 
    m_profilingEnabled = true; 

//    vaRenderingModuleRegistrar::CreateSingletonIfNotCreated( );

    for( int i = 0; i < BINARY_EMBEDDER_ITEM_COUNT; i++ )
    {
        wchar_t * name = BINARY_EMBEDDER_NAMES[i];
        unsigned char * data = BINARY_EMBEDDER_DATAS[i];
        int64 dataSize = BINARY_EMBEDDER_SIZES[i];
        int64 timeStamp = BINARY_EMBEDDER_TIMES[i];

        vaFileTools::EmbeddedFilesRegister( name, data, dataSize, timeStamp );
    }
}

void vaRenderDevice::InitializeBase( )
{
    new vaProfiler( );

    assert( IsRenderThread() );

    m_canvas2D  = shared_ptr< vaDebugCanvas2D >( new vaDebugCanvas2D( vaRenderingModuleParams( *this ) ) );
    m_canvas3D  = shared_ptr< vaDebugCanvas3D >( new vaDebugCanvas3D( vaRenderingModuleParams( *this ) ) );

    m_textureTools = std::make_shared<vaTextureTools>( *this );
    m_renderMaterialManager = std::make_shared<vaRenderMaterialManager>( *this ); // shared_ptr<vaRenderMaterialManager>( VA_RENDERING_MODULE_CREATE( vaRenderMaterialManager, *this ) );
    m_renderMeshManager = shared_ptr<vaRenderMeshManager>( VA_RENDERING_MODULE_CREATE( vaRenderMeshManager, *this ) );
    m_assetPackManager = std::make_shared<vaAssetPackManager>( *this );//shared_ptr<vaAssetPackManager>( VA_RENDERING_MODULE_CREATE( vaAssetPackManager, *this ) );

    m_renderGlobals = VA_RENDERING_MODULE_CREATE_SHARED( vaRenderGlobals, *this );

    // fullscreen triangle stuff
    {
        m_fsVertexShader    = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexShader, *this );
        m_fsVertexBufferZ0  = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexBuffer, *this );
        m_fsVertexBufferZ1  = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexBuffer, *this );
        m_copyResourcePS    = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, *this );

        std::vector<vaVertexInputElementDesc> inputElements;
        inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32A32_FLOAT,    0,  0, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "TEXCOORD", 0,       vaResourceFormat::R32G32_FLOAT,          0, 16, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

        // full screen pass vertex shader
        {
            const char * pVSString = "void main( inout const float4 xPos : SV_Position, inout float2 UV : TEXCOORD0 ) { }";
            m_fsVertexShader->CreateShaderAndILFromBuffer( pVSString, "vs_5_0", "main", inputElements, vaShaderMacroContaner(), true );
        }

        // copy resource shader
        {
            string shaderCode = 
                "Texture2D g_source           : register( t0 );                 \n"
                "float4 main( in const float4 xPos : SV_Position ) : SV_Target  \n"
                "{                                                              \n"
                "   return g_source.Load( int3( xPos.xy, 0 ) );                 \n"
                "}                                                              \n";

            m_copyResourcePS->CreateShaderFromBuffer( shaderCode, "ps_5_0", "main", vaShaderMacroContaner(), true );
        }

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
}

void vaRenderDevice::DeinitializeBase( )
{
    assert( IsRenderThread() );
    m_renderGlobals             = nullptr;
    m_fsVertexShader            = nullptr;
    m_fsVertexBufferZ0          = nullptr;
    m_fsVertexBufferZ1          = nullptr;
    m_copyResourcePS            = nullptr;
    m_canvas2D                  = nullptr;
    m_canvas3D                  = nullptr;
    m_assetPackManager          = nullptr;
    m_textureTools              = nullptr;
    m_renderMaterialManager     = nullptr;
    m_renderMeshManager         = nullptr;
    m_shaderManager             = nullptr;
    vaBackgroundTaskManager::GetInstancePtr()->ClearAndRestart();

    delete vaProfiler::GetInstancePtr( );
}

vaRenderDevice::~vaRenderDevice( ) 
{ 
    assert( IsRenderThread() );
    assert( !m_frameStarted );
    assert( m_disabled );
    assert( m_renderGlobals == nullptr );   // forgot to call DeinitializeBase()?
}

vaTextureTools & vaRenderDevice::GetTextureTools( )
{
    assert( IsRenderThread() );
    assert( m_textureTools != nullptr );
    return *m_textureTools;
}

vaRenderMaterialManager & vaRenderDevice::GetMaterialManager( )
{
    assert( m_renderMaterialManager != nullptr );
    return *m_renderMaterialManager;
}

vaRenderMeshManager &     vaRenderDevice::GetMeshManager( )
{
    assert( m_renderMeshManager != nullptr );
    return *m_renderMeshManager;
}

vaAssetPackManager &     vaRenderDevice::GetAssetPackManager( )
{
    assert( IsRenderThread() );
    assert( m_assetPackManager != nullptr );
    return *m_assetPackManager;
}

vaRenderGlobals & vaRenderDevice::GetRenderGlobals( )
{
    assert( IsRenderThread() );
    assert( m_renderGlobals != nullptr );
    return *m_renderGlobals;
}

void vaRenderDevice::BeginFrame( float deltaTime )
{
    assert( !m_disabled );
    assert( IsRenderThread() );
    m_totalTime += deltaTime;
    m_lastDeltaTime = deltaTime;
    assert( !m_frameStarted );
    m_frameStarted = true;
    m_currentFrameIndex++;

    if( m_mainDeviceContext != nullptr )
        m_mainDeviceContext->SetRenderTarget( GetCurrentBackbuffer(), nullptr, true );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    // for DX12 there's an empty frame done before creating the swapchain - ignore some of the checks during that
    if( m_swapChainTextureSize != vaVector2i(0, 0) )
    {
        assert( m_imguiFrameStarted );      // ImGui always has frame set so anyone can imgui anything at any time (if on the main thread)
    }
#endif
}

void vaRenderDevice::EndAndPresentFrame( int vsyncInterval )
{
    assert( !m_disabled );
    assert( IsRenderThread() );
    vsyncInterval;
    assert( vaThreading::IsMainThread() );
    assert( m_frameStarted );
    m_frameStarted = false;

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    // for DX12 there's an empty frame done before creating the swapchain - ignore some of the checks during that
    if( m_swapChainTextureSize != vaVector2i(0, 0) )
    {
        assert( m_imguiFrameStarted );
        if( !m_imguiFrameRendered )
        {
            // if we haven't rendered anything, reset imgui to avoid any unnecessary accumulation
            ImGuiEndFrame( );
            ImGuiNewFrame( );
        }
        m_imguiFrameRendered = false;
    }
#endif
}

void vaRenderDevice::FillFullscreenPassRenderItem( vaGraphicsItem & renderItem, bool zIs0 ) const
{
    assert( !m_disabled );

    // this should be thread-safe as long as the lifetime of the device is guaranteed
    renderItem.Topology         = vaPrimitiveTopology::TriangleList;
    renderItem.VertexShader     = GetFSVertexShader();
    renderItem.VertexBuffer     = (zIs0)?(GetFSVertexBufferZ0()):(GetFSVertexBufferZ1());
    renderItem.DrawType         = vaGraphicsItem::DrawType::DrawSimple;
    renderItem.DrawSimpleParams.VertexCount = 3;
}


namespace VertexAsylum
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    void ImSetBigClearSansRegular( ImFont * font );
    void ImSetBigClearSansBold(    ImFont * font );
#endif`
}

void vaRenderDevice::ImGuiCreate( )
{
    assert( IsRenderThread() );
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

    // enable docking
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
}

void vaRenderDevice::ImGuiDestroy( )
{
    assert( IsRenderThread() );
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::DestroyContext();
#endif
}

void vaRenderDevice::ImGuiEndFrame( )
{
    assert( !m_disabled );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    assert( m_imguiFrameStarted );
    ImGui::EndFrame();
    m_imguiFrameStarted = false;
#endif
}

void vaRenderDevice::ImGuiRender( vaRenderDeviceContext & renderContext )
{
    assert( !m_disabled );

    renderContext;
    assert( m_frameStarted );
    assert( !m_imguiFrameRendered );    // can't render twice per frame due to internal imgui constraints!
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGuiEndFrameAndRender( renderContext );
    m_imguiFrameRendered = true;
    ImGuiNewFrame();
#endif
}
