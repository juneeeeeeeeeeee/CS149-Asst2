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
    _isDone = false;
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
    runAsyncWithDeps(runnable, num_total_tasks, {});
    sync();
}

void TaskSystemParallelThreadPoolSleeping::threadLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        _worker_cv.wait(lock, [this] {
            return _isDone || !_taskQueue.empty();
        });

        if (_isDone && _taskQueue.empty()) break;
        TaskUnitInfo* currentTaskUnit = _taskQueue.front();
        _taskQueue.pop();
        lock.unlock();

        currentTaskUnit->runnable->runTask(currentTaskUnit->id, currentTaskUnit->group->numTotalTasks);
        
        lock.lock();
        if (currentTaskUnit->group->completedTasks.fetch_add(1) == currentTaskUnit->group->numTotalTasks - 1) {
            // dependency check
            for (TaskID dependentID : currentTaskUnit->group->dependents) {
                TaskGroupInfo* dependentTaskGroup = _allTaskGroups[dependentID];
                if (dependentTaskGroup->dependenciesLeft.fetch_sub(1) == 1) {
                    for (int i = 0; i < dependentTaskGroup -> numTotalTasks; i++) {
                        _taskQueue.push(new TaskUnitInfo{i, dependentTaskGroup->runnable, dependentTaskGroup});
                    }
                    _worker_cv.notify_all();
                }
            }
            _allTaskGroups.erase(currentTaskUnit->group->id);
            _activeTaskGroups.fetch_sub(1);
            if (_activeTaskGroups.load() == 0) {
                // notify sync function
                delete currentTaskUnit->group;
                _sync_cv.notify_one();
            }
        }
        delete currentTaskUnit;
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
        dependentTaskGroup->dependents.push_back(newTaskGroup->id);
    }
    if (deps.size() == 0) {
        for (int i = 0; i < num_total_tasks; i++) {
            _taskQueue.push(new TaskUnitInfo{i, runnable, newTaskGroup});
        }
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
    std::unique_lock<std::mutex> lock(_mutex);
    _sync_cv.wait(lock, [this]{
        return !_activeTaskGroups.load();
    });
    lock.unlock();
    return;
}
