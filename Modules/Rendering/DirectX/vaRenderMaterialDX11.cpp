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

#include "Rendering/DirectX/vaRenderMaterialDX11.h"

#include "Rendering/DirectX/vaTextureDX11.h"
#include "Rendering/DirectX/vaRenderingToolsDX11.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/vaTextureHelpers.h"

using namespace VertexAsylum;

vaRenderMaterialDX11::vaRenderMaterialDX11( const vaRenderingModuleParams & params ) : vaRenderMaterial( params )
{
}

vaRenderMaterialDX11::~vaRenderMaterialDX11( )
{
}

void RegisterRenderMaterialDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaRenderMaterial, vaRenderMaterialDX11 );
    //VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaRenderMaterialManager, vaRenderMaterialManagerDX11 );
}
