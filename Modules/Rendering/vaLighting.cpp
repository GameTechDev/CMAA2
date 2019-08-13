///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
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

#include "vaLighting.h"
#include "Rendering/Shaders/vaSharedTypes.h"

#include "Rendering/vaTextureHelpers.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

void vaLight::CorrectLimits( )
{
    Intensity       = vaVector3::ComponentMax( vaVector3( 0.0f, 0.0f, 0.0f ), Intensity );
    Direction       = Direction.Normalized();
    Up              = Up.Normalized();
    Size            = vaMath::Max( 1e-4f, Size );
    SpotInnerAngle  = vaMath::Clamp( SpotInnerAngle, 0.0f, VA_PIf );
    SpotOuterAngle  = vaMath::Clamp( SpotOuterAngle, SpotInnerAngle, VA_PIf ); 

    switch( Type )
    {
        case( vaLight::Type::Ambient ):     Size = 0.2f; CastShadows = false; break;
        case( vaLight::Type::Directional ): Size = 0.2f; break;
        case( vaLight::Type::Point ):       break;
        case( vaLight::Type::Spot ):        break;
        default: assert( false ); 
    }
}

bool vaLight::Serialize( vaXMLSerializer & serializer )
{
    if( serializer.GetVersion() <= 0 )
        if( !serializer.SerializeOpenChildElement( "Light" ) )
        { assert( false ); return false; }

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<string>( "Name", Name ) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "Type", (int32&)Type) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "Intensity"     ,   Intensity         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "Position"      ,   Position          ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "Direction"     ,   Direction         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "Up"            ,   Up                ) );
    //VERIFY_TRUE_RETURN_ON_FALSE( 
                                    serializer.Serialize<float>( "Size", Size );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<float>( "SpotInnerAngle",   SpotInnerAngle    ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<float>( "SpotOuterAngle",   SpotOuterAngle    ) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<bool>( "CastShadows",      CastShadows,     false ) );

    if( serializer.GetVersion() <= 0 )
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( "Light" ) );

    if( serializer.IsReading( ) )
    {
        CorrectLimits( );
    }

    return true;
}

void vaLight::UIPropertiesItemDraw( ) 
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    const vector<string> lightTypes = { "Ambient", "Directional", "Point", "Spot" };

    if( ImGui::Button( "Rename" ) )
        ImGuiEx_PopupInputStringBegin( "Rename light", Name );
    ImGuiEx_PopupInputStringTick( Name );

    ImGuiEx_Combo( "Type", (int&)Type, lightTypes );

    ImGui::InputFloat3( "Intensity", &Intensity.x );

    if( (Type == vaLight::Type::Point) || (Type == vaLight::Type::Spot) )
    {
        ImGui::InputFloat3( "Position", &Position.x );
        ImGui::InputFloat( "Size", &Size );
    }
    else
    {
        ImGui::InputFloat3( "Position (for UI)", &Position.x );
        ImGui::InputFloat( "Size (for UI)", &Size );
    }
    if( (Type == vaLight::Type::Directional) || (Type == vaLight::Type::Spot) )
    {
        ImGui::InputFloat3( "Direction", &Direction.x );
    }
    if( Type == vaLight::Type::Spot )
    {
        ImGui::InputFloat( "SpotInnerAngle", &SpotInnerAngle );
        ImGui::InputFloat( "SpotOuterAngle", &SpotOuterAngle );
    }
    ImGui::Checkbox( "CastShadows", &CastShadows );

    CorrectLimits( );
#endif
}

void VertexAsylum::vaLight::Reset( )
{
    *this = vaLight();
}

void vaFogSphere::CorrectLimits( )
{
    RadiusInner     = vaMath::Max( RadiusInner, 0.0f );
    RadiusOuter     = vaMath::Clamp( RadiusOuter, RadiusInner, 100000000.0f );
    BlendCurvePow   = vaMath::Clamp( BlendCurvePow  , 0.001f, 1000.0f );
    BlendMultiplier = vaMath::Clamp( BlendMultiplier, 0.0f, 1.0f );
}

bool vaFogSphere::Serialize( vaXMLSerializer & serializer )
{
 //   if( serializer.GetVersion() <= 0 )
 //       if( !serializer.SerializeOpenChildElement( "FogSphere" ) )
 //           { assert( false ); return false; }

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<bool>( "Enabled",          Enabled         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<bool>( "UseCustomCenter",  UseCustomCenter ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "Center",           Center          ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "Color",            Color           ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<float>( "RadiusInner",      RadiusInner     ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<float>( "RadiusOuter",      RadiusOuter     ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<float>( "BlendCurvePow",    BlendCurvePow   ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<float>( "BlendMultiplier",  BlendMultiplier ) );

//    if( serializer.GetVersion() <= 0 )
//        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( "FogSphere" ) );

    if( serializer.IsReading( ) )
    {
        CorrectLimits( );
    }

    return true;
}

void vaFogSphere::UIPropertiesItemDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::Checkbox( "Enabled", &Enabled );
    ImGui::Checkbox( "UseCustomCenter", &UseCustomCenter );
    ImGui::InputFloat3( "Center", &Center.x );
    
    ImVec4 col( Color.x, Color.y, Color.z, 1.0 );
    ImGui::ColorEdit3( "Color", &Color.x, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_Float );

    ImGui::InputFloat( "Inner radius",    &RadiusInner );
    ImGui::InputFloat( "Outer radius",    &RadiusOuter );

    ImGui::InputFloat( "Blend curve pow", &BlendCurvePow );
    ImGui::InputFloat( "Blend multiplier", &BlendMultiplier );

    CorrectLimits( );
#endif
}


vaLighting::vaLighting( const vaRenderingModuleParams & params ) : vaRenderingModule( vaRenderingModuleParams(params) ), 
    m_constantsBuffer( params ),
    m_applyDirectionalAmbientPS( params ),
    m_applyDirectionalAmbientShadowedPS( params ),
    vaUIPanel( "Lighting", 0, false, vaUIPanel::DockLocation::DockedLeftBottom )
{
    m_debugInfo = "Lighting";

    // just setup some basic lights
    m_lights.push_back( std::make_shared<vaLight>( vaLight::MakeAmbient( "DefaultAmbient", vaVector3( 0.05f, 0.05f, 0.15f ) ) ) );

    m_lights.push_back( std::make_shared<vaLight>( vaLight::MakeDirectional( "DefaultDirectional", vaVector3( 1.0f, 1.0f, 0.9f ), vaVector3( 0.0f, -1.0f, -1.0f ).Normalized() ) ) );
}

vaLighting::~vaLighting( )
{

}

bool vaLighting::AllocateShadowStorageTextureIndex( const shared_ptr<vaShadowmap> & shadowmap, int & outTextureIndex, shared_ptr<vaTexture> & outTextureArray )
{
    assert( shadowmap != nullptr );
    assert( &*shadowmap->GetLighting().lock() == this );
    assert( shadowmap->GetStorageTextureIndex() == -1 );
    auto light = shadowmap->GetLight().lock();

    switch( light->Type )
    {
        case( vaLight::Type::Ambient ):     
        {
            // no shadows for ambient light
            assert( false );
            return false;
        } break;
        case( vaLight::Type::Directional ):
        {
            // not yet implemented
            assert( false );
            return false;
        } break;
        case( vaLight::Type::Point ):
        case( vaLight::Type::Spot ): 
        {
            for( int i = 0; i < _countof(m_shadowCubeArrayCurrentUsers); i++ )
            {
                // slot not in use (either never used or weak_ptr pointing to deleted object)
                if( m_shadowCubeArrayCurrentUsers[i].lock( ) == nullptr )
                {
                    m_shadowCubeArrayCurrentUsers[i] = shadowmap;
                    outTextureIndex = i;
                    outTextureArray = m_shadowCubeArrayTexture;
                    return true;
                }
            }
            return false;
        }
        break;
    }
    assert( false ); 
    return false;
}

shared_ptr<vaShadowmap> vaLighting::FindShadowmapForLight( const shared_ptr<vaLight> & light )
{
    for( int i = 0; i < m_shadowmaps.size( ); i++ )
    {
        if( m_shadowmaps[i]->GetLight().lock() == light )
            return m_shadowmaps[i];
    }
    return nullptr;
}

void vaLighting::UpdateShaderConstants( vaSceneDrawContext & drawContext )
{
    vaMatrix4x4 mat = drawContext.Camera.GetViewMatrix( ) * drawContext.Camera.GetProjMatrix( );
    
    LightingShaderConstants consts;
    consts.FogCenter            = ( m_fogSettings.UseCustomCenter ) ? ( vaVector3::TransformCoord( m_fogSettings.Center, drawContext.Camera.GetViewMatrix() ) ) : ( vaVector3( 0.0f, 0.0f, 0.0f ) );
    consts.FogEnabled           = m_fogSettings.Enabled?1:0;
    consts.FogColor             = m_fogSettings.Color;
    consts.FogRadiusInner       = m_fogSettings.RadiusInner;
    consts.FogRadiusOuter       = m_fogSettings.RadiusOuter;
    consts.FogBlendCurvePow     = m_fogSettings.BlendCurvePow;
    consts.FogBlendMultiplier   = m_fogSettings.BlendMultiplier;
    consts.FogRange             = m_fogSettings.RadiusOuter - m_fogSettings.RadiusInner;
    
    
    if( m_envmapTexture != nullptr )
    {
        consts.EnvmapEnabled    = true;
        consts.EnvmapMultiplier = m_envmapColorMultiplier;
        consts.EnvmapRotation   = drawContext.Camera.GetInvViewMatrix() * vaMatrix4x4( m_envmapRotation );
    }
    else
    {
        consts.EnvmapEnabled    = false;
        consts.EnvmapMultiplier = 0.0f;
        consts.EnvmapRotation   = vaMatrix4x4::Identity;
    }

    consts.LightCountDirectional    = 0;
    consts.LightCountSpotAndPoint   = 0;

    consts.AmbientLightIntensity     = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f );

    vaMatrix4x4 viewMatrix = drawContext.Camera.GetViewMatrix( );

    vector<ShaderLightSpot> pointLights;

    for( int i = 0; i < m_lights.size( ); i++ )
    {
        const vaLight & light = *m_lights[i];

        shared_ptr<vaShadowmap> shadowmap = FindShadowmapForLight( m_lights[i] );
        if( shadowmap != nullptr && shadowmap->GetStorageTextureIndex() == -1 )
            shadowmap = nullptr;

        // doesn't make sense to have negative intensity (negative lights not supported, sorry)
        assert( light.Intensity.x >= 0.0f && light.Intensity.y >= 0.0f && light.Intensity.z >= 0.0f );

        // skip if intensity is low
        if( light.Intensity.Length() < VA_EPSf )
            continue;
       
        // (for point&spot lights that attenuate - this should probably go elsewhere, be part of the scene or something)
        float effectiveRadius = light.EffectiveRadius( );

        switch( light.Type )
        {
        case( vaLight::Type::Ambient ):
            consts.AmbientLightIntensity += vaVector4( light.Intensity, 0.0f );
            break;
        case( vaLight::Type::Directional ):
            //assert( vaMath::NearEqual( light.Direction.Length(), 1.0f, 1e-4f ) );
            if( consts.LightCountDirectional + 1 < ShaderLightDirectional::MaxLights )
            {
                consts.LightsDirectional[consts.LightCountDirectional].Intensity        = light.Intensity;
                consts.LightsDirectional[consts.LightCountDirectional].IntensityLength  = light.Intensity.Length();
                consts.LightsDirectional[consts.LightCountDirectional].Direction        = vaVector3::TransformNormal( light.Direction.Normalized(), viewMatrix ).Normalized();
                consts.LightsDirectional[consts.LightCountDirectional].Dummy1           = 0.0f;
                if( shadowmap != nullptr )
                {
                    assert( false );
                }
                consts.LightCountDirectional++;
            }
            else { VA_WARN( "vaLighting - requested more than the max number of directional lights (%d)", ShaderLightDirectional::MaxLights ); }
            break;
        case( vaLight::Type::Point ):
            assert( light.Size > 0 );
            {
                ShaderLightSpot shLight;
                shLight.Intensity           = light.Intensity;
                shLight.IntensityLength     = light.Intensity.Length();
                shLight.Position            = vaVector3::TransformCoord( light.Position, viewMatrix );
                shLight.EffectiveRadius     = effectiveRadius;
                shLight.Direction           = vaVector3( 0.0f, 0.0f, 0.0f );
                shLight.Size                = light.Size;
                shLight.SpotInnerAngle      = VA_PIf + VA_EPSf;// 10.0f; // dummy values
                shLight.SpotOuterAngle      = VA_PIf + 2*VA_EPSf;// 10.0f; // dummy values
                shLight.CubeShadowIndex     = (float)((shadowmap != nullptr)?(shadowmap->GetStorageTextureIndex()):(-1));
                shLight.Dummy1              = 0.0f;
                pointLights.push_back( shLight );
            }
            break;
        case( vaLight::Type::Spot ):
            assert( light.Size > 0 );
            if( consts.LightCountSpotAndPoint + 1 < ShaderLightSpot::MaxLights )
            {
                ShaderLightSpot shLight;
                shLight.Intensity           = light.Intensity;
                shLight.IntensityLength     = light.Intensity.Length();
                shLight.Position            = vaVector3::TransformCoord( light.Position, viewMatrix );
                shLight.EffectiveRadius     = effectiveRadius;
                shLight.Direction           = vaVector3::TransformNormal( light.Direction.Normalized(), viewMatrix ).Normalized();
                shLight.Size                = light.Size;
                shLight.SpotInnerAngle      = light.SpotInnerAngle;
                shLight.SpotOuterAngle      = light.SpotOuterAngle;
                shLight.CubeShadowIndex     = (float)((shadowmap != nullptr)?(shadowmap->GetStorageTextureIndex()):(-1));
                shLight.Dummy1              = 0.0f;
                consts.LightsSpotAndPoint[consts.LightCountSpotAndPoint] = shLight;
                consts.LightCountSpotAndPoint++;
            }
            else { VA_WARN( "vaLighting - requested more than the max number of spot lights (%d)", ShaderLightSpot::MaxLights ); }
            break;
        default: assert( false ); // error or not implemented
        }
    }

    // so far we've only added spot lights above, points went to a separate array to be added below
    consts.LightCountSpotOnly = consts.LightCountSpotAndPoint;

    // since sin(x) is close to x for very small x values then this actually works good enough
    consts.CubeShadowDirJitterOffset            = m_shadowCubePCFFilterScale / m_shadowCubeResolution;
    consts.CubeShadowViewDistJitterOffset       = GetCubeShadowViewspaceDepthOffsets( ).y;
    consts.CubeShadowViewDistJitterOffsetSqrt   = vaMath::Sqrt( consts.CubeShadowViewDistJitterOffset );

    for( int i = 0; i < pointLights.size( ); i++ )
    {
        if( consts.LightCountSpotAndPoint + 1 < ShaderLightSpot::MaxLights )
        {
            consts.LightsSpotAndPoint[consts.LightCountSpotAndPoint] = pointLights[i];
            consts.LightCountSpotAndPoint++;
        }
        else { VA_WARN( "vaLighting - requested more than the max number of spot/point lights (%d)", ShaderLightSpot::MaxLights ); }
    }

    m_constantsBuffer.Update( drawContext.RenderDeviceContext, consts );
}

void vaLighting::SetLights( const vector<shared_ptr<vaLight>> & lights )
{ 
    m_lights = lights;
}

shared_ptr<vaShadowmap> vaLighting::GetNextHighestPriorityShadowmapForRendering( )
{
    shared_ptr<vaShadowmap> ret = nullptr;
    float highestFoundAge       = 0.0f;

    for( int i = 0; i < m_shadowmaps.size( ); i++ )
    {
        if( m_shadowmaps[i]->GetDataAge( ) > highestFoundAge )
        {
            ret             = m_shadowmaps[i];
            highestFoundAge = m_shadowmaps[i]->GetDataAge( );
        }
    }
    return ret;
}

void vaLighting::DestroyShadowmapTextures( )
{
    assert( m_shadowmapTexturesCreated );
    assert( false ); // not implemented yet - should clean up links

    m_shadowCubeDepthTexture = nullptr;
    m_shadowCubeArrayTexture = nullptr;
    
    m_shadowmapTexturesCreated = false;
}

void vaLighting::CreateShadowmapTextures( )
{
    assert( !m_shadowmapTexturesCreated );

    vaTexture::SetNextCreateFastClearDSV( m_shadowCubeDepthFormat, 0.0f, 0 );
    m_shadowCubeDepthTexture = vaTexture::Create2D( GetRenderDevice(), m_shadowCubeDepthFormat, m_shadowCubeResolution, m_shadowCubeResolution, 1, 6, 1, vaResourceBindSupportFlags::DepthStencil,
        vaResourceAccessFlags::Default, vaResourceFormat::Unknown, vaResourceFormat::Unknown, m_shadowCubeDepthFormat, vaResourceFormat::Unknown, vaTextureFlags::Cubemap, vaTextureContentsType::DepthBuffer );

    vaTexture::SetNextCreateFastClearRTV( m_shadowCubeFormat, vaVector4( 10000.0f, 10000.0f, 10000.0f, 10000.0f ) );
    m_shadowCubeArrayTexture = vaTexture::Create2D( GetRenderDevice(), m_shadowCubeFormat, m_shadowCubeResolution, m_shadowCubeResolution, 1, 6*m_shadowCubeMapCount, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::RenderTarget,
        vaResourceAccessFlags::Default, m_shadowCubeFormat, m_shadowCubeFormat, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaTextureFlags::Cubemap, vaTextureContentsType::LinearDepth );

    m_shadowmapTexturesCreated = true;
}

void vaLighting::Tick( float deltaTime )
{
    if( !m_shadowmapTexturesCreated )
        CreateShadowmapTextures( );

    // create shadowmaps for lights that need shadows; if already there, don't re-create, but if shadowmap exists
    // without a corresponding light then remove it (no pooling yet but probably not needed since textures are
    // held by vaLighting anyways)
    vector<bool> shadowMapInUse( m_shadowmaps.size(), false );
    for( int i = 0; i < m_lights.size(); i++ ) 
    {
        if( m_lights[i]->CastShadows && (m_lights[i]->Intensity.Length() >= VA_EPSf) )
        {
            bool found = false;
            for( int j = 0; !found && j < m_shadowmaps.size( ); j++ )
            {
                if( m_lights[i] == m_shadowmaps[j]->GetLight().lock() )
                {
                    shadowMapInUse[j] = true;
                    found = true;
                }
            }
            if( !found )
            {
                shared_ptr<vaShadowmap> newShadowMap = vaShadowmap::Create( GetRenderDevice(), m_lights[i], this->shared_from_this() );
                if( newShadowMap != nullptr )
                {
                    m_shadowmaps.push_back( newShadowMap );
                    shadowMapInUse.push_back( true );
                }
            }
        }
    }
    assert( shadowMapInUse.size() == m_shadowmaps.size() );
    // if not in use, remove - not optimal but hey good enough for now
    for( int j = 0; j < m_shadowmaps.size( ); j++ )
    {
        if( !shadowMapInUse[j] )
        {
            m_shadowmaps.erase( m_shadowmaps.begin()+j );
            shadowMapInUse.erase( shadowMapInUse.begin()+j );
            j--;
        }
    }

    for( int i = 0; i < m_shadowmaps.size( ); i++ )
    {
        m_shadowmaps[i]->Tick( deltaTime );
    }
}

void vaShadowmap::Tick( float deltaTime ) 
{ 
    auto & light = m_light.lock( ); 
    if( light == nullptr )
    {
        assert( false );
        return;
    }

   
    vaVector3 newLightPos = light->Position;

    bool hasChanges = m_includeDynamicObjects;
    if( !m_lastLightState.NearEqual( *light ) )
    {
        m_lastLightState = *light;
        hasChanges = true;
    }

    if( hasChanges ) 
        m_dataAge += deltaTime; 
}

void vaCubeShadowmap::Tick( float deltaTime )
{
    shared_ptr<vaLighting> lighting = m_lightingSystem.lock();
    assert( lighting != nullptr );
    
    // find texture storage if available
    if( m_storageTextureIndex == -1 )
    {
        int outTextureIndex;
        shared_ptr<vaTexture> outTextureArray;
        if( lighting->AllocateShadowStorageTextureIndex( this->shared_from_this( ), outTextureIndex, outTextureArray ) )
        {
            m_storageTextureIndex = outTextureIndex;

            // reset the light 
            m_lastLightState = vaLight();

            const shared_ptr<vaTexture> & cubeDepth = lighting->GetCubemapDepthTexture( );
            assert( cubeDepth != nullptr );

            m_cubemapArraySRV = vaTexture::CreateView( outTextureArray, vaResourceBindSupportFlags::ShaderResource, outTextureArray->GetSRVFormat(), vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaResourceFormat::Unknown, 
                    vaTextureFlags::Cubemap | vaTextureFlags::CubemapButArraySRV, 0, -1, outTextureIndex*6, 6 );

            for( int i = 0; i < 6; i++ )
            {
                m_cubemapArrayRTVs[i] = vaTexture::CreateView( outTextureArray, vaResourceBindSupportFlags::RenderTarget, vaResourceFormat::Unknown, outTextureArray->GetRTVFormat(), vaResourceFormat::Unknown, vaResourceFormat::Unknown, 
                    vaTextureFlags::Cubemap, 0, -1, outTextureIndex*6+i, 1 );
                m_cubemapSliceDSVs[i] = vaTexture::CreateView( cubeDepth, vaResourceBindSupportFlags::DepthStencil, vaResourceFormat::Unknown, vaResourceFormat::Unknown, cubeDepth->GetDSVFormat(), vaResourceFormat::Unknown, 
                    vaTextureFlags::Cubemap, 0, -1, i, 1 );
            }

        }
        else
        {
            // ran out of space? oh well, just skip this one
            m_storageTextureIndex = -1;
        }
    }

    vaShadowmap::Tick( deltaTime );
}

void vaLighting::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::Text( "Lights: %d", (int)m_lights.size() );

    ImGui::Text( "Shadowmaps: %d", (int)m_shadowmaps.size() );
    vaUIPropertiesItem * ptrsToDisplay[4096];
    int countToShow = std::min( (int)m_shadowmaps.size( ), (int)_countof( ptrsToDisplay ) );
    for( int i = 0; i < countToShow; i++ ) ptrsToDisplay[i] = m_shadowmaps[i].get( );

    int currentShadowmap = -1;
    for( int i = 0; i < countToShow; i++ )
    {
        if( m_UI_SelectedShadow.lock( ) == m_shadowmaps[i] )
            currentShadowmap = i;
        ptrsToDisplay[i] = m_shadowmaps[i].get( );
    }

    vaUIPropertiesItem::DrawList( "Shadowmaps", ptrsToDisplay, countToShow, currentShadowmap, 0.0f, 90, 140.0f + ImGui::GetContentRegionAvailWidth( ) );
    if( currentShadowmap >= 0 && currentShadowmap < countToShow )
        m_UI_SelectedShadow = m_shadowmaps[currentShadowmap];

    ImGui::Text("Shadowmap offset settings");
    bool changed = false;
    changed |= ImGui::InputFloat( "CubeFlatOffsetAdd" , &m_shadowCubeFlatOffsetAdd , 0.1f );
    changed |= ImGui::InputFloat( "CubeFlatOffsetScale" , &m_shadowCubeFlatOffsetScale , 0.1f );
    changed |= ImGui::InputFloat( "CubeSlopeOffsetAdd", &m_shadowCubeSlopeOffsetAdd, 0.1f );
    changed |= ImGui::InputFloat( "CubeSlopeOffsetScale", &m_shadowCubeSlopeOffsetScale, 0.1f );
    if( changed )
    {
        for( const shared_ptr<vaShadowmap> & shadowmap : m_shadowmaps )
        {
            shadowmap->Invalidate();
        }
    }
    ImGui::InputFloat( "CubePCFFilterScale", &m_shadowCubePCFFilterScale, 0.1f );
#endif
}


vaCubeShadowmap::vaCubeShadowmap( vaRenderDevice & device, const shared_ptr<vaLighting> & lightingSystem, const shared_ptr<vaLight> & light ) : vaShadowmap( device, lightingSystem, light )
{
}

void vaCubeShadowmap::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    shared_ptr<vaLight> light = GetLight( ).lock( );
    if( light == nullptr )
    {
        ImGui::Text( "<null>" );
    }
    else
    {
        ImGui::Text( "Corresponding light: %s", light->Name.c_str( ) );
    }

    GetRenderDevice().GetTextureTools().UIDrawImGuiInfo( m_cubemapArraySRV );
#endif
}

shared_ptr<vaShadowmap> vaShadowmap::Create( vaRenderDevice & device, const shared_ptr<vaLight> & light, const shared_ptr<vaLighting> & lightingSystem )
{
    switch( light->Type )
    {

    case( vaLight::Type::Directional ):
        assert( false );    // not yet implemented
        return nullptr;
        break;
    case( vaLight::Type::Point ):
    case( vaLight::Type::Spot ):
        return shared_ptr<vaCubeShadowmap>( new vaCubeShadowmap( device, lightingSystem, light ) );
        break;
    case( vaLight::Type::Area ):
        assert( false );    // not yet implemented
        return nullptr;
        break;
    case( vaLight::Type::Ambient ):
        assert( false ); // shadow for ambient light?
    default:
        assert( false );
        return nullptr;
        break;
    }
}

void vaCubeShadowmap::SetToRenderSelectionFilter( vaRenderSelectionFilter & filter ) const
{
    shared_ptr<vaLight> light = m_light.lock();
    assert( light != nullptr );
    if( light == nullptr )
        return;
    filter = vaRenderSelectionFilter( light->Position );
}

vaDrawResultFlags vaCubeShadowmap::Draw( vaRenderDeviceContext & renderContext, vaRenderSelection & renderSelection )
{
    if( m_storageTextureIndex == -1 )
        return vaDrawResultFlags::UnspecifiedError;

    shared_ptr<vaLight> light = m_light.lock( );
    assert( light != nullptr );
    if( light == nullptr )
        return vaDrawResultFlags::UnspecifiedError;
    shared_ptr<vaLighting> lightingSystem = m_lightingSystem.lock();
    assert( lightingSystem != nullptr );
    if( lightingSystem == nullptr )
        return vaDrawResultFlags::UnspecifiedError;
    
    vaCameraBase cameraFrontCubeFace( false );

    cameraFrontCubeFace.SetYFOV( 90.0f / 180.0f * VA_PIf );
    cameraFrontCubeFace.SetNearPlane( vaMath::Max( 0.001f, light->Size ) );
    cameraFrontCubeFace.SetFarPlane( light->EffectiveRadius() );
    cameraFrontCubeFace.SetViewportSize( m_cubemapSliceDSVs[0]->GetSizeX(), m_cubemapSliceDSVs[0]->GetSizeY() );
    cameraFrontCubeFace.SetPosition( light->Position );
    cameraFrontCubeFace.SetOrientation( vaQuaternion::FromYawPitchRoll( 0.0f, 0.0f, 0.0f ) );
    cameraFrontCubeFace.Tick( 0.0f, false );

    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    shared_ptr<vaTexture> * destinationCubeDSVs = m_cubemapSliceDSVs;
    shared_ptr<vaTexture> * destinationCubeRTVs = m_cubemapArrayRTVs;
    {
        VA_SCOPE_CPUGPU_TIMER( CubemapDepthOnly, renderContext );

        vaRenderDeviceContext::RenderOutputsState outputs = renderContext.GetOutputs();

        vaVector3 position = cameraFrontCubeFace.GetPosition( );
        vaCameraBase tempCamera = cameraFrontCubeFace;

        // draw all 6 faces - this should get optimized to GS in the future
        for( int i = 0; i < 6; i++ )
        {
            // I hope this clears just the single slice on all HW
            destinationCubeDSVs[i]->ClearDSV( renderContext, true, cameraFrontCubeFace.GetUseReversedZ( ) ? ( 0.0f ) : ( 1.0f ), false, 0 );
            destinationCubeRTVs[i]->ClearRTV( renderContext, vaVector4( 10000.0f, 10000.0f, 10000.0f, 10000.0f ) );

            vaVector3 lookAtDir, upVec;

            // see https://msdn.microsoft.com/en-us/library/windows/desktop/bb204881(v=vs.85).aspx
            switch( i )
            {
            case 0: // positive x (+y up)
                lookAtDir   = vaVector3( 1.0f, 0.0f, 0.0f );
                upVec       = vaVector3( 0.0f, 1.0f, 0.0f );
                break;
            case 1: // negative x (+y up)
                lookAtDir   = vaVector3( -1.0f, 0.0f, 0.0f );
                upVec       = vaVector3( 0.0f, 1.0f, 0.0f );
                break;
            case 2: // positive y (-z up)
                lookAtDir   = vaVector3( 0.0f, 1.0f, 0.0f );
                upVec       = vaVector3( 0.0f, 0.0f, -1.0f );
                break;
            case 3: // negative y (z up)
                lookAtDir   = vaVector3( 0.0f, -1.0f, 0.0f );
                upVec       = vaVector3( 0.0f, 0.0f, 1.0f );
                break;
            case 4: // positive z (y up)
                lookAtDir   = vaVector3( 0.0f, 0.0f, 1.0f );
                upVec       = vaVector3( 0.0f, 1.0f, 0.0f );
                break;
            case 5: // negative z (y up)
                lookAtDir   = vaVector3( 0.0f, 0.0f, -1.0f );
                upVec       = vaVector3( 0.0f, 1.0f, 0.0f );
                break;
            }

            tempCamera.SetOrientationLookAt( position + lookAtDir, upVec );
            tempCamera.Tick( 0, false );
        
            vaSceneDrawContext drawContext( renderContext, tempCamera, vaDrawContextOutputType::CustomShadow, vaDrawContextFlags::CustomShadowmapShaderLinearDistanceToCenter );
            drawContext.ViewspaceDepthOffsets = lightingSystem->GetCubeShadowViewspaceDepthOffsets();

            //renderContext.SetRenderTarget( nullptr, destinationCubeDepth, true );
            renderContext.SetRenderTarget( destinationCubeRTVs[i], destinationCubeDSVs[i], true );

            drawResults |= GetRenderDevice().GetMeshManager().Draw( drawContext, *renderSelection.MeshList, vaBlendMode::Opaque, vaRenderMeshDrawFlags::SkipTransparencies | vaRenderMeshDrawFlags::EnableDepthTest | vaRenderMeshDrawFlags::EnableDepthWrite | vaRenderMeshDrawFlags::SkipNonShadowCasters );
        }

        renderContext.SetOutputs(outputs);
    }

    if( drawResults == vaDrawResultFlags::None )
        SetUpToDate();
    return drawResults;
}

void vaLighting::UpdateResourcesIfNeeded( vaSceneDrawContext & drawContext )
{
    drawContext; // unreferenced

    if( m_shadersDirty )
    {
        m_shadersDirty = false;

        m_applyDirectionalAmbientPS->CreateShaderFromFile( m_shaderFileToUse.c_str(), "ps_5_0", "ApplyDirectionalAmbientPS", m_staticShaderMacros, false );
        m_applyDirectionalAmbientShadowedPS->CreateShaderFromFile( m_shaderFileToUse.c_str(), "ps_5_0", "ApplyDirectionalAmbientShadowedPS", m_staticShaderMacros, false );

        //m_applyTonemapPS.CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "ApplyTonemapPS", m_staticShaderMacros );
    }
}
