우선 tasksys.h 파일에서 두 개의 구조체를 만들었다. 
```cpp
typedef struct _TaskGroupInfo {
    TaskID id; // group
    IRunnable* runnable;
    int numTotalTasks;
    std::atomic<int> completedTasks;
    std::atomic<int> dependenciesLeft;
    std::vector<TaskID> dependents;
} TaskGroupInfo;

typedef struct _TaskUnitInfo {
    TaskID id; // single task
    IRunnable* runnable;
    TaskGroupInfo* group; // used when task group end
} TaskUnitInfo;
```
앞의 `TaskGroupInfo` 구조체는 task group의 정보를 관리한다. `TaskUnitInfo` 구조체는 각각의 개별 task의 정보를 관리한다. `TaskUnitInfo` 구조체에서 `group` 변수를 통해 해당 task가 속한 task group을 접근할 수 있다. 

```cpp
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
    private:
        int _numThreads;
        std::thread* threads;
        std::map<TaskID, TaskGroupInfo*> _allTaskGroups;
        std::queue<TaskUnitInfo*> _taskQueue; // single task
        std::mutex _mutex;
        std::atomic<int> _activeTaskGroups;
        std::atomic<int> _nextTaskGroupId;
        std::condition_variable _worker_cv;
        std::condition_variable _sync_cv;
        bool _isDone;
        void threadLoop();
};
```
여기서 task group의 id와 `TaskGroupInfo*`를 연결하는 map인 `_allTaskGroups`를 추가하였다. 또한 `sync()` 함수에 대한 조건 변수인 `_sync_cv`를 추가하였다. task group의 개수를 관리하기 위해 atomic 변수인 `_activeTaskGroups`와 `_nextTaskGroupId`를 추가하였다. 

thread를 spawn하는 함수를 아래와 같이 수정하였다. `_nextTaskGroupId` 변수를 0으로 초기화해준다. 
```cpp
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
```

`run()` 함수를 아래와 같이 수정하였다. 
```cpp
void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    runAsyncWithDeps(runnable, num_total_tasks, {});
    sync();
}
```

`threadLoop()` 함수를 아래와 같이 수정하였다. 
```cpp
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
```
여기서 현재 task group이 완료되었을 때 해당하는 task group에 dependent한 다른 task group들의 task들을 전부 `_taskQueue`에 넣는다. 모든 task group이 완료되면 `sync()` 함수에서 사용하는 조건 변수인 `_sync_cv`를 깨운다. 

다음으로 `runAsyncWithDeps()` 함수는 아래와 같다. 
```cpp
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
```
`newTaskGroup`을 만들어서 map인 `_allTaskGroups`에 key와 value를 설정해준다. `_activeTaskGroups` 변수를 1 증가시킨다. 만약 `newTaskGroup`이 어떠한 dependency도 없다면 바로 `_taskQueue`에 task를 하나씩 넣는다. 

다음으로 `sync()`를 작성하였다. 
```cpp
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
```
조건 변수를 이용해 기다리는 것 말고는 일이 없다. 

```
===================================================================================
Test name: mandelbrot_chunked_async
===================================================================================
[Serial]:               [429.545] ms
[Parallel + Always Spawn]:              [429.412] ms
[Parallel + Thread Pool + Spin]:                [431.476] ms
[Parallel + Thread Pool + Sleep]:               [64.348] ms
===================================================================================
```
하나민 실험해보긴 했으나 대충 나머지도 잘 돌아가지 않을까 싶다. 