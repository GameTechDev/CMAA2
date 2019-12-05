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

#include "Core/vaUI.h"

namespace VertexAsylum
{
    class vaCameraBase;

    class vaCameraControllerBase : public std::enable_shared_from_this<vaCameraControllerBase>, public vaUIPropertiesItem
    {
    protected:
        weak_ptr< vaCameraBase >                    m_attachedCamera;

    protected:
        vaCameraControllerBase() {}
    
    public:
        virtual ~vaCameraControllerBase() {}

    protected:
        friend class vaCameraBase;

        shared_ptr<vaCameraBase>                    GetAttachedCamera( ) { return m_attachedCamera.lock(); }

        virtual void                                CameraAttached( const shared_ptr<vaCameraBase> & camera );
        virtual void                                CameraTick( float deltaTime, vaCameraBase & camera, bool hasFocus )  { deltaTime; camera; hasFocus; }

    private:
        virtual string                              UIPropertiesItemGetDisplayName( ) const override    { return "CameraControllerBase"; }
        virtual void                                UIPropertiesItemDraw( ) override                    { }
    };

    class vaCameraControllerFreeFlight : public vaCameraControllerBase
    {

    protected:
        float                   m_yaw;
        float                   m_pitch;
        float                   m_roll;

        // a reference for yaw pitch roll calculations: default is X is forward, Z is up, Y is right
        const vaMatrix4x4       m_baseOrientation;

        float                   m_accumMouseDeltaX;
        float                   m_accumMouseDeltaY;
        vaVector3               m_accumMove;
        float                   m_rotationSpeed;
        float                   m_movementSpeed;
        float                   m_inputSmoothingLerpK;

        float                   m_movementSpeedAccelerationModifier;

        bool                    m_moveWhileNotCaptured;

    private:

    public:
        vaCameraControllerFreeFlight( );
        virtual  ~vaCameraControllerFreeFlight( );

    protected:
        virtual void                                CameraAttached( const shared_ptr<vaCameraBase> & camera );
        virtual void                                CameraTick( float deltaTime, vaCameraBase & camera, bool hasFocus );

    public:
        void                                        SetMoveWhileNotCaptured( bool moveWhileNotCaptured )    { m_moveWhileNotCaptured = moveWhileNotCaptured; }
        bool                                        GetMoveWhileNotCaptured( )                              { return m_moveWhileNotCaptured; }

    private:
        virtual string                              UIPropertiesItemGetDisplayName( ) const override   { return "CameraControllerFreeFlight"; }
        virtual void                                UIPropertiesItemDraw( ) override                    { }
    };

    class vaCameraControllerFlythrough : public vaCameraControllerBase
    {
    
    public:
        struct Keyframe
        {
            vaVector3               Position;
            vaQuaternion            Orientation;
            float                   Time;
            float                   UserParam0;
            float                   UserParam1;

            Keyframe( const vaVector3 & position, const vaQuaternion & orientation, float time, float userParam0 = 0.0f, float userParam1 = 0.0f ) : Position( position ), Orientation( orientation ), Time( time ), UserParam0( userParam0 ), UserParam1( userParam1 ) { }
        };

    protected:
        std::vector<Keyframe>       m_keys;
        float                       m_currentTime;
        float                       m_totalTime;
        bool                        m_enableLoop;

        float                       m_playSpeed;

        //float                       m_userParam0;
        //float                       m_userParam1;

        bool                        m_fixedUp;
        vaVector3                   m_fixedUpVec;

    private:

    public:
        vaCameraControllerFlythrough( );
        virtual  ~vaCameraControllerFlythrough( );

        void                                        AddKey( const Keyframe & key );
        
        float                                       GetTotalTime( ) const                                   { return m_totalTime; }
        float                                       GetPlayTime( ) const                                    { return m_currentTime; }
        void                                        SetPlayTime( float newTime )                            { if( m_enableLoop ) newTime = fmodf( newTime, m_totalTime ); m_currentTime = vaMath::Clamp( newTime, 0.0f, m_totalTime ); }
        float                                       GetPlaySpeed( ) const                                   { return m_playSpeed; }
        void                                        SetPlaySpeed( float newSpeed )                          { m_playSpeed = newSpeed; }
        bool                                        GetLoop( ) const                                        { return m_enableLoop; }
        void                                        SetLoop( bool enableLoop )                              { m_enableLoop = enableLoop; }
        
        void                                        SetFixedUp( bool enabled, const vaVector3 & upVec = vaVector3( 0.0f, 0.0f, 1.0f ) )   { m_fixedUp = enabled; m_fixedUpVec = upVec; }

    protected:
        bool                                        FindKeys( float time, int & keyIndexFrom, int & keyIndexTo );

    protected:
        virtual void                                CameraAttached( const shared_ptr<vaCameraBase> & camera );
        virtual void                                CameraTick( float deltaTime, vaCameraBase & camera, bool hasFocus );
    
    private:
        virtual string                              UIPropertiesItemGetDisplayName( ) const override   { return "CameraControllerFocusLocationsFlythrough"; }
        virtual void                                UIPropertiesItemDraw( ) override;
    };

}