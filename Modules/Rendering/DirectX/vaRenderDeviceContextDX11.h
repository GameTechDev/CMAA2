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

#include "Rendering/DirectX/vaRenderDeviceDX11.h"
#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaShaderDX11.h"

#include "Rendering/vaRenderingIncludes.h"


namespace VertexAsylum
{
    class vaRenderDeviceContextDX11 : public vaRenderDeviceContext
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Device *              m_device;
        ID3D11DeviceContext *       m_deviceContext;

    private:
        // // fullscreen pass stuff
        // vaAutoRMI<vaVertexShader>   m_fullscreenVS;
        // ID3D11Buffer *              m_fullscreenVB;
        // ID3D11Buffer *              m_fullscreenVBDynamic;

        vaAutoRMI<vaPixelShader>    m_copyResourcePS;

        ID3D11SamplerState *        m_shadowCmpSamplerState     = nullptr;

        bool                        m_imguiFrameStarted         = false;

        bool                        m_itemsStarted              = false;
        //vaRenderItem                m_lastRenderItem;
        vaSceneDrawContext *        m_lastSceneDrawContext      = nullptr;

    protected:
        explicit                    vaRenderDeviceContextDX11( const vaRenderingModuleParams & params );
        virtual                     ~vaRenderDeviceContextDX11( );

    public:
        static vaRenderDeviceContext * Create( vaRenderDevice & device, ID3D11DeviceContext * deviceContext );
        ID3D11DeviceContext *       GetDXContext( ) const                               { return m_deviceContext; }

    public:
        void                        SetStandardSamplers( );
        void                        UnsetStandardSamplers( );
        // virtual void                FullscreenPassDraw( vaPixelShader & pixelShader, vaBlendMode blendMode = vaBlendMode::Opaque );

        // void                        FullscreenPassDraw( ID3D11PixelShader * pixelShader, ID3D11BlendState * blendState = vaDirectXTools::GetBS_Opaque(), ID3D11DepthStencilState * depthStencilState = vaDirectXTools::GetDSS_DepthDisabled_NoDepthWrite(), UINT stencilRef = 0, float ZDistance = 0.0f );

        virtual void                CopySRVToRTV( shared_ptr<vaTexture> destination, shared_ptr<vaTexture> source ) override;

        virtual void                BeginItems( ) override;
        virtual vaDrawResultFlags   ExecuteItem( const vaRenderItem & renderItem ) override;
        virtual vaDrawResultFlags   ExecuteItem( const vaComputeItem & renderItem ) override;
        virtual void                EndItems( ) override;

        void                        CheckNoStatesSet( );

    private:
        void                        Initialize( ID3D11DeviceContext * deviceContext );
        void                        Destroy( );

        void                        UpdateSceneDrawContext( vaSceneDrawContext * sceneDrawContext );


    protected:
        virtual void               ImGuiCreate( ) override;
        virtual void               ImGuiDestroy( ) override;
        virtual void               ImGuiNewFrame( ) override;
        virtual void               ImGuiDraw( ) override;

    private:
        // vaRenderDeviceContext
        virtual void                UpdateViewport( );
        virtual void                UpdateRenderTargetsDepthStencilUAVs( );
    };

}