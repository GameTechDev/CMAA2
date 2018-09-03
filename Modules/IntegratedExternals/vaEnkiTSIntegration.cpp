/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EnkiTS (C++11 branch):   https://github.com/dougbinks/enkiTS/tree/C++11
// EnkiTS license:          MIT-like (https://github.com/dougbinks/enkiTS/blob/master/License.txt)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "IntegratedExternals/vaEnkiTSIntegration.h"

#include "Core/Misc/vaProfiler.h"

using namespace VertexAsylum;

vaEnkiTS::vaEnkiTS( int threadsToUse )// : m_shuttingDown( false )
{
    m_TS.GetProfilerCallbacks()->threadStart    = vaProfiler::EnkiThreadStartCallback;
    m_TS.GetProfilerCallbacks()->waitStart      = vaProfiler::EnkiWaitStartCallback;
    m_TS.GetProfilerCallbacks()->waitStop       = vaProfiler::EnkiWaitStopCallback;

    m_TS.Initialize( threadsToUse );
}

vaEnkiTS::~vaEnkiTS( )
{
//    m_shuttingDown = true;
    m_TS.WaitforAllAndShutdown();
}
