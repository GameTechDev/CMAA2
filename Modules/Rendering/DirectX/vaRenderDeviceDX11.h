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
#include "Rendering/vaDebugCanvas.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
//#include "Rendering/DirectX/vaDebugCanvas2DDX11.h"
//#include "Rendering/DirectX/vaDebugCanvas3DDX11.h"

namespace VertexAsylum
{
    class vaApplicationWin;

    class vaRenderDeviceDX11 : public vaRenderDevice
    {
    private:
        IDXGIFactory1 *             m_DXGIFactory;
        ID3D11Device *              m_device;
        ID3D11DeviceContext *       m_deviceImmediateContext;
        IDXGISwapChain *            m_swapChain;
        IDXGIOutput *               m_mainOutput;

        ID3D11RenderTargetView *    m_mainRenderTargetView;
        D3D11_TEXTURE2D_DESC        m_backbufferTextureDesc;

        ID3D11Texture2D *           m_mainDepthStencil;
        ID3D11DepthStencilView *    m_mainDepthStencilView;
        ID3D11ShaderResourceView *  m_mainDepthSRV;

        vaApplicationWin *          m_application;

        vaNestedProfilerNode *      m_frameProfilingNode = nullptr;

        HWND                        m_hwnd                  = 0;

//    private:
//        vaDirectXCore               m_directXCore;

    private:
        //static vaRenderDeviceDX11 *    s_mainDevice;

    public:
        vaRenderDeviceDX11( const vector<wstring> & shaderSearchPaths = { vaCore::GetExecutableDirectory( ) } );
        virtual ~vaRenderDeviceDX11( void );

    public:
        HWND                        GetHWND( ) { return m_hwnd; }

    private:
        bool                        Initialize( const vector<wstring> & shaderSearchPaths );
        void                        Deinitialize( );

    protected:
        virtual void                CreateSwapChain( int width, int height, bool windowed, HWND hwnd );
        virtual bool                ResizeSwapChain( int width, int height, bool windowed );                    // returns true if actually resized

        virtual bool                IsFullscreen( );
        virtual bool                IsSwapChainCreated( ) const                                                 { return m_swapChain != NULL; }

        virtual void                FindClosestFullscreenMode( int & width, int & height );

        virtual void                BeginFrame( float deltaTime );
        virtual void                EndAndPresentFrame( int vsyncInterval = 0 );

    public:

        ID3D11DeviceContext *       GetImmediateRenderContext( ) const                                          { assert( vaThreading::IsMainThread() ); return m_deviceImmediateContext; }
        ID3D11Device *              GetPlatformDevice( ) const                                                  { return m_device; }

        virtual vaShaderManager &   GetShaderManager( ) override;

        void                        SetMainRenderTargetToImmediateContext( );

        //static vaRenderDeviceDX11 &    GetMainInstance( )                                                     { assert( s_mainDevice != NULL ); return *s_mainDevice; }

        void                        NameObject( ID3D11DeviceChild * object, const char * permanentNameString );
        void                        NameObject( ID3D11Resource * resource, const char * permanentNameString );


    private:
        void                        CreateSwapChainRelatedObjects( );
        void                        ReleaseSwapChainRelatedObjects( );

        static void                 RegisterModules( );
    };

}