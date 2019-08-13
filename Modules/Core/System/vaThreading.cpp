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

#include "vaThreading.h"

#include "Core/vaMath.h"

#include "Core/Misc/vaProfiler.h"

#ifdef VA_IMGUI_INTEGRATION_ENABLED
#include "IntegratedExternals/vaImguiIntegration.h"
#endif

using namespace VertexAsylum;

std::atomic<std::thread::id> vaThreading::s_mainThreadID;

void vaThreading::SetMainThread( )
{
    VA_NAME_THREAD( "Main" );

    // make sure this happens only once
    assert( s_mainThreadID == std::thread::id() );

    s_mainThreadID = std::this_thread::get_id();
}

bool vaThreading::IsMainThread( )
{
    // make sure the main thread ID was set
    assert( s_mainThreadID != std::thread::id() );

    return s_mainThreadID == std::this_thread::get_id();
}

vaBackgroundTaskManager::vaBackgroundTaskManager( )  
{
    int physicalPackages, physicalCores, logicalCores;
    vaThreading::GetCPUCoreCountInfo( physicalPackages, physicalCores, logicalCores );

    // adhoc heuristic for determining the number of worker threads
    m_threadPoolSize = vaMath::Max( 2, ( physicalCores + logicalCores - 1 ) / 2 );
}

void vaBackgroundTaskManager::ClearAndRestart( )
{
    assert( !m_stopped );
    if( m_stopped )
        return;

    // prevent any subsequent spawns
    {
        std::unique_lock<mutex> spawnLock( m_spawnMutex );
        m_stopped = true;
    }

    // Signal to all tasks that they need to get stopped and remove all waiting pooled tasks
    {
        std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
        for( int i = 0; i < (int)m_currentTasks.size(); i++ )
            m_currentTasks[i]->Context.ForceStop = true;
    }

    // Wait for all tasks to finish
    ClearFinishedTasks();
    {
        std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
        while( m_currentTasks.size() > 0 )
        {
            shared_ptr<TaskInternal> firstTask = m_currentTasks[0];
            tasksLock.unlock();
            WaitUntilFinished( firstTask );
            ClearFinishedTasks();
            tasksLock.lock();
        }
    }
    assert( m_waitingPooledTasks.empty() );

    // restart
    {
        std::unique_lock<mutex> spawnLock( m_spawnMutex );
        m_stopped = false;
    }
}

void vaBackgroundTaskManager::Run( const shared_ptr<TaskInternal> & _task )
{
    assert( !_task->IsFinished );
    std::thread thread( [this, _task]() 
    {
        shared_ptr<TaskInternal> task = _task;

        bool loopDone = true;
        do
        {
            assert( !task->IsFinished );
            // if( !task->Context.ForceStop ) // <- not sure if we want this
                task->Result = task->UserFunction( task->Context );
            assert( !task->IsFinished );
            task->Context.Progress = 1.0f;

            {
                std::unique_lock<std::mutex> cvLock( task->WaitFinishedMutex );
                task->IsFinished = true;
                task->WaitFinishedCV.notify_all();
            }

            // using thread pool - if we can spawn, spawn, if we can't then 
            if( ( task->Flags & SpawnFlags::UseThreadPool ) != 0 )
            {
                std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
                // if we're a threadpool task and there's some threadpool tasks waiting, continue working
                if( !m_waitingPooledTasks.empty() )
                {
                    task = m_waitingPooledTasks.front();
                    m_waitingPooledTasks.pop();
                    task->PooledWaiting = false;
                    loopDone = false;
                }
                else
                {
                    m_currentThreadPoolUseCount--;
                    loopDone = true;
                }
            }
        } while ( !loopDone );
    });
    thread.detach(); // run free little one!!
}

bool vaBackgroundTaskManager::Spawn( shared_ptr<Task> & outTask, const string & taskName, SpawnFlags flags, const std::function< bool( TaskContext & context ) > & taskFunction )
{
    std::unique_lock<mutex> spawnLock( m_spawnMutex );
    assert( !m_stopped );
    if( m_stopped ) return false;

    shared_ptr<vaBackgroundTaskManager::TaskInternal> newTask = std::make_shared<vaBackgroundTaskManager::TaskInternal>( taskName, flags, taskFunction );
    outTask = newTask;

    {
        std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
        m_currentTasks.push_back( newTask );

        // using thread pool - if we can spawn, spawn, if we can't then add to waiting list
        if( ( newTask->Flags & SpawnFlags::UseThreadPool ) != 0 )
        {
            if( m_currentThreadPoolUseCount >= m_threadPoolSize )
            {
                m_waitingPooledTasks.push( newTask );
                newTask->PooledWaiting = true;
                return true;
            }
            else
            {
                m_currentThreadPoolUseCount++;
            }
        }
    }

    Run( newTask );

    return true;
}

void vaBackgroundTaskManager::WaitUntilFinished( const shared_ptr<Task> & _task )
{
    if( _task == nullptr )
        return;
    const shared_ptr<TaskInternal> task =  std::static_pointer_cast<TaskInternal>(_task);
    {
        std::unique_lock<std::mutex> cvLock( task->WaitFinishedMutex );
        while( !task->IsFinished )
            task->WaitFinishedCV.wait(cvLock);
    }
    assert( task->IsFinished );
}

float vaBackgroundTaskManager::GetProgress( const shared_ptr<Task> & _task )
{
    const shared_ptr<TaskInternal> task =  std::static_pointer_cast<TaskInternal>(_task);

    float progress = task->Context.Progress;
    if( task->IsFinished )
        assert( task->Context.Progress == 1.0f );

    return vaMath::Clamp( progress, 0.0f, 1.0f );
}

bool vaBackgroundTaskManager::IsFinished( const shared_ptr<Task> & _task )
{
    const shared_ptr<TaskInternal> task =  std::static_pointer_cast<TaskInternal>(_task);
    return task->IsFinished;
}

void vaBackgroundTaskManager::MarkForStopping( const shared_ptr<Task> & _task )
{
    const shared_ptr<TaskInternal> task =  std::static_pointer_cast<TaskInternal>(_task);
    task->Context.ForceStop = true;
}

void vaBackgroundTaskManager::ClearFinishedTasks( )
{
    // Clear all 'done' tasks from the array
    std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
    for( int i = (int)m_currentTasks.size()-1; i >= 0; i-- )
    {
        if( m_currentTasks[i]->IsFinished )
        {
            // replace now empty one by last if not already last
            if( i != (int)m_currentTasks.size()-1 )
                m_currentTasks[i] = m_currentTasks.back();
            // erase last
            m_currentTasks.pop_back();
        }
    }
}

void vaBackgroundTaskManager::ImGuiTaskProgress( const shared_ptr<Task> & _task )
{
    const shared_ptr<TaskInternal> task =  std::static_pointer_cast<TaskInternal>(_task);
    bool taskWaiting = task->PooledWaiting;
    taskWaiting;
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    if( taskWaiting )
    {
        ImGui::PushStyleColor( ImGuiCol_Text,           ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) );
    }
    ImGui::ProgressBar( task->Context.Progress, ImVec2(-1, 0), task->Name.c_str() );
    if( taskWaiting )
    {
        ImGui::PopStyleColor( 1 );
    }
#endif
}


void vaBackgroundTaskManager::InsertImGuiContentInternal( const vector<shared_ptr<TaskInternal>> & tasks )
{
    tasks;
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    for( const shared_ptr<TaskInternal> & task : tasks )
    {
        ImGuiTaskProgress( task );
    }
#endif
}

void vaBackgroundTaskManager::InsertImGuiContent( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    vector<shared_ptr<TaskInternal>> tasksCopy;
    ClearFinishedTasks();
    {
        std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
        for( auto obj : m_currentTasks  ) if( (obj->Flags & SpawnFlags::ShowInUI) != 0 && !obj->Context.HideInUI ) tasksCopy.push_back( obj );
    }
    if( tasksCopy.size() == 0 )
        return;
    std::sort( tasksCopy.begin(), tasksCopy.end(), [](const shared_ptr<TaskInternal> & a, const shared_ptr<TaskInternal> & b) -> bool { return a->Name < b->Name; } );
    InsertImGuiContentInternal( tasksCopy );
#endif
}

void vaBackgroundTaskManager::InsertImGuiWindow( const string & title )
{
    title;

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    vector<shared_ptr<TaskInternal>> tasksCopy;
    ClearFinishedTasks();
    {
        std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
        for( auto obj : m_currentTasks  ) if( (obj->Flags & SpawnFlags::ShowInUI) != 0 && !obj->Context.HideInUI ) tasksCopy.push_back( obj );
    }

//#define VISUAL_DEBUGGING_ENABLED

#ifndef VISUAL_DEBUGGING_ENABLED
    if( tasksCopy.size() == 0 )
        return;
#endif
    std::sort( tasksCopy.begin(), tasksCopy.end(), [](const shared_ptr<TaskInternal> & a, const shared_ptr<TaskInternal> & b) -> bool { return a->Name < b->Name; } );
   
    ImGuiIO& io = ImGui::GetIO();
    
    float lineHeight  = ImGui::GetFrameHeightWithSpacing();
    //float lineHeight  = ImGui::GetFrameHeight();
    ImVec2 windowSize = ImVec2( 500.0f, lineHeight * tasksCopy.size() + 30.0f 
#ifdef VISUAL_DEBUGGING_ENABLED
        + 70.0f 
#endif
    );

    ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x/2.0f - windowSize.x / 2.0f, /*io.DisplaySize.y/2.0f - windowSize.y / 2.0f*/ 10 ), ImGuiCond_Appearing );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
    ImGui::SetNextWindowSize( ImVec2( windowSize.x, windowSize.y ), ImGuiCond_Always );

    ImGuiWindowFlags windowFlags = 0;
    windowFlags |= ImGuiWindowFlags_NoResize                ;
    //windowFlags |= ImGuiWindowFlags_NoMove                  ;
    windowFlags |= ImGuiWindowFlags_NoScrollbar             ;
    windowFlags |= ImGuiWindowFlags_NoScrollWithMouse       ;
    windowFlags |= ImGuiWindowFlags_NoCollapse              ;
    //windowFlags |= ImGuiWindowFlags_NoInputs                ;
    windowFlags |= ImGuiWindowFlags_NoFocusOnAppearing      ;
    //windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus   ;
    windowFlags |= ImGuiWindowFlags_NoDocking      ;
    windowFlags |= ImGuiWindowFlags_NoSavedSettings      ;

    if( ImGui::Begin( title.c_str( ), nullptr, windowFlags ) )
    {
#ifdef VISUAL_DEBUGGING_ENABLED
        shared_ptr<vaBackgroundTaskManager::Task> lastTask = nullptr;
        {
            std::unique_lock<mutex> tasksLock( m_currentTasksMutex );
            if( m_currentTasks.size() > 0 )
                lastTask = m_currentTasks.back();
        }
        static int lastTaskIndex = 0;
    
        if( ImGui::Button("TEST START TASK") )
        {
            int taskLength = vaRandom::Singleton.NextIntRange( 200 );
            bool spawnSubTask = vaRandom::Singleton.NextIntRange( 200 ) > 175;
            lastTaskIndex++;
            vaBackgroundTaskManager::GetInstance().Spawn( vaStringTools::Format( "test_%d", lastTaskIndex ), SpawnFlags::UseThreadPool | SpawnFlags::ShowInUI, [taskLength, spawnSubTask]( vaBackgroundTaskManager::TaskContext & context ) 
                {
                    if( spawnSubTask )
                    {
                        vaBackgroundTaskManager::GetInstance().Spawn( vaStringTools::Format( "test_%d", lastTaskIndex ), SpawnFlags::UseThreadPool | SpawnFlags::ShowInUI, [taskLength]( vaBackgroundTaskManager::TaskContext & context ) 
                        {  
                            for( int i = 0; i < taskLength; i++ )
                            {
                                context.Progress = (float)(i) / (float)(taskLength-1);
                                if( context.ForceStop )
                                    break;
                                vaThreading::Sleep( 100 );
                            }
                            return true;
                        } );
                    }

                    for( int i = 0; i < taskLength; i++ )
                    {
                        context.Progress = (float)(i) / (float)(taskLength-1);
                        if( context.ForceStop )
                            break;
                        vaThreading::Sleep( 100 );
                    }
                    return true;
                } );
        }
        if( lastTask != nullptr )
        {
            if( ImGui::Button("TEST FORCE STOP LAST TASK") )
                vaBackgroundTaskManager::GetInstance().MarkForStopping( lastTask );
            if( ImGui::Button("TEST WAIT LAST TASK") )
                vaBackgroundTaskManager::GetInstance().WaitUntilFinished( lastTask );
        }
        ImGui::Separator();
#endif
        InsertImGuiContentInternal( tasksCopy );
    }
    ImGui::End( );
#endif
}
