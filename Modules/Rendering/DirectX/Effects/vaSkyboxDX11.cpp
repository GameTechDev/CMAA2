/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VertexAsylum Codebase, Copyright (c) Filip Strugar.
// Contents of this file are distributed under MIT license (https://en.wikipedia.org/wiki/MIT_License)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaShaderDX11.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/Effects/vaSkybox.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "Rendering/DirectX/vaTextureDX11.h"

namespace VertexAsylum
{

    class vaSkyboxDX11 : public vaSkybox
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    private:

    protected:
        vaSkyboxDX11( const vaRenderingModuleParams & params ) : vaSkybox( params ) { }
        ~vaSkyboxDX11( ) { }
    };

}

using namespace VertexAsylum;

void RegisterSkyboxDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSkybox, vaSkyboxDX11 );
}