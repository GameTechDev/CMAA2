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

#include "Core/Misc/vaResourceFormats.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "vaRendering.h"

#define LOG_COLORS_SHADERS  (vaVector4( 0.4f, 0.9f, 1.0f, 1.0f ) )

#ifdef _DEBUG
#define VA_HOLD_SHADER_DISASM
#endif

#define VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
#define VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE

namespace VertexAsylum
{
    typedef std::vector<std::pair<std::string, std::string>> vaShaderMacroContaner;

    // There are 3 states that the shader can be in (+transitions): 
    //  - "clean" state:                    when created or after a call to Clear()
    //  - "initialized with data" state:    just after a call to Create* functions but before they finished compiling or if the Create* compilation failed or if DestroyShader() was called after Create
    //  - "created" (compiled) state:       after API compilation was success and everything is fine (use IsCreated() to check for this); one can step back to "initialized" state with DestroyShader() and step back up with CreateShader()
    // Transitions can only be initiated from the main thread at the moment.
    // Transitions from "clean" to "initialized" are quick; Create* functions will run a background thread that will complete the transition to "created" state; transition back from "created" to "clean" or "initialized" to "clean" are done with Clear()
    // The only usable state is "created" and it can be checked (from any thread) using IsCreated() or by trying to obtain API shader handle
    //
    // TODO: would be good to clean up naming and make the transitions/states more obvious - maybe call them "clean", "uncooked", "cooked" or similar? :)
    //
    class vaShader : public VertexAsylum::vaRenderingModule
    {
        static std::vector<vaShader *>  s_allShaderList;
        static mutex                    s_allShaderListMutex;
        static std::atomic_int          s_activelyCompilingShaderCount;
        int                             m_allShaderListIndex;

    public:
        enum class State
        {
            Empty,      // afer Clear() or at construction
            Uncooked,   // initialized with data required to compile but not compiled yet (or currently compiling, or couldn't compile due to an error)
            Cooked      // compiled and ready to use
        };

    protected:
        State                           m_state                 = State::Empty;

        static std::atomic_int64_t      s_lastUniqueShaderContentsID;               // used to assign unique IDs to shaders every time they get recompiled; can be made persistent between application loads by using shader cache
        int64                           m_uniqueContentsID      = -1;               // every time shader gets compiled (transitions to Cooked state) it gets a new unique ID, otherwise -1; can be made persistent between application loads by using shader cache

        // either loaded from the file system, or the code is set manually as a string
        wstring                         m_shaderFilePath;
        string                          m_shaderCode;
        string                          m_shaderModel;
        string                          m_entryPoint;
        bool                            m_forceImmediateCompile = false;

        vaShaderMacroContaner           m_macros;

        // just for information/debugging purposes
        bool                            m_lastLoadedFromCache;

#ifdef VA_HOLD_SHADER_DISASM
        std::string                     m_disasm;
        bool                            m_disasmAutoDumpToFile;
#endif

        shared_ptr<vaBackgroundTaskManager::Task>
                                        m_backgroundCreationTask;
        mutable mutex                   m_allShaderDataMutex;


    protected:
                                        vaShader( const vaRenderingModuleParams & params );
    public:
        virtual                         ~vaShader( );

        virtual void                    CreateShaderFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vaShaderMacroContaner & macros, bool forceImmediateCompile );
        void                            CreateShaderFromFile( const string & filePath, const string & shaderModel, const string & entryPoint, const vaShaderMacroContaner & macros, bool forceImmediateCompile )    { CreateShaderFromFile( vaStringTools::SimpleWiden(filePath), shaderModel, entryPoint, macros, forceImmediateCompile ); }

        virtual void                    CreateShaderFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vaShaderMacroContaner & macros, bool forceImmediateCompile );

//        virtual void                    SetToAPI( vaRenderDeviceContext & renderContext, bool assertOnOverwrite = true )     = 0;
//        virtual void                    UnsetFromAPI( vaRenderDeviceContext & renderContext, bool assertOnNotSet = true )    = 0;

        virtual void                    Clear( )        = 0;
        virtual void                    Reload( );
        static void                     ReloadAll( );
        virtual void                    WaitFinishIfBackgroundCreateActive( );  // sometimes you absolutely need the shader and don't care about blocking / sync point

        // IsEmpty returns 'true' before CreateShaderFromXXX is called at which point it returns 'false'; if multithreaded compilation is enabled, IsCreated will still return 'false' until the compile finishes
        virtual bool                    IsEmpty( )      { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); if( allShaderDataLock.owns_lock() ) return m_state == State::Empty; else return false; } // if locked it's not empty!
        // IsCreated returns 'false' until CreateShaderFromXXX is called AND background multithreaded compilation is done; once the shader is available for use it returns 'true'
        virtual bool                    IsCreated( )    = 0;

        int64                           GetUniqueContentsID( ) const    { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); return m_uniqueContentsID; }
                
        bool                            IsLoadedFromCache( ) const      { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); return m_lastLoadedFromCache; }

        const std::string &             GetEntryPoint( ) const          { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); return m_entryPoint; }

#ifdef VA_HOLD_SHADER_DISASM
        void                            SetShaderDisasmAutoDumpToFile( bool enable )    { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex );  m_disasmAutoDumpToFile = enable; }
        const string &                  GetShaderDisAsm( ) const                        { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); return m_disasm; }
#else
        void                            SetShaderDisasmAutoDumpToFile( bool enable )    { enable; }
        string                          GetShaderDisAsm( ) const                        { assert( false ); return ""; }
#endif
        //
    protected:
        void                            GetMacrosAsIncludeFile( string & outString );
        virtual void                    CreateShader( ) = 0;
        virtual void                    DestroyShader( ) = 0;
        //
    public:
        static const std::vector<vaShader *> & 
                                        GetAllShaderList( )             { return s_allShaderList; }
        static mutex &                  GetAllShaderListMutex( )        { return s_allShaderListMutex; }
        static int32                    GetNumberOfCompilingShaders( )  { return s_activelyCompilingShaderCount; }

        // you'll need this if casting directly from vaShader to any of the platform-dependent ones - but it's slow.
        // template< typename CastToType >
        // CastToType                      SafeCast( )                                                                 
        // {
        //     // damn you virtual inheritance - why did I do it this way? oh well...
        //     CastToType ret = dynamic_cast< CastToType >( this );
        //     assert( ret != NULL );
        //     return ret;
        // }
    };

    class vaPixelShader : public virtual vaShader
    {
        void *                      m_cachedPlatformPtr = nullptr;

    public:
        vaPixelShader( ) { };
        virtual ~vaPixelShader( ) { }

       // to avoid dynamic_cast for performance reasons (as this specific call happens very, very frequently) 
       template< typename OutType > OutType SafeCast( ) { return vaCachedDynamicCast<OutType>( this, m_cachedPlatformPtr ); }
    };

    class vaComputeShader : public virtual vaShader
    {
        void *                      m_cachedPlatformPtr = nullptr;

    public:
        vaComputeShader( ) { };
        virtual ~vaComputeShader( ) { }

       // to avoid dynamic_cast for performance reasons (as this specific call happens very, very frequently) 
       template< typename OutType > OutType SafeCast( ) { return vaCachedDynamicCast<OutType>( this, m_cachedPlatformPtr ); }
    };

    class vaHullShader : public virtual vaShader
    {
        void *                      m_cachedPlatformPtr = nullptr;

    public:
        vaHullShader( ) { };
        virtual ~vaHullShader( ) { }

       // to avoid dynamic_cast for performance reasons (as this specific call happens very, very frequently) 
       template< typename OutType > OutType SafeCast( ) { return vaCachedDynamicCast<OutType>( this, m_cachedPlatformPtr ); }
    };

    class vaDomainShader : public virtual vaShader
    {
        void *                      m_cachedPlatformPtr = nullptr;

    public:
        vaDomainShader( ) { };
        virtual ~vaDomainShader( ) { }

       // to avoid dynamic_cast for performance reasons (as this specific call happens very, very frequently) 
       template< typename OutType > OutType SafeCast( ) { return vaCachedDynamicCast<OutType>( this, m_cachedPlatformPtr ); }
    };

    class vaGeometryShader : public virtual vaShader
    {
        void *                      m_cachedPlatformPtr = nullptr;

    public:
        vaGeometryShader( ) { };
        virtual ~vaGeometryShader( ) { }

       // to avoid dynamic_cast for performance reasons (as this specific call happens very, very frequently) 
       template< typename OutType > OutType SafeCast( ) { return vaCachedDynamicCast<OutType>( this, m_cachedPlatformPtr ); }
    };

    struct vaVertexInputElementDesc
    {
        enum class InputClassification : uint32
        {
            PerVertexData = 0,
            PerInstanceData = 1,
        };

        static const uint32 AppendAlignedElement = 0xffffffff;

        string              SemanticName;
        uint32              SemanticIndex;
        vaResourceFormat     Format;             // texture format for this? yeah the name is not ideal but oh well
        uint32              InputSlot;
        uint32              AlignedByteOffset;
        InputClassification InputSlotClass;
        uint32              InstanceDataStepRate;

        vaVertexInputElementDesc( ) { }
        vaVertexInputElementDesc( const string & semanticName, uint32 semanticIndex, vaResourceFormat format, uint32 inputSlot, uint32 alignedByteOffset, InputClassification inputSlotClass, uint32 instanceDataStepRate )
            :
            SemanticName( semanticName ),
            SemanticIndex( semanticIndex ),
            Format( format ),
            InputSlot( inputSlot ),
            AlignedByteOffset( alignedByteOffset ),
            InputSlotClass( inputSlotClass ),
            InstanceDataStepRate( instanceDataStepRate ) { }
    };

    class vaVertexInputLayoutDesc
    {
        vector<vaVertexInputElementDesc>    m_elementDescArray;
        string                              m_hashString;

    public:
        vaVertexInputLayoutDesc( ) { UpdateHash( ); }
        vaVertexInputLayoutDesc( const vector<vaVertexInputElementDesc> & descArray ) { m_elementDescArray = descArray; UpdateHash( ); }
        vaVertexInputLayoutDesc( const vaVertexInputLayoutDesc & copy ) { m_elementDescArray = copy.m_elementDescArray; m_hashString = copy.m_hashString; }

    public:
        const string &  GetHashString( ) const { return m_hashString; }
        const vector<vaVertexInputElementDesc> &
            ElementArray( ) const { return m_elementDescArray; }

        void            Clear( ) { m_elementDescArray.clear( ); UpdateHash( ); }

    private:
        void            UpdateHash( )
        {
            m_hashString = vaStringTools::Format( "%d ", (int)m_elementDescArray.size( ) ) + " ";

            for( int i = 0; i < m_elementDescArray.size( ); i++ )
            {
                m_hashString += m_elementDescArray[i].SemanticName + " " + vaStringTools::Format( "%x %x %x %x %x %x ",
                    m_elementDescArray[i].SemanticIndex, (uint32)m_elementDescArray[i].Format, m_elementDescArray[i].InputSlot,
                    m_elementDescArray[i].AlignedByteOffset, (uint32)m_elementDescArray[i].InputSlotClass, m_elementDescArray[i].InstanceDataStepRate );
            }
        }
    };

    class vaVertexShader : public virtual vaShader
    {
        void *                      m_cachedPlatformPtr = nullptr;

    protected:
        vaVertexInputLayoutDesc     m_inputLayout;
    public:
        vaVertexShader( ) { };
        virtual ~vaVertexShader( ) { }

        virtual void                CreateShaderAndILFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile )     = 0;
        virtual void                CreateShaderAndILFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile )  = 0;

       // to avoid dynamic_cast for performance reasons (as this specific call happens very, very frequently) 
       template< typename OutType > OutType SafeCast( ) { return vaCachedDynamicCast<OutType>( this, m_cachedPlatformPtr ); }
    };

    // Singleton utility class for handling shaders
    class vaShaderManager : public vaRenderingModule
    {
    private:
        friend class vaShaderSharedManager;
        friend class vaMaterialManager;

    public:
        struct Settings
        {
            bool    WarningsAreErrors;

            Settings( )
            {
                WarningsAreErrors = true;
            }
        };

    protected:
        std::deque<wstring>                                 m_searchPaths;
        Settings                                            m_settings;
        shared_ptr<vaBackgroundTaskManager::Task>           m_backgroundShaderCompilationProgressIndicator;

    protected:
        vaShaderManager( vaRenderDevice & device );
        ~vaShaderManager( );

    public:
        // pushBack (searched last) or pushFront (searched first)
        virtual void        RegisterShaderSearchPath( const std::wstring & path, bool pushBack = true )     = 0;
        virtual wstring     FindShaderFile( const wstring & fileName )                                      = 0;

        Settings &          Settings( ) { return m_settings; }
    };

}