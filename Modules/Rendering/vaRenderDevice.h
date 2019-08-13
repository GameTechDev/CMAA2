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

    struct vaGraphicsItem;
    struct vaSceneDrawContext;

    class vaTextureTools;
    class vaRenderMaterialManager;
    class vaRenderMeshManager;
    class vaAssetPackManager;

    class vaShaderManager;

    class vaVertexShader;
    class vaPixelShader;
    class vaVertexBuffer;
    
    class vaRenderGlobals;

    enum class vaFullscreenState : int32
    {
        Unknown = 0,
        Windowed = 1,
        Fullscreen = 2,
        FullscreenBorderless = 3
    };

    class vaRenderDevice
    {
    public:
        vaEvent<void(vaRenderDevice &)>         e_DeviceFullyInitialized;
        vaEvent<void()>                         e_DeviceAboutToBeDestroyed;

    protected:
        struct SimpleVertex
        {
            float   Position[4];
            float   UV[2];

            SimpleVertex( ) {};
            SimpleVertex( float px, float py, float pz, float pw, float uvx, float uvy ) { Position[0] = px; Position[1] = py; Position[2] = pz; Position[3] = pw; UV[0] = uvx; UV[1] = uvy; }
        };
        shared_ptr<vaVertexShader>              m_fsVertexShader;  
        shared_ptr<vaVertexBuffer>              m_fsVertexBufferZ0;
        shared_ptr<vaVertexBuffer>              m_fsVertexBufferZ1;
        shared_ptr<vaPixelShader>               m_copyResourcePS;  

    protected:

        int64                                   m_currentFrameIndex            = 0;

        shared_ptr<vaDebugCanvas2D>             m_canvas2D;
        shared_ptr<vaDebugCanvas3D>             m_canvas3D;

        shared_ptr<vaRenderDeviceContext>       m_mainDeviceContext;

        bool                                    m_profilingEnabled;

        // maybe do https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2 for the future...
        shared_ptr<vaTextureTools>              m_textureTools;
        shared_ptr<vaRenderGlobals>             m_renderGlobals;
        shared_ptr<vaRenderMaterialManager>     m_renderMaterialManager;
        shared_ptr<vaRenderMeshManager>         m_renderMeshManager;
        shared_ptr<vaAssetPackManager>          m_assetPackManager;
        shared_ptr<vaShaderManager>             m_shaderManager;

        vaVector2i                              m_swapChainTextureSize = { 0, 0 };
        string                                  m_adapterNameShort;
        uint                                    m_adapterVendorID;
        string                                  m_adapterNameID;             // adapterNameID is a mix of .Description and [.SubSysId] that uniquely identifies the current graphics device on the system


        double                                  m_totalTime             = 0.0;
        float                                   m_lastDeltaTime         = 0.0f;
        bool                                    m_frameStarted = false;
        //std::mutex                              m_frameMutex;

        bool                                    m_imguiFrameStarted = false;
        bool                                    m_imguiFrameRendered = false;

        // a lot of the vaRenderDevice functionality is locked to the thread that created the object
        std::atomic<std::thread::id>            m_threadID              = std::this_thread::get_id();

        vaFullscreenState                       m_fullscreenState       = vaFullscreenState::Unknown;
        
        // set when window is destroyed, presents and additional rendering is no longer possible but device is still not destroyed
        bool                                    m_disabled             = false;

    public:
        static const int                        c_BackbufferCount    = 2;

    public:
        vaRenderDevice( );
        virtual ~vaRenderDevice( );

    protected:
        void                                InitializeBase( );
        void                                DeinitializeBase( );

    public:
        virtual void                        CreateSwapChain( int width, int height, HWND hwnd, vaFullscreenState fullscreenState )  = 0;
        virtual bool                        ResizeSwapChain( int width, int height, vaFullscreenState fullscreenState )             = 0;    // return true if actually resized (for further handling)

        void                                SetDisabled( )                                                             { m_disabled = true; }

        const vaVector2i &                  GetSwapChainTextureSize( ) const { return m_swapChainTextureSize; }

        virtual shared_ptr<vaTexture>       GetCurrentBackbuffer( ) const                                               = 0;

        virtual bool                        IsSwapChainCreated( ) const                                                 = 0;
        virtual void                        SetWindowed( )                                                              = 0;    // disable fullscreen
        virtual vaFullscreenState           GetFullscreenState( ) const                                                 { return m_fullscreenState; }

       
        virtual void                        BeginFrame( float deltaTime )              ;
        virtual void                        EndAndPresentFrame( int vsyncInterval = 0 );

        bool                                IsRenderThread( ) const                                                     { return m_threadID == std::this_thread::get_id(); }
        bool                                IsFrameStarted( ) const                                                     { return m_frameStarted; }

        double                              GetTotalTime( ) const                                                       { return m_totalTime; }
        int64                               GetCurrentFrameIndex( ) const                                               { return m_currentFrameIndex; }

        vaRenderDeviceContext *             GetMainContext( ) const                                                     { assert( vaThreading::IsMainThread() ); return m_mainDeviceContext.get(); }
                
        vaDebugCanvas2D &                   GetCanvas2D( )                                                              { return *m_canvas2D; }
        vaDebugCanvas3D &                   GetCanvas3D( )                                                              { return *m_canvas3D; }


        bool                                IsProfilingEnabled( )                                                       { return m_profilingEnabled; }

        const string &                      GetAdapterNameShort( ) const                                                { return m_adapterNameShort; }
        const string &                      GetAdapterNameID( ) const                                                   { return m_adapterNameID; }
        uint                                GetAdapterVendorID( ) const                                                 { return m_adapterVendorID; }
        virtual string                      GetAPIName( ) const                                                         = 0;

        // Fullscreen 
        const shared_ptr<vaVertexShader> &  GetFSVertexShader( ) const                                                  { return m_fsVertexShader;   }
        const shared_ptr<vaVertexBuffer> &  GetFSVertexBufferZ0( ) const                                                { return m_fsVertexBufferZ0; }
        const shared_ptr<vaVertexBuffer> &  GetFSVertexBufferZ1( ) const                                                { return m_fsVertexBufferZ1; }
        const shared_ptr<vaPixelShader>  &  GetFSCopyResourcePS( ) const                                                { return m_copyResourcePS;   }
        void                                FillFullscreenPassRenderItem( vaGraphicsItem & renderItem, bool zIs0 = true ) const;

    public:
        // ImGui gets drawn into the main device context ( GetMainContext() ) - this is fixed for now but could be a parameter
        void                                ImGuiRender( vaRenderDeviceContext & renderContext );

    protected:
        virtual void                        ImGuiCreate( );
        virtual void                        ImGuiDestroy( );
        virtual void                        ImGuiNewFrame( ) = 0;
        virtual void                        ImGuiEndFrame( );
        // ImGui gets drawn into the main device context ( GetMainContext() ) - this is fixed for now but could be a parameter
        virtual void                        ImGuiEndFrameAndRender( vaRenderDeviceContext & renderContext ) = 0;



    public:
        // These are essentially API dependencies - they require graphics API to be initialized so there's no point setting them up separately
        vaTextureTools &                    GetTextureTools( );
        vaRenderMaterialManager &           GetMaterialManager( );
        vaRenderMeshManager &               GetMeshManager( );
        vaAssetPackManager &                GetAssetPackManager( );
        virtual vaShaderManager &           GetShaderManager( )                                                         = 0;
        vaRenderGlobals &                   GetRenderGlobals( );


    public:
        template< typename CastToType >
        CastToType                          SafeCast( )                                                                 
        {
#ifdef _DEBUG
            CastToType ret = dynamic_cast< CastToType >( this );
            assert( ret != NULL );
            return ret;
#else
            return static_cast< CastToType >( this );
#endif
        }

        // ID3D11Device *             GetPlatformDevice( ) const                                  { m_device; }
    };

}