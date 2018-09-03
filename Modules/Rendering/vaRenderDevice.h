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

#include "Core/vaCoreIncludes.h"
#include "Core/vaEvent.h"


namespace VertexAsylum
{
    class vaDebugCanvas2D;
    class vaDebugCanvas3D;

    class vaTexture;
    class vaRenderDeviceContext;
    struct vaSceneDrawContext;

    class vaTextureTools;
    class vaRenderMaterialManager;
    class vaRenderMeshManager;
    class vaAssetPackManager;

    class vaShaderManager;

    class vaRenderDevice
    {
    public:
        vaEvent<void(void)>                     e_DeviceFullyInitialized;

    protected:

        int64                                   m_renderFrameCounter;

        shared_ptr<vaDebugCanvas2D>             m_canvas2D;
        shared_ptr<vaDebugCanvas3D>             m_canvas3D;

        shared_ptr<vaTexture>                   m_mainColor;    // aka m_swapChainColor
        shared_ptr<vaTexture>                   m_mainDepth;    // aka m_swapChainDepth
        shared_ptr<vaRenderDeviceContext>       m_mainDeviceContext;

        bool                                    m_profilingEnabled;

        bool                                    m_showIMGUI     = true;

        shared_ptr<vaTextureTools>              m_textureTools;
        shared_ptr<vaRenderMaterialManager>     m_renderMaterialManager;
        shared_ptr<vaRenderMeshManager>         m_renderMeshManager;
        shared_ptr<vaAssetPackManager>          m_assetPackManager;
        shared_ptr<vaShaderManager>             m_shaderManager;

        vaVector2i                              m_swapChainSize = { 0, 0 };
        wstring                                 m_adapterNameShort;
        wstring                                 m_adapterNameID;
        uint                                    m_adapterVendorID;

        std::atomic_bool                        m_frameStarted = false;
        std::mutex                              m_frameMutex;

    public:
        static const int                        c_DefaultBackbufferCount    = 2;

    public:
        vaRenderDevice( );
        virtual ~vaRenderDevice( );

        void                            InitializeBase( );
        void                            DeinitializeBase( );

    public:
        virtual void                    CreateSwapChain( int width, int height, bool windowed, HWND hwnd )          = 0;
        virtual bool                    ResizeSwapChain( int width, int height, bool windowed )                     = 0;    // return true if actually resized (for further handling)

        const vaVector2i &              GetSwapChainSize( ) const { return m_swapChainSize; }

        std::shared_ptr<vaTexture>      GetMainChainColor( ) const                                                  { return m_mainColor; }
        std::shared_ptr<vaTexture>      GetMainChainDepth( ) const                                                  { return m_mainDepth; }

        virtual bool                    IsFullscreen( )                                                             = 0;
        virtual bool                    IsSwapChainCreated( ) const                                                 = 0;
        
        virtual void                    FindClosestFullscreenMode( int & width, int & height )                      = 0;

        // main 
        virtual void                    BeginFrame( float deltaTime )              ;
        virtual void                    EndAndPresentFrame( int vsyncInterval = 0 );

        bool                            IsFrameStarted( ) const                                                     { return m_frameStarted; }

        // this mutex gets locked between BeginFrame and EndAndPresentFrame; locking it outside ensures no rendering or device changes will happen so one can freely create textures/resources/whatever
        std::mutex &                    GetFrameMutex( )                                                            { return m_frameMutex; }

        int64                           GetFrameCount( ) const                                                      { return m_renderFrameCounter; }

        vaRenderDeviceContext *         GetMainContext( )                                                           { assert( vaThreading::IsMainThread() ); return m_mainDeviceContext.get(); }
                
        vaDebugCanvas2D &               GetCanvas2D( )                                                              { return *m_canvas2D; }
        vaDebugCanvas3D &               GetCanvas3D( )                                                              { return *m_canvas3D; }


        bool                            IsProfilingEnabled( )                                                       { return m_profilingEnabled; }

        const wstring &                 GetAdapterNameShort( ) const                                                { return m_adapterNameShort; }
        const wstring &                 GetAdapterNameID( ) const                                                   { return m_adapterNameID; }
        uint                            GetAdapterVendorID( ) const                                                 { return m_adapterVendorID; }

    public:
        bool                            ImGuiIsVisible( ) const                 { return m_showIMGUI; }
        void                            ImGuiSetVisible( bool visible )         { m_showIMGUI = visible; }

    protected:
        virtual void                    ImGuiCreate( );
        virtual void                    ImGuiDestroy( );



    public:
        // These are essentially API dependencies - they require graphics API to be initialized so there's no point setting them up separately
        vaTextureTools &                GetTextureTools( );
        vaRenderMaterialManager &       GetMaterialManager( );
        vaRenderMeshManager &           GetMeshManager( );
        vaAssetPackManager &            GetAssetPackManager( );
        virtual vaShaderManager &       GetShaderManager( )                                                         = 0;


    public:
        template< typename CastToType >
        CastToType                      SafeCast( )                                                                 
        { 
           CastToType ret = dynamic_cast< CastToType >( this );
           assert( ret != NULL );
           return ret;
        }

        // ID3D11Device *             GetPlatformDevice( ) const                                  { m_device; }
    };

}