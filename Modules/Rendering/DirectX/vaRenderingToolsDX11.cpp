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

#include "vaRenderingToolsDX11.h"

#include "Rendering/vaRenderingTools.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaRenderDeviceDX11.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/DirectX/vaTriangleMeshDX11.h"
#include "Rendering/DirectX/vaTextureDX11.h"
#include "Rendering/DirectX/vaRenderBuffersDX11.h"
#include "Rendering/DirectX/vaGPUTimerDX11.h"

using namespace VertexAsylum;

namespace VertexAsylum
{

    ID3D11BlendState * DX11BlendModeFromVABlendMode( vaBlendMode blendMode )
    {
        ID3D11BlendState * blendState = vaDirectXTools::GetBS_Opaque( );
        switch( blendMode )
        {
        case vaBlendMode::Opaque:               blendState = vaDirectXTools::GetBS_Opaque( );            break;
        case vaBlendMode::Additive:             blendState = vaDirectXTools::GetBS_Additive( );          break;
        case vaBlendMode::AlphaBlend:           blendState = vaDirectXTools::GetBS_AlphaBlend( );        break;
        case vaBlendMode::PremultAlphaBlend:    blendState = vaDirectXTools::GetBS_PremultAlphaBlend( ); break;
        case vaBlendMode::Mult:                 blendState = vaDirectXTools::GetBS_Mult( );              break;
        default: assert( false );
        }
        return blendState;
    }

}