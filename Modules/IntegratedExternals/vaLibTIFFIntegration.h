/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// http://simplesystems.org/libtiff/libtiff.html
// Libtiff license: Apache-2 - see LICENSE file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "Core/vaCore.h"

#ifdef VA_LIBTIFF_INTEGRATION_ENABLED

namespace LibTiff
{
    #include "IntegratedExternals\libtiff\tiff.h"
    #include "IntegratedExternals\libtiff\tiffio.h"
}

#endif // VA_REMOTERY_INTEGRATION_ENABLED