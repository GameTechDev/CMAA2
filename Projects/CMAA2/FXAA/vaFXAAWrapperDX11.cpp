///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Scene/vaSceneIncludes.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "Rendering/DirectX/vaTextureDX11.h"

#include "Rendering/DirectX/vaRenderBuffersDX11.h"

#include "vaFXAAWrapper.h"


namespace VertexAsylum
{

    class vaFXAAWrapperDX11 : public vaFXAAWrapper
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    private:

        ID3D11SamplerState *            m_LinearSampler                 = nullptr;

        Preset                          m_usedPreset                    = (Preset)-1;

        bool                            m_useOptionalLuma               = false;

        vaAutoRMI<vaPixelShader>        m_FXAAEffect_3_11_PS;


    protected:
        explicit vaFXAAWrapperDX11( const vaRenderingModuleParams & params );
        ~vaFXAAWrapperDX11( );

    private:
        virtual vaDrawResultFlags       Draw( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma ) override;
        virtual void                    CleanupTemporaryResources( ) override;

    private:
        bool                            UpdateResources( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma );
    };

}

using namespace VertexAsylum;

static const bool c_useTypedUAVStores = false;

vaFXAAWrapperDX11::vaFXAAWrapperDX11( const vaRenderingModuleParams & params ) 
    :   vaFXAAWrapper( params ), 
        m_FXAAEffect_3_11_PS( params )
{
    ID3D11Device * device = params.RenderDevice.SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice();
    HRESULT hr;
    {
        CD3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC( CD3D11_DEFAULT() );

        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        V( device->CreateSamplerState( &desc, &m_LinearSampler ) );
     }
}

vaFXAAWrapperDX11::~vaFXAAWrapperDX11( )
{
    CleanupTemporaryResources();

    SAFE_RELEASE( m_LinearSampler   );
}

void vaFXAAWrapperDX11::CleanupTemporaryResources( )
{
}

bool vaFXAAWrapperDX11::UpdateResources( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma )
{
    deviceContext; inputColor;

    bool useOptionalLuma = optionalInLuma != nullptr;

    if( m_usedPreset != m_settings.Preset || m_useOptionalLuma != useOptionalLuma )
    {
        m_usedPreset = m_settings.Preset;
        m_useOptionalLuma = useOptionalLuma;

        vector< pair< string, string > > shaderMacros;
        
        if( m_useOptionalLuma )
            shaderMacros.push_back( { "FXAA_LUMA_SEPARATE_R8", "1" } );

        switch( m_usedPreset )
        {
        case vaFXAAWrapper::PRESET_LOW:     shaderMacros.push_back( { "FXAA_QUALITY__PRESET", "10" } );  break;
        case vaFXAAWrapper::PRESET_MEDIUM:  shaderMacros.push_back( { "FXAA_QUALITY__PRESET", "12" } );  break;
        case vaFXAAWrapper::PRESET_HIGH:    shaderMacros.push_back( { "FXAA_QUALITY__PRESET", "25" } );  break;
        case vaFXAAWrapper::PRESET_ULTRA:   shaderMacros.push_back( { "FXAA_QUALITY__PRESET", "39" } );  break;
        default: assert( false ); 
        }

        m_FXAAEffect_3_11_PS->CreateShaderFromFile( L"FXAA/FXAAWrapper.hlsl", "ps_5_0", "FXAAEffectPS", shaderMacros );
    }
    return true;
}

vaDrawResultFlags vaFXAAWrapperDX11::Draw( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma )
{
    if( !UpdateResources( deviceContext, inputColor, optionalInLuma ) )
    { assert( false ); return vaDrawResultFlags::UnspecifiedError; }

    UpdateConstants( deviceContext, inputColor );

    vaRenderItem renderItem;

    deviceContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ConstantBuffers[0]       = m_constantsBuffer;
    renderItem.ShaderResourceViews[0]   = inputColor;
    renderItem.ShaderResourceViews[1]   = optionalInLuma;   // can be null, that's fine!
    renderItem.PixelShader = m_FXAAEffect_3_11_PS;

    ID3D11SamplerState * linearSampler = m_LinearSampler;

    renderItem.PreDrawHook = [linearSampler]( const vaRenderItem &, vaRenderDeviceContext & deviceContext )  
    { vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &deviceContext )->GetDXContext( )->PSSetSamplers( 0, 1, &linearSampler ); return true; };
    renderItem.PostDrawHook = []( const vaRenderItem &, vaRenderDeviceContext & deviceContext )  
    { ID3D11SamplerState * nullSampler = nullptr; vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &deviceContext )->GetDXContext( )->PSSetSamplers( 0, 1, &nullSampler ); };

    return deviceContext.ExecuteSingleItem( renderItem );
}

void RegisterFXAAWrapperDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaFXAAWrapper, vaFXAAWrapperDX11 );
}