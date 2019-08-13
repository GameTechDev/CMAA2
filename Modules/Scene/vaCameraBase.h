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
    class vaCameraControllerBase;

    class vaCameraBase : public vaUIPanel, public std::enable_shared_from_this<vaCameraBase>
    {
    protected:
        // attached controller
        std::shared_ptr < vaCameraControllerBase >
            m_controller;
        //
        bool const                      m_visibleInUI;
        //                      
        // Primary values
        bool                            m_YFOVMain;
        bool                            m_useReversedZ;
        float                           m_YFOV;
        float                           m_XFOV;
        float                           m_aspect;
        float                           m_nearPlane;
        float                           m_farPlane;
        int                             m_viewportWidth;
        int                             m_viewportHeight;
        //
        vaVector2                       m_subpixelOffset;   // a.k.a. jitter!
        //
        // in world space
        vaVector3                       m_position;
        vaQuaternion                    m_orientation;
        //                            
        // Secondary values (updated by Tick() )
        vaVector3                       m_direction;
        vaMatrix4x4                     m_worldTrans;
        vaMatrix4x4                     m_viewTrans;
        vaMatrix4x4                     m_projTrans;
        //
    public:
        vaCameraBase( bool visibleInUI );
        virtual ~vaCameraBase( );
        //
        // everything but the m_controller gets copied!
        vaCameraBase( const vaCameraBase & other );
        vaCameraBase & operator = ( const vaCameraBase & other );
        //
    public:
        // All angles in radians!
        float                           GetYFOV( ) const                                    { return m_YFOV; }
        float                           GetXFOV( ) const                                    { return m_XFOV; }
        float                           GetAspect( ) const                                  { return m_aspect; }
        //                              
        void                            SetYFOV( float yFov )                               { m_YFOV = yFov; m_YFOVMain = true;    UpdateSecondaryFOV( ); }
        void                            SetXFOV( float xFov )                               { m_XFOV = xFov; m_YFOVMain = false;   UpdateSecondaryFOV( ); }
        bool                            GetYFOVMain( )                                      { return m_YFOVMain; }
        //
        void                            SetNearPlane( float nearPlane )                     { m_nearPlane = nearPlane; }
        void                            SetFarPlane( float farPlane )                       { m_farPlane = farPlane; }
        //
        float                           GetNearPlane( ) const                               { return m_nearPlane; }
        float                           GetFarPlane( ) const                                { return m_farPlane; }
        //
        void                            SetPosition( const vaVector3 & newPos )             { m_position = newPos; }
        void                            SetOrientation( const vaQuaternion & newOri )       { m_orientation = newOri; }
        void                            SetOrientationLookAt( const vaVector3 & lookAtPos, const vaVector3 & upVector = vaVector3( 0.0f, 0.0f, 1.0f ) );
        //
        const vaVector3 &               GetPosition( ) const                                 { return m_position; }
        const vaQuaternion &            GetOrientation( ) const                              { return m_orientation; }
        const vaVector3 &               GetDirection( ) const                                { return m_direction; }
        //                              
        void                            CalcFrustumPlanes( vaPlane planes[6] ) const;
        //                   
        const vaMatrix4x4 &             GetWorldMatrix( ) const                             { return m_worldTrans; }
        const vaMatrix4x4 &             GetViewMatrix( ) const                              { return m_viewTrans; }
        const vaMatrix4x4 &             GetProjMatrix( ) const                              { return m_projTrans; }
        // same as GetWorldMatrix !!
        const vaMatrix4x4 &             GetInvViewMatrix( ) const                           { return m_worldTrans; }
        //                      
        void                            GetZOffsettedProjMatrix( vaMatrix4x4 & outMat, float zModMul = 1.0f, float zModAdd = 0.0f );
        //                         
        // Using "ReversedZ" uses projection matrix that results in inverted NDC z, to better match floating point depth buffer precision.
        // Highly advisable if using floating point depth, but requires "inverted" depth testing and care in other parts.
        void                            SetUseReversedZ( bool useReversedZ )                { m_useReversedZ = useReversedZ; }
        bool                            GetUseReversedZ( ) const                            { return m_useReversedZ; }
        //
        //
        /*
        void                            FillSelectionParams( VertexAsylum::vaSRMSelectionParams & selectionParams );
        void                            FillRenderParams( VertexAsylum::vaSRMRenderParams & renderParams );
        */
        //
        virtual void                    SetViewportSize( int width, int height );
        //
        int                             GetViewportWidth( ) const                           { return m_viewportWidth; }
        int                             GetViewportHeight( ) const                          { return m_viewportHeight; }
        //
        void                            SetSubpixelOffset( vaVector2 & subpixelOffset )     { m_subpixelOffset = subpixelOffset; }  // a.k.a. 'jitter'
        vaVector2                       GetSubpixelOffset( )                                { return m_subpixelOffset; }
        //
        // void                            UpdateFrom( vaCameraBase & copyFrom );
        //
        bool                            Load( vaStream & inStream );
        bool                            Save( vaStream & outStream ) const;
        //
        const std::shared_ptr<vaCameraControllerBase> &
                                        GetAttachedController( )                    { return m_controller; }
        void                            AttachController( const std::shared_ptr<vaCameraControllerBase> & cameraController );
        
        // hasFocus only relevant if there's an attached controller - perhaps the controller should get this state from elsewhere
        void                            Tick( float deltaTime, bool hasFocus );

		// if getting all camera setup from outside (such as from VR headset), use this to set the pose and projection and update all dependencies
        void                            TickManual( const vaVector3 & position, const vaQuaternion & orientation, const vaMatrix4x4 & projection );

        // Compute worldspace position and direction of a ray going from the screenPos from near to far.
        // Don't forget to include 0.5 offsets to screenPos if converting from integer position (for ex., mouse coords) for the correct center of the pixel (if such precision is important).
        void                            GetScreenWorldRay( const vaVector2 & screenPos, vaVector3 & outRayPos, vaVector3 & outRayDir );
        void                            GetScreenWorldRay( const vaVector2i & screenPos, vaVector3 & outRayPos, vaVector3 & outRayDir )         { return GetScreenWorldRay( vaVector2( screenPos.x + 0.5f, screenPos.y + 0.5f ), outRayPos, outRayDir ); }

    private:
        // vaUIPanel
        // virtual string                  UIPanelGetDisplayName( ) const;
        virtual bool                    UIPanelIsListed( ) const                    { return m_visibleInUI; }
        virtual void                    UIPanelDraw( ) override;

    protected:
        void                            UpdateSecondaryFOV( );
        //
        virtual void                    SetAspect( float aspect )                    { m_aspect = aspect; }
        //*/
    };

}
