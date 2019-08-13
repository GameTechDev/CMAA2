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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Project info
//
// VertexAsylum codebase was originally created by filip.strugar@intel.com for personal research & development use.
//
// It was intended to be an MIT-licensed platform for experimentation with DirectX, with rudimentary asset loading 
// through AssImp, simple rendering pipeline and support for at-runtime shader recompilation, basic post-processing, 
// basic GPU profiling and UI using Imgui, basic non-optimized vector math and other helpers.
//
// While the original codebase was designed in a platform independent way, the current state is that it only supports 
// DirectX 11 on Windows Desktop and HLSL shaders. So, it's not platform or API independent. I've abstracted a lot of 
// core stuff, and the graphics API parts are abstracted through the VA_RENDERING_MODULE_CREATE system, which once 
// handled Dx9 and OpenGL modules but, for now, just DX11 support exists.
//
// It is very much work in progress with future development based on short term needs, so feel free to use it any way 
// you like (and feel free to contribute back to it), but also use it at your own peril!
// 
// Filip Strugar, 14 October 2016
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// CRT's memory leak detection
#if defined(DEBUG) || defined(_DEBUG)

#define _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC_NEW
#include <stdlib.h>
#include <crtdbg.h>

#endif

#include "..\vaConfig.h"

#include "vaSTL.h"

#include "vaPlatformBase.h"

#include "vaCoreTypes.h"

#include <assert.h>
#include <stdlib.h>

// not platform independent yet - maybe use http://graemehill.ca/minimalist-cross-platform-uuid-guid-generation-in-c++/
#define __INLINE_ISEQUAL_GUID
#include <initguid.h>
#include <cguid.h>


namespace VertexAsylum
{
    class vaSystemManagerBase;

    struct vaGUID : GUID
    {
        vaGUID( ) { }
        vaGUID( const char * str );
        vaGUID( unsigned long data1, unsigned short data2, unsigned short data3, unsigned char data4[8] ) { Data1 = data1; Data2 = data2; Data3 = data3; for( int i = 0; i < _countof( Data4 ); i++ ) Data4[i] = data4[i]; }
        vaGUID( unsigned long data1, unsigned short data2, unsigned short data3, unsigned char data40, unsigned char data41, unsigned char data42, unsigned char data43, unsigned char data44, unsigned char data45, unsigned char data46, unsigned char data47 ) { Data1 = data1; Data2 = data2; Data3 = data3; Data4[0] = data40; Data4[1] = data41; Data4[2] = data42; Data4[3] = data43; Data4[4] = data44; Data4[5] = data45; Data4[6] = data46; Data4[7] = data47; }
        explicit vaGUID( const GUID & copy ) { Data1 = copy.Data1; Data2 = copy.Data2; Data3 = copy.Data3; for( int i = 0; i < _countof( Data4 ); i++ ) Data4[i] = copy.Data4[i]; }
        vaGUID( const vaGUID & copy ) { Data1 = copy.Data1; Data2 = copy.Data2; Data3 = copy.Data3; for( int i = 0; i < _countof( Data4 ); i++ ) Data4[i] = copy.Data4[i]; }

        vaGUID & operator =( const GUID & copy ) { Data1 = copy.Data1; Data2 = copy.Data2; Data3 = copy.Data3; for( int i = 0; i < _countof( Data4 ); i++ ) Data4[i] = copy.Data4[i]; return *this; }

        const static vaGUID          Null;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // vaCore 
    class vaCore
    {
    private:
        static bool                   s_initialized;
        static bool                   s_managersInitialized;

        static bool                   s_currentlyInitializing;
        static bool                   s_currentlyDeinitializing;

        static bool                   s_appQuitFlag;
        static bool                   s_appQuitButRestartFlag;

    public:

        // Initialize the system - must be called before any other calls
        static void                   Initialize( bool liveRestart = false );

        // This must only be called from the same thread that called Initialize
        static void                   Deinitialize( bool liveRestart = false );

        // // This must only be called from the same thread that called Initialize
        // static void                Tick( float deltaTime );

        static void                   Error( const wchar_t * messageFormat, ... );
        static void                   Warning( const wchar_t * messageFormat, ... );
        static void                   DebugOutput( const wstring & message );

        static bool                   MessageBoxYesNo( const wchar_t * title, const wchar_t * messageFormat, ... );

        static wstring                GetWorkingDirectory( );
        static wstring                GetExecutableDirectory( );

        static vaGUID                 GUIDCreate( );
        static const vaGUID &         GUIDNull( );
        static wstring                GUIDToString( const vaGUID & id );
        static vaGUID                 GUIDFromString( const wstring & str );
        static string                 GUIDToStringA( const vaGUID & id );
        static vaGUID                 GUIDFromStringA( const string & str );
        //      static int32                  GUIDGetHashCode( const vaGUID & id );

        static string                 GetCPUIDName( );

        static bool                   GetAppQuitFlag( ) { return s_appQuitFlag; }
        static bool                   GetAppQuitButRestartingFlag( ) { return s_appQuitButRestartFlag; }
        static void                   SetAppQuitFlag( bool quitFlag, bool quitButRestart = false ) { s_appQuitFlag = quitFlag; s_appQuitButRestartFlag = quitButRestart && quitFlag; };

    private:

    };
    typedef vaCore    vaLevel0;
    ////////////////////////////////////////////////////////////////////////////////////////////////

    inline vaGUID::vaGUID( const char * str )
    {
        *this = vaCore::GUIDFromStringA( str );
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // vaCoreInitDeinit - RAII for vaCore
    class vaCoreInitDeinit
    {
    public:
        vaCoreInitDeinit( ) { vaCore::Initialize( ); }
        ~vaCoreInitDeinit( ) { vaCore::Deinitialize( ); }
    };
    ////////////////////////////////////////////////////////////////////////////////////////////////


    struct vaGUIDComparer
    {
        bool operator()( const vaGUID & Left, const vaGUID & Right ) const
        {
            // comparison logic goes here
            return memcmp( &Left, &Right, sizeof( Right ) ) < 0;
        }
    };

    // Various defines

#define VA__T(x)        L ## x
#define VA_T(x)         VA__T(x)
#define VA_NT(x)        L#x


#ifdef _DEBUG
#define VA_ASSERT_ALWAYS( format, ... )                           do { vaCore::Warning( L"\n%s(%d): " format , VA_T(__FILE__), __LINE__, __VA_ARGS__ ); assert( false ); } while( false )
#define VA_ASSERT( condition, format, ... )  if( !(condition) )   do { VA_ASSERT_ALWAYS( format, __VA_ARGS__ ); } while( false ) //{ vaCore::Warning( L"%s:%d\n" format , VA_T(__FILE__), __LINE__, __VA_ARGS__ ); assert( false ); } 
#else
#define VA_ASSERT_ALWAYS( format, ... )  { }
#define VA_ASSERT( condition, format, ... )  { }
#endif

// recoverable error or a warning
#define VA_WARN( format, ... )                                      do { vaCore::Warning( L"%s:%d : " format , VA_T(__FILE__), __LINE__, __VA_ARGS__); } while( false )

// irrecoverable error, save the log and die gracefully
#define VA_ERROR( format, ... )                                     do { vaCore::Error( L"%s:%d : " format , VA_T(__FILE__), __LINE__, __VA_ARGS__); } while( false )

#define VA_VERIFY( x, format, ... )                                 do { bool __va_verify_res = (x); if( !__va_verify_res ) { VA_ASSERT_ALWAYS( format, __VA_ARGS__ ); } } while( false )
#define VA_VERIFY_RETURN_IF_FALSE( x, format, ... )                 do { bool __va_verify_res = (x); if( !__va_verify_res ) { VA_ASSERT_ALWAYS( format, __VA_ARGS__ ); return false; } } while( false )

// Other stuff

// warning C4201: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )

// warning C4239: nonstandard extension used : 'default argument' 
#pragma warning( disable : 4239 )

#ifndef IsNullGUID
    inline bool IsNullGUID( REFGUID rguid ) { return IsEqualGUID( GUID_NULL, rguid ) != 0; }
#endif

    // just adds an assert in debug
    template< typename OutTypeName, typename InTypeName >
    inline OutTypeName vaSaferStaticCast( InTypeName ptr )
    {
#ifdef _DEBUG
        OutTypeName ret = dynamic_cast<OutTypeName>( ptr );
        //assert( ret != nullptr );
        return ret;
#else
        return static_cast <OutTypeName>( ptr );
#endif
    }

    // cached dynamic cast for when we know the return type is always the same - it's rather trivial now, 
    // could be made safe for release path too
    template< typename OutTypeName, typename InTypeName >
    inline OutTypeName vaCachedDynamicCast( InTypeName thisPtr, void * & cachedVoidPtr )
    {
        if( cachedVoidPtr == nullptr )
            cachedVoidPtr = static_cast<void*>( dynamic_cast<OutTypeName>( thisPtr ) );
        OutTypeName retVal = static_cast<OutTypeName>( cachedVoidPtr );
#ifdef _DEBUG
        OutTypeName validPtr = dynamic_cast<OutTypeName>( thisPtr );
        assert( validPtr != NULL );
        assert( retVal == validPtr );
#endif
        return retVal;
    }


    // legacy - should be removed
#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           { hr = (x); if( FAILED(hr) ) { VA_ASSERT_ALWAYS( L"FAILED(hr) == true" ); } } //DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { VA_ASSERT_ALWAYS( L"FAILED(hr) == true" ); return hr; } } //return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)          { if (p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p)    { if (p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)         { if (p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE_ARRAY
#define SAFE_RELEASE_ARRAY(p)   { for( int i = 0; i < _countof(p); i++ ) if (p[i]) { (p[i])->Release(); (p[i])=NULL; } }
#endif

#ifndef VERIFY_TRUE_RETURN_ON_FALSE
#define VERIFY_TRUE_RETURN_ON_FALSE( x )        \
do                                              \
{                                               \
if( !(x) )                                      \
{                                               \
    assert( false );                            \
    return false;                               \
}                                               \
} while( false )
#endif

#define VA_COMBINE1(X,Y) X##Y  // helper macro
#define VA_COMBINE(X,Y) VA_COMBINE1(X,Y)

}