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

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/vaRenderingIncludes.h"

namespace VertexAsylum
{
    struct vaShaderCacheKey11;
    class vaShaderCacheEntry11;

    class vaShaderDX11 : public virtual vaShader
    {
    protected:
        IUnknown *                      m_shader;

    public:
        vaShaderDX11( );
        virtual ~vaShaderDX11( );
        //
        virtual void                    Clear( ) override;
        //
        virtual bool                    IsCreated( ) override { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); return allShaderDataLock.owns_lock() && m_shader != nullptr; }
        //
    protected:
        virtual void                    CreateCacheKey( vaShaderCacheKey11 & outKey );
        //
    protected:
        //
        virtual void                    DestroyShader( ) override { DestroyShaderBase( ); }
        void                            DestroyShaderBase( );
        //
        ID3DBlob *                      CreateShaderBase( bool & loadedFromCache );
    };

#pragma warning ( push )
#pragma warning ( disable : 4250 )

    class vaPixelShaderDX11 : public vaShaderDX11, public vaPixelShader
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    public:
        vaPixelShaderDX11( const vaRenderingModuleParams & params ) : vaShader( params ) { }
        virtual ~vaPixelShaderDX11( ) {}

    public:
        ID3D11PixelShader *         GetShader( );

        operator ID3D11PixelShader * ( ) { return GetShader( ); }

        // virtual void                SetToD3DContext( ID3D11DeviceContext * context )
        // {
        //     context->PSSetShader( GetShader( ), NULL, 0 );
        // }

//        virtual void                SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )  override;
//        virtual void                UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true ) override;


    protected:
        virtual void                CreateShader( ) override;
    };

    class vaComputeShaderDX11 : public vaShaderDX11, public vaComputeShader
    {
    public:
        vaComputeShaderDX11( const vaRenderingModuleParams & params ) : vaShader( params ) { }
        virtual ~vaComputeShaderDX11( ) {}

    public:
        ID3D11ComputeShader *       GetShader( );

        operator ID3D11ComputeShader * ( ) { return GetShader( ); }

//        virtual void                SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )  override;
//        virtual void                UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true ) override;
//        virtual void                Dispatch( vaRenderDeviceContext & renderContext, uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ ) override;

    protected:
        virtual void                CreateShader( ) override;
    };

    class vaHullShaderDX11 : public vaShaderDX11, public vaHullShader
    {
    public:
        vaHullShaderDX11( const vaRenderingModuleParams & params ) : vaShader( params ) { }
        virtual ~vaHullShaderDX11( ) {}

    public:
        ID3D11HullShader *         GetShader( );

        operator ID3D11HullShader * ( ) { return GetShader( ); }

        // virtual void               SetToD3DContext( ID3D11DeviceContext * context )
        // {
        //     context->HSSetShader( GetShader( ), NULL, 0 );
        // }

//        virtual void                SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )  override;
//        virtual void                UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true ) override;

    protected:
        virtual void               CreateShader( ) override;

    };

    class vaDomainShaderDX11 : public vaShaderDX11, public vaDomainShader
    {
    public:
        vaDomainShaderDX11( const vaRenderingModuleParams & params ) : vaShader( params ) { }
        virtual ~vaDomainShaderDX11( ) {}

    public:
        ID3D11DomainShader *       GetShader( );

        operator ID3D11DomainShader * ( ) { return GetShader( ); }

        // virtual void               SetToD3DContext( ID3D11DeviceContext * context )
        // {
        //     context->DSSetShader( GetShader( ), NULL, 0 );
        // }

//        virtual void                SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )  override;
//        virtual void                UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true ) override;

    protected:
        virtual void               CreateShader( ) override;

    };

    class vaGeometryShaderDX11 : public vaShaderDX11, public vaGeometryShader
    {
    public:
        vaGeometryShaderDX11( const vaRenderingModuleParams & params ) : vaShader( params ) { }
        virtual ~vaGeometryShaderDX11( ) {}

    public:
        ID3D11GeometryShader *       GetShader( );

        operator ID3D11GeometryShader * ( ) { return GetShader( ); }

//        virtual void                SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )  override;
//        virtual void                UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true ) override;

    protected:
        virtual void               CreateShader( ) override;

    };

    class vaVertexShaderDX11 : public vaShaderDX11, public vaVertexShader
    {
    protected:
        ID3D11InputLayout *         m_inputLayoutDX;

        void                        SetInputLayout( D3D11_INPUT_ELEMENT_DESC * elements, uint32 elementCount );

    public:
        vaVertexShaderDX11( const vaRenderingModuleParams & params );
        virtual ~vaVertexShaderDX11( );

    public:
        ID3D11VertexShader *        GetShader( );
        ID3D11InputLayout *         GetInputLayout( );
        //
        operator ID3D11VertexShader * ( ) { return GetShader( ); }
        //
        virtual void                CreateShaderAndILFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile ) override;
        virtual void                CreateShaderAndILFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile ) override;

        // doesn't work with current CreateShaderAndILFrom... due to clearing the inputlayout - need to fix this; not clearing doesn't cause anything bad, just uses more memory
        // virtual void                Clear( ) override { m_inputLayout.Clear(); vaShaderDX11::Clear(); }

        // platform independent <-> dx conversions
        static vaVertexInputLayoutDesc  LayoutVAFromDX( D3D11_INPUT_ELEMENT_DESC * elements, uint32 elementCount );

        // !!warning!! returned char pointers in descriptors outLayout point to inLayout values so make sure you keep inLayout alive until outLayout is alive
        static void                 LayoutDXFromVA( vector<D3D11_INPUT_ELEMENT_DESC> & outLayout, const vaVertexInputLayoutDesc & inLayout );

    public:

        // virtual void                SetToD3DContext( ID3D11DeviceContext * context )
        // {
        //     if( m_inputLayoutDX != NULL )
        //         context->IASetInputLayout( GetInputLayout( ) );
        //     context->VSSetShader( GetShader( ), NULL, 0 );
        // }

//        virtual void                SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )  override;
//        virtual void                UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true ) override;

    protected:
        virtual void                CreateShader( ) override;
        virtual void                DestroyShader( ) override;

        virtual void                CreateCacheKey( vaShaderCacheKey11 & outKey );
    };


#pragma warning ( pop )

    struct vaShaderCacheKey11
    {
        std::string                StringPart;

        bool                       operator == ( const vaShaderCacheKey11 & cmp ) const { return this->StringPart == cmp.StringPart; }
        bool                       operator < ( const vaShaderCacheKey11 & cmp ) const { return this->StringPart < cmp.StringPart; }
        bool                       operator >( const vaShaderCacheKey11 & cmp ) const { return this->StringPart > cmp.StringPart; }

        void                       Save( vaStream & outStream ) const;
        void                       Load( vaStream & inStream );
    };

    class vaShaderCacheEntry11
    {
    public:
        struct FileDependencyInfo
        {
            std::wstring            FilePath;
            int64                   ModifiedTimeDate;

            FileDependencyInfo( ) : FilePath( L"" ), ModifiedTimeDate( 0 ) { }
            FileDependencyInfo( const wstring & filePath );
            FileDependencyInfo( const wstring & filePath, int64 modifiedTimeDate );
            FileDependencyInfo( vaStream & inStream ) { Load( inStream ); }

            bool                    IsModified( );

            void                    Save( vaStream & outStream ) const;
            void                    Load( vaStream & inStream );
        };

    private:

        ID3DBlob *                                               m_compiledShader;
        std::vector<vaShaderCacheEntry11::FileDependencyInfo>      m_dependencies;

    private:
        vaShaderCacheEntry11( ) {}
        explicit vaShaderCacheEntry11( const vaShaderCacheEntry11 & copy ) { copy; }
    public:
        vaShaderCacheEntry11( ID3DBlob * compiledShader, std::vector<vaShaderCacheEntry11::FileDependencyInfo> & dependencies );
        explicit vaShaderCacheEntry11( vaStream & inFile ) { m_compiledShader = NULL; Load( inFile ); }
        ~vaShaderCacheEntry11( );

        bool                       IsModified( );
        ID3DBlob *                 GetCompiledShader( ) { m_compiledShader->AddRef( ); return m_compiledShader; }

        void                       Save( vaStream & outStream ) const;
        void                       Load( vaStream & inStream );
    };

    // Singleton utility class for handling shaders
    class vaDirectX11ShaderManager : public vaShaderManager, public vaSingletonBase < vaDirectX11ShaderManager > // oooo I feel so dirty here but oh well
    {
    private:
        friend class vaShaderSharedManager;
        friend class vaMaterialManager;
        friend class vaShaderDX11;

    private:
        std::map<vaShaderCacheKey11, vaShaderCacheEntry11 *>    m_cache;
        mutable mutex                                       m_cacheMutex;
#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
        wstring                                             m_cacheFilePath;

        // since cache loading from the disk happens only once, use this to ensure loading proc has grabbed m_cacheMutex - once it's unlocked we know it finished
        std::atomic_bool                                    m_cacheLoadStarted = false;
        std::mutex                                          m_cacheLoadStartedMutex;
        std::condition_variable                             m_cacheLoadStartedCV;
#endif

        shared_ptr<int>                                     m_objLifetimeToken;

    public:
        vaDirectX11ShaderManager( vaRenderDevice & device );
        ~vaDirectX11ShaderManager( );

    public:
        ID3DBlob *          FindInCache( vaShaderCacheKey11 & key, bool & foundButModified );
        void                AddToCache( vaShaderCacheKey11 & key, ID3DBlob * shaderBlob, std::vector<vaShaderCacheEntry11::FileDependencyInfo> & dependencies );
        void                ClearCache( );

        // pushBack (searched last) or pushFront (searched first)
        virtual void        RegisterShaderSearchPath( const std::wstring & path, bool pushBack = true )     override;

        virtual wstring     FindShaderFile( const wstring & fileName )                                      override;

    private:
        void                ClearCacheInternal( );          // will not lock m_cacheMutex - has to be done before calling!!
        void                LoadCacheInternal( );           // will not lock m_cacheMutex - has to be done before calling!!
        void                SaveCacheInternal( ) const;     // will not lock m_cacheMutex - has to be done before calling!!
    };

}