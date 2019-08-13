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

#include "vaCoreIncludes.h"
#include "vaSingleton.h"
#include "System\vaStream.h"

namespace VertexAsylum
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // vaUIDObject/vaUIDObjectRegistrar is used to assign GUIDs to objects and allow search by GUIDs
    // in order to establish connections persistent over different runs.
    //
    // Multi threading support was added later and changes the behavior: instead of registering & 
    // unregistering the objects in the constructor/destructor, it now relies on user manually 
    // doing this by using UIDObject_Track/UIDObject_Untrack.
    // Since the object can be found by ID from any thread at any time after it starts tracking,
    // the care must be made to avoid any synchronization issues; at minimum UIDObject_Track 
    // must not be called during object construction and UIDObject_Untrack must not be called
    // during destruction.
    class vaUIDObject 
    {
    private:
        friend class vaUIDObjectRegistrar;
        vaGUID /*const*/                             m_uid;                                 // removed const to be able to have SwapIDs but no one else anywhere should ever be modifying this!!
        bool                                         m_tracked;                            // will be false on startup and become true on UIDObject_MakeOrphan()

    protected:
        explicit vaUIDObject( const vaGUID & uid );
        virtual ~vaUIDObject( );

    public:
        const vaGUID &                               UIDObject_GetUID( ) const { assert( UIDObject_IsTracked() ); return m_uid; }

        bool                                         UIDObject_IsTracked( ) const;
        bool                                         UIDObject_Track( );
        bool                                         UIDObject_Untrack( );
    };

    class vaUIDObjectRegistrar : public vaSingletonBase< vaUIDObjectRegistrar >
    {
    protected:
        friend class vaUIDObject;

        map< vaGUID, vaUIDObject*, vaGUIDComparer >  m_objectsMap;
        mutex                                       m_objectsMapMutex;

    private:
        friend class vaCore;
        vaUIDObjectRegistrar( );
        ~vaUIDObjectRegistrar( )
        {
            std::unique_lock<mutex> mapLock( m_objectsMapMutex );
            // not 0? memory leak or not all objects deleted before the registrar was deleted (bug)
            assert( m_objectsMap.size( ) == 0 );
        }

    public:
        bool                                         IsTracked( const vaUIDObject * obj ) const;
        bool                                         Track( vaUIDObject * obj )                     { std::unique_lock<mutex> mapLock( m_objectsMapMutex ); return TrackNoMutexLock(obj);      }
        bool                                         Untrack( vaUIDObject * obj )                   { std::unique_lock<mutex> mapLock( m_objectsMapMutex ); return UntrackNoMutexLock(obj);    }


    public:
        template< class T >
        static T *                                   Find( const vaGUID & uid );

        // faster version of Find - you provide a weakPointer that might or might not point to the object with the ID - if it is,
        // it's a cheap call; if not, regular Find() is performed
        template< class T >
        inline std::shared_ptr<T>                    FindCached( const vaGUID & uid, std::weak_ptr<T> & inOutCachedPtr );

        // template< class T >
        // static void                                  ReconnectDependency( std::shared_ptr<T> & outSharedPtr, const vaGUID & uid );

        // Exchange two object IDs
        void                                         SwapIDs( vaUIDObject & a, vaUIDObject & b );

    private:
        // perhaps make these public? one could use it to track/untrack a bunch of objects at once or similar
        mutex &                                     GetMutex( ) { return m_objectsMapMutex; }
        bool                                         TrackNoMutexLock( vaUIDObject * obj );
        bool                                         UntrackNoMutexLock( vaUIDObject * obj );
        template< class T >
        static T *                                   FindNoMutexLock( const vaGUID & uid );

        void                                         UntrackIfTracked( vaUIDObject * obj )                   { std::unique_lock<mutex> mapLock( m_objectsMapMutex ); if( obj->m_tracked ) UntrackNoMutexLock(obj);    }
    };

    // inline 

    inline bool vaUIDObject::UIDObject_IsTracked( ) const
    {
        return vaUIDObjectRegistrar::GetInstance( ).IsTracked( this );
    }

    inline bool vaUIDObject::UIDObject_Track( )
    {
        return vaUIDObjectRegistrar::GetInstance( ).Track( this );
    }

    inline bool vaUIDObject::UIDObject_Untrack( )
    {
        return vaUIDObjectRegistrar::GetInstance( ).Untrack( this );
    }

    inline bool vaUIDObjectRegistrar::IsTracked( const vaUIDObject * obj ) const
    {
        std::unique_lock<mutex> mapLock( vaUIDObjectRegistrar::GetInstance( ).m_objectsMapMutex );
        return obj->m_tracked;
    }

    template< class T>
    inline T * vaUIDObjectRegistrar::Find( const vaGUID & uid )
    {
        std::unique_lock<mutex> mapLock( vaUIDObjectRegistrar::GetInstance( ).m_objectsMapMutex );
        return FindNoMutexLock<T>( uid );
    }

    template< class T>
    inline T * vaUIDObjectRegistrar::FindNoMutexLock( const vaGUID & uid )
    {
        if( uid == vaCore::GUIDNull( ) )
            return nullptr;

        auto it = vaUIDObjectRegistrar::GetInstance( ).m_objectsMap.find( uid );
        if( it == vaUIDObjectRegistrar::GetInstance( ).m_objectsMap.end( ) )
        {
            return nullptr;
        }
        else
        {
            if( !it->second->m_tracked )
            {
                VA_ERROR( "vaUIDObjectRegistrar::FindNoMutexLock() - Something has gone really bad here - object is not marked as tracked but was found in the map. Don't ignore it." );
                return nullptr;
            }
#ifdef _DEBUG
            assert( it->second->m_uid == uid );
            T * ret = dynamic_cast<T*>( it->second );
            assert( ret != NULL );
            return ret;
#else
            return static_cast<T*>( it->second );
#endif
        }
    }

    template< class T >
    inline std::shared_ptr<T> vaUIDObjectRegistrar::FindCached( const vaGUID & uid, std::weak_ptr<T> & inOutCachedPtr )
    {
        if( uid == vaCore::GUIDNull( ) )
        {
            inOutCachedPtr.reset( );
            return nullptr;
        }

        shared_ptr<T> object = inOutCachedPtr.lock( );

        if( object == nullptr || object->m_uid != uid )
        {
            std::unique_lock<mutex> mapLock( vaUIDObjectRegistrar::GetInstance( ).m_objectsMapMutex );
            T * objPtr = FindNoMutexLock<T>( uid );
            if( objPtr != nullptr )
            {
                object = std::static_pointer_cast<T>( objPtr->shared_from_this( ) );
                inOutCachedPtr = object;
            }
            else
            {
                object = nullptr;
                inOutCachedPtr.reset( );
            }
        }
        return object;
    }

    inline bool SaveUIDObjectUID( vaStream & outStream, const shared_ptr<vaUIDObject> & obj )
    {
        if( obj == nullptr )
        {
            VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaGUID>( vaCore::GUIDNull( ) ) );
        }
        else
        {
            VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaGUID>( obj->UIDObject_GetUID( ) ) );
        }
        return true;
    }

}