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
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
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
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    _numThreads = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::thread* threads = new std::thread[_numThreads];
    for (int i = 0; i < _numThreads; i++) {
        threads[i] = std::thread([=] {
            for (int j = i; j < num_total_tasks; j += _numThreads) {
                runnable->runTask(j, num_total_tasks);
            }
        });
    }
    for (int i = 0; i < _numThreads; i++) {
        threads[i].join();
    }
    delete[] threads;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
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
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    threads = new std::thread[num_threads];
    for (int i=0; i<num_threads; i++) {
        threads[i] = std::thread(&TaskSystemParallelThreadPoolSpinning::threadLoop, this);
    }
    _numThreads = num_threads;
    _isDone = false;
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    _isDone = true;
    for (int i = 0; i < _numThreads; i++) {
        threads[i].join();
    }
    delete[] threads;
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    _numTotalTasks = num_total_tasks;
    _completedTasks.store(0);
    _queueMutex.lock();
    for (int i = 0; i < num_total_tasks; i++) {
        _taskQueue.push({runnable, i});
    }
    _queueMutex.unlock();

    while (_completedTasks.load() < _numTotalTasks) { // task is not done
        std::this_thread::yield();
    }
}

void TaskSystemParallelThreadPoolSpinning::threadLoop() {
    int taskId;
    IRunnable* runnable;
    while (!_isDone) {
        taskId = -1;
        _queueMutex.lock();
        if (!_taskQueue.empty()) {
            runnable = _taskQueue.front().first;
            taskId = _taskQueue.front().second;
            _taskQueue.pop();
        }
        _queueMutex.unlock();
        if (taskId != -1) {
            runnable->runTask(taskId, _numTotalTasks);
            _completedTasks.fetch_add(1);
        }
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
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
    _queueCond.notify_all();
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
    _numTotalTasks = num_total_tasks;
    _completedTasks.store(0);
    _queueMutex.lock();
    for (int i = 0; i < num_total_tasks; i++) {
        _taskQueue.push({runnable, i});
    }
    _queueMutex.unlock();
    _queueCond.notify_all();

    while (_completedTasks.load() < _numTotalTasks) { // task is not done
        std::this_thread::yield();
    }
}

void TaskSystemParallelThreadPoolSleeping::threadLoop() {
    int taskId;
    IRunnable* runnable;
    while (true) {
        std::unique_lock<std::mutex> lock(_queueMutex);
        _queueCond.wait(lock, [this] {
            return _isDone || !_taskQueue.empty();
        });

        if (_isDone && _taskQueue.empty()) break;
        runnable = _taskQueue.front().first;
        taskId = _taskQueue.front().second;
        _taskQueue.pop();
        lock.unlock();
        runnable->runTask(taskId, _numTotalTasks);
        _completedTasks.fetch_add(1);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
