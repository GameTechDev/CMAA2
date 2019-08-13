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

#include "vaCameraBase.h"

#include "vaCameraControllers.h"

#include "IntegratedExternals/vaImguiIntegration.h"

//#define HACKY_FLYTHROUGH_RECORDER
#ifdef HACKY_FLYTHROUGH_RECORDER
#include "Core/vaInput.h"
#endif

using namespace VertexAsylum;

#pragma warning( disable : 4996 )


vaCameraBase::vaCameraBase( bool visibleInUI ) : vaUIPanel( "Camera", 0, visibleInUI, vaUIPanel::DockLocation::DockedLeftBottom ), m_visibleInUI( visibleInUI )
{
    m_YFOVMain          = true;
    m_YFOV              = 60.0f / 180.0f * VA_PIf;
    m_XFOV              = 0.0f;
    m_aspect            = 1.0f;
    m_nearPlane         = 0.1f;
    m_farPlane          = 100000.0f;
    m_viewportWidth     = 64;
    m_viewportHeight    = 64;
    m_position          = vaVector3( 0.0f, 0.0f, 0.0f );
    m_orientation       = vaQuaternion::Identity;
    //
    m_viewTrans         = vaMatrix4x4::Identity;
    m_projTrans         = vaMatrix4x4::Identity;
    //
    m_useReversedZ      = true;
    //
    m_subpixelOffset    = vaVector2( 0.0f, 0.0f );
    //
    UpdateSecondaryFOV( );
}
//
vaCameraBase::~vaCameraBase( )
{
}
//
vaCameraBase::vaCameraBase( const vaCameraBase & other ) : vaUIPanel( "CameraCopy", 0, false ), m_visibleInUI( false )
{
    *this = other;
}

vaCameraBase & vaCameraBase::operator = ( const vaCameraBase & other )
{
    assert( this != &other );

    // this assert is a reminder that you might have to update below if you have added new variables
    int dbgSizeOfSelf = sizeof(*this);
    assert( dbgSizeOfSelf == 416 ); dbgSizeOfSelf;

    m_YFOV              = other.m_YFOV;
    m_XFOV              = other.m_XFOV;
    m_YFOVMain          = other.m_YFOVMain;
    m_aspect            = other.m_aspect;
    m_nearPlane         = other.m_nearPlane;
    m_farPlane          = other.m_farPlane;
    m_viewportWidth     = other.m_viewportWidth;
    m_viewportHeight    = other.m_viewportHeight;
    m_useReversedZ      = other.m_useReversedZ;
    m_position          = other.m_position;
    m_orientation       = other.m_orientation;
    m_worldTrans        = other.m_worldTrans;
    m_viewTrans         = other.m_viewTrans;
    m_projTrans         = other.m_projTrans;
    m_direction         = other.m_direction;
    m_subpixelOffset    = other.m_subpixelOffset;
    return *this;
}

//
#define RETURN_FALSE_IF_FALSE( x ) if( !(x) ) return false;
//
// void vaCameraBase::UpdateFrom( vaCameraBase & copyFrom )
// {
//     m_YFOVMain          = copyFrom.m_YFOVMain      ;
//     m_YFOV              = copyFrom.m_YFOV          ;
//     m_XFOV              = copyFrom.m_XFOV          ;
//     m_aspect            = copyFrom.m_aspect        ;
//     m_nearPlane         = copyFrom.m_nearPlane     ;
//     m_farPlane          = copyFrom.m_farPlane      ;
//     m_viewportWidth     = copyFrom.m_viewportWidth ;
//     m_viewportHeight    = copyFrom.m_viewportHeight; 
//     m_position          = copyFrom.m_position      ;
//     m_orientation       = copyFrom.m_orientation   ;
//     UpdateSecondaryFOV( );
// }
//
bool vaCameraBase::Load( vaStream & inStream )
{
    float dummyAspect;
    int dummyWidth;
    int dummyHeight;

    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_YFOVMain )       );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_YFOV )           );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_XFOV )           );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( dummyAspect )      );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_nearPlane )      );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_farPlane )       );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( dummyWidth )  );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( dummyHeight ) );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_position )       );
    RETURN_FALSE_IF_FALSE( inStream.ReadValue( m_orientation )    );
    UpdateSecondaryFOV( );
    return true;
}
//
bool vaCameraBase::Save( vaStream & outStream ) const
{
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_YFOVMain        )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_YFOV            )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_XFOV            )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_aspect          )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_nearPlane       )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_farPlane        )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_viewportWidth   )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_viewportHeight  )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_position        )  );
    RETURN_FALSE_IF_FALSE( outStream.WriteValue( m_orientation     )  );
    return true;
}
//
void vaCameraBase::AttachController( const std::shared_ptr<vaCameraControllerBase> & cameraController )
{
    if( cameraController == nullptr && m_controller != nullptr )
    {
        m_controller->CameraAttached( nullptr );
        m_controller = nullptr;
        return;
    }

    if( cameraController != nullptr )
    {
        {   // new controller is currently attached to another camera?
            auto alreadyAttachedCamera = cameraController->GetAttachedCamera( );
            if( alreadyAttachedCamera != nullptr )
            {
                // detach previously attached
                alreadyAttachedCamera->AttachController( nullptr );
            }
        }
        {   // camera has an existing controller attached?
            auto alreadyAttachedCamera = (m_controller==nullptr)?(nullptr):(m_controller->GetAttachedCamera( ));
            if( alreadyAttachedCamera != nullptr )
            {
                // detach previously attached
                alreadyAttachedCamera->AttachController( nullptr );
            }
        }
        m_controller = cameraController;
        m_controller->CameraAttached( this->shared_from_this( ) );
    }
    else
    {
        m_controller = cameraController;
    }

}
//
void vaCameraBase::Tick( float deltaTime, bool hasFocus )
{
    if( (m_controller != nullptr) && (deltaTime != 0.0f) )
        m_controller->CameraTick( deltaTime, *this, hasFocus );

    UpdateSecondaryFOV( );

    const vaQuaternion & orientation = GetOrientation( );
//    const vaVector3 & position = GetPosition( );
    
    m_worldTrans = vaMatrix4x4::FromQuaternion( orientation );
    m_worldTrans.SetTranslation( m_position );

    m_direction = m_worldTrans.GetAxisZ();  // forward

    m_viewTrans = m_worldTrans.Inverse( );

    if( m_useReversedZ )
        m_projTrans = vaMatrix4x4::PerspectiveFovLH( m_YFOV, m_aspect, m_farPlane, m_nearPlane );
    else
        m_projTrans = vaMatrix4x4::PerspectiveFovLH( m_YFOV, m_aspect, m_nearPlane, m_farPlane );

    if( m_subpixelOffset != vaVector2( 0, 0 ) )
    {
        m_projTrans = m_projTrans * vaMatrix4x4::Translation( 2.0f * m_subpixelOffset.x / (float)m_viewportWidth, -2.0f * m_subpixelOffset.y / (float)m_viewportHeight, 0.0f );
    }

    // a hacky way to record camera flythroughs!
#ifdef HACKY_FLYTHROUGH_RECORDER
    if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( ( vaKeyboardKeys )'K' ) && vaInputKeyboardBase::GetCurrent( )->IsKeyDown( KK_CONTROL ) )
    {
        vaFileStream fileOut;
        if( fileOut.Open( vaCore::GetExecutableDirectory( ) + L"camerakeys.txt", FileCreationMode::Append ) )
        {
            string newKey = vaStringTools::Format( "m_flythroughCameraController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( %.3ff, %.3ff, %.3ff ), vaQuaternion( %.3ff, %.3ff, %.3ff, %.3ff ), keyTime ) ); keyTime+=keyTimeStep;\n\n", 
                m_position.x, m_position.y, m_position.z, m_orientation.x, m_orientation.y, m_orientation.z, m_orientation.w );
            fileOut.WriteTXT( newKey );
            VA_LOG( "Logging camera key: %s", newKey.c_str() );
        }
    }
#endif
}
//
void vaCameraBase::TickManual( const vaVector3 & position, const vaQuaternion & orientation, const vaMatrix4x4 & projection )
{
    assert( m_controller == nullptr );

    m_position          = position;
    m_orientation       = orientation;
    m_projTrans         = projection;

    m_worldTrans = vaMatrix4x4::FromQuaternion( orientation );
    m_worldTrans.SetTranslation( m_position );
    m_direction = m_worldTrans.GetAxisZ();  // forward
    m_viewTrans = m_worldTrans.Inverse( );

    float tanHalfFOVY   = 1.0f / m_projTrans.m[1][1];
    float tanHalfFOVX   = 1.0F / m_projTrans.m[0][0];
    m_YFOV      = atanf( tanHalfFOVY ) * 2;
    m_aspect    = tanHalfFOVX / tanHalfFOVY;
    m_XFOV      = m_YFOV / m_aspect;
}
//
void vaCameraBase::SetViewportSize( int width, int height )
{
    m_viewportWidth = width;
    m_viewportHeight = height;
    m_aspect = width / (float)height;
}
//
void vaCameraBase::CalcFrustumPlanes( vaPlane planes[6] ) const
{
    vaMatrix4x4 cameraViewProj = m_viewTrans * m_projTrans;

    vaGeometry::CalculateFrustumPlanes( planes, cameraViewProj );
}
//
void vaCameraBase::GetZOffsettedProjMatrix( vaMatrix4x4 & mat, float zModMul, float zModAdd )
{
    UpdateSecondaryFOV( );

    float modNear   = m_nearPlane * zModMul + zModAdd;
    float modFar    = m_farPlane * zModMul + zModAdd;

    if( m_useReversedZ )
        mat = vaMatrix4x4::PerspectiveFovLH( m_YFOV, m_aspect, modFar, modNear );
    else
        mat = vaMatrix4x4::PerspectiveFovLH( m_YFOV, m_aspect, modNear, modFar );
}
//
void vaCameraBase::SetOrientationLookAt( const vaVector3 & lookAtPos, const vaVector3 & upVector )
{
    vaMatrix4x4 lookAt = vaMatrix4x4::LookAtLH( m_position, lookAtPos, upVector );

    SetOrientation( vaQuaternion::FromRotationMatrix( lookAt ).Inversed( ) );
}
//
void vaCameraBase::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    //ImGui::PushStyleColor( ImGuiCol_Text, titleColor );
    //ImGui::Text( "Info:" );
    //ImGui::PopStyleColor( 1 );
    ImGui::Text( vaStringTools::Format( "Camera (pos: %.2f, %.2f, %.2f, dir: %.3f, %.3f, %.3f)", m_position.x, m_position.y, m_position.z, m_direction.x, m_direction.y, m_direction.z ).c_str() );

    if( m_YFOVMain )
    {
        float yfov = m_YFOV / (VA_PIf) * 180.0f;
        ImGui::InputFloat( "FOV Y", &yfov, 5.0f, 0.0f );
        m_YFOV = vaMath::Clamp( yfov, 20.0f, 140.0f ) * (VA_PIf) / 180.0f;
        if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Camera Y field of view" );
    }
    else
    {
        float xfov = m_XFOV / (VA_PIf) * 180.0f ;
        ImGui::InputFloat( "FOV X", &xfov, 5.0f, 0.0f );
        m_XFOV = vaMath::Clamp( xfov, 20.0f, 140.0f ) * (VA_PIf) / 180.0f;
        if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Camera X field of view" );
    }

    if( m_controller == nullptr )
        ImGui::Text( "No controller attached" );
    else
    {
        ImGui::Text( "Attached controller: '%s'", m_controller->UIPropertiesItemGetDisplayName().c_str() );
        m_controller->UIPropertiesItemDraw();
    }
#endif
}

/*
void vaCameraBase::FillSelectionParams( vaSRMSelectionParams & selectionParams )
{
selectionParams.MainCameraPos = XMFLOAT4( m_position.x, m_position.y, m_position.z, 1.0f );
selectionParams.MainCameraDir = XMFLOAT4( m_lookDir.x, m_lookDir.y, m_lookDir.z, 1.0f );

XMVECTOR planes[6];
GetFrustumPlanes( planes );
for( int i = 0; i < 6; i++ )
XMStoreFloat4( &selectionParams.MainCameraFrustum[i], planes[i] );

selectionParams.MainCameraClipNear  = m_nearPlane;
selectionParams.MainCameraClipFar   = m_farPlane;
selectionParams.MainCameraViewRange = m_farPlane;
}
//
void vaCameraBase::FillRenderParams( vaSRMRenderParams & renderParams )
{
renderParams.CameraView = *GetViewMatrix();
renderParams.CameraProj = *GetProjMatrix();

renderParams.CameraPos = XMFLOAT4( m_position.x, m_position.y, m_position.z, 1.0f );
renderParams.CameraDir = XMFLOAT4( m_lookDir.x, m_lookDir.y, m_lookDir.z, 1.0f );

renderParams.CameraClipNear      = m_nearPlane;
renderParams.CameraClipFar       = m_farPlane;
renderParams.CameraFOVY          = m_FOV;
renderParams.CameraAspectRatio   = m_aspect;
}
*/
//
void vaCameraBase::UpdateSecondaryFOV( )
{
    if( m_YFOVMain )
    {
        m_XFOV = m_YFOV / m_aspect;
    }
    else
    {
        m_YFOV = m_XFOV * m_aspect;
    }
}
//
void vaCameraBase::GetScreenWorldRay( const vaVector2 & screenPos, vaVector3 & outRayPos, vaVector3 & outRayDir )
{
    vaVector3 screenNearNDC = vaVector3( screenPos.x / m_viewportWidth * 2.0f - 1.0f, 1.0f - screenPos.y / m_viewportHeight * 2.0f, m_useReversedZ?1.0f:0.0f );
    vaVector3 screenFarNDC = vaVector3( screenPos.x / m_viewportWidth * 2.0f - 1.0f, 1.0f - screenPos.y / m_viewportHeight * 2.0f, m_useReversedZ?0.0f:1.0f );

    vaMatrix4x4 viewProj = GetViewMatrix( ) * GetProjMatrix( );
    vaMatrix4x4 viewProjInv;

    if( !viewProj.Inverse( viewProjInv ) )
    {
        assert( false );
        return;
    }
    outRayPos = vaVector3::TransformCoord( screenNearNDC, viewProjInv );
    outRayDir = (vaVector3::TransformCoord( screenFarNDC, viewProjInv ) - outRayPos).Normalized();
}
