///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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

#include "vaImageCompareTool.h"

#include "Core/vaInput.h"

#include "Core/System/vaFileTools.h"

using namespace VertexAsylum;


vaImageCompareTool::vaImageCompareTool( const vaRenderingModuleParams & params ) : vaRenderingModule( params ),
    m_visualizationPS( params ),
    m_constants( params )
{
    m_screenshotCapturePath = L"";
    m_saveReferenceScheduled = false;
    m_compareReferenceScheduled = false;
    m_referenceTextureStoragePath = vaCore::GetExecutableDirectory() + L"reference.png";
    m_initialized = false;

    m_visualizationPS->CreateShaderFromFile( "vaHelperTools.hlsl", "ps_5_0", "ImageCompareToolVisualizationPS" );
}

vaImageCompareTool::~vaImageCompareTool( )
{

}

void vaImageCompareTool::SaveAsReference( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & colorInOut )
{
    if( ( m_referenceTexture == nullptr ) || ( m_referenceTexture->GetSizeX( ) != colorInOut->GetSizeX( ) ) || ( m_referenceTexture->GetSizeY( ) != colorInOut->GetSizeY( ) ) || ( m_referenceTexture->GetResourceFormat( ) != colorInOut->GetResourceFormat( ) ) )
    {
        assert( colorInOut->GetType() == vaTextureType::Texture2D );
        assert( colorInOut->GetMipLevels() == 1 );
        assert( colorInOut->GetSizeZ() == 1 );
        assert( colorInOut->GetSampleCount() == 1 );
        m_referenceTexture = vaTexture::Create2D( GetRenderDevice(), colorInOut->GetSRVFormat(), colorInOut->GetSizeX(), colorInOut->GetSizeY(), 1, 1, 1, vaResourceBindSupportFlags::ShaderResource, 
            vaTextureAccessFlags::None, colorInOut->GetSRVFormat() );
    }

    m_referenceTexture->CopyFrom( apiContext, colorInOut );
}

vaVector4 vaImageCompareTool::CompareWithReference( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & colorInOut, shared_ptr<vaPostProcess> & postProcess )
{
    // Bail if reference image not captured, or size/format mismatch - please capture a reference image first.
    if( ( m_referenceTexture == nullptr ) || ( m_referenceTexture->GetSizeX( ) != colorInOut->GetSizeX( ) ) || ( m_referenceTexture->GetSizeY( ) != colorInOut->GetSizeY( ) ) ) //|| ( m_referenceTexture->GetSRVFormat( ) != colorInOut->GetSRVFormat( ) ) )
        return vaVector4( -1, -1, -1, -1 );

    // MSE is in .x, PSNR is in .y
    return postProcess->CompareImages( apiContext, m_referenceTexture, colorInOut );
}

void vaImageCompareTool::RenderTick( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & colorInOut, shared_ptr<vaPostProcess> & postProcess )
{
    if( m_referenceTexture != nullptr )
    {
        if( ( vaInputKeyboardBase::GetCurrent( ) != NULL ) && vaInputKeyboardBase::GetCurrent( )->IsKeyDown( KK_CONTROL ) )
        {
            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( vaKeyboardKeys::KK_OEM_MINUS ) )
                m_visualizationType = vaImageCompareTool::VisType::None;
            else if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( vaKeyboardKeys::KK_OEM_PLUS ) )
                m_visualizationType = vaImageCompareTool::VisType::ShowReference;
            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( vaKeyboardKeys::KK_OEM_4 ) )
                m_visualizationType = vaImageCompareTool::VisType::ShowDifference;
            else if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( vaKeyboardKeys::KK_OEM_6 ) )
                m_visualizationType = vaImageCompareTool::VisType::ShowDifferenceX10;
        }
    }

    if( !m_initialized && m_referenceTexture == nullptr )
    {
        if( vaFileTools::FileExists( m_referenceTextureStoragePath ) )
        {
            m_referenceTexture = vaTexture::CreateFromImageFile( GetRenderDevice(), m_referenceTextureStoragePath );
            if( m_referenceTexture != nullptr )
            {
                VA_LOG( L"CompareTool: Reference image loaded from " + m_referenceTextureStoragePath );
            }
        }
        m_initialized = true;
    }

    if( m_helperTexture == nullptr || ( m_helperTexture->GetSizeX( ) != colorInOut->GetSizeX( ) ) || ( m_helperTexture->GetSizeY( ) != colorInOut->GetSizeY( ) ) || ( m_helperTexture->GetSRVFormat( ) != colorInOut->GetSRVFormat( ) ) )
    {
        m_helperTexture = vaTexture::Create2D( GetRenderDevice(), colorInOut->GetSRVFormat( ), colorInOut->GetSizeX( ), colorInOut->GetSizeY( ), 1, 1, 1, vaResourceBindSupportFlags::ShaderResource,
            vaTextureAccessFlags::None, colorInOut->GetSRVFormat( ) );
    }

    if( m_screenshotCapturePath != L"" )
    {
        if( postProcess->SaveTextureToPNGFile( apiContext, m_screenshotCapturePath, *colorInOut ) )
        {
            VA_LOG( L"CompareTool: Screenshot saved to " + m_screenshotCapturePath );
        }
        else
        {
            VA_LOG_ERROR( L"CompareTool: Error saving screenshot to " + m_screenshotCapturePath );
        }
        m_screenshotCapturePath = L"";
    }

    if( m_saveReferenceScheduled )
    {
        SaveAsReference( apiContext, colorInOut );
        m_saveReferenceScheduled = false;

        if( m_referenceTexture != nullptr )
        {
            VA_LOG( "CompareTool: Reference image captured" );
            if( postProcess->SaveTextureToPNGFile( apiContext, m_referenceTextureStoragePath, *m_referenceTexture ) )
            {
                VA_LOG( L"CompareTool: Reference image saved to " + m_referenceTextureStoragePath );
            }
            else
            {
                VA_LOG_ERROR( L"CompareTool: Error saving screenshot to " + m_referenceTextureStoragePath );
            }
        }
        else
        {
            VA_LOG_ERROR( "CompareTool: Error capturing reference image." );
        }
    }

    if( m_compareReferenceScheduled )
    {
        vaVector4 diff = CompareWithReference( apiContext, colorInOut, postProcess );
        if( diff.x == -1 )
        {
            VA_LOG_ERROR( "CompareTool: Reference image not captured, or size/format mismatch - please capture a reference image first." );
        }
        else
        {
            VA_LOG_SUCCESS( "CompareTool: Comparing current screen image with saved reference: PSNR: %.3f (MSE: %f)", diff.y, diff.x );
        }
        m_compareReferenceScheduled = false;
    }

    if( m_visualizationType != vaImageCompareTool::VisType::None && m_referenceTexture != nullptr && m_helperTexture != nullptr )
    {
        if( !( ( m_referenceTexture == nullptr ) || ( m_referenceTexture->GetSizeX( ) != colorInOut->GetSizeX( ) ) || ( m_referenceTexture->GetSizeY( ) != colorInOut->GetSizeY( ) ) ) )// || ( m_referenceTexture->GetSRVFormat( ) != colorInOut->GetSRVFormat( ) ) ) )
        {
            m_helperTexture->CopyFrom( apiContext, colorInOut );

            ImageCompareToolShaderConstants consts; memset( &consts, 0, sizeof(consts) );
            consts.VisType = (int)m_visualizationType;
            m_constants.Update( apiContext, consts );

            vaRenderItem renderItem;
            apiContext.FillFullscreenPassRenderItem( renderItem );
            renderItem.ConstantBuffers[ IMAGE_COMPARE_TOOL_BUFFERSLOT ]         = m_constants.GetBuffer();
            renderItem.ShaderResourceViews[ IMAGE_COMPARE_TOOL_TEXTURE_SLOT0 ]  = m_referenceTexture;
            renderItem.ShaderResourceViews[ IMAGE_COMPARE_TOOL_TEXTURE_SLOT1 ]  = m_helperTexture;
            renderItem.PixelShader          = m_visualizationPS;
            apiContext.ExecuteSingleItem( renderItem );
        }
    }
}

void vaImageCompareTool::IHO_Draw( )
{
    ImGui::PushItemWidth( 120.0f );

    ImGui::BeginGroup( );
    m_saveReferenceScheduled = ImGui::Button( "Save ref" );
    ImGui::SameLine( );
    m_compareReferenceScheduled = ImGui::Button( "Compare with ref" );

    if( m_referenceTexture == nullptr )
    {
        ImGui::Text( "No reference captured!" );
    }
    else
    {
        vector<string> elements; 
        elements.push_back( "None" ); 
        elements.push_back( "ShowReference" ); 
        elements.push_back( "ShowDifference" ); 
        elements.push_back( "ShowDifferenceX10"); 
        elements.push_back( "ShowDifferenceX100" );

        ImGuiEx_Combo( "Visualization", (int&)m_visualizationType, elements );
    }

    if( ImGui::Button( "Save to screenshot.png" ) )
        m_screenshotCapturePath = vaCore::GetExecutableDirectory() + L"screenshot.png";

    ImGui::EndGroup( );

    ImGui::PopItemWidth( );
}
