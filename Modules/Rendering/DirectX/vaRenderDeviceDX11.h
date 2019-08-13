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

#include "Rendering/vaRenderDevice.h"
#include "Rendering/vaRendering.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"

namespace VertexAsylum
{
    class vaRenderDeviceDX11 : public vaRenderDevice
    {
    private:
        string                      m_preferredAdapterNameID = "";
        IDXGIFactory1 *             m_DXGIFactory;
        ID3D11Device *              m_device;
        ID3D11DeviceContext *       m_deviceImmediateContext;
        IDXGISwapChain *            m_swapChain;
        IDXGIOutput *               m_mainOutput;   // no longer needed really!

        ID3D11RenderTargetView *    m_mainRenderTargetView;
        D3D11_TEXTURE2D_DESC        m_backbufferTextureDesc;

        vaNestedProfilerNode *      m_frameProfilingNode = nullptr;

        HWND                        m_hwnd                  = 0;

        shared_ptr<vaTexture>       m_mainColor;    // aka backbuffer

        vector< std::function<void( vaRenderDeviceDX11 & device )> >
                                    m_beginFrameCallbacks;

    public:
        // adapterNameID is a mix of .Description and [.SubSysId]
        vaRenderDeviceDX11( const string & preferredAdapterNameID = "", const vector<wstring> & shaderSearchPaths = { vaCore::GetExecutableDirectory( ), vaCore::GetExecutableDirectory( ) + L"../../Modules/Rendering/Shaders" } );
        virtual ~vaRenderDeviceDX11( void );

    public:
        HWND                        GetHWND( ) { return m_hwnd; }

    private:
        bool                        Initialize( const vector<wstring> & shaderSearchPaths );
        void                        Deinitialize( );

    protected:
        virtual void                CreateSwapChain( int width, int height, HWND hwnd, vaFullscreenState fullscreenState ) override;
        virtual bool                ResizeSwapChain( int width, int height, vaFullscreenState fullscreenState ) override;       // returns true if actually resized
        virtual void                SetWindowed( ) override;

        //virtual bool                IsFullscreen( );
        virtual bool                IsSwapChainCreated( ) const override                                    { return m_swapChain != NULL; }

        // virtual void                FindClosestFullscreenMode( int & width, int & height );

        virtual void                BeginFrame( float deltaTime );
        virtual void                EndAndPresentFrame( int vsyncInterval = 0 );

        virtual shared_ptr<vaTexture>   GetCurrentBackbuffer( ) const override                              { return m_mainColor; }


    public:

        ID3D11DeviceContext *       GetImmediateRenderContext( ) const                                      { assert( vaThreading::IsMainThread() ); return m_deviceImmediateContext; }
        ID3D11Device *              GetPlatformDevice( ) const                                              { return m_device; }

        virtual vaShaderManager &   GetShaderManager( ) override;

        void                        NameObject( ID3D11DeviceChild * object, const char * permanentNameString );
        void                        NameObject( ID3D11Resource * resource, const char * permanentNameString );

        void                        ExecuteAtBeginFrame( const std::function<void( vaRenderDeviceDX11 & device )> & callback )  { assert( vaThreading::IsMainThread() );  m_beginFrameCallbacks.push_back( callback );  }

    private:
        void                        CreateSwapChainRelatedObjects( );
        void                        ReleaseSwapChainRelatedObjects( );

        static void                 RegisterModules( );

    protected:
        virtual void                ImGuiCreate( ) override;
        virtual void                ImGuiDestroy( ) override;
        virtual void                ImGuiNewFrame( ) override;
        // ImGui gets drawn into the main device context ( GetMainContext() ) - this is fixed for now but could be a parameter
        virtual void                ImGuiEndFrameAndRender( vaRenderDeviceContext & renderContext ) override;

    public:
        virtual string              GetAPIName( ) const override                                            { return StaticGetAPIName(); }
        static string               StaticGetAPIName( )                                                     { return "DirectX11"; }
        static void                 StaticEnumerateAdapters( vector<pair<string, string>> & outAdapters );
    };

}