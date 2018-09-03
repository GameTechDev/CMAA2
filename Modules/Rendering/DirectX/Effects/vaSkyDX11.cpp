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

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaShaderDX11.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/Effects/vaSky.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/Shaders/vaSharedTypes.h"

namespace VertexAsylum
{

    class vaSkyDX11 : public vaSky
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    private:

        ID3D11DepthStencilState *           m_depthStencilState;

    protected:
        vaSkyDX11( const vaRenderingModuleParams & params );
        ~vaSkyDX11( );

    private:
        // RenderingObject
        virtual void                    Draw( vaSceneDrawContext & drawContext );
    };

}

using namespace VertexAsylum;


vaSkyDX11::vaSkyDX11( const vaRenderingModuleParams & params ) : vaSky( params )
{
    params; // unreferenced

    m_depthStencilState = NULL;

    ID3D11Device * device = params.RenderDevice.SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice();

    HRESULT hr;
    // Create depth stencil
    {
        CD3D11_DEPTH_STENCIL_DESC desc = CD3D11_DEPTH_STENCIL_DESC( CD3D11_DEFAULT( ) );
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        V( device->CreateDepthStencilState( &desc, &m_depthStencilState ) );
    }
}

vaSkyDX11::~vaSkyDX11( )
{
    SAFE_RELEASE( m_depthStencilState );
}

void vaSkyDX11::Draw( vaSceneDrawContext & drawContext )
{
    drawContext;

    // needs to be ported to vaRenderItem-based approach, same as vaSkybox
    assert( false );

    /*
    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &drawContext.APIContext )->GetDXContext( );

    {
        const vaSky::Settings & settings = GetSettings( );
        SimpleSkyConstants consts;

        vaMatrix4x4 view = drawContext.Camera.GetViewMatrix( );
        vaMatrix4x4 proj = drawContext.Camera.GetProjMatrix( );

        view.r3 = vaVector4( 0.0f, 0.0f, 0.0f, 1.0f );

        vaMatrix4x4 viewProj = view * proj;

        consts.ProjToWorld = viewProj.Inverse( );
        consts.SunDir = vaVector4( GetSunDir( ), 1.0f );

        consts.SkyColorLowPow = settings.SkyColorLowPow;
        consts.SkyColorLowMul = settings.SkyColorLowMul;
        consts.SkyColorLow = settings.SkyColorLow;
        consts.SkyColorHigh = settings.SkyColorHigh;
        consts.SunColorPrimary = settings.SunColorPrimary;
        consts.SunColorSecondary = settings.SunColorSecondary;
        consts.SunColorPrimaryPow = settings.SunColorPrimaryPow;
        consts.SunColorPrimaryMul = settings.SunColorPrimaryMul;
        consts.SunColorSecondaryPow = settings.SunColorSecondaryPow;
        consts.SunColorSecondaryMul = settings.SunColorSecondaryMul;

        m_constantsBuffer.Update( drawContext.APIContext, consts );
    }

    // make sure we're not overwriting anything else, and set our constant buffers
    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, (ID3D11Buffer*)NULL, SKY_CONSTANTS_BUFFERSLOT );
    m_constantsBuffer.SetToAPISlot( drawContext.APIContext, SKY_CONSTANTS_BUFFERSLOT );

    if( drawContext.Camera.GetUseReversedZ() )
        m_screenTriangleVertexBufferReversedZ.SetToAPI( drawContext.APIContext, 0, 0 );
    else
        m_screenTriangleVertexBuffer.SetToAPI( drawContext.APIContext, 0, 0 );

    dx11Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );


    m_vertexShader->SetToAPI( drawContext.APIContext );
    m_pixelShader->SetToAPI( drawContext.APIContext );

    dx11Context->RSSetState( NULL );
    dx11Context->OMSetDepthStencilState( (drawContext.Camera.GetUseReversedZ())?( vaDirectXTools::GetDSS_DepthEnabledGE_NoDepthWrite( ) ):( vaDirectXTools::GetDSS_DepthEnabledLE_NoDepthWrite( ) ), 0 );
    float blendFactor[4] = { 0, 0, 0, 0 };
    dx11Context->OMSetBlendState( NULL, blendFactor, 0xFFFFFFFF );

    dx11Context->Draw( 3, 0 );

    dx11Context->PSSetShader( nullptr, nullptr, 0 );

    // make sure nothing messed with our constant buffers and nothing uses them after
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11Buffer*)NULL, SKY_CONSTANTS_BUFFERSLOT );
    m_constantsBuffer.UnsetFromAPISlot( drawContext.APIContext, SKY_CONSTANTS_BUFFERSLOT );
    m_vertexShader->UnsetFromAPI( drawContext.APIContext );
    m_pixelShader->UnsetFromAPI( drawContext.APIContext );
    */
}

void RegisterSkyDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSky, vaSkyDX11 );
}