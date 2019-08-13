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
//
// !!!!!!!!!!! TODO REMINDER !!!!!!!!!!! 
// At the moment this is a copy-pasted and modified version of vaShaderDX11.cpp; many of the things here could be
// shared and unified under just vaShaderDX - probably the whole shader cache code (still keep 2 separate caches but
// code can be identical) and maybe some other stuff
// !!!!!!!!!!! TODO REMINDER !!!!!!!!!!! 

#include "vaDirectXTools.h"

#include <assert.h>

#include "sys/stat.h"

#include <D3DCompiler.h>

#include <sstream>

#include <inttypes.h>

#include "vaShaderDX12.h"

#include "Rendering/DirectX/vaRenderDeviceDX12.h"

// TODO: replace with vaXXHash
// #include "Core/Misc/vaCRC64.h"

#include "Core/System/vaFileTools.h"



using namespace VertexAsylum;

#ifdef NDEBUG
#define           STILL_LOAD_FROM_CACHE_IF_ORIGINAL_FILE_MISSING
#endif

using namespace std;

namespace VertexAsylum
{

    const D3D_SHADER_MACRO c_builtInMacros[] = { { "VA_COMPILED_AS_SHADER_CODE", "1" }, { "VA_DIRECTX", "12" }, { 0, 0 } };

    class vaShaderIncludeHelper12 : public ID3DInclude
    {
        std::vector<vaShaderCacheEntry12::FileDependencyInfo> &     m_dependenciesCollector;

        std::vector< std::pair<string, string> >                    m_foundNamePairs;

        wstring                                                     m_relativePath;

        string                                                      m_macrosAsIncludeFile;

        vaShaderIncludeHelper12( const vaShaderIncludeHelper12 & c ) = delete; //: m_dependenciesCollector( c.m_dependenciesCollector ), m_relativePath( c.m_relativePath ), m_macros( c.m_macros )
        //{
        //    assert( false );
        //}    // to prevent warnings (and also this object doesn't support copying/assignment)
        void operator = ( const vaShaderIncludeHelper12 & ) = delete;
        //{
        //    assert( false );
        //}    // to prevent warnings (and also this object doesn't support copying/assignment)

    public:
        vaShaderIncludeHelper12( std::vector<vaShaderCacheEntry12::FileDependencyInfo> & dependenciesCollector, const wstring & relativePath, const string & macrosAsIncludeFile ) : m_dependenciesCollector( dependenciesCollector ), m_relativePath( relativePath ), m_macrosAsIncludeFile( macrosAsIncludeFile )
        {
        }

        STDMETHOD( Open )( D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes )
        {
            IncludeType; // unreferenced
            pParentData; // unreferenced

            // special case to handle macros - no need to add dependencies here, macros are CRC-ed separately
            if( pFileName == string( "MagicMacrosMagicFile.h" ) || pFileName == string( "magicmacrosmagicfile.h" ) )
            {
                int length = (int)m_macrosAsIncludeFile.size( );
                void * pData = new char[length];
                memcpy( pData, m_macrosAsIncludeFile.c_str( ), length );
                *ppData = pData;
                *pBytes = length;
                return S_OK;
            }

            vaShaderCacheEntry12::FileDependencyInfo fileDependencyInfo;
            std::shared_ptr<vaMemoryStream>        memBuffer;

            wstring fileNameR = m_relativePath + vaStringTools::SimpleWiden( string( pFileName ) );
            wstring fileNameA = vaStringTools::SimpleWiden( string( pFileName ) );

            // First try the file system...
            wstring fullFileName = vaDirectX12ShaderManager::GetInstance( ).FindShaderFile( fileNameR.c_str( ) );
            if( fullFileName == L"" )
                fullFileName = vaDirectX12ShaderManager::GetInstance( ).FindShaderFile( fileNameA.c_str( ) );
            if( fullFileName != L"" )
            {
                fileDependencyInfo = vaShaderCacheEntry12::FileDependencyInfo( fullFileName.c_str( ) );
                memBuffer = vaFileTools::LoadFileToMemoryStream( fullFileName.c_str( ) );
            }
            else
            {
                // ...then try embedded storage
                vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + fileNameR );
                if( !embeddedData.HasContents( ) )
                    embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + fileNameA );

                if( !embeddedData.HasContents( ) )
                {
                    VA_ERROR( L"Error trying to find shader file '%s' / '%s'!", fileNameR.c_str( ), fileNameA.c_str( ) );
                    return E_FAIL;
                }
                fileDependencyInfo = vaShaderCacheEntry12::FileDependencyInfo( embeddedData.Name.c_str(), embeddedData.TimeStamp );
                memBuffer = embeddedData.MemStream;
            }

            m_dependenciesCollector.push_back( fileDependencyInfo );
            m_foundNamePairs.push_back( std::make_pair( pFileName, vaStringTools::SimpleNarrow(fullFileName) ) );

            int length = (int)memBuffer->GetLength( );
            void * pData = new char[length];
            memcpy( pData, memBuffer->GetBuffer( ), length );
            *ppData = pData;
            *pBytes = length;
            return S_OK;
        }
        STDMETHOD( Close )( LPCVOID pData )
        {
            delete[] pData;
            return S_OK;
        }
        const std::vector< std::pair<string, string> > & GetFoundNameParis( )       { return m_foundNamePairs; }
    };

    // this is used to make the error reporting report to correct file (or embedded storage)
    static string CorrectErrorIfNotFullPath12( const string & errorText, vaShaderIncludeHelper12 & includeHelper )
    {
        string ret;

        string codeStr( errorText );

        stringstream ss( codeStr );
        string line;
        //int lineNum = 1;
        while( std::getline( ss, line ) )
        {
            size_t fileSeparator = line.find( ':' );
            if( fileSeparator != string::npos )
            {
                string filePlusLinePart = line.substr( 0, fileSeparator );
                string errorPart = line.substr( fileSeparator );

                size_t lineInfoSeparator = filePlusLinePart.find( '(' );
                if( lineInfoSeparator != string::npos )
                {
                    string filePart = filePlusLinePart.substr( 0, lineInfoSeparator );
                    string lineInfoPart = filePlusLinePart.substr( lineInfoSeparator );

                    wstring fileName = vaStringTools::SimpleWiden( filePart );

                    for( auto & fnp : includeHelper.GetFoundNameParis() )
                    {
                        if( vaStringTools::CompareNoCase( filePart, fnp.first ) == 0 )
                        {
                            filePart = fnp.second;
                            break;
                        }
                    }

                    line = filePart + lineInfoPart + errorPart;
                }
            }
            ret += line + "\n";
        }
        return ret;
    }

    vaShaderDX12::vaShaderDX12( const vaRenderingModuleParams & params ) : vaShader( params )
    {
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now

#ifdef VA_HOLD_SHADER_DISASM
        m_disasmAutoDumpToFile = false;
#endif
    }
    //
    vaShaderDX12::~vaShaderDX12( )
    {
        assert( vaDirectX12ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex );
        DestroyShaderBase( );
    }
    //
    void vaShaderDX12::Clear( )
    {
        assert( vaThreading::IsMainThread() );  // creation/cleaning only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
        m_state             = vaShader::State::Empty;
        m_uniqueContentsID  = -1;
        DestroyShader( );
        assert( m_shaderBlob == nullptr );
        m_entryPoint        = "";
        m_shaderFilePath    = L"";
        m_shaderCode        = "";
        m_shaderModel       = "";
#ifdef VA_HOLD_SHADER_DISASM
        m_disasm            = "";
#endif
    }

    void vaShaderDX12::DestroyShaderBase( )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        m_lastLoadedFromCache = false;
        m_shaderBlob.Reset();
        if( m_state != State::Empty )
        {
            m_state             = State::Uncooked;
            m_uniqueContentsID  = -1;
        }
    }
    //
    void vaShaderDX12::CreateCacheKey( vaShaderCacheKey12 & outKey )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        // supposed to be empty!
        assert( outKey.StringPart.size( ) == 0 );
        outKey.StringPart.clear( );

        outKey.StringPart += vaStringTools::Format( "%d", (int)m_macros.size( ) ) + " ";
        for( uint32 i = 0; i < m_macros.size( ); i++ )
        {
            outKey.StringPart += m_macros[i].first + " ";
            outKey.StringPart += m_macros[i].second + " ";
        }
        outKey.StringPart += m_shaderModel + " ";
        outKey.StringPart += m_entryPoint + " ";
        outKey.StringPart += vaStringTools::ToLower( vaStringTools::SimpleNarrow( m_shaderFilePath ) ) + " ";
    }
    //
    void vaVertexShaderDX12::CreateCacheKey( vaShaderCacheKey12 & outKey )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        vaShaderDX12::CreateCacheKey( outKey );
        
        outKey.StringPart += m_inputLayout.GetHashString();
    }
    //
    static HRESULT CompileShaderFromFile( const wchar_t* szFileName, const string & macrosAsIncludeFile, LPCSTR szEntryPoint,
        LPCSTR szShaderModel, ID3DBlob** ppBlobOut, vector<vaShaderCacheEntry12::FileDependencyInfo> & outDependencies, HWND hwnd )
    {
        HRESULT hr = S_OK;


        DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
//#if defined( DEBUG ) || defined( _DEBUG )
        // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
        // Setting this flag improves the shader debugging experience, but still allows 
        // the shaders to be optimized and to run exactly the way they will run in 
        // the release configuration of this program.
        dwShaderFlags |= D3DCOMPILE_DEBUG;
//#endif
        dwShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

        if( vaDirectX12ShaderManager::GetInstance( ).Settings( ).WarningsAreErrors )
            dwShaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;

        // find the file
        wstring fullFileName = vaDirectX12ShaderManager::GetInstance( ).FindShaderFile( szFileName );

        if( fullFileName != L"" )
        {
            bool attemptReload;

            do
            {
                outDependencies.clear( );
                outDependencies.push_back( vaShaderCacheEntry12::FileDependencyInfo( szFileName ) );

                wstring relativePath;
                vaFileTools::SplitPath( szFileName, &relativePath, nullptr, nullptr );

                vaShaderIncludeHelper12 includeHelper( outDependencies, relativePath, macrosAsIncludeFile );

                attemptReload = false;
                ID3DBlob* pErrorBlob;
                hr = D3DCompileFromFile( fullFileName.c_str( ), c_builtInMacros, (ID3DInclude*)&includeHelper,
                    szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
                if( FAILED( hr ) )
                {
                    if( pErrorBlob != nullptr )
                    {
                        wstring absFileName = vaFileTools::GetAbsolutePath( fullFileName );

                        OutputDebugStringA( vaStringTools::Format( "\nError while compiling '%s' shader, SM: '%s', EntryPoint: '%s' :\n", vaStringTools::SimpleNarrow( absFileName ).c_str( ), szShaderModel, szEntryPoint ).c_str( ) );
                        OutputDebugStringA( CorrectErrorIfNotFullPath12( (char*)pErrorBlob->GetBufferPointer( ), includeHelper ).c_str( ) );
#if 1 || defined( _DEBUG ) // display always for now
                        wstring errorMsg = vaStringTools::SimpleWiden( (char*)pErrorBlob->GetBufferPointer( ) );
                        wstring shmStr = vaStringTools::SimpleWiden( szShaderModel );
                        wstring entryStr = vaStringTools::SimpleWiden( szEntryPoint );
                        wstring fullMessage = vaStringTools::Format( L"Error while compiling '%s' shader, SM: '%s', EntryPoint: '%s'."
                            L"\n\n---\n%s\n---\nAttempt reload?",
                            fullFileName.c_str( ), shmStr.c_str( ), entryStr.c_str( ), errorMsg.c_str( ) );
                        if( MessageBox( hwnd, fullMessage.c_str( ), L"Shader compile error", MB_YESNO | MB_SETFOREGROUND ) == IDYES )
                            attemptReload = true;
#endif
                    }
                    SAFE_RELEASE( pErrorBlob );
                    if( !attemptReload )
                        return hr;
                }
                SAFE_RELEASE( pErrorBlob );
            } while( attemptReload );
        }
        else
        {
            // ...then try embedded storage
            vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + wstring( szFileName ) );

            if( !embeddedData.HasContents( ) )
            {
                VA_ERROR( L"Error trying to find shader file '%s'!", szFileName );
                return E_FAIL;
            }

            vaShaderCacheEntry12::FileDependencyInfo fileDependencyInfo = vaShaderCacheEntry12::FileDependencyInfo( szFileName, embeddedData.TimeStamp );

            outDependencies.clear( );
            outDependencies.push_back( fileDependencyInfo );

            wstring relativePath;
            vaFileTools::SplitPath( szFileName, &relativePath, nullptr, nullptr );

            vaShaderIncludeHelper12 includeHelper( outDependencies, relativePath, macrosAsIncludeFile );

            ID3DBlob* pErrorBlob;
            string ansiName = vaStringTools::SimpleNarrow( embeddedData.Name );
            hr = D3DCompile( (LPCSTR)embeddedData.MemStream->GetBuffer( ), (SIZE_T)embeddedData.MemStream->GetLength( ), ansiName.c_str( ), c_builtInMacros, (ID3DInclude*)&includeHelper,
                szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
            if( FAILED( hr ) )
            {
                if( pErrorBlob != nullptr )
                {
                    OutputDebugStringA( CorrectErrorIfNotFullPath12( (char*)pErrorBlob->GetBufferPointer( ), includeHelper ).c_str( ) );
#if 1 || defined( _DEBUG ) // display always for now
                    wstring errorMsg = vaStringTools::SimpleWiden( (char*)pErrorBlob->GetBufferPointer( ) );
                    wstring shmStr = vaStringTools::SimpleWiden( szShaderModel );
                    wstring entryStr = vaStringTools::SimpleWiden( szEntryPoint );
                    wstring fullMessage = vaStringTools::Format( L"Error while compiling '%s' shader, SM: '%s', EntryPoint: '%s'."
                        L"\n\n---\n%s\n---",
                        fullFileName.c_str( ), shmStr.c_str( ), entryStr.c_str( ), errorMsg.c_str( ) );
                    MessageBox( hwnd, fullMessage.c_str( ), L"Shader compile error", MB_OK | MB_SETFOREGROUND );
#endif
                }
                SAFE_RELEASE( pErrorBlob );
                return hr;
            }
            SAFE_RELEASE( pErrorBlob );
        }

        return S_OK;
    }
    //
    static HRESULT CompileShaderFromBuffer( const string & shaderCode, string & macrosAsIncludeFile, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, HWND hwnd )
    {
        HRESULT hr = S_OK;

        DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
        // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
        // Setting this flag improves the shader debugging experience, but still allows 
        // the shaders to be optimized and to run exactly the way they will run in 
        // the release configuration of this program.
        dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

        string combinedShaderCode = macrosAsIncludeFile;
        if( macrosAsIncludeFile.size() > 0 ) 
            combinedShaderCode +="\n";
        combinedShaderCode += shaderCode;

        vector<vaShaderCacheEntry12::FileDependencyInfo> unusedDependencies;
        vaShaderIncludeHelper12 includeHelper( unusedDependencies, L"", macrosAsIncludeFile );

        ID3DBlob* pErrorBlob;
        hr = D3DCompile( combinedShaderCode.c_str( ), combinedShaderCode.size( ), nullptr, c_builtInMacros, &includeHelper, szEntryPoint, szShaderModel,
            dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
        if( FAILED( hr ) )
        {
            if( pErrorBlob != nullptr )
            {
                OutputDebugStringA( CorrectErrorIfNotFullPath12( (char*)pErrorBlob->GetBufferPointer( ), includeHelper ).c_str( ) );
#if 1 || defined( _DEBUG ) // display always for now
                //wstring errorMsg = vaStringTools::SimpleWiden( (char*)pErrorBlob->GetBufferPointer( ) );
                string fullMessage = vaStringTools::Format( "Error while compiling '%s' shader, SM: '%s', EntryPoint: '%s'. \n\n---\n%s\n---",
                    "<from_buffer>", szShaderModel, szEntryPoint, pErrorBlob->GetBufferPointer( ) );
                MessageBoxA( hwnd, fullMessage.c_str( ), "Shader compile error", MB_OK | MB_SETFOREGROUND );
#endif
            }
            SAFE_RELEASE( pErrorBlob );
            return hr;
        }
        SAFE_RELEASE( pErrorBlob );
        return S_OK;
    }
    //
    ID3DBlob * vaShaderDX12::CreateShaderBase( bool & loadedFromCache )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        ID3DBlob * shaderBlob = nullptr;
        loadedFromCache = false;

        auto & device = GetRenderDevice().SafeCast<vaRenderDeviceDX12*>( )->GetPlatformDevice();
        if( device == nullptr )
            return nullptr;
        if( ( m_shaderFilePath.size( ) == 0 ) && ( m_shaderCode.size( ) == 0 ) )
        {
            vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L" Shader has no file or code provided - cannot compile" );
            return nullptr; // still no path or code set provided
        }

        string macrosAsIncludeFile;
        GetMacrosAsIncludeFile( macrosAsIncludeFile );

        if( m_shaderFilePath.size( ) != 0 )
        {
            vaShaderCacheKey12 cacheKey;
            CreateCacheKey( cacheKey );

#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
            bool foundButModified;
            shaderBlob = vaDirectX12ShaderManager::GetInstance( ).FindInCache( cacheKey, foundButModified );

            if( shaderBlob == nullptr )
            {
                wstring entryw = vaStringTools::SimpleWiden( m_entryPoint );
                wstring shadermodelw = vaStringTools::SimpleWiden( m_shaderModel );

                if( foundButModified )
                    vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L" > file '%s' for '%s', entry '%s', found in cache but modified; recompiling...", m_shaderFilePath.c_str( ), shadermodelw.c_str( ), entryw.c_str( ) );
                else
                    vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L" > file '%s' for '%s', entry '%s', not found in cache; recompiling...", m_shaderFilePath.c_str( ), shadermodelw.c_str( ), entryw.c_str( ) );
            }
#endif

            loadedFromCache = shaderBlob != nullptr;

            if( shaderBlob == nullptr )
            {
                vector<vaShaderCacheEntry12::FileDependencyInfo> dependencies;

                CompileShaderFromFile( m_shaderFilePath.c_str( ), macrosAsIncludeFile, m_entryPoint.c_str( ), m_shaderModel.c_str( ), &shaderBlob, dependencies, GetRenderDevice().SafeCast<vaRenderDeviceDX12*>()->GetHWND( ) );

                if( shaderBlob != nullptr )
                {
                    shaderBlob->AddRef();
                    ULONG refCount = shaderBlob->Release();
                    assert( refCount == 1 ); refCount;

                    vaDirectX12ShaderManager::GetInstance( ).AddToCache( cacheKey, shaderBlob, dependencies );

                    shaderBlob->AddRef();
                    refCount = shaderBlob->Release();
                    assert( refCount == 2 ); refCount;
                }
            }
        }
        else if( m_shaderCode.size( ) != 0 )
        {
            CompileShaderFromBuffer( m_shaderCode, macrosAsIncludeFile, m_entryPoint.c_str( ), m_shaderModel.c_str( ), &shaderBlob, GetRenderDevice().SafeCast<vaRenderDeviceDX12*>()->GetHWND( ) );
            if( shaderBlob != nullptr )
            {
                shaderBlob->AddRef();
                ULONG refCount = shaderBlob->Release();
                assert( refCount == 1 ); refCount;
            }
        }
        else
        {
            assert( false );
        }


#ifdef VA_HOLD_SHADER_DISASM
        if( shaderBlob != nullptr )
        {
            ID3DBlob * disasmBlob = nullptr;

            HRESULT hr = D3DDisassemble( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, nullptr, &disasmBlob );
            assert( SUCCEEDED( hr ) ); hr;

            m_disasm = "";
            if( ( disasmBlob != nullptr ) && ( disasmBlob->GetBufferPointer( ) != nullptr ) )
            {
                m_disasm = (const char *)disasmBlob->GetBufferPointer( );

                if( m_disasmAutoDumpToFile )
                {
                    wstring fileName = vaCore::GetWorkingDirectory( );

                    vaShaderCacheKey12 cacheKey;
                    CreateCacheKey( cacheKey );
                    // vaCRC64 crc;
                    // crc.AddString( cacheKey.StringPart );

                    fileName += L"shaderdump_" + vaStringTools::SimpleWiden( m_entryPoint ) + L"_" + vaStringTools::SimpleWiden( m_shaderModel ) /*+ L"_" + vaStringTools::SimpleWiden( vaStringTools::Format( "0x%" PRIx64, crc.GetCurrent() ) )*/ + L".txt";

                    vaStringTools::WriteTextFile( fileName, m_disasm );
                }

            }

            SAFE_RELEASE( disasmBlob );
        }
#endif

        return shaderBlob;
    }
    //
    void vaShaderDX12::CreateShader( )
    {
        assert( vaDirectX12ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();

        assert( m_shaderBlob == nullptr );

        m_shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        assert( m_shaderBlob.Get( ) != nullptr );
        // ULONG refCount = m_shaderBlob.Get( )->Release( );
        // assert( refCount == 1 || refCount == 2 ); refCount; // refcount will be 1 if it is not cached, 2 if it is cached or more if cached and shared with other instances

        m_state             = State::Cooked;
        m_uniqueContentsID  = ++s_lastUniqueShaderContentsID;
    }
    //
    vaVertexShaderDX12::~vaVertexShaderDX12( )
    {
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );
    }
    //
    void vaVertexShaderDX12::SetInputLayout( D3D12_INPUT_ELEMENT_DESC * elements, uint32 elementCount )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 

        m_inputLayout = LayoutVAFromDX( elements, elementCount );
        LayoutDXFromVA( m_inputLayoutDX12, m_inputLayout );
    }
    //
    void vaVertexShaderDX12::CreateShaderAndILFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile )
    {
        assert( vaDirectX12ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            m_inputLayout = vaVertexInputLayoutDesc( inputLayoutElements );
            LayoutDXFromVA( m_inputLayoutDX12, m_inputLayout );
        }

        vaShaderDX12::CreateShaderFromFile( filePath, shaderModel, entryPoint, macros, forceImmediateCompile );
    }
    //
    void vaVertexShaderDX12::CreateShaderAndILFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile )
    {
        assert( vaDirectX12ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            m_inputLayout = vaVertexInputLayoutDesc( inputLayoutElements );
            LayoutDXFromVA( m_inputLayoutDX12, m_inputLayout );
        }
        vaShaderDX12::CreateShaderFromBuffer( shaderCode, shaderModel, entryPoint, macros, forceImmediateCompile );
    }
    //
    void vaVertexShaderDX12::CreateShader( )
    {
        assert( vaDirectX12ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();

        assert( m_shaderBlob == nullptr );

        m_shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        assert( m_shaderBlob.Get( ) != nullptr );
        // ULONG refCount = m_shaderBlob.Get( )->Release( );
        // assert( refCount == 1 || refCount == 2 ); refCount; // refcount will be 1 if it is not cached, 2 if it is cached or more if cached and shared with other instances

        m_state             = State::Cooked;
        m_uniqueContentsID  = ++s_lastUniqueShaderContentsID;
    }
    //
    void vaVertexShaderDX12::DestroyShader( )
    {
        assert( vaDirectX12ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();
        vaShaderDX12::DestroyShader( );
    }
    //
    vaVertexInputLayoutDesc vaVertexShaderDX12::LayoutVAFromDX( D3D12_INPUT_ELEMENT_DESC * elements, uint32 elementCount )
    {
        vector<vaVertexInputElementDesc> descArray;

        for( int i = 0; i < (int)elementCount; i++ )
        {
            const D3D12_INPUT_ELEMENT_DESC & src = elements[i];
            vaVertexInputElementDesc desc;
            desc.SemanticName           = src.SemanticName;
            desc.SemanticIndex          = src.SemanticIndex;
            desc.Format                 = VAFormatFromDXGI(src.Format);
            desc.InputSlot              = src.InputSlot;
            desc.AlignedByteOffset      = src.AlignedByteOffset;
            desc.InputSlotClass         = (vaVertexInputElementDesc::InputClassification)src.InputSlotClass;
            desc.InstanceDataStepRate   = src.InstanceDataStepRate;

            descArray.push_back( desc );
        }
        return vaVertexInputLayoutDesc(descArray);
    }
    //
    // !!warning!! returned char pointers in descriptors outLayout point to inLayout values so make sure you keep inLayout alive until outLayout is alive
    void vaVertexShaderDX12::LayoutDXFromVA( shared_ptr< vector< D3D12_INPUT_ELEMENT_DESC > > & outLayout, const vaVertexInputLayoutDesc & inLayout )
    {
        const vector<vaVertexInputElementDesc> & srcArray = inLayout.ElementArray( );

        outLayout = std::make_shared<vector<D3D12_INPUT_ELEMENT_DESC>>();
        outLayout->resize( srcArray.size() );

        for( int i = 0; i < srcArray.size(); i++ )
        {
            const vaVertexInputElementDesc & src = srcArray[i];
            D3D12_INPUT_ELEMENT_DESC desc;
            
            // I hate doing this
            desc.SemanticName           = src.SemanticName.c_str();
            desc.SemanticIndex          = src.SemanticIndex;
            desc.Format                 = (DXGI_FORMAT)src.Format;
            desc.InputSlot              = src.InputSlot;
            desc.AlignedByteOffset      = src.AlignedByteOffset;
            desc.InputSlotClass         = ( D3D12_INPUT_CLASSIFICATION )src.InputSlotClass;
            desc.InstanceDataStepRate   = src.InstanceDataStepRate;

            (*outLayout)[i] = desc;
        }
    }
    //
    vaDirectX12ShaderManager::vaDirectX12ShaderManager( vaRenderDevice & device ) : vaShaderManager( device )
    {
        assert( vaThreading::IsMainThread() );
        {
#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
            // lock here just in case we put something after the loading task 
            std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

            // this should maybe be set externally, but good enough for now
            m_cacheFilePath = vaCore::GetExecutableDirectory( ) + L"cache\\";

            // Do we need per-adapter caches? probably not but who cares - different adapter >usually< means different machine so recaching anyway
#if defined( DEBUG ) || defined( _DEBUG )
        m_cacheFilePath += L"shaders_dx12_debug_" + vaStringTools::SimpleWiden( vaStringTools::ReplaceSpacesWithUnderscores( GetRenderDevice().GetAdapterNameID( ) ) );
#else
        m_cacheFilePath += L"shaders_dx12_release_" + vaStringTools::SimpleWiden( vaStringTools::ReplaceSpacesWithUnderscores( GetRenderDevice().GetAdapterNameID( ) ) );
#endif
        }

        LoadCacheInternal();
#endif // VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
    }
    //
    //
    vaDirectX12ShaderManager::~vaDirectX12ShaderManager( )
    {
        assert( vaThreading::IsMainThread() );

#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
        {
            std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );    // this is also needed to make sure loading finished!
            SaveCacheInternal( );
        }
#endif
        ClearCache( );

        // Ensure no shaders remain
        {
            std::unique_lock<mutex> shaderListLock( vaShader::GetAllShaderListMutex( ) );
            for( auto it : vaShader::GetAllShaderList( ) )
            {
                VA_LOG_ERROR( "Shader '%s' not unloaded", it->GetEntryPoint( ).c_str() );
            }
            assert( vaShader::GetAllShaderList().size( ) == 0 );
        }
    }
    //
    void vaDirectX12ShaderManager::RegisterShaderSearchPath( const std::wstring & path, bool pushBack )
    {
        wstring cleanedSearchPath = vaFileTools::CleanupPath( path + L"\\", false );
        if( pushBack )
            m_searchPaths.push_back( cleanedSearchPath );
        else
            m_searchPaths.push_front( cleanedSearchPath );
    }
    //
    wstring vaDirectX12ShaderManager::FindShaderFile( const wstring & fileName )
    {
        assert( m_searchPaths.size() > 0 ); // forgot to call RegisterShaderSearchPath?
        for( unsigned int i = 0; i < m_searchPaths.size( ); i++ )
        {
            std::wstring filePath = m_searchPaths[i] + L"\\" + fileName;
            if( vaFileTools::FileExists( filePath.c_str( ) ) )
            {
                return vaFileTools::GetAbsolutePath( filePath ); // vaFileTools::CleanupPath( filePath, false );
            }
            if( vaFileTools::FileExists( ( vaCore::GetWorkingDirectory( ) + filePath ).c_str( ) ) )
            {
                return vaFileTools::GetAbsolutePath( vaCore::GetWorkingDirectory( ) + filePath ); //vaFileTools::CleanupPath( vaCore::GetWorkingDirectory( ) + filePath, false );
            }
        }

        if( vaFileTools::FileExists( fileName ) )
            return vaFileTools::GetAbsolutePath( fileName ); // vaFileTools::CleanupPath( fileName, false );

        if( vaFileTools::FileExists( ( vaCore::GetWorkingDirectory( ) + fileName ).c_str( ) ) )
            return vaFileTools::GetAbsolutePath( vaCore::GetWorkingDirectory( ) + fileName ); // vaFileTools::CleanupPath( vaCore::GetWorkingDirectory( ) + fileName, false );

        return L"";
    }
    //
    void vaDirectX12ShaderManager::LoadCacheInternal( )
    {
        auto loadingLambda = [this]( vaBackgroundTaskManager::TaskContext & context ) 
        {
            // lock here just in case we put something after the loading task 
            std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

            {
                std::unique_lock<std::mutex> cacheLoadStartedMutexLock( m_cacheLoadStartedMutex );
                m_cacheLoadStarted = true;
            }
            m_cacheLoadStartedCV.notify_all();

            wstring fullFileName = m_cacheFilePath;

            if( fullFileName == L"" )
                return false;

            if( !vaFileTools::FileExists( fullFileName ) )
                return false;

            ClearCacheInternal( );

            vaFileStream inFile;
            if( inFile.Open( fullFileName.c_str( ), FileCreationMode::Open, FileAccessMode::Read ) )
            {
                int version = -1;
                inFile.ReadValue<int32>( version );

                assert( version == 0 );

                int32 entryCount = 0;
                inFile.ReadValue<int32>( entryCount );

                for( int i = 0; i < entryCount; i++ )
                {
                    context.Progress = float(i)/float(entryCount-1);

                    vaShaderCacheKey12 key;
                    key.Load( inFile );
                    vaShaderCacheEntry12 * entry = new vaShaderCacheEntry12( inFile );

                    m_cache.insert( std::pair<vaShaderCacheKey12, vaShaderCacheEntry12 *>( key, entry ) );
                }

                int32 terminator;
                inFile.ReadValue<int32>( terminator );
                assert( terminator == 0xFF );
            }
            return true;
        };

        bool multithreaded = true;
        if( multithreaded )
        {
            vaBackgroundTaskManager::GetInstance().Spawn( "Loading Shader Cache", vaBackgroundTaskManager::SpawnFlags::ShowInUI, loadingLambda );
            
            // make sure we've started and taken the thread
            {
                std::unique_lock<std::mutex> lk(m_cacheLoadStartedMutex);
                m_cacheLoadStartedCV.wait(lk, [this] { return (bool)m_cacheLoadStarted; } );
            }
        }
        else
        {
            loadingLambda( vaBackgroundTaskManager::TaskContext() );
        }
    }
    //
    void vaDirectX12ShaderManager::SaveCacheInternal( ) const
    {
        // must already be locked by this point!
        // std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );
        m_cacheMutex.assert_locked_by_caller();

        wstring fullFileName = m_cacheFilePath;

        wstring cacheDir;
        vaFileTools::SplitPath( fullFileName, &cacheDir, nullptr, nullptr );
        vaFileTools::EnsureDirectoryExists( cacheDir.c_str( ) );

        vaFileStream outFile;
        outFile.Open( fullFileName.c_str( ), FileCreationMode::Create );

        outFile.WriteValue<int32>( 0 );                 // version;

        outFile.WriteValue<int32>( (int32)m_cache.size( ) );    // number of entries

        for( std::map<vaShaderCacheKey12, vaShaderCacheEntry12 *>::const_iterator it = m_cache.cbegin( ); it != m_cache.cend( ); ++it )
        {
            // Save key
            ( *it ).first.Save( outFile );

            // Save data
            ( *it ).second->Save( outFile );
        }

        outFile.WriteValue<int32>( 0xFF );  // EOF;
    }
    //
    void vaDirectX12ShaderManager::ClearCacheInternal( )
    {
        m_cacheMutex.assert_locked_by_caller();
        for( std::map<vaShaderCacheKey12, vaShaderCacheEntry12 *>::iterator it = m_cache.begin( ); it != m_cache.end( ); ++it )
        {
            delete ( *it ).second;;
        }
        m_cache.clear( );
    }
    //
    void vaDirectX12ShaderManager::ClearCache( )
    {
        std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );
        ClearCacheInternal();
    }
    //
    ID3DBlob * vaDirectX12ShaderManager::FindInCache( vaShaderCacheKey12 & key, bool & foundButModified )
    {
        std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

        foundButModified = false;

        std::map<vaShaderCacheKey12, vaShaderCacheEntry12 *>::iterator it = m_cache.find( key );

        if( it != m_cache.end( ) )
        {
            if( ( *it ).second->IsModified( ) )
            {
                foundButModified = true;

                // Have to recompile...
                delete ( *it ).second;
                m_cache.erase( it );

                return nullptr;
            }

            ID3DBlob * retBlob = ( *it ).second->GetCompiledShader( );
            return retBlob;
        }
        return nullptr;
    }
    //
    void vaDirectX12ShaderManager::AddToCache( vaShaderCacheKey12 & key, ID3DBlob * shaderBlob, std::vector<vaShaderCacheEntry12::FileDependencyInfo> & dependencies )
    {
        std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

        std::map<vaShaderCacheKey12, vaShaderCacheEntry12 *>::iterator it = m_cache.find( key );

        if( it != m_cache.end( ) )
        {
            // Already in? can happen with parallel compilation I guess?
            // well let's just not do anything then
            return;
            // delete ( *it ).second;
            // m_cache.erase( it );
        }

        m_cache.insert( std::pair<vaShaderCacheKey12, vaShaderCacheEntry12 *>( key, new vaShaderCacheEntry12( shaderBlob, dependencies ) ) );
    }
    //
    vaShaderCacheEntry12::FileDependencyInfo::FileDependencyInfo( const wstring & filePath )
    {
        wstring fullFileName = vaDirectX12ShaderManager::GetInstance( ).FindShaderFile( filePath );

        if( fullFileName == L"" )
        {
            VA_ERROR( L"Error trying to find shader file '%s'!", filePath.c_str() );

            assert( false );
            this->FilePath = L"";
            this->ModifiedTimeDate = 0;
        }
        else
        {
            this->FilePath = filePath;

            // struct _stat64 fileInfo;
            // _wstat64( fullFileName.c_str( ), &fileInfo ); // check error code?
            //this->ModifiedTimeDate = fileInfo.st_mtime;

            // maybe add some CRC64 here too? that would require reading contents of every file and every dependency which would be costly! 
            WIN32_FILE_ATTRIBUTE_DATA attrInfo;
            ::GetFileAttributesEx( fullFileName.c_str( ), GetFileExInfoStandard, &attrInfo );
            this->ModifiedTimeDate = ( ( (int64)attrInfo.ftLastWriteTime.dwHighDateTime ) << 32 ) | ( (int64)attrInfo.ftLastWriteTime.dwLowDateTime );
        }
    }
    //
    vaShaderCacheEntry12::FileDependencyInfo::FileDependencyInfo( const wstring & filePath, int64 modifiedTimeDate )
    {
        this->FilePath = filePath;
        this->ModifiedTimeDate = modifiedTimeDate;
    }
    //
    bool vaShaderCacheEntry12::FileDependencyInfo::IsModified( )
    {
        wstring fullFileName = vaDirectX12ShaderManager::GetInstance( ).FindShaderFile( this->FilePath.c_str( ) );

        //vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, (L"vaShaderCacheEntry12::FileDependencyInfo::IsModified, file name %s", fullFileName.c_str() ) );

        if( fullFileName == L"" )  // Can't find the file?
        {
            vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + this->FilePath );

            if( !embeddedData.HasContents( ) )
            {
                //vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L"        > embedded data has NO contents" );
#ifdef STILL_LOAD_FROM_CACHE_IF_ORIGINAL_FILE_MISSING
                return false;
#else
                VA_ERROR( L"Error trying to find shader file '%s'!", this->FilePath.c_str( ) );
                return true;
#endif
            }
            else
            {
                //vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS,  L"        > embedded data has contents" );
                return this->ModifiedTimeDate != embeddedData.TimeStamp;
            }
        }

        // struct _stat64 fileInfo;
        // _wstat64( fullFileName.c_str( ), &fileInfo ); // check error code?
        // return this->ModifiedTimeDate != fileInfo.st_mtime;

        // maybe add some CRC64 here too? that would require reading contents of every file and every dependency which would be costly! 
        WIN32_FILE_ATTRIBUTE_DATA attrInfo;
        ::GetFileAttributesEx( fullFileName.c_str( ), GetFileExInfoStandard, &attrInfo );
        bool ret = this->ModifiedTimeDate != ( ( ( (int64)attrInfo.ftLastWriteTime.dwHighDateTime ) << 32 ) | ( (int64)attrInfo.ftLastWriteTime.dwLowDateTime ) );

        // if( ret )
        // {
        //     vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS,  (const wchar_t*)((ret)?(L"   > shader file '%s' modified "):(L"   > shader file '%s' NOT modified ")), fullFileName.c_str() );
        // }

        return ret;
    }
    //
    vaShaderCacheEntry12::vaShaderCacheEntry12( ID3DBlob * compiledShader, std::vector<vaShaderCacheEntry12::FileDependencyInfo> & dependencies )
    {
        m_compiledShader = compiledShader;
        m_compiledShader->AddRef( );

        m_dependencies = dependencies;
    }
    //
    vaShaderCacheEntry12::~vaShaderCacheEntry12( )
    {
        m_compiledShader->Release( );
    }
    //
    bool vaShaderCacheEntry12::IsModified( )
    {
        for( std::vector<vaShaderCacheEntry12::FileDependencyInfo>::iterator it = m_dependencies.begin( ); it != m_dependencies.end( ); ++it )
        {
            if( ( *it ).IsModified( ) )
                return true;
        }
        return false;
    }
    //
    void vaShaderCacheKey12::Save( vaStream & outStream ) const
    {
        outStream.WriteString( this->StringPart.c_str( ) );
    }
    //
    void vaShaderCacheEntry12::FileDependencyInfo::Save( vaStream & outStream ) const
    {
        outStream.WriteString( this->FilePath.c_str( ) );
        outStream.WriteValue<int64>( this->ModifiedTimeDate );
    }
    //
    void vaShaderCacheEntry12::Save( vaStream & outStream ) const
    {
        outStream.WriteValue<int32>( ( int32 )this->m_dependencies.size( ) );       // number of dependencies

        for( std::vector<vaShaderCacheEntry12::FileDependencyInfo>::const_iterator it = m_dependencies.cbegin( ); it != m_dependencies.cend( ); ++it )
        {
            ( *it ).Save( outStream );
        }

        int bufferSize = (int)m_compiledShader->GetBufferSize( );

        outStream.WriteValue<int32>( bufferSize );
        outStream.Write( m_compiledShader->GetBufferPointer( ), bufferSize );
    }
    //
    void vaShaderCacheKey12::Load( vaStream & inStream )
    {
        inStream.ReadString( this->StringPart );
    }
    //
    void vaShaderCacheEntry12::FileDependencyInfo::Load( vaStream & inStream )
    {
        inStream.ReadString( this->FilePath );
        inStream.ReadValue<int64>( this->ModifiedTimeDate );
    }
    //
    void vaShaderCacheEntry12::Load( vaStream & inStream )
    {
        int dependencyCount;
        inStream.ReadValue<int32>( dependencyCount );

        for( int i = 0; i < dependencyCount; i++ )
        {
            m_dependencies.push_back( vaShaderCacheEntry12::FileDependencyInfo( inStream ) );
        }

        int bufferSize;
        inStream.ReadValue<int32>( bufferSize );

        D3DCreateBlob( bufferSize, &m_compiledShader );
        inStream.Read( m_compiledShader->GetBufferPointer( ), bufferSize );
    }
}




void RegisterShaderDX12( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaPixelShader,    vaPixelShaderDX12     );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaComputeShader,  vaComputeShaderDX12   );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaHullShader,     vaHullShaderDX12      );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaDomainShader,   vaDomainShaderDX12    );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaGeometryShader, vaGeometryShaderDX12  );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaVertexShader,   vaVertexShaderDX12    );
}
