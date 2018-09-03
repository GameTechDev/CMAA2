///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Intel Corporation
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

#include "vaUIDObject.h"

#include "vaLog.h"

using namespace VertexAsylum;

vaUIDObject::vaUIDObject( const vaGUID & uid ) : 
    m_uid( uid ), m_tracked( false )
{ 
}

vaUIDObject::~vaUIDObject( ) 
{
    vaUIDObjectRegistrar::GetInstance().UntrackIfTracked( this );
    assert( !UIDObject_IsTracked() );
}

vaUIDObjectRegistrar::vaUIDObjectRegistrar()
{
    assert( vaThreading::IsMainThread() );
}

bool vaUIDObjectRegistrar::TrackNoMutexLock( vaUIDObject * obj )
{
    if( obj->m_tracked )
    {
        // VA_LOG_WARNING( "vaUIDObjectRegistrar::Track() - object already tracked" );
        return false;
    }

    auto it = m_objectsMap.find( obj->m_uid );
    if( it != m_objectsMap.end( ) )
    {
        VA_LOG_ERROR( "vaUIDObjectRegistrar::Track() - object with the same UID already exists: this is a potential bug, the new object will not be tracked and will not be searchable by vaUIDObjectRegistrar::Find" );
        return false;
    }
    else
    {
        m_objectsMap.insert( std::make_pair( obj->m_uid, obj ) );
        obj->m_tracked = true;
        return true;
    }
}

bool vaUIDObjectRegistrar::UntrackNoMutexLock( vaUIDObject * obj )
{
    // if not tracked just ignore it, it's probably fine, no reason we can allow untrack multiple times
    if( !obj->m_tracked )
        return false;

    auto it = m_objectsMap.find( obj->m_uid );
    if( it == m_objectsMap.end( ) )
    {
        VA_ERROR( "vaUIDObjectRegistrar::Untrack() - A tracked vaUIDObject couldn't be found: this is an indicator of a more serious error such as an algorithm bug or a memory overwrite. Don't ignore it." );
        return false;
    }
    else
    {
        // if this isn't correct, we're removing wrong object - this is a serious error, don't ignore it!
        if( obj != it->second )
        {
            VA_ERROR( "vaUIDObjectRegistrar::Untrack() - A tracked vaUIDObject could be found in the map but the pointers don't match: this is an indicator of a more serious error such as an algorithm bug or a memory overwrite. Don't ignore it." );
            return false;
        }
        else
        {
            obj->m_tracked = false;
            m_objectsMap.erase( it );
            return true;
        }
    }
}


void vaUIDObjectRegistrar::SwapIDs( vaUIDObject & a, vaUIDObject & b )
{
    std::unique_lock<mutex> mapLock( m_objectsMapMutex );

    bool aWasTracked = a.m_tracked;
    if( a.m_tracked )
        UntrackNoMutexLock( &a );
    bool bWasTracked = a.m_tracked;
    if( b.m_tracked )
        UntrackNoMutexLock( &b );

    // swap UIDs in objects
    std::swap( a.m_uid, b.m_uid );

    // swap tracking as well - I think this is what we want, the UID that was in to stay in
    if( bWasTracked )
        TrackNoMutexLock( &a );
    if( aWasTracked )
        TrackNoMutexLock( &b );

    // probably faster
/*
        assert( a.m_tracked );
        assert( b.m_tracked );
        assert( &a != &b );

        auto itA = m_objectsMap.find( a.UIDObject_GetUID( ) );
        auto itB = m_objectsMap.find( b.UIDObject_GetUID( ) );

        if( itA == m_objectsMap.end( ) || itB == m_objectsMap.end( ) )
        {
            VA_ERROR( "Error while swapping vaUIDObject IDs - something went wrong." );
            assert( false );
        }
        else
        {
            assert( itA->second == &a );
            assert( itB->second == &b );

            // swap values in the map
            std::swap( itA->second, itB->second );
        }
        */
}