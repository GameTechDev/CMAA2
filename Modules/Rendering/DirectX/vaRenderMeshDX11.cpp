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

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaShaderDX11.h"

#include "Rendering/vaRenderMesh.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/DirectX/Effects/vaSimpleShadowMapDX11.h"

#include "Rendering/DirectX/vaTriangleMeshDX11.h"

#include "Rendering/DirectX/vaRenderMaterialDX11.h"

namespace VertexAsylum
{
    class vaRenderMeshManagerDX11 : public vaRenderMeshManager
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    protected:
        vaRenderMeshManagerDX11( const vaRenderingModuleParams & params );
        ~vaRenderMeshManagerDX11( );

    private:
//        // vaRenderMeshManager implementation
//        virtual void                            Draw( vaSceneDrawContext & drawContext, const vaRenderMeshDrawList & list, vaBlendMode blendMode, vaRenderMeshDrawFlags drawFlags );

    public:
    };

}

using namespace VertexAsylum;


vaRenderMeshManagerDX11::vaRenderMeshManagerDX11( const vaRenderingModuleParams & params ) : vaRenderMeshManager( params )
{
    params; // unreferenced
}

vaRenderMeshManagerDX11::~vaRenderMeshManagerDX11( )
{
}

void RegisterRenderMeshDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaRenderMeshManager, vaRenderMeshManagerDX11 );
}

