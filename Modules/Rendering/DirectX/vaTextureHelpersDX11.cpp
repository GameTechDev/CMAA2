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
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/vaTextureHelpers.h"

#include "vaTextureDX11.h"

#include "vaRenderingToolsDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Core/Misc/vaProfiler.h"

namespace VertexAsylum
{

    // OBSOLETE - scheduled for delete; use vaTexture::TryMap stuff instead
    class vaTextureCPU2GPUDX11 : public vaTextureCPU2GPU
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    protected:
        friend class vaTexture;
                                        vaTextureCPU2GPUDX11( const vaRenderingModuleParams & params );
        virtual                         ~vaTextureCPU2GPUDX11( )   ;

    public:
        // vaTextureCPU2GPU
        virtual void                    TickGPU( vaRenderDeviceContext & context );
    };

    // OBSOLETE - scheduled for delete; use vaTexture::TryMap stuff instead
    class vaTextureGPU2CPUDX11 : public vaTextureGPU2CPU
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    protected:
        friend class vaTexture;
                                        vaTextureGPU2CPUDX11( const vaRenderingModuleParams & params );
        virtual                         ~vaTextureGPU2CPUDX11( )   ;

    public:
        // vaTextureCPU2GPU
        virtual void                    TickGPU( vaRenderDeviceContext & context );
    };
}

using namespace VertexAsylum;

vaTextureCPU2GPUDX11::vaTextureCPU2GPUDX11( const vaRenderingModuleParams & params ) : vaTextureCPU2GPU( params )
{ 
    params; // unreferenced
    assert( false );     // OBSOLETE - scheduled for delete; use vaTexture::TryMap stuff instead
}

vaTextureCPU2GPUDX11::~vaTextureCPU2GPUDX11( )
{ 
}

void vaTextureCPU2GPUDX11::TickGPU( vaRenderDeviceContext & context  )
{
    if( m_GPUTexture == nullptr )
    {
        assert( false );
        return;
    }

//    VA_SCOPE_CPUGPU_TIMER( vaTextureCPU2GPUDX11_TickGPU, context );
    if( !m_uploadRequested )
        return;

    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );

    if( m_GPUTexture->GetType( ) == vaTextureType::Texture2D )
    {
        //ID3D11Texture2D * texture2D = m_GPUTexture->SafeCast<vaTextureDX11*>()->GetTexture2D();
        ID3D11Resource * resource = m_GPUTexture->SafeCast<vaTextureDX11*>()->GetResource();

        assert( m_CPUData.size() == m_GPUTexture->GetMipLevels() );
        for( int i = 0; i < m_GPUTexture->GetMipLevels(); i++ )
        {
            auto cpuSubresource = GetSubresource( i );
            assert( cpuSubresource != nullptr );            // not fatal, checked below but still unexpected, indicating a bug
            assert( cpuSubresource->Buffer != nullptr );    // not fatal, checked below but still unexpected, indicating a bug

            if( (cpuSubresource != nullptr) && (cpuSubresource->Buffer != nullptr) )
            {
                D3D11_BOX destBox;
                destBox.left    = 0;
                destBox.top     = 0;
                destBox.front   = 0;
                destBox.back    = 1;
                destBox.right   = cpuSubresource->SizeX;
                destBox.bottom  = cpuSubresource->SizeY;
//                VA_SCOPE_CPUGPU_TIMER( UpdateSubresource, context );
                dx11Context->UpdateSubresource( resource, i, /*&destBox*/NULL, cpuSubresource->Buffer, cpuSubresource->RowPitch, cpuSubresource->RowPitch * cpuSubresource->SizeY );

                /*
                D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                HRESULT hr = dx11Context->Map( resource, i, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource );

                if( SUCCEEDED( hr ) )
                {
                    if( mappedSubresource.RowPitch != cpuSubresource->RowPitch )
                    {
                        for( int y = 0; y < m_GPUTexture->GetSizeY(); y++ )
                        {
                            byte * srcData = cpuSubresource->Buffer + cpuSubresource->RowPitch * y;
                            byte * dstData = (byte *)(mappedSubresource.pData) + mappedSubresource.RowPitch * y;
                            memcpy( dstData, srcData, cpuSubresource->RowPitch );
                        }
                    }
                    else
                    {
                        assert( false ); // not tested
                        memcpy( mappedSubresource.pData, cpuSubresource->Buffer, cpuSubresource->SizeInBytes );
                    }

                    dx11Context->Unmap( resource, i );
                    cpuSubresource->Modified = false;
                }
                else
                {
                    // directx error, check HR
                    assert( false );
                }
                */
            }
        }

        m_uploadRequested = false;
    }
    //dx11Context->CopyResource( m_GPUTexture->SafeCast<vaTextureDX11*>()->GetResource(), m_CPUTexture->SafeCast<vaTextureDX11*>()->GetResource() );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


vaTextureGPU2CPUDX11::vaTextureGPU2CPUDX11( const vaRenderingModuleParams & params ) : vaTextureGPU2CPU( params )
{ 
    params; // unreferenced
    assert( false );     // OBSOLETE - scheduled for delete; use vaTexture::TryMap stuff instead
}

vaTextureGPU2CPUDX11::~vaTextureGPU2CPUDX11( )
{ 
}

void vaTextureGPU2CPUDX11::TickGPU( vaRenderDeviceContext & context  )
{
    if( (m_GPUTexture == nullptr) || (m_CPUTexture == nullptr) )
    {
        assert( false );
        return;
    }

    VA_SCOPE_CPUGPU_TIMER( vaTextureGPU2CPUDX11_TickGPU, context );

    m_ticksFromLastDownloadRequest++;

    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );

    if( m_directMappedMarkForUnmap )
    {
        assert( m_directMapped );
        assert( m_directMapMode );
        if( ( m_GPUTexture->GetType( ) == vaTextureType::Texture2D ) )
        {
            ID3D11Resource * cpuDX11Resource = m_CPUTexture->SafeCast<vaTextureDX11*>()->GetResource();

            for( int i = 0; i < m_GPUTexture->GetMipLevels(); i++ )
            {
                auto cpuSubresource = GetSubresource( i );
                assert( cpuSubresource != nullptr );            // not fatal, checked below but still unexpected, indicating a bug
                assert( cpuSubresource->Buffer != nullptr );    // not fatal, checked below but still unexpected, indicating a bug

                if( !((cpuSubresource != nullptr) && (cpuSubresource->Buffer != nullptr)) )
                    continue;

                assert( m_CPUDataDownloaded[i] );

                {
//                    VA_SCOPE_CPU_TIMER( unmap );
                    dx11Context->Unmap( cpuDX11Resource, i );
                }
                cpuSubresource->Buffer          = nullptr;
                cpuSubresource->RowPitch        = 0;
                cpuSubresource->DepthPitch      = 0; //mappedSubresource.DepthPitch;
                cpuSubresource->SizeInBytes     = 0;

                m_CPUDataDownloaded[i] = false;
            }
        }
        m_directMappedMarkForUnmap = false;
        m_directMapped = false;
        m_downloadFinished = false;

        assert( !m_downloadRequested );
        return;
    }

    if( m_downloadRequested && !m_downloadFinished )
    {
        bool stillWorkToDo = false;

        // first frame since requested?
        if( m_ticksFromLastDownloadRequest == 1 )
        {
            if( m_CPUTexture != m_GPUTexture )
                dx11Context->CopyResource( m_CPUTexture->SafeCast<vaTextureDX11*>()->GetResource(), m_GPUTexture->SafeCast<vaTextureDX11*>()->GetResource() );
        }

        bool skipThis = m_ticksFromLastDownloadRequest <= m_copyToMapDelayTicks;
        stillWorkToDo = skipThis;

        if( !skipThis && ( m_GPUTexture->GetType( ) == vaTextureType::Texture2D ) )
        {
            ID3D11Resource * cpuDX11Resource = m_CPUTexture->SafeCast<vaTextureDX11*>()->GetResource();

            assert( m_CPUData.size() == m_GPUTexture->GetMipLevels() );
            for( int i = 0; i < m_GPUTexture->GetMipLevels(); i++ )
            {
                auto cpuSubresource = GetSubresource( i );
                assert( cpuSubresource != nullptr );            // not fatal, checked below but still unexpected, indicating a bug
                if( cpuSubresource == nullptr )
                    continue;

                if( m_CPUDataDownloaded[i] )
                    continue;

//                VA_SCOPE_CPUGPU_TIMER( MapCopyUnmap, context );

                D3D11_MAPPED_SUBRESOURCE mappedSubresource; memset( &mappedSubresource, 0, sizeof(mappedSubresource) );
                HRESULT hr;
                {
                    VA_SCOPE_CPU_TIMER( map );
                    hr = dx11Context->Map( cpuDX11Resource, i, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedSubresource );
                }
                if( SUCCEEDED( hr ) )
                {
                    if( m_directMapMode )
                    {
                        cpuSubresource->Buffer          = (byte*)mappedSubresource.pData;
                        cpuSubresource->RowPitch        = mappedSubresource.RowPitch;
                        cpuSubresource->DepthPitch      = 0; //mappedSubresource.DepthPitch;
                        cpuSubresource->SizeInBytes     = cpuSubresource->SizeY * cpuSubresource->RowPitch;
                        //assert( cpuSubresource->DepthPitch == 0 );
                        assert( mappedSubresource.DepthPitch == cpuSubresource->SizeInBytes );
                    }
                    else
                    {
                        assert( cpuSubresource->Buffer != nullptr );    // not fatal, checked below but still unexpected, indicating a bug

                        VA_SCOPE_CPU_TIMER( copy );
                        if( mappedSubresource.RowPitch != (UINT)cpuSubresource->RowPitch )
                        {
                            for( int y = 0; y < cpuSubresource->SizeY; y++ )
                            {
                                byte * dstData = cpuSubresource->Buffer + cpuSubresource->RowPitch * y;
                                byte * srcData = (byte *)(mappedSubresource.pData) + mappedSubresource.RowPitch * y;
                                memcpy( dstData, srcData, cpuSubresource->RowPitch );
                            }
                        }
                        else
                        {
                            memcpy( cpuSubresource->Buffer, mappedSubresource.pData, cpuSubresource->SizeInBytes );
                        }

                        dx11Context->Unmap( cpuDX11Resource, i );
                    }
                    m_CPUDataDownloaded[i] = true;

                    continue;
                }
                else
                {
                    int dbg = 0;
                    dbg++;
                }
                stillWorkToDo = true;
            }
        }

        if( !stillWorkToDo )
        {
            for( int i = 0; i < m_GPUTexture->GetMipLevels( ); i++ )
            {
                assert( m_CPUDataDownloaded[i] );
            }

            m_downloadFinished = true;
            m_downloadRequested = false;

            if( m_directMapMode )
            {
                m_directMapped = true;
                assert( !m_directMappedMarkForUnmap );
            }
        }
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterTextureToolsDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaTextureCPU2GPU, vaTextureCPU2GPUDX11 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaTextureGPU2CPU, vaTextureGPU2CPUDX11 );
}