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

#include "Rendering/DirectX/vaRenderGlobalsDX11.h"

#include "Rendering/DirectX/vaTextureDX11.h"

#include "Rendering/DirectX/vaRenderBuffersDX11.h"
#include "Rendering/DirectX/vaLightingDX11.h"

using namespace VertexAsylum;

vaRenderGlobalsDX11::vaRenderGlobalsDX11( const vaRenderingModuleParams & params ) : vaRenderGlobals( params )
{
}

vaRenderGlobalsDX11::~vaRenderGlobalsDX11()
{
}

void vaRenderGlobalsDX11::SetAPISceneGlobals( vaSceneDrawContext & drawContext )
{
//    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &drawContext.RenderDeviceContext )->GetDXContext( );

    UpdateShaderConstants( drawContext );

    m_constantsBuffer.GetBuffer()->SafeCast<vaConstantBufferDX11*>()->SetToAPISlot( drawContext.RenderDeviceContext, SHADERGLOBAL_CONSTANTSBUFFERSLOT );
}

void vaRenderGlobalsDX11::UnsetAPISceneGlobals( vaSceneDrawContext & drawContext )
{
//    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &drawContext.RenderDeviceContext )->GetDXContext( );

    m_constantsBuffer.GetBuffer()->SafeCast<vaConstantBufferDX11*>()->UnsetFromAPISlot( drawContext.RenderDeviceContext, SHADERGLOBAL_CONSTANTSBUFFERSLOT );
}

void vaRenderGlobalsDX11::UpdateDebugOutputFloats( vaSceneDrawContext & drawContext )
{
    // 1.) queue resource GPU->CPU copy
    vaTextureDX11 * src = vaSaferStaticCast< vaTextureDX11 * >( m_shaderDebugOutputGPUTextures[ (int)( GetRenderDevice().GetCurrentFrameIndex() % c_shaderDebugOutputSyncDelay ) ].get() );
    vaTextureDX11 * dst = vaSaferStaticCast< vaTextureDX11 * >( m_shaderDebugOutputCPUTextures[ (int)( GetRenderDevice().GetCurrentFrameIndex() % c_shaderDebugOutputSyncDelay ) ].get() );

    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &drawContext.RenderDeviceContext )->GetDXContext( );
    dx11Context->CopyResource( dst->GetResource(), src->GetResource() );

    // 2.) get data from oldest (next) CPU resource
    vaTextureDX11 * readTex = vaSaferStaticCast< vaTextureDX11 * >( m_shaderDebugOutputCPUTextures[(int)( (GetRenderDevice().GetCurrentFrameIndex() + 1) % c_shaderDebugOutputSyncDelay )].get( ) );
    ID3D11Texture1D * readTexDX11 = readTex->GetTexture1D();
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = dx11Context->Map( readTexDX11, 0, D3D11_MAP_READ, 0, &mapped );
    if( !FAILED(hr) )
    {
        memcpy( m_shaderDebugFloats, mapped.pData, sizeof(m_shaderDebugFloats) );
        dx11Context->Unmap( readTexDX11, 0 );
    }
    else
    {
        assert( false );
    }
}

void RegisterRenderGlobalsDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaRenderGlobals, vaRenderGlobalsDX11 );
}
