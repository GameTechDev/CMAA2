/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include <vector>

#include "Rendering/DirectX/vaDirectXIncludes.h"
// #include <d3d10.h>
// #include <d3dx10.h>
// #include <d3d9.h>
// #include <dxerr.h>
// #include <dxgi.h>


class NoMSAA : public DXGI_SAMPLE_DESC {
    public:
        inline NoMSAA() { 
            Count = 1;
            Quality = 0;
        }
};


class RenderTarget {
    public:
        RenderTarget(ID3D11Device *device, int width, int height,
            DXGI_FORMAT format,
            const DXGI_SAMPLE_DESC &sampleDesc=NoMSAA(),
            bool typeless=true);

        /**
         * These two are just convenience constructors to build from existing
         * resources.
         */
        RenderTarget(ID3D11Device *device, ID3D11Texture2D *texture2D, DXGI_FORMAT format);
        RenderTarget(ID3D11Device *device,
            ID3D11RenderTargetView *renderTargetView,
            ID3D11ShaderResourceView *shaderResourceView);

        ~RenderTarget();

        operator ID3D11Texture2D * () const { return texture2D; }
        operator ID3D11RenderTargetView * () const { return renderTargetView; }
        operator ID3D11RenderTargetView *const * () const { return &renderTargetView; }
        operator ID3D11ShaderResourceView * () const { return shaderResourceView; }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        void setViewport( ID3D11DeviceContext * context, float minDepth=0.0f, float maxDepth=1.0f) const;

        static DXGI_FORMAT makeTypeless(DXGI_FORMAT format);

    private:
        void createViews(D3D11_TEXTURE2D_DESC desc, DXGI_FORMAT format);

        ID3D11Device *device;
        int width, height;
        ID3D11Texture2D *texture2D;
        ID3D11RenderTargetView *renderTargetView;
        ID3D11ShaderResourceView *shaderResourceView;
};


class DepthStencil {
    public:
        DepthStencil(ID3D11Device *device, int width, int height,
            DXGI_FORMAT texture2DFormat = DXGI_FORMAT_R32_TYPELESS, 
            DXGI_FORMAT depthStencilViewFormat = DXGI_FORMAT_D32_FLOAT, 
            DXGI_FORMAT shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT,
            const DXGI_SAMPLE_DESC &sampleDesc=NoMSAA());
        ~DepthStencil();

        operator ID3D11Texture2D * const () { return texture2D; }
        operator ID3D11DepthStencilView * const () { return depthStencilView; }
        operator ID3D11ShaderResourceView * const () { return shaderResourceView; }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        void setViewport( ID3D11DeviceContext * context, float minDepth=0.0f, float maxDepth=1.0f) const;

    private:
        ID3D11Device *device;
        int width, height;
        ID3D11Texture2D *texture2D;
        ID3D11DepthStencilView *depthStencilView;
        ID3D11ShaderResourceView *shaderResourceView;
};


class BackbufferRenderTarget {
    public:
        BackbufferRenderTarget(ID3D11Device *device, IDXGISwapChain *swapChain);
        ~BackbufferRenderTarget();

        operator ID3D11Texture2D * () const { return texture2D; }
        operator ID3D11RenderTargetView * () const { return renderTargetView; }
        operator ID3D11RenderTargetView *const * () const { return &renderTargetView; }
        operator ID3D11ShaderResourceView * () const { return shaderResourceView; }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

    private:
        int width, height;
        ID3D11Texture2D *texture2D;
        ID3D11RenderTargetView *renderTargetView;
        ID3D11ShaderResourceView *shaderResourceView;
};

struct D3DXVECTOR3 {
    float X, Y, Z;
    D3DXVECTOR3( ) { }
    D3DXVECTOR3( float x, float y, float z ) : X( x ), Y( y ), Z( z ) { }
};
struct D3DXVECTOR2 {
    float X, Y;
    D3DXVECTOR2( ) { }
    D3DXVECTOR2( float x, float y ) : X( x ), Y( y ) { }
};

// const D3D11_INPUT_ELEMENT_DESC layout[] = {
//     { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//     { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
// };
// UINT numElements = sizeof( layout ) / sizeof( D3D11_INPUT_ELEMENT_DESC );

class Quad {
    public:
        Quad(ID3D11Device *device); //, const D3D10_PASS_DESC &desc);
        ~Quad();
        //void setInputLayout( ID3D11DeviceContext * context ) { context->IASetInputLayout(vertexLayout); }
        void draw( ID3D11DeviceContext * context );

    private:
        ID3D11Device *device;
        ID3D11Buffer *buffer;
        //ID3D11InputLayout *vertexLayout;
};


class FullscreenTriangle {
    public:
        FullscreenTriangle(ID3D11Device *device); //, const D3D10_PASS_DESC &desc);
        ~FullscreenTriangle();
        //void setInputLayout( ID3D11DeviceContext * context ) { context->IASetInputLayout(vertexLayout); }
        void draw( ID3D11DeviceContext * context );

    private:
        ID3D11Device *device;
        ID3D11Buffer *buffer;
        //ID3D11InputLayout *vertexLayout;
};


class SaveViewportsScope {
    public: 
        SaveViewportsScope(ID3D11DeviceContext * context);
        ~SaveViewportsScope();

    private:
        ID3D11DeviceContext * context;
        UINT numViewports;
        std::vector<D3D11_VIEWPORT> viewports;
};


class SaveRenderTargetsScope {
    public: 
        SaveRenderTargetsScope(ID3D11DeviceContext * context);
        ~SaveRenderTargetsScope();

    private:
        ID3D11DeviceContext * context;
        ID3D11RenderTargetView *renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ID3D11DepthStencilView *depthStencil;
};


class SaveInputLayoutScope {
    public: 
        SaveInputLayoutScope(ID3D11DeviceContext * context);
        ~SaveInputLayoutScope();

    private:
        ID3D11DeviceContext * context;
        ID3D11InputLayout *inputLayout;
};


class SaveBlendStateScope {
    public:
        SaveBlendStateScope(ID3D11DeviceContext * context);
        ~SaveBlendStateScope();

    private:
        ID3D11DeviceContext * context;
        ID3D11BlendState *blendState;
        FLOAT blendFactor[4];
        UINT sampleMask;
};


class SaveDepthStencilScope {
    public:
        SaveDepthStencilScope(ID3D11DeviceContext * context);
        ~SaveDepthStencilScope();

    private:
        ID3D11DeviceContext * context;
        ID3D11DepthStencilState *depthStencilState;
        UINT stencilRef;
};

// class PerfEventScope {
//     public:
//         PerfEventScope(const std::wstring &eventName) { D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), eventName.c_str()); }
//         ~PerfEventScope() { D3DPERF_EndEvent(); }
// };

class Utils {
    public:
        static ID3D11Texture2D *createStagingTexture(ID3D11Device *device, ID3D11Texture2D *texture);
        static D3D11_VIEWPORT viewportFromView(ID3D11View *view);
        static D3D11_VIEWPORT viewportFromTexture2D(ID3D11Texture2D *texture2D);
};

#endif
