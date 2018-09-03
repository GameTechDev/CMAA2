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

// TODO: this can all go into platform independent code now

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaShaderDX11.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"
#include "Rendering/DirectX/vaRenderBuffersDX11.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "Rendering/DirectX/vaTextureDX11.h"

#include "vaZoomTool.h"

namespace VertexAsylum
{
    // TODO: this can all go into platform independent code now

    class vaZoomToolDX11 : public vaZoomTool
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    private:

    protected:
        vaZoomToolDX11( const vaRenderingModuleParams & params );
        ~vaZoomToolDX11( );

    private:
        // RenderingObject
//        virtual void                    Draw( vaRenderDeviceContext & apiContext, vaTexture & colorInOut );

    private:
    };

}

using namespace VertexAsylum;


vaZoomToolDX11::vaZoomToolDX11( const vaRenderingModuleParams & params ) : vaZoomTool( params )
{
}

vaZoomToolDX11::~vaZoomToolDX11( )
{
}

//void vaZoomToolDX11::Draw( vaRenderDeviceContext & apiContext, vaTexture & colorInOut )
//{
//    // TODO: this can all go into platform independent code now (using vaComputeItem)
//
//    if( !m_settings.Enabled )
//        return;
//
//    vaRenderDeviceContext::RenderOutputsState rtState = apiContext.GetOutputs( );
//
//    apiContext.SetRenderTarget( nullptr, nullptr, false );
//
//    UpdateConstants( apiContext );
//
//    // TODO: switch to vaRenderItem path
//
//    m_constantsBuffer.GetBuffer()->SafeCast<vaConstantBufferDX11*>()->SetToAPISlot( apiContext, ZOOMTOOL_CONSTANTS_BUFFERSLOT );
//
//    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &apiContext )->GetDXContext( );
//
//    ID3D11UnorderedAccessView * UAVs[]                      = { colorInOut.SafeCast<vaTextureDX11*>( )->GetUAV( ) };
//    ID3D11UnorderedAccessView * nullUAVs[_countof( UAVs )]  = { nullptr };
//    UINT UAVInitialCounts[_countof( UAVs )]                 = { (UINT)-1 };
//
//    dx11Context->CSSetUnorderedAccessViews( 0, _countof(UAVInitialCounts), UAVs, UAVInitialCounts );
//
//    int threadGroupCountX = ( colorInOut.GetSizeX( ) + 16 - 1 ) / 16;
//    int threadGroupCountY = ( colorInOut.GetSizeY( ) + 16 - 1 ) / 16;
//
//    shared_ptr<vaComputeShader> zoomShader = ( vaResourceFormatHelpers::IsFloat( colorInOut.GetUAVFormat() ) ) ? ( m_CSZoomToolFloat.get() ) : ( m_CSZoomToolUnorm.get() );
//    {
//        VA_SCOPE_CPUGPU_TIMER( ZoomToolDraw, apiContext );
//        zoomShader->Dispatch( apiContext, threadGroupCountX, threadGroupCountY, 1 );
//    }
//
//    // Reset API states
//    dx11Context->CSSetUnorderedAccessViews( 0, _countof(nullUAVs), nullUAVs, UAVInitialCounts );
//
//    // restore previous RTs
//    apiContext.SetOutputs( rtState );
//
//    m_constantsBuffer.GetBuffer()->SafeCast<vaConstantBufferDX11*>()->UnsetFromAPISlot( apiContext, ZOOMTOOL_CONSTANTS_BUFFERSLOT );
//}

void RegisterZoomToolDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaZoomTool, vaZoomToolDX11 );
}