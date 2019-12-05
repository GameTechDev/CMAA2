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

#include "vaDebugCanvas.h"

//#include "Rendering/DirectX/vaDirectXIncludes.h"
//#include "Rendering/DirectX/vaRenderBuffersDX11.h"
//#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

using namespace VertexAsylum;

vaDebugCanvas2D::vaDebugCanvas2D( const vaRenderingModuleParams & params )
    : m_vertexBuffer( params, m_vertexBufferSize, nullptr, true ),
      m_pixelShader( params ),
      m_vertexShader( params )
{
    std::vector<vaVertexInputElementDesc> inputElements;
    inputElements.push_back( { "SV_Position",   0,  vaResourceFormat::R32G32B32A32_FLOAT,    0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    inputElements.push_back( { "COLOR",         0,  vaResourceFormat::B8G8R8A8_UNORM,        0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    inputElements.push_back( { "TEXCOORD",      0,  vaResourceFormat::R32G32B32A32_FLOAT,    0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    inputElements.push_back( { "TEXCOORD",      1,  vaResourceFormat::R32G32_FLOAT,          0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

    m_vertexShader->CreateShaderAndILFromFile( L"vaCanvas.hlsl", "vs_5_0", "VS_Canvas2D", inputElements, vaShaderMacroContaner(), false );
    m_pixelShader->CreateShaderFromFile( L"vaCanvas.hlsl", "ps_5_0", "PS_Canvas2D", vaShaderMacroContaner(), false );

    m_vertexBufferCurrentlyUsed = 0;
    //m_vertexBufferSize = 0;

    m_vertexBufferCurrentlyUsed = 0;
}

vaDebugCanvas2D::~vaDebugCanvas2D( )
{
}

#define vaDirectXCanvas2D_FORMAT_WSTR() \
   va_list args; \
   va_start(args, text); \
   int nBuf; \
   wchar_t szBuffer[2048]; \
   nBuf = _vsnwprintf_s(szBuffer, _countof(szBuffer), _countof(szBuffer)-1, text, args); \
   assert(nBuf < sizeof(szBuffer)); \
   va_end(args); \
   //
#define vaDirectXCanvas2D_FORMAT_STR() \
   va_list args; \
   va_start(args, text); \
   int nBuf; \
   char szBuffer[2048]; \
   nBuf = _vsnprintf_s(szBuffer, _countof(szBuffer), _countof(szBuffer)-1, text, args); \
   assert(nBuf < sizeof(szBuffer)); \
   va_end(args); \


void vaDebugCanvas2D::DrawString( int x, int y, const wchar_t * text, ... )
{
    vaDirectXCanvas2D_FORMAT_WSTR( );
    m_drawStringLines.push_back( DrawStringItem( x, y, 0xFF000000, 0x00000000, szBuffer ) );
}
//
void vaDebugCanvas2D::DrawString( int x, int y, unsigned int penColor, const wchar_t * text, ... )
{
    vaDirectXCanvas2D_FORMAT_WSTR( );
    m_drawStringLines.push_back( DrawStringItem( x, y, penColor, 0x00000000, szBuffer ) );
}
//
void vaDebugCanvas2D::DrawString( int x, int y, unsigned int penColor, unsigned int shadowColor, const wchar_t * text, ... )
{
    vaDirectXCanvas2D_FORMAT_WSTR( );
    m_drawStringLines.push_back( DrawStringItem( x, y, penColor, shadowColor, szBuffer ) );
}
//
void vaDebugCanvas2D::DrawString( int x, int y, const char * text, ... )
{
    vaDirectXCanvas2D_FORMAT_STR( );
    m_drawStringLines.push_back( DrawStringItem( x, y, 0xFF000000, 0x00000000, vaStringTools::SimpleWiden( std::string( szBuffer ) ).c_str( ) ) );
}
//
void vaDebugCanvas2D::DrawString( int x, int y, unsigned int penColor, const char * text, ... )
{
    vaDirectXCanvas2D_FORMAT_STR( );
    m_drawStringLines.push_back( DrawStringItem( x, y, penColor, 0x00000000, vaStringTools::SimpleWiden( std::string( szBuffer ) ).c_str( ) ) );
}
//
void vaDebugCanvas2D::DrawString( int x, int y, unsigned int penColor, unsigned int shadowColor, const char * text, ... )
{
    vaDirectXCanvas2D_FORMAT_STR( );
    m_drawStringLines.push_back( DrawStringItem( x, y, penColor, shadowColor, vaStringTools::SimpleWiden( std::string( szBuffer ) ).c_str( ) ) );
}
//
void vaDebugCanvas2D::DrawLine( float x0, float y0, float x1, float y1, unsigned int penColor )
{
    m_drawLines.push_back( DrawLineItem( x0, y0, x1, y1, penColor ) );
}
//
void vaDebugCanvas2D::DrawRectangle( float x0, float y0, float width, float height, unsigned int penColor )
{
    DrawLine( x0 - 0.5f, y0, x0 + width, y0, penColor );
    DrawLine( x0 + width, y0, x0 + width, y0 + height, penColor );
    DrawLine( x0 + width, y0 + height, x0, y0 + height, penColor );
    DrawLine( x0, y0 + height, x0, y0, penColor );
}
//
void vaDebugCanvas2D::FillRectangle( float x0, float y0, float width, float height, unsigned int brushColor )
{
    m_drawRectangles.push_back( DrawRectangleItem( x0, y0, width, height, brushColor ) );
}
//
void vaDebugCanvas2D::DrawCircle( float x, float y, float radius, unsigned int penColor, float tess )
{
    tess = vaMath::Clamp( tess, 0.0f, 1.0f );

    float circumference = 2 * VA_PIf * radius;
    int steps = (int)( circumference / 4.0f * tess );
    steps = vaMath::Clamp( steps, 5, 32768 );

    float cxp = x + cosf( 0 * 2 * VA_PIf ) * radius;
    float cyp = y + sinf( 0 * 2 * VA_PIf ) * radius;

    for( int i = 1; i <= steps; i++ )
    {
        float p = i / (float)steps;
        float cx = x + cosf( p * 2 * VA_PIf ) * radius;
        float cy = y + sinf( p * 2 * VA_PIf ) * radius;

        DrawLine( cxp, cyp, cx, cy, penColor );

        cxp = cx;
        cyp = cy;
    }
}

void vaDebugCanvas2D::CleanQueued( )
{
    m_drawRectangles.clear( );
    m_drawLines.clear( );
    m_drawStringLines.clear( );
}

void vaDebugCanvas2D::Render( vaRenderDeviceContext & renderContext, int canvasWidth, int canvasHeight, bool bJustClearData )
{
    //ID3D11DeviceContext * context = renderContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    // Fill shapes first
    if( !bJustClearData )
    {
        uint32 rectsDrawn = 0;
        while( rectsDrawn < m_drawRectangles.size( ) )
        {
            if( ( m_vertexBufferCurrentlyUsed + 6 ) >= m_vertexBufferSize )
            {
                m_vertexBufferCurrentlyUsed = 0;
            }

            vaResourceMapType mapType = ( m_vertexBufferCurrentlyUsed == 0 ) ? ( vaResourceMapType::WriteDiscard ) : ( vaResourceMapType::WriteNoOverwrite );
            if( m_vertexBuffer.Map( renderContext, mapType ) )
            {
                CanvasVertex2D * vertices = m_vertexBuffer.GetMappedData( );
                int drawFromVertex = m_vertexBufferCurrentlyUsed;

                while( ( rectsDrawn < m_drawRectangles.size( ) ) && ( ( m_vertexBufferCurrentlyUsed + 6 ) < m_vertexBufferSize ) )
                {
                    const int index = m_vertexBufferCurrentlyUsed;

                    const DrawRectangleItem & rect = m_drawRectangles[rectsDrawn];

                    vertices[index + 0] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( rect.x, rect.y ), rect.color );
                    vertices[index + 1] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( rect.x + rect.width, rect.y ), rect.color );
                    vertices[index + 2] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( rect.x, rect.y + rect.height ), rect.color );
                    vertices[index + 3] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( rect.x, rect.y + rect.height ), rect.color );
                    vertices[index + 4] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( rect.x + rect.width, rect.y ), rect.color );
                    vertices[index + 5] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( rect.x + rect.width, rect.y + rect.height ), rect.color );

                    m_vertexBufferCurrentlyUsed += 6;
                    rectsDrawn++;
                }
                int drawVertexCount = m_vertexBufferCurrentlyUsed - drawFromVertex;
                m_vertexBuffer.Unmap( renderContext );

                vaGraphicsItem renderItem;

                renderItem.CullMode     = vaFaceCull::None;
                renderItem.BlendMode    = vaBlendMode::AlphaBlend;
                renderItem.VertexShader = m_vertexShader;
                renderItem.VertexBuffer = m_vertexBuffer.GetBuffer();
                renderItem.Topology     = vaPrimitiveTopology::TriangleList;
                renderItem.PixelShader  = m_pixelShader;
                renderItem.SetDrawSimple( drawVertexCount, drawFromVertex );

                renderContext.ExecuteSingleItem( renderItem );
            }
            else
            {
                assert( false );
            }
        }
    }

    // Lines
    if( !bJustClearData )
    {
        uint32 linesDrawn = 0;
        while( linesDrawn < m_drawLines.size( ) )
        {
            if( ( m_vertexBufferCurrentlyUsed + 2 ) >= m_vertexBufferSize )
            {
                m_vertexBufferCurrentlyUsed = 0;
            }

            vaResourceMapType mapType = ( m_vertexBufferCurrentlyUsed == 0 ) ? ( vaResourceMapType::WriteDiscard ) : ( vaResourceMapType::WriteNoOverwrite );
            if( m_vertexBuffer.Map( renderContext, mapType ) )
            {
                CanvasVertex2D * vertices = m_vertexBuffer.GetMappedData( );
                int drawFromVertex = m_vertexBufferCurrentlyUsed;

                while( ( linesDrawn < m_drawLines.size( ) ) && ( ( m_vertexBufferCurrentlyUsed + 2 ) < m_vertexBufferSize ) )
                {
                    const int index = m_vertexBufferCurrentlyUsed;

                    vertices[index + 0] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( m_drawLines[linesDrawn].x0, m_drawLines[linesDrawn].y0 ), m_drawLines[linesDrawn].penColor );
                    vertices[index + 1] = CanvasVertex2D( canvasWidth, canvasHeight, vaVector2( m_drawLines[linesDrawn].x1, m_drawLines[linesDrawn].y1 ), m_drawLines[linesDrawn].penColor );

                    m_vertexBufferCurrentlyUsed += 2;
                    linesDrawn++;
                }
                int drawVertexCount = m_vertexBufferCurrentlyUsed - drawFromVertex;
                m_vertexBuffer.Unmap( renderContext );

                vaGraphicsItem renderItem;

                renderItem.CullMode     = vaFaceCull::None;
                renderItem.BlendMode    = vaBlendMode::AlphaBlend;
                renderItem.VertexShader = m_vertexShader;
                renderItem.VertexBuffer = m_vertexBuffer.GetBuffer();
                renderItem.Topology     = vaPrimitiveTopology::LineList;
                renderItem.PixelShader  = m_pixelShader;
                renderItem.SetDrawSimple( drawVertexCount, drawFromVertex );

                renderContext.ExecuteSingleItem( renderItem );
            }
            else
            {
                assert( false );
            }
        }
    }

    // Text
    if( !bJustClearData && m_drawStringLines.size( ) > 0 )
    {
        assert( false ); // not implemented for DX12 so remove it for now :(

        // m_font.Begin( );
        // m_font.SetInsertionPos( 5, 5 );
        // m_font.SetForegroundColor( 0xFFFFFFFF );
        // 
        // for( size_t i = 0; i < m_drawStringLines.size( ); i++ )
        // {
        //     if( ( m_drawStringLines[i].shadowColor & 0xFF000000 ) == 0 ) continue;
        // 
        //     m_font.SetInsertionPos( m_drawStringLines[i].x + 1, m_drawStringLines[i].y + 1 );
        //     m_font.SetForegroundColor( m_drawStringLines[i].shadowColor );
        //     m_font.DrawTextLine( m_drawStringLines[i].text.c_str( ) );
        // }
        // 
        // for( size_t i = 0; i < m_drawStringLines.size( ); i++ )
        // {
        //     m_font.SetInsertionPos( m_drawStringLines[i].x, m_drawStringLines[i].y );
        //     m_font.SetForegroundColor( m_drawStringLines[i].penColor );
        //     m_font.DrawTextLine( m_drawStringLines[i].text.c_str( ) );
        // }
        // 
        // m_font.End( );
    }

    CleanQueued( );
}

/*

      //struct Circle2D
      //{
      //   float fX, fY, fRadiusFrom, fRadiusTo;
      //   u_int uColour;

      //   Circle2D( )		{}
      //   Circle2D( float fX, float fY, float fRadiusFrom, float fRadiusTo, u_int uColour ) : fX(fX), fY(fY), fRadiusFrom(fRadiusFrom), fRadiusTo(fRadiusTo), uColour(uColour) {}
      //};

      //struct PolyLinePoint2D
      //{
      //   float fX, fY;
      //   float fThickness;
      //   u_int uColour;

      //   PolyLinePoint2D( )		{}
      //   PolyLinePoint2D( float fX, float fY, float fThickness, u_int uColour ) : fX(fX), fY(fY), fThickness(fThickness), uColour(uColour) { }
      //};

      //static const u_int														g_nItemBufferSize = 4096;

      //static Collection_Vector<Direct3D_2DRenderer::Circle2D>		g_xCircles;
      //static Collection_Vector<Direct3D_2DRenderer::Line2D>		g_xLines;

      //static Direct3D_2DRenderer::SlightlyLessSimpleVertex								g_xDrawVertices[g_nItemBufferSize*6];

*/

/*


void Direct3D_2DRenderer::Flush( )
{
   IDirect3DSurface9* pSurf = NULL;
   Direct3D::D3DDevice->GetRenderTarget( 0, &pSurf );

   D3DSURFACE_DESC xDesc;
   pSurf->GetDesc( &xDesc );

   SAFE_RELEASE( pSurf );

   Direct3D::D3DDevice->SetRenderState( D3DRS_SRGBWRITEENABLE, FALSE );

   Render::CurrentStates.RequestZBufferEnabled(false);
   Render::CurrentStates.RequestZBufferWriteEnabled(false);
   Render::CurrentStates.RequestCullMode( _CULLMODE_NONE );
   Render::CurrentStates.RequestWireFrameMode( false );
   Render::CurrentStates.RequestTranslucencyMode( _TRANSLUCENCY_NORMAL );
   Render::CurrentStates.RequestZBufferEnabled(false);

   g_pxEffect->SetParameterByName( "g_xScreenSize", D3DXVECTOR4( (float)xDesc.Width, (float)xDesc.Height, 1.0f / (float)xDesc.Width, 1.0f / (float)xDesc.Height ) );

   Direct3D::D3DDevice->SetVertexDeclaration( g_pxVertDecl );

   if( g_xLines.GetSize() > 0 )
   {
      u_int uVertexCount = g_xLines.GetSize() * 2;
      for( u_int i = 0; i < g_xLines.GetSize(); i++ )
      {
         Line2D& xLine = g_xLines[i];
         SlightlyLessSimpleVertex* pVerts = &g_xDrawVertices[i*2];
         pVerts[0] = SlightlyLessSimpleVertex( Vector_2( xLine.fXFrom, xLine.fYFrom ), Vector_2( 0.0f, 0.0f ), Vector_2( 0.0f, 0.0f ), xLine.uColour );
         pVerts[1] = SlightlyLessSimpleVertex( Vector_2( xLine.fXTo, xLine.fYTo ), Vector_2( 0.0f, 0.0f ), Vector_2( 0.0f, 0.0f ), xLine.uColour );
      }

      g_pxEffect->Begin( false, 2 );
      g_pxEffect->RenderAllPrimitivePassesUp( D3DPT_LINELIST, uVertexCount/2, g_xDrawVertices, sizeof( SlightlyLessSimpleVertex ) );
      g_pxEffect->End( );
   }

   if( g_xCircles.GetSize() > 0 )
   {
      u_int uVertexCount = g_xCircles.GetSize() * 6;
      for( u_int i = 0; i < g_xCircles.GetSize(); i++ )
      {
         Circle2D& xCircle = g_xCircles[i];
         SlightlyLessSimpleVertex* pVerts = &g_xDrawVertices[i*6];
         pVerts[0] = SlightlyLessSimpleVertex( Vector_2( xCircle.fX - xCircle.fRadiusTo - 1.0f, xCircle.fY - xCircle.fRadiusTo - 1.0f ), Vector_2( xCircle.fX, xCircle.fY ), Vector_2( xCircle.fRadiusFrom, xCircle.fRadiusTo ), xCircle.uColour );
         pVerts[1] = SlightlyLessSimpleVertex( Vector_2( xCircle.fX + xCircle.fRadiusTo + 1.0f, xCircle.fY - xCircle.fRadiusTo - 1.0f ), Vector_2( xCircle.fX, xCircle.fY ), Vector_2( xCircle.fRadiusFrom, xCircle.fRadiusTo ), xCircle.uColour );
         pVerts[2] = SlightlyLessSimpleVertex( Vector_2( xCircle.fX - xCircle.fRadiusTo - 1.0f, xCircle.fY + xCircle.fRadiusTo + 1.0f ), Vector_2( xCircle.fX, xCircle.fY ), Vector_2( xCircle.fRadiusFrom, xCircle.fRadiusTo ), xCircle.uColour );
         pVerts[3] = SlightlyLessSimpleVertex( Vector_2( xCircle.fX - xCircle.fRadiusTo - 1.0f, xCircle.fY + xCircle.fRadiusTo + 1.0f ), Vector_2( xCircle.fX, xCircle.fY ), Vector_2( xCircle.fRadiusFrom, xCircle.fRadiusTo ), xCircle.uColour );
         pVerts[4] = SlightlyLessSimpleVertex( Vector_2( xCircle.fX + xCircle.fRadiusTo + 1.0f, xCircle.fY - xCircle.fRadiusTo - 1.0f ), Vector_2( xCircle.fX, xCircle.fY ), Vector_2( xCircle.fRadiusFrom, xCircle.fRadiusTo ), xCircle.uColour );
         pVerts[5] = SlightlyLessSimpleVertex( Vector_2( xCircle.fX + xCircle.fRadiusTo + 1.0f, xCircle.fY + xCircle.fRadiusTo + 1.0f ), Vector_2( xCircle.fX, xCircle.fY ), Vector_2( xCircle.fRadiusFrom, xCircle.fRadiusTo ), xCircle.uColour );
      }

      g_pxEffect->Begin( false, 0 );
      g_pxEffect->RenderAllPrimitivePassesUp( D3DPT_TRIANGLELIST, uVertexCount/3, g_xDrawVertices, sizeof( SlightlyLessSimpleVertex ) );
      g_pxEffect->End( );
   }

   g_xLines.Clear();
   g_xCircles.Clear();

   const bool bSRGB = Direct3D::UseGammaCorrection();
   Direct3D::D3DDevice->SetRenderState( D3DRS_SRGBWRITEENABLE, bSRGB );
}

void Direct3D_2DRenderer::DrawPolyline( PolyLinePoint2D* axPoints, int iPointCount )
{
   const int iMaxPolylinePointCount = g_nItemBufferSize-2;
   Assert( iPointCount < iMaxPolylinePointCount, "Direct3D_2DRenderer::DrawPolyline does not support as many points (will be clamped)" );
   iPointCount = Maths::Min( iPointCount, iMaxPolylinePointCount );

   IDirect3DSurface9* pSurf = NULL;
   Direct3D::D3DDevice->GetRenderTarget( 0, &pSurf );

   D3DSURFACE_DESC xDesc;
   pSurf->GetDesc( &xDesc );

   SAFE_RELEASE( pSurf );

   Direct3D::D3DDevice->SetRenderState( D3DRS_SRGBWRITEENABLE, FALSE );

   Render::CurrentStates.RequestZBufferEnabled(false);
   Render::CurrentStates.RequestZBufferWriteEnabled(false);
   Render::CurrentStates.RequestCullMode( _CULLMODE_NONE );
   Render::CurrentStates.RequestWireFrameMode( false );
   Render::CurrentStates.RequestTranslucencyMode( _TRANSLUCENCY_NORMAL );
   Render::CurrentStates.RequestZBufferEnabled(false);

   g_pxEffect->SetParameterByName( "g_xScreenSize", D3DXVECTOR4( (float)xDesc.Width, (float)xDesc.Height, 1.0f / (float)xDesc.Width, 1.0f / (float)xDesc.Height ) );

   Direct3D::D3DDevice->SetVertexDeclaration( g_pxVertDecl );

   u_int uVertexCount = (iPointCount-1) * 6;

   Vector_2 xDirPrev;
   Vector_2 xDirCurr;
   for( int i = -1; i < (iPointCount-1); i++ )
   {
      Vector_2 xDirNext;
      const PolyLinePoint2D& xPtNext		= axPoints[i+1];


      if( i < (iPointCount-2) )
      {
         const PolyLinePoint2D& xPtNextNext		= axPoints[i+2];
         xDirNext = Vector_2( xPtNextNext.fX, xPtNextNext.fY ) - Vector_2( xPtNext.fX, xPtNext.fY );
         xDirNext.Normalise();
      }

      if( i >= 0 )
      {
         const PolyLinePoint2D& xPtCurrent	= axPoints[i];

         float fThicknessIn	= xPtCurrent.fThickness;
         float fThicknessOut = xPtNext.fThickness;

         float fDotIn = xDirPrev * xDirCurr;
         float fDotOut = xDirCurr * xDirNext;

         float fInAngle	= Maths::ArcCosine( Maths::ClampToRange( fDotIn,	-0.9999f, 1.0f ) );
         float fOutAngle = Maths::ArcCosine( Maths::ClampToRange( fDotOut,	-0.9999f, 1.0f ) );
         float fInDist	= Maths::Tangent( fInAngle*0.5f );
         float fOutDist	= Maths::Tangent( fOutAngle*0.5f );

         Vector_2 xDirCurrLeft = Vector_2( +xDirCurr.y, -xDirCurr.x );

         Vector_2 xFrom( xPtCurrent.fX, xPtCurrent.fY );
         Vector_2 xTo( xPtNext.fX, xPtNext.fY );

         float fThicknessInMod = fThicknessIn * 0.5f + 1.0f;
         float fThicknessOutMod = fThicknessOut * 0.5f + 1.0f;

         fInDist		*= Maths::Sign( xDirCurrLeft * xDirPrev );
         fOutDist	*= Maths::Sign( xDirCurrLeft * xDirNext );


         Vector_2 xCFromLeft	= xFrom 		- xDirCurr * (fThicknessInMod * fInDist);
         Vector_2 xCFromRight	= xFrom 		+ xDirCurr * (fThicknessInMod * fInDist);
         Vector_2 xCToLeft		= xTo			- xDirCurr * (fThicknessOutMod * fOutDist);
         Vector_2 xCToRight	= xTo			+ xDirCurr * (fThicknessOutMod * fOutDist);
         Vector_2 xFromLeft	= xCFromLeft	+ xDirCurrLeft * fThicknessInMod;
         Vector_2 xFromRight	= xCFromRight	- xDirCurrLeft * fThicknessInMod;
         Vector_2 xToLeft		= xCToLeft		+ xDirCurrLeft * fThicknessOutMod;
         Vector_2 xToRight		= xCToRight		- xDirCurrLeft * fThicknessOutMod;

         SlightlyLessSimpleVertex* pVerts = &g_xDrawVertices[i*6];
         pVerts[0] = SlightlyLessSimpleVertex( xFromLeft,		xCFromLeft,		Vector_2( fThicknessIn * 0.5f, 0.0f ),	xPtCurrent.uColour );
         pVerts[1] = SlightlyLessSimpleVertex( xToLeft,			xCToLeft,		Vector_2( fThicknessOut * 0.5f, 0.0f ),	xPtNext.uColour );
         pVerts[2] = SlightlyLessSimpleVertex( xFromRight,		xCFromRight,	Vector_2( fThicknessIn * 0.5f, 0.0f ),	xPtCurrent.uColour );
         pVerts[3] = SlightlyLessSimpleVertex( xFromRight,		xCFromRight,	Vector_2( fThicknessIn * 0.5f, 0.0f ),	xPtCurrent.uColour );
         pVerts[4] = SlightlyLessSimpleVertex( xToLeft,			xCToLeft,		Vector_2( fThicknessOut * 0.5f, 0.0f ), 	xPtNext.uColour );
         pVerts[5] = SlightlyLessSimpleVertex( xToRight,			xCToRight,		Vector_2( fThicknessOut * 0.5f, 0.0f ), 	xPtNext.uColour );
      }
      else
      {
         xDirCurr = xDirNext;
      }

      xDirPrev = xDirCurr;
      xDirCurr = xDirNext;
   }

   g_pxEffect->Begin( false, 3 );
   g_pxEffect->RenderAllPrimitivePassesUp( D3DPT_TRIANGLELIST, uVertexCount/3, g_xDrawVertices, sizeof( SlightlyLessSimpleVertex ) );
   g_pxEffect->End( );

   const bool bSRGB = Direct3D::UseGammaCorrection();
   Direct3D::D3DDevice->SetRenderState( D3DRS_SRGBWRITEENABLE, bSRGB );
}

#endif

*/


/*
float4	g_xScreenSize;

void VShader( inout float4 xColour : COLOR, inout float4 xPos : Position, inout float4 xUV : TEXCOORD0, out float2 xOrigScreenPos : TEXCOORD1 )
{
   xOrigScreenPos = xPos.xy;

   xPos.xy *= g_xScreenSize.zw;
   xPos.xy *= float2( 2.0, -2.0 );
   xPos.xy += float2( -1.0, 1.0 );
}

void PShader_Circle( inout float4 xColour : COLOR, in float4 xUV : TEXCOORD0, in float2 xOrigScreenPos : TEXCOORD1 )
{
   float2 xDelta = xOrigScreenPos.xy - xUV.xy;
   float fDistSq = dot( xDelta, xDelta );
   float fRadius1 = xUV.z;
   float fRadius2 = xUV.w;

   if( !((fDistSq >= fRadius1*fRadius1) && (fDistSq < fRadius2*fRadius2)) )
      discard;
}

void PShader_Rectangle( inout float4 xColour : COLOR, in float4 xUV : TEXCOORD0, in float2 xOrigScreenPos : TEXCOORD1 )
{
}

void PShader_Line( inout float4 xColour : COLOR, in float4 xUV : TEXCOORD0, in float2 xOrigScreenPos : TEXCOORD1 )
{

}

void PShader_LineAA( inout float4 xColour : COLOR, in float4 xUV : TEXCOORD0, in float2 xOrigScreenPos : TEXCOORD1 )
{
   float2 xDist = xOrigScreenPos - xUV.xy;

   xColour.a *= saturate( xUV.z - length( xDist ) + 0.5 );
}

technique Circle
{
    pass p0
    {
      VertexShader	= compile vs_3_0 VShader();
      PixelShader		= compile ps_3_0 PShader_Circle();
    }
}

technique Rectangle
{
    pass p0
    {
      VertexShader	= compile vs_3_0 VShader();
      PixelShader		= compile ps_3_0 PShader_Rectangle();
    }
}

technique Line
{
    pass p0
    {
      VertexShader	= compile vs_3_0 VShader();
      PixelShader		= compile ps_3_0 PShader_Line();
    }
}

technique LineAA
{
    pass p0
    {
      VertexShader	= compile vs_3_0 VShader();
      PixelShader		= compile ps_3_0 PShader_LineAA();
    }
}

*/


#include "Rendering/vaStandardShapes.h"

using namespace VertexAsylum;

vaDebugCanvas3D::vaDebugCanvas3D( const vaRenderingModuleParams & params )
    :   m_triVertexBuffer( params, m_triVertexBufferSizeInVerts, nullptr, true ),
        m_lineVertexBuffer( params, m_lineVertexBufferSizeInVerts, nullptr, true ),
    m_vertexShader( params ),
    m_pixelShader( params )
{
    std::vector<vaVertexInputElementDesc> inputElements;
    inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32A32_FLOAT,    0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    inputElements.push_back( { "COLOR",        0,   vaResourceFormat::B8G8R8A8_UNORM,        0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

    m_vertexShader->CreateShaderAndILFromFile( L"vaCanvas.hlsl", "vs_5_0", "VS_Canvas3D", inputElements, vaShaderMacroContaner{}, false );
    m_pixelShader->CreateShaderFromFile( L"vaCanvas.hlsl", "ps_5_0", "PS_Canvas3D", vaShaderMacroContaner{}, false );

    m_triVertexBufferCurrentlyUsed = 0;
    m_triVertexBufferStart = 0;

    m_lineVertexBufferCurrentlyUsed = 0;
    m_lineVertexBufferStart = 0;

    vaStandardShapes::CreateSphere( m_sphereVertices, m_sphereIndices, 2, true );

    m_triVertexBufferCurrentlyUsed = 0;
    m_triVertexBufferStart = 0;
    m_lineVertexBufferCurrentlyUsed = 0;
    m_lineVertexBufferStart = 0;
}

vaDebugCanvas3D::~vaDebugCanvas3D( )
{
    //m_triVertexBuffer.Destroy( );
    m_triVertexBufferCurrentlyUsed = 0;
    //m_triVertexBufferSizeInVerts = 0;
    m_triVertexBufferStart = 0;

    //m_lineVertexBuffer.Destroy( );
    m_lineVertexBufferCurrentlyUsed = 0;
    //m_lineVertexBufferSizeInVerts = 0;
    m_lineVertexBufferStart = 0;
}

void vaDebugCanvas3D::CleanQueued( )
{
    m_drawItems.clear( );
    m_drawItemsTransforms.clear( );
    m_drawLines.clear( );
    m_drawLinesTransformed.clear( );
    m_drawTrianglesTransformed.clear( );
}

void vaDebugCanvas3D::RenderLine( vaSceneDrawContext & drawContext, const CanvasVertex3D & a, const CanvasVertex3D & b )
{
    if( ( m_lineVertexBufferCurrentlyUsed + 2 ) >= m_lineVertexBufferSizeInVerts )
    {
        FlushLines( drawContext );
        m_lineVertexBufferCurrentlyUsed = 0;
        m_lineVertexBufferStart = 0;
    }

    vaResourceMapType mapType = ( m_lineVertexBufferCurrentlyUsed == 0 ) ? ( vaResourceMapType::WriteDiscard ) : ( vaResourceMapType::WriteNoOverwrite );
    if( m_lineVertexBuffer.Map( drawContext.RenderDeviceContext, mapType ) )
    {
        CanvasVertex3D * vertices = m_lineVertexBuffer.GetMappedData( );
        vertices[m_lineVertexBufferCurrentlyUsed++] = a;
        vertices[m_lineVertexBufferCurrentlyUsed++] = b;
        m_lineVertexBuffer.Unmap( drawContext.RenderDeviceContext );
    }
}

void vaDebugCanvas3D::RenderLineBatch( vaSceneDrawContext & drawContext, DrawLineTransformed * itemFrom, size_t count )
{
    if( ( m_lineVertexBufferCurrentlyUsed + count * 2 ) >= m_lineVertexBufferSizeInVerts )
    {
        FlushLines( drawContext );
        m_lineVertexBufferCurrentlyUsed = 0;
        m_lineVertexBufferStart = 0;
    }

    vaResourceMapType mapType = ( m_lineVertexBufferCurrentlyUsed == 0 ) ? ( vaResourceMapType::WriteDiscard ) : ( vaResourceMapType::WriteNoOverwrite );
    if( m_lineVertexBuffer.Map( drawContext.RenderDeviceContext, mapType ) )
    {
        CanvasVertex3D * vertices = m_lineVertexBuffer.GetMappedData( );
        for( size_t i = 0; i < count; i++ )
        {
            DrawLineTransformed & line = itemFrom[i];

            vertices[m_lineVertexBufferCurrentlyUsed++] = line.v0;
            vertices[m_lineVertexBufferCurrentlyUsed++] = line.v1;
        }
        m_lineVertexBuffer.Unmap( drawContext.RenderDeviceContext );
    }
}

void vaDebugCanvas3D::FlushLines( vaSceneDrawContext & drawContext )
{
    int verticesToRender = m_lineVertexBufferCurrentlyUsed - m_lineVertexBufferStart;

    if( verticesToRender > 0 )
    {
        vaGraphicsItem renderItem;

        renderItem.DepthEnable      = true;
        renderItem.DepthWriteEnable = false;
        renderItem.DepthFunc        = ( drawContext.Camera.GetUseReversedZ() )?( vaComparisonFunc::GreaterEqual ):( vaComparisonFunc::LessEqual );
        renderItem.CullMode         = vaFaceCull::None;
        renderItem.BlendMode        = vaBlendMode::AlphaBlend;
        renderItem.VertexShader     = m_vertexShader;
        renderItem.VertexBuffer     = m_lineVertexBuffer.GetBuffer();
        renderItem.Topology         = vaPrimitiveTopology::LineList;
        renderItem.PixelShader      = m_pixelShader;
        renderItem.SetDrawSimple( verticesToRender, m_lineVertexBufferStart );

        drawContext.RenderDeviceContext.ExecuteSingleItem( renderItem );
    }
    m_lineVertexBufferStart = m_lineVertexBufferCurrentlyUsed;
}

void vaDebugCanvas3D::RenderTriangle( vaSceneDrawContext & drawContext, const CanvasVertex3D & a, const CanvasVertex3D & b, const CanvasVertex3D & c )
{
    if( ( m_triVertexBufferCurrentlyUsed + 3 ) >= m_triVertexBufferSizeInVerts )
    {
        FlushTriangles( drawContext );
        m_triVertexBufferCurrentlyUsed = 0;
        m_triVertexBufferStart = 0;
    }

    vaResourceMapType mapType = ( m_triVertexBufferCurrentlyUsed == 0 ) ? ( vaResourceMapType::WriteDiscard ) : ( vaResourceMapType::WriteNoOverwrite );
    if( m_triVertexBuffer.Map( drawContext.RenderDeviceContext, mapType ) )
    {
        CanvasVertex3D * vertices = m_triVertexBuffer.GetMappedData( );
        vertices[m_triVertexBufferCurrentlyUsed++] = a;
        vertices[m_triVertexBufferCurrentlyUsed++] = b;
        vertices[m_triVertexBufferCurrentlyUsed++] = c;
        m_triVertexBuffer.Unmap( drawContext.RenderDeviceContext );
    }
}

void vaDebugCanvas3D::RenderTrianglesBatch( vaSceneDrawContext & drawContext, DrawTriangleTransformed * itemFrom, size_t count )
{
    if( ( m_triVertexBufferCurrentlyUsed + count * 3 ) >= m_triVertexBufferSizeInVerts )
    {
        FlushTriangles( drawContext );
        m_triVertexBufferCurrentlyUsed = 0;
        m_triVertexBufferStart = 0;
    }

    vaResourceMapType mapType = ( m_triVertexBufferCurrentlyUsed == 0 ) ? ( vaResourceMapType::WriteDiscard ) : ( vaResourceMapType::WriteNoOverwrite );
    if( m_triVertexBuffer.Map( drawContext.RenderDeviceContext, mapType ) )
    {
        CanvasVertex3D * vertices = m_triVertexBuffer.GetMappedData( );
        for( size_t i = 0; i < count; i++ )
        {
            DrawTriangleTransformed & triangle = itemFrom[i];

            vertices[m_triVertexBufferCurrentlyUsed++] = triangle.v0;
            vertices[m_triVertexBufferCurrentlyUsed++] = triangle.v1;
            vertices[m_triVertexBufferCurrentlyUsed++] = triangle.v2;
        }
        m_triVertexBuffer.Unmap( drawContext.RenderDeviceContext );
    }
}

void vaDebugCanvas3D::FlushTriangles( vaSceneDrawContext & drawContext )
{
    int verticesToRender = m_triVertexBufferCurrentlyUsed - m_triVertexBufferStart;

    if( verticesToRender > 0 )
    {
        vaGraphicsItem renderItem;

        renderItem.DepthEnable      = true;
        renderItem.DepthWriteEnable = false;
        renderItem.DepthFunc        = ( drawContext.Camera.GetUseReversedZ() )?( vaComparisonFunc::GreaterEqual ):( vaComparisonFunc::LessEqual );
        renderItem.CullMode         = vaFaceCull::None;
        renderItem.BlendMode        = vaBlendMode::AlphaBlend;
        renderItem.VertexShader     = m_vertexShader;
        renderItem.VertexBuffer     = m_triVertexBuffer.GetBuffer();
        renderItem.Topology         = vaPrimitiveTopology::TriangleList;
        renderItem.PixelShader      = m_pixelShader;
        renderItem.SetDrawSimple( verticesToRender, m_triVertexBufferStart );

        drawContext.RenderDeviceContext.ExecuteSingleItem( renderItem );
    }
    m_triVertexBufferStart = m_triVertexBufferCurrentlyUsed;
}

void vaDebugCanvas3D::Render( vaSceneDrawContext & drawContext, const vaMatrix4x4 & viewProj, bool bJustClearData )
{
    if( !bJustClearData )
    {
        vaMatrix4x4 tempMat;

        // first do triangles
        for( size_t i = 0; i < m_drawItems.size( ); i++ )
        {
            DrawItem & item = m_drawItems[i];

            // use viewProj by default
            const vaMatrix4x4 * trans = &viewProj;

            // or if the object has its own transform matrix, 'add' it to the viewProj
            if( item.transformIndex != -1 )
            {
                //assert( false ); // this is broken; lines are different from triangles; add InternalDrawLine that accepts already transformed lines...
                vaMatrix4x4 &local = m_drawItemsTransforms[item.transformIndex];
                tempMat = local * viewProj;
                trans = &tempMat;
            }

            if( item.type == Triangle )
            {
                CanvasVertex3D a0( item.v0, item.brushColor, trans );
                CanvasVertex3D a1( item.v1, item.brushColor, trans );
                CanvasVertex3D a2( item.v2, item.brushColor, trans );

                if( ( item.brushColor & 0xFF000000 ) != 0 )
                {
                    InternalDrawTriangle( a0, a1, a2 );
                }

                if( ( item.penColor & 0xFF000000 ) != 0 )
                {
                    a0.color = item.penColor;
                    a1.color = item.penColor;
                    a2.color = item.penColor;

                    InternalDrawLine( a0, a1 );
                    InternalDrawLine( a1, a2 );
                    InternalDrawLine( a2, a0 );
                }
            }

            if( item.type == Box )
            {

                const vaVector3 & boxMin = item.v0;
                const vaVector3 & boxMax = item.v1;

                vaVector3 va0( boxMin.x, boxMin.y, boxMin.z );
                vaVector3 va1( boxMax.x, boxMin.y, boxMin.z );
                vaVector3 va2( boxMax.x, boxMax.y, boxMin.z );
                vaVector3 va3( boxMin.x, boxMax.y, boxMin.z );
                vaVector3 vb0( boxMin.x, boxMin.y, boxMax.z );
                vaVector3 vb1( boxMax.x, boxMin.y, boxMax.z );
                vaVector3 vb2( boxMax.x, boxMax.y, boxMax.z );
                vaVector3 vb3( boxMin.x, boxMax.y, boxMax.z );

                CanvasVertex3D a0( va0, item.brushColor, trans );
                CanvasVertex3D a1( va1, item.brushColor, trans );
                CanvasVertex3D a2( va2, item.brushColor, trans );
                CanvasVertex3D a3( va3, item.brushColor, trans );
                CanvasVertex3D b0( vb0, item.brushColor, trans );
                CanvasVertex3D b1( vb1, item.brushColor, trans );
                CanvasVertex3D b2( vb2, item.brushColor, trans );
                CanvasVertex3D b3( vb3, item.brushColor, trans );

                if( ( item.brushColor & 0xFF000000 ) != 0 )
                {
                    InternalDrawTriangle( a0, a2, a1 );
                    InternalDrawTriangle( a2, a0, a3 );

                    InternalDrawTriangle( b0, b1, b2 );
                    InternalDrawTriangle( b2, b3, b0 );

                    InternalDrawTriangle( a0, a1, b1 );
                    InternalDrawTriangle( b1, b0, a0 );

                    InternalDrawTriangle( a1, a2, b2 );
                    InternalDrawTriangle( b1, a1, b2 );

                    InternalDrawTriangle( a2, a3, b3 );
                    InternalDrawTriangle( b3, b2, a2 );

                    InternalDrawTriangle( a3, a0, b0 );
                    InternalDrawTriangle( b0, b3, a3 );
                }

                if( ( item.penColor & 0xFF000000 ) != 0 )
                {
                    a0.color = item.penColor;
                    a1.color = item.penColor;
                    a2.color = item.penColor;
                    a3.color = item.penColor;
                    b0.color = item.penColor;
                    b1.color = item.penColor;
                    b2.color = item.penColor;
                    b3.color = item.penColor;

                    InternalDrawLine( a0, a1 );
                    InternalDrawLine( a1, a2 );
                    InternalDrawLine( a2, a3 );
                    InternalDrawLine( a3, a0 );
                    InternalDrawLine( a0, b0 );
                    InternalDrawLine( a1, b1 );
                    InternalDrawLine( a2, b2 );
                    InternalDrawLine( a3, b3 );
                    InternalDrawLine( b0, b1 );
                    InternalDrawLine( b1, b2 );
                    InternalDrawLine( b2, b3 );
                    InternalDrawLine( b3, b0 );
                }
            }

            if( item.type == Sphere )
            {
                if( ( item.brushColor & 0xFF000000 ) != 0 )
                {
                    for( size_t j = 0; j < m_sphereIndices.size( ); j += 3 )
                    {
                        vaVector3 sCenter = item.v0;
                        float sRadius = item.v1.x;

                        CanvasVertex3D a0( m_sphereVertices[m_sphereIndices[j + 0]] * sRadius + sCenter, item.brushColor, trans );
                        CanvasVertex3D a1( m_sphereVertices[m_sphereIndices[j + 1]] * sRadius + sCenter, item.brushColor, trans );
                        CanvasVertex3D a2( m_sphereVertices[m_sphereIndices[j + 2]] * sRadius + sCenter, item.brushColor, trans );

                        InternalDrawTriangle( a0, a1, a2 );
                    }
                }

                if( ( item.penColor & 0xFF000000 ) != 0 )
                {
                    for( size_t j = 0; j < m_sphereIndices.size( ); j += 3 )
                    {
                        vaVector3 sCenter = item.v0;
                        float sRadius = item.v1.x;

                        CanvasVertex3D a0( m_sphereVertices[m_sphereIndices[j + 0]] * sRadius + sCenter, item.penColor, trans );
                        CanvasVertex3D a1( m_sphereVertices[m_sphereIndices[j + 1]] * sRadius + sCenter, item.penColor, trans );
                        CanvasVertex3D a2( m_sphereVertices[m_sphereIndices[j + 2]] * sRadius + sCenter, item.penColor, trans );

                        InternalDrawLine( a0, a1 );
                        InternalDrawLine( a1, a2 );
                        InternalDrawLine( a2, a0 );
                    }
                }
            }
        }
        size_t batchSize = 512;
        for( size_t i = 0; i < m_drawTrianglesTransformed.size( ); i += batchSize )
            RenderTrianglesBatch( drawContext, m_drawTrianglesTransformed.data( ) + i, vaMath::Min( batchSize, m_drawTrianglesTransformed.size( ) - i ) );

        FlushTriangles( drawContext );

        // then add the lines (non-transformed to transformed)
        for( size_t i = 0; i < m_drawLines.size( ); i++ )
            m_drawLinesTransformed.push_back( DrawLineTransformed( CanvasVertex3D(m_drawLines[i].v0, m_drawLines[i].penColor0, &viewProj), CanvasVertex3D(m_drawLines[i].v1, m_drawLines[i].penColor1, &viewProj) ) );

        for( size_t i = 0; i < m_drawLinesTransformed.size( ); i += batchSize )
            RenderLineBatch( drawContext, m_drawLinesTransformed.data( ) + i, vaMath::Min( batchSize, m_drawLinesTransformed.size( ) - i ) );

        FlushLines( drawContext );

    }
    CleanQueued( );
}
