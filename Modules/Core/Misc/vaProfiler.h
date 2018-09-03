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

// WARNING: the CPU side of this profiler is not really useful at all - better to be replaced with Remotery or another
// freely available instrumentation library.

#include "Core/vaCoreIncludes.h"

#ifdef VA_REMOTERY_INTEGRATION_ENABLED
#include "IntegratedExternals\vaRemoteryIntegration.h"
#endif


namespace VertexAsylum
{
    class vaGPUTimer;
    class vaRenderDeviceContext;

    enum class vaProfilerTimingsDisplayType : int32
    {
        LastFrame,
        LastXFramesAverage,
        LastXFramesMax,
    };

    class vaNestedProfilerNode
    {
    public:
        static const int                c_historyFrameCount     = 128;

    private:
        string const                    m_name;
        bool                            m_selected;
        bool                            m_aggregateIfSameNameInScope;

        vaNestedProfilerNode * const    m_parentNode;
        map< string, vaNestedProfilerNode * >
                                        m_childNodes;
        vector< vaNestedProfilerNode* > m_sortedChildNodes;

        int                             m_childNodeUsageFrameCounter;
        static const int                c_totalMaxChildNodes    = 1000;     // not designed for more than this for performance reasons

        double                          m_startTimeCPU;        // 0 if not started
        double                          m_totalTimeCPU;
        double                          m_exclusiveTimeCPU;

        unique_ptr<vaGPUTimer>          m_GPUProfiler;
        double                          m_totalTimeGPU;
        double                          m_exclusiveTimeGPU;

        int64                           m_lastUsedProfilerFrameIndex;
        int                             m_nodeUsageIndex;

        int                             m_framesUntouched;

        static int                      s_totalNodes;
        static const int                c_totalMaxNodes         = 10000;    // not designed for more than this, would become too slow

        bool                            m_hasGPUTimings;

        int                             m_historyLastIndex;
        double                          m_historyTotalTimeCPU[c_historyFrameCount];
        double                          m_historyTotalTimeGPU[c_historyFrameCount];
        double                          m_historyExclusiveTimeCPU[c_historyFrameCount];
        double                          m_historyExclusiveTimeGPU[c_historyFrameCount];
        double                          m_averageTotalTimeCPU;
        double                          m_averageTotalTimeGPU;
        double                          m_averageExclusiveTimeCPU;
        double                          m_averageExclusiveTimeGPU;
        double                          m_maxTotalTimeCPU;
        double                          m_maxTotalTimeGPU;
        double                          m_maxExclusiveTimeCPU;
        double                          m_maxExclusiveTimeGPU;
            
        static const int                c_untouchedFramesKeep = c_historyFrameCount * 2;

    protected:
        friend class vaProfiler;
        vaNestedProfilerNode( vaNestedProfilerNode * parentNode, const string & name, vaRenderDeviceContext * renderDeviceContext, bool selected );
        ~vaNestedProfilerNode( );

    protected:
        vaNestedProfilerNode *          StartScope( const string & name, vaRenderDeviceContext * renderDeviceContext, double currentTime, int64 profilerFrameIndex, bool newNodeSelectedDefault );
        void                            StopScope( double currentTime );
        //
    protected:
        void                            RemoveOrphans( );
        void                            Proccess( );
        void                            Display( const string & namePath, int depth, bool cpu, vaProfilerTimingsDisplayType displayType );
        //
    public:
        // Warning: node returned here is only guaranteed to remain valud until next vaProfiler::NewFrame( ) gets called
        const vaNestedProfilerNode *    FindSubNode( const string & name ) const;

        int                             GetFrameHistoryLength( ) const              { return c_historyFrameCount;       }

        // Warning: memory returned here is only guaranteed to remain valud until next vaProfiler::NewFrame( ) gets called
        const double *                  GetFrameHistoryTotalTileCPU( ) const        { return m_historyTotalTimeCPU;     }
        const double *                  GetFrameHistoryTotalTimeGPU( ) const        { return m_historyTotalTimeGPU;     }
        const double *                  GetFrameHistoryExclusiveTimeCPU( ) const    { return m_historyExclusiveTimeCPU; }
        const double *                  GetFrameHistoryExclusiveTimeGPU( ) const    { return m_historyExclusiveTimeGPU; }
        
        // last valid index value in the circular buffer 
        int                             GetFrameHistoryLastIndex( ) const           { return m_historyLastIndex; }

        double                          GetFrameLastTotalTimeCPU( ) const           { return m_historyTotalTimeCPU[     (m_historyLastIndex + c_historyFrameCount - 1) % c_historyFrameCount ]; }
        double                          GetFrameLastTotalTimeGPU( ) const           { return m_historyTotalTimeGPU[     (m_historyLastIndex + c_historyFrameCount - 1) % c_historyFrameCount ]; }
        double                          GetFrameLastExclusiveTimeCPU( ) const       { return m_historyExclusiveTimeCPU[ (m_historyLastIndex + c_historyFrameCount - 1) % c_historyFrameCount ]; }
        double                          GetFrameLastExclusiveTimeGPU( ) const       { return m_historyExclusiveTimeGPU[ (m_historyLastIndex + c_historyFrameCount - 1) % c_historyFrameCount ]; }
        double                          GetFrameAverageTotalTimeCPU( ) const        { return m_averageTotalTimeCPU;     }
        double                          GetFrameAverageTotalTimeGPU( ) const        { return m_averageTotalTimeGPU;     }
        double                          GetFrameAverageExclusiveTimeCPU( ) const    { return m_averageExclusiveTimeCPU; }
        double                          GetFrameAverageExclusiveTimeGPU( ) const    { return m_averageExclusiveTimeGPU; }
        double                          GetFrameMaxTotalTimeCPU( ) const            { return m_maxTotalTimeCPU;         }
        double                          GetFrameMaxTotalTimeGPU( ) const            { return m_maxTotalTimeGPU;         }
        double                          GetFrameMaxExclusiveTimeCPU( ) const        { return m_maxExclusiveTimeCPU;     }
        double                          GetFrameMaxExclusiveTimeGPU( ) const        { return m_maxExclusiveTimeGPU;     }
    };

    class vaProfiler : public vaSingletonBase<vaProfiler>
    {
    public:

    private:
#ifdef VA_REMOTERY_INTEGRATION_ENABLED
        Remotery *                      m_remotery;
#endif

        vaNestedProfilerNode            m_root;
        vaNestedProfilerNode *          m_currentScope;
        vaNestedProfilerNode *          m_lastScope;

        vaSystemTimer                   m_timer;

        vaProfilerTimingsDisplayType    m_displayType;

        int64                           m_profilerFrameIndex;
    
    protected:
        friend class vaCore;
        vaProfiler( );
        ~vaProfiler( );

    public:
        vaNestedProfilerNode *          StartScope( const string & name, vaRenderDeviceContext * renderDeviceContext, bool newNodeSelectedDefault );
        void                            StopScope( vaNestedProfilerNode * node );

    public:
        void                            NewFrame( );
        void                            InsertImGuiContent( bool showCPU );
        const vaNestedProfilerNode *    FindNode( const string & name ) const;

        void                            MakeLastScopeSelected( );

    private:
        friend class vaEnkiTS;
        static void                     EnkiThreadStartCallback( uint32_t threadnum_ );
        static void                     EnkiWaitStartCallback( uint32_t threadnum_ );
        static void                     EnkiWaitStopCallback( uint32_t threadnum_ );
    };

    class vaScopeTimer
    {
        vaNestedProfilerNode * const    m_node;

        static bool                     s_disableScopeTimer;

    private:
        //
    public:
        vaScopeTimer( const string & name, vaRenderDeviceContext * renderDeviceContext = nullptr, bool aggregateIfSameNameInScope = false );
        virtual ~vaScopeTimer( );

        static bool                 IsEnabled( )                { return !s_disableScopeTimer; }
        static void                 SetEnabled( bool enabled )  { s_disableScopeTimer = !enabled; }
    };

#define VA_SCOPE_PROFILER_ENABLED

#ifdef VA_SCOPE_PROFILER_ENABLED

    #if defined(VA_REMOTERY_INTEGRATION_ENABLED)

        // #define VA_SCOPE_CPU_TIMER( name )                                          vaScopeTimer scope_##name( #name );                 rmt_ScopedCPUSample( name, 0 )
        // #define VA_SCOPE_CPU_TIMER_CUSTOMNAME( nameVar, customName )                vaScopeTimer scope_##name( customName );            rmt_ScopedCPUSampleDynamic( name, 0 )
        // #define VA_SCOPE_CPU_TIMER_AGGREGATE( name )                                vaScopeTimer scope_##name( #name, nullptr, true );  rmt_ScopedCPUSample( name, RMTSF_Aggregate )
         #define VA_SCOPE_CPU_TIMER( name )                                          rmt_ScopedCPUSample( name, 0 )
         #define VA_SCOPE_CPU_TIMER_CUSTOMNAME( nameVar, customName )                rmt_ScopedCPUSampleDynamic( name, 0 )
         #define VA_SCOPE_CPU_TIMER_AGGREGATE( name )                                rmt_ScopedCPUSample( name, RMTSF_Aggregate )

        #define VA_NAME_THREAD( name )                                              rmt_SetCurrentThreadName( name )

    #else

        #define VA_SCOPE_CPU_TIMER( name )                                          vaScopeTimer scope_##name( #name );
        #define VA_SCOPE_CPU_TIMER_CUSTOMNAME( nameVar, customName )                vaScopeTimer scope_##name( customName );
        #define VA_SCOPE_CPU_TIMER_AGGREGATE( name )                                vaScopeTimer scope_##name( #name, nullptr, true );

        #define VA_NAME_THREAD( name )                                              

    #endif

    #define VA_SCOPE_MAKE_LAST_SELECTED( )                                          { vaProfiler::GetInstance().MakeLastScopeSelected( ); }

#else

    #define VA_SCOPE_CPU_TIMER( name )                      
    #define VA_SCOPE_CPU_TIMER_NAMED( nameVar, nameScope )  
    #define VA_SCOPE_CPU_TIMER_AGGREGATE( name )

    #define VA_NAME_THREAD( name )                                              

#endif


}
