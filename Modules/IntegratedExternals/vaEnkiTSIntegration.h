/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EnkiTS (C++11 branch):   https://github.com/dougbinks/enkiTS/tree/C++11
// EnkiTS license:          MIT-like (https://github.com/dougbinks/enkiTS/blob/master/License.txt)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "Core/vaCoreIncludes.h"

#ifdef VA_ENKITS_INTEGRATION_ENABLED

#include "IntegratedExternals\enkiTS\TaskScheduler.h"

#endif // VA_ENKITS_INTEGRATION_ENABLED

namespace VertexAsylum
{

    class vaEnkiTS : public vaSingletonBase<vaEnkiTS>
    {
        enki::TaskScheduler     m_TS;

//        bool                    m_shuttingDown;

    public:

    protected:
        friend class vaCore;
        explicit vaEnkiTS( int threadsToUse );
        ~vaEnkiTS( );

    public:

		// Adds the TaskSet to pipe and returns if the pipe is not full.
		// If the pipe is full, pTaskSet is run.
		// should only be called from main thread, or within a task
		void            AddTaskSetToPipe( enki::ITaskSet * pTaskSet )                               { m_TS.AddTaskSetToPipe( pTaskSet ); }

		// Runs the TaskSets in pipe until true == pTaskSet->GetIsComplete();
		// should only be called from thread which created the taskscheduler , or within a task
		// if called with 0 it will try to run tasks, and return if none available.
		void            WaitforTaskSet( enki::ITaskSet * pTaskSet )                                 { m_TS.WaitforTaskSet( pTaskSet ); }

		// Waits for all task sets to complete - not guaranteed to work unless we know we
		// are in a situation where tasks aren't being continuously added.
		void            WaitforAll()                                                                { m_TS.WaitforAll(); }

		// Returns the number of threads created for running tasks + 1
		// to account for the main thread.
		uint32_t        GetNumTaskThreads() const                                                   { return m_TS.GetNumTaskThreads(); }
    };


    // 	// A utility task set for creating tasks based on std::func.
	// typedef std::function<void (TaskSetPartition range, uint32_t threadnum  )> TaskSetFunction;
	// class TaskSet : public ITaskSet
	// {
	// public:
	// 	TaskSet() = default;
	// 	TaskSet( TaskSetFunction func_ ) : m_Function( func_ ) {}
	// 	TaskSet( uint32_t setSize_, TaskSetFunction func_ ) : ITaskSet( setSize_ ), m_Function( func_ ) {}
    // 
    // 
	// 	virtual void            ExecuteRange( TaskSetPartition range, uint32_t threadnum  )
	// 	{
	// 		m_Function( range, threadnum );
	// 	}
    // 
	// 	TaskSetFunction m_Function;
	// };

}