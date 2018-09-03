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

#include "vaCore.h"

#include "vaPlatformBase.h"

// Frequently used includes
#include <assert.h>


namespace VertexAsylum
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Simple base class for a singleton.
    //  - ensures that the class is indeed a singleton
    //  - provides access to it
    //  1.) inherit YourClass from vaSingletonBase<YourClass>
    //  2.) you're responsible for creation/destruction of the object and its thread safety!
    //
    template< class SingletonType >
    class vaSingletonBase
    {
    private:
        static std::atomic<SingletonType *>   s_instance;

    protected:
        vaSingletonBase( )
        {
            SingletonType * previous = s_instance.exchange( static_cast<SingletonType *>( this ) );
            assert( previous == NULL );
             previous; // unreferenced in Release
        }
        virtual ~vaSingletonBase( )
        {
            SingletonType * previous = s_instance.exchange( nullptr );
            assert( previous != NULL );
            previous; // unreferenced in Release
        }

    public:

        static SingletonType &                GetInstance( ) { SingletonType * retVal = s_instance.load( ); assert( retVal != NULL ); return *retVal; }
        static SingletonType *                GetInstancePtr( ) { return s_instance; }
    };
    //
    template <class SingletonType>
    std::atomic<SingletonType *> vaSingletonBase<SingletonType>::s_instance; // = nullptr;
    ////////////////////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Simple base class for a multiton.
    //  - just keeps a list of objects of the type (array, not map/dictionary!)
    template< class MultitonType >
    class vaMultitonBase
    {
    public:
        struct LockedInstances
        {
            mutex &                         Mutex;
            const vector<MultitonType *> &  Instances;

            LockedInstances( mutex & mutex, vector<MultitonType *> & instances ) : Mutex(mutex), Instances(instances) { mutex.lock(); }
            ~LockedInstances( ) { Mutex.unlock(); }
        };
    private:
        static mutex                        s_allInstancesMutex;
        static vector<MultitonType *>       s_allInstances;
        
        int                                 m_instanceIndex;

    protected:
        vaMultitonBase( )
        {
            LockedInstances li = GetInstances(); // lock instances!
            m_instanceIndex = (int)li.Instances.size();
            s_allInstances.push_back( (MultitonType*)this );
        } 
        virtual ~vaMultitonBase( )
        {
            LockedInstances li = GetInstances(); // lock instances!
            
            assert( s_allInstances.size() > 0 );

            // we're not the last one? swap with the last 
            if( m_instanceIndex != li.Instances.size()-1 )
            {
                MultitonType * lastOne = s_allInstances.back();
                lastOne->m_instanceIndex = m_instanceIndex;
                s_allInstances[m_instanceIndex] = lastOne;
            }
            s_allInstances.pop_back(); 
        }

    public:

        static LockedInstances              GetInstances( ) { return LockedInstances( s_allInstancesMutex, s_allInstances ); }
    };
    //
    template <class MultitonType>
    mutex                                   vaMultitonBase<MultitonType>::s_allInstancesMutex;
    template <class MultitonType>
    vector<MultitonType *>                  vaMultitonBase<MultitonType>::s_allInstances;
    ////////////////////////////////////////////////////////////////////////////////////////////////
}