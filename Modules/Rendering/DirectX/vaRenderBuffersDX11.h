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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaResourceFormatsDX11.h"

#include "Rendering/vaRenderingIncludes.h"


namespace VertexAsylum
{
    class vaConstantBufferDX11 : public vaConstantBuffer, public vaShaderResourceSRVDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaConstantBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaConstantBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int bufferSize, const void * initialData ) override;
        virtual void                        Destroy( ) override;

    public:
        void                                SetToAPISlot( vaRenderDeviceContext & apiContext, int slot, bool assertOnOverwrite = true );
        void                                UnsetFromAPISlot( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet = true );

    public:
        void                                Update( ID3D11DeviceContext * dx11Context, const void * data, uint32 dataSize );

    public:
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return nullptr; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return nullptr; }
    };


    class vaIndexBufferDX11 : public vaIndexBuffer, public vaShaderResourceSRVDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaIndexBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaIndexBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int indexCount, const void * initialData ) override;
        virtual void                        Destroy( ) override;

    public:
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return nullptr; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return nullptr; }
    };


    class vaVertexBufferDX11 : public vaVertexBuffer, public vaShaderResourceSRVDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaVertexBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaVertexBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int vertexCount, int vertexSize, const void * initialData, bool dynamic ) override;
        virtual void                        Destroy( ) override;

        virtual bool                        TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait );
        virtual void                        Unmap( vaRenderDeviceContext & apiContext );

    public:
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return nullptr; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return nullptr; }

    };
    
    class vaStructuredBufferDX11 : public vaStructuredBuffer, public vaShaderResourceSRVDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;
        ID3D11UnorderedAccessView *         m_bufferUAV = nullptr;
        ID3D11ShaderResourceView *          m_bufferSRV = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaStructuredBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaStructuredBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize ) override;
        virtual bool                        Create( int elementCount, int structureByteSize, bool hasCounter, const void * initialData ) override;
        virtual void                        Destroy( ) override;

    public:
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return m_bufferUAV; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return m_bufferSRV; }

    };

}