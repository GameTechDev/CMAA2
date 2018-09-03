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

#include "Rendering/vaRenderDevice.h"
#include "Rendering/vaTextureHelpers.h"

#include "vaAssetPack.h"

#include "vaRenderMesh.h"

#include "Rendering/vaDebugCanvas.h"

#include "Rendering/vaTexture.h"

#include "Core/System/vaFileTools.h"

#include "EmbeddedRenderingMedia.cpp"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaRenderDevice::vaRenderDevice( )
{ 
    assert( vaThreading::IsMainThread() );
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
    assert( vaThreading::IsMainThread() );
    m_canvas2D  = shared_ptr< vaDebugCanvas2D >( new vaDebugCanvas2D( vaRenderingModuleParams( *this ) ) );
    m_canvas3D  = shared_ptr< vaDebugCanvas3D >(  new vaDebugCanvas3D( vaRenderingModuleParams( *this ) ) );

    m_textureTools = std::make_shared<vaTextureTools>( *this );
    m_renderMaterialManager = std::make_shared<vaRenderMaterialManager>( *this ); // shared_ptr<vaRenderMaterialManager>( VA_RENDERING_MODULE_CREATE( vaRenderMaterialManager, *this ) );
    m_renderMeshManager = shared_ptr<vaRenderMeshManager>( VA_RENDERING_MODULE_CREATE( vaRenderMeshManager, *this ) );
    m_assetPackManager = std::make_shared<vaAssetPackManager>( *this );//shared_ptr<vaAssetPackManager>( VA_RENDERING_MODULE_CREATE( vaAssetPackManager, *this ) );
}

void vaRenderDevice::DeinitializeBase( )
{
    assert( vaThreading::IsMainThread() );
    m_canvas2D                  = nullptr;
    m_canvas3D                  = nullptr;
    m_assetPackManager          = nullptr;
    m_textureTools              = nullptr;
    m_renderMaterialManager     = nullptr;
    m_renderMeshManager         = nullptr;
    m_shaderManager             = nullptr;
    vaBackgroundTaskManager::GetInstancePtr()->StopManager();
}

vaRenderDevice::~vaRenderDevice( ) 
{ 
    assert( vaThreading::IsMainThread() );
    assert( !m_frameStarted );
}

void vaRenderDevice::ImGuiCreate( ) 
{
    assert( vaThreading::IsMainThread() );
    m_mainDeviceContext->ImGuiCreate();
}

void vaRenderDevice::ImGuiDestroy( )
{
    assert( vaThreading::IsMainThread() );
    m_mainDeviceContext->ImGuiDestroy();
}

vaTextureTools & vaRenderDevice::GetTextureTools( )
{
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
    assert( m_assetPackManager != nullptr );
    return *m_assetPackManager;
}

void vaRenderDevice::BeginFrame( float deltaTime )
{
    assert( vaThreading::IsMainThread() );
    deltaTime;
    assert( !m_frameStarted );
    m_frameMutex.lock();
    m_frameStarted = true;
}

void vaRenderDevice::EndAndPresentFrame( int vsyncInterval )
{
    vsyncInterval;
    assert( vaThreading::IsMainThread() );
    assert( m_frameStarted );
    m_frameStarted = false;
    m_frameMutex.unlock();
}
