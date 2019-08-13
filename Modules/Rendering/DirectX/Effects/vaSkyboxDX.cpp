/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VertexAsylum Codebase, Copyright (c) Filip Strugar.
// Contents of this file are distributed under MIT license (https://en.wikipedia.org/wiki/MIT_License)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Core/vaCoreIncludes.h"

#include "Rendering/Effects/vaSkybox.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX12.h"

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

    class vaSkyboxDX12 : public vaSkybox
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    private:

    protected:
        vaSkyboxDX12( const vaRenderingModuleParams & params ) : vaSkybox( params ) { }
        ~vaSkyboxDX12( ) { }
    };

}



using namespace VertexAsylum;

void RegisterSkyboxDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSkybox, vaSkyboxDX11 );
}

void RegisterSkyboxDX12( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaSkybox, vaSkyboxDX12 );
}