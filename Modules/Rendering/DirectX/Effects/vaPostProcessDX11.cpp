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

#include "Rendering/Effects/vaPostProcess.h"

#include "Rendering/DirectX/vaShaderDX11.h"
#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"
#include "Rendering/DirectX/vaRenderDeviceDX11.h"
#include "Rendering/DirectX/vaTextureDX11.h"

#include "Rendering/DirectX/vaRenderingToolsDX11.h"

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP) || (_WIN32_WINNT > _WIN32_WINNT_WIN8)
#include <wincodec.h>
#endif


#include "IntegratedExternals/DirectXTex/ScreenGrab/ScreenGrab.h"

namespace VertexAsylum
{

    class vaPostProcessDX11 : public vaPostProcess
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    protected:
        explicit vaPostProcessDX11( const vaRenderingModuleParams & params );
        ~vaPostProcessDX11( );

    public:
        virtual bool                            SaveTextureToDDSFile( vaRenderDeviceContext & apiContext, const wstring & path, vaTexture & texture );
        virtual bool                            SaveTextureToPNGFile( vaRenderDeviceContext & apiContext, const wstring & path, vaTexture & texture );
    };

}

using namespace VertexAsylum;

vaPostProcessDX11::vaPostProcessDX11( const vaRenderingModuleParams & params ) : vaPostProcess( params )
{
    params; // unreferenced
}

vaPostProcessDX11::~vaPostProcessDX11( )
{
}

bool vaPostProcessDX11::SaveTextureToDDSFile( vaRenderDeviceContext & apiContext, const wstring & path, vaTexture & texture )
{
    vaRenderDeviceContextDX11 * apiContextDX11 = apiContext.SafeCast<vaRenderDeviceContextDX11*>( );
    ID3D11DeviceContext * dx11Context = apiContextDX11->GetDXContext( );

    if( !SUCCEEDED( DirectX::SaveDDSTextureToFile( dx11Context, texture.SafeCast<vaTextureDX11*>( )->GetResource( ), path.c_str() ) ) )
    {
        VA_LOG_ERROR( L"vaPostProcessDX11::SaveTextureToDDSFile failed ('%s')!", path.c_str() );
        return false;
    }
    else
    {
        return true;
    }
}

bool vaPostProcessDX11::SaveTextureToPNGFile( vaRenderDeviceContext & apiContext, const wstring & path, vaTexture & texture )
{
    vaRenderDeviceContextDX11 * apiContextDX11 = apiContext.SafeCast<vaRenderDeviceContextDX11*>( );
    ID3D11DeviceContext * dx11Context = apiContextDX11->GetDXContext( );

    if( !SUCCEEDED( DirectX::SaveWICTextureToFile( dx11Context, texture.SafeCast<vaTextureDX11*>( )->GetResource( ), GUID_ContainerFormatPng, path.c_str( ) ) ) )
    {
        VA_LOG_ERROR( L"vaPostProcessDX11::SaveTextureToPNGFile failed ('%s')!", path.c_str() );
        return false;
    }
    else
    {
        return true;
    }
}


void RegisterPostProcessDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaPostProcess, vaPostProcessDX11 );
}
