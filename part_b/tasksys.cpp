#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    threads = new std::thread[num_threads];
    for (int i=0; i<num_threads; i++) {
        threads[i] = std::thread(&TaskSystemParallelThreadPoolSleeping::threadLoop, this);
    }
    _numThreads = num_threads;
    _nextTaskGroupId.store(0);
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    _isDone = true;
    _worker_cv.notify_all();
    for (int i = 0; i < _numThreads; i++) {
        threads[i].join();
    }
    delete[] threads;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    runAsnycWithDeps(runnable, num_total_tasks, {});
    sync();
}

void TaskSystemParallelThreadPoolSleeping::threadLoop() {
    
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        _worker_cv.wait(lock, [this] {
            return _isDone || !_readyQueue.empty();
        });

        if (_isDone && _readyQueue.empty()) break;
        TaskGroupInfo* currentTaskGroup = _readyQueue.front();
        _readyQueue.pop();
        lock.unlock();

        for (int i = 0; i < currentTaskGroup->numTotalTasks; i++) {
            currentTaskGroup->runnable->runTask(i, currentTaskGroup->numTotalTasks);
            currentTaskGroup->completedTasks.fetch_add(1);
        }
        
        lock.lock();
        // dependency check
        for (TaskID dependentID : currentTaskGroup->dependents) {
            TaskGroupInfo* dependentTaskGroup = _allTaskGroups[dependentID];
            dependentTaskGroup->dependenciesLeft.fetch_sub(1);
            if (dependentTaskGroup->dependenciesLeft.load() == 0) {
                _readyQueue.push(dependentTaskGroup);
                _worker_cv.notify_all();
            }
        }
        _activeTaskGroups.fetch_sub(1);
        _allTaskGroups.erase(currentTaskGroup->id);
        if (_activeTaskGroups.load() == 0) {
            // notify sync function
        }
        lock.unlock();
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //
    std::unique_lock<std::mutex> lock(_mutex);
    TaskGroupInfo* newTaskGroup = new TaskGroupInfo;
    newTaskGroup->id = _nextTaskGroupId.fetch_add(1);
    newTaskGroup->runnable = runnable;
    newTaskGroup->numTotalTasks = num_total_tasks;
    newTaskGroup->completedTasks.store(0);
    newTaskGroup->dependenciesLeft.store(deps.size());
    newTaskGroup->dependents = {};
    
    // do something if it has dependencies
    for (TaskID dependentID : deps) {
        TaskGroupInfo* dependentTaskGroup = _allTaskGroups[dependentID];
        dependentTaskGroup->dependents.push_back(newTaskGroup);
    }
    if (deps.size() == 0) {
        _readyQueue.push(newTaskGroup);
        _worker_cv.notify_all();
    }
    _allTaskGroups[newTaskGroup->id] = newTaskGroup;
    _activeTaskGroups.fetch_add(1);

    lock.unlock();
    return newTaskGroup->id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
