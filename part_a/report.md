# parallel task 시스템으로 이동
우선 serial하게 구현되어 있는 task를 parallel하게 바꾸어야 한다. 
```cpp
TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    _numThreads = num_threads;
}
```
thread를 spawn할 때 `num_threads` 변수를 넘겨주는데 이는 지역변수이므로 `ITaskSystem` class에 넣어주어야 한다. 이를 위해 `ITaskSystem` class에 `_numThread` 변수를 넣어주었다. 
```cpp
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
    private:
        int _numThreads;
};
```
이제 `run()`을 작성하여야 한다. 우선 가장 간단한 방법으로 구현할 것을 명시하였기 때문에 interleaving 방식으로 정적으로 task를 각각의 thread에 나누어 줄 것이다. (그렇지 않은 경우 mutex와 task queue를 이용해 동적인 방법으로 구현할 수 있다. )
```cpp
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
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
```
다른 부분보다 lambda 함수를 처음 보아 당황하였다. lambda 함수에서 [=]는 외부의 변수를 값으로 캡처해온다는 뜻이다. [&]으로 주소를 캡처할 경우 외부의 변수(예를 들어 `i`)가 변해 에러가 뜬다. 

한편 이게 어떻게 parallel하게 동작하는지 의문이었는데 std::thread()는 thread를 생성하기만 하며, 스케쥴러가 실행하기로 하면 lambda 함수 내의 context가 실행된다(고 잼민이가 그랬다...). 

실행 결과는 아래와 같다. 
```
Test name: super_super_light
[Serial]:               [5.054] ms
[Parallel + Always Spawn]:              [149.115] ms

Test name: mandelbrot_chunked
[Serial]:               [427.286] ms
[Parallel + Always Spawn]:              [64.472] ms
```
super_super_light과 같이 간단한 작업의 경우 thread를 만들고 파괴하거나 context switching을 하는 것에 의한 overhead가 생겨 serial보다 훨씬 느려진다. mandelbrot_chunked와 같이 계산이 많은 작업의 경우 이러한 overhead가 상대적으로 작아지고 여러 core에서 동시에 작업해 얻는 이득이 커져 serial보다 훨씬 빨라진다. 

여기서 단순히 코드를 병렬화한다고 빨라지는 것이 아님을 알 수 있다. 

# thread pool 사용
각각의 thread를 bulk로 생성하고 작업이 완료되면 join한다. 이때 각각의 thread는 task들을 task queue에서 가져간다. task queue에 동시에 접근하는 것을 막기 위해 mutex를 사용했다. 

우선 thread를 생성하는 code를 수정하여야 한다. 
```cpp
TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    threads = new std::thread[num_threads];
    for (int i=0; i<num_threads; i++) {
        threads[i] = std::thread(&TaskSystemParallelThreadPoolSpinning::threadLoop, this);
    }
    _numThreads = num_threads;
    _isDone = false;
}
```
각각의 thread가 class 안의 함수 `threadLoop()`을 실행하게 하였다. 또한 `_numThreads`와 `_isDone` 변수를 class에 넣어 동일 class 내의 다른 함수에서도 사용할 수 있게 하였다. `_isDone`은 모든 task가 완료되었는지 나타내는 flag이다. 

`run()` 함수는 아래와 같다. 
```cpp
void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
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
```
class 내의 다른 함수들에서 변수 `num_total_tasks`를 사용하기 위해 class에 변수 `_numTotalTasks`를 추가했다. `_completedTasks` 변수는 지금까지 완료된 task의 개수를 의미한다. 여러 thread에서 동시에 접근하여 오류가 날 수 있기 때문에 atomic operation을 적용하였다. task queue에 mutex를 적용하기 위해 `_queueMutex`를 사용하였다. 이 queue에 task들을 모두 push해준다. task가 완료되지 않았을 때 다른 thread로 작업을 전환한다. 

이렇게 변수와 함수를 추가한 class `TaskSystemParallelThreadPoolSpinning`는 아래와 같다. 
```cpp
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
    private:
        int _numThreads;
        std::thread* threads;
        std::queue<std::pair<IRunnable*, int>> _taskQueue;
        std::mutex _queueMutex;
        std::atomic<int> _completedTasks;
        int _numTotalTasks;
        bool _isDone;
        void threadLoop();
};
```

전체 task가 완료되었을 때의 code는 아래와 같다. 
```cpp
TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    _isDone = true;
    for (int i = 0; i < _numThreads; i++) {
        threads[i].join();
    }
    delete[] threads;
}
```
`_isDone` flag가 class 안의 함수 `threadLoop()`에서 조건문으로 쓰이기 때문에 `_isDone` flag를 true로 변경해주면 worker thread들이 전부 종료된다. 

class 안에 새로 추가한 함수 `threadLoop()`은 아래와 같다. 
```cpp
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
```
queue에서 하나의 원소를 뽑아 task를 실행하는 코드이다. 

이때 실행 결과는 아래와 같다. 
```
Results for: super_super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                5.027     5.205       0.97  (OK)
[Parallel + Always Spawn]               118.286   108.734     1.09  (OK)
[Parallel + Thread Pool + Spin]         9.938     22.009      0.45  (OK)
================================================================================
Executing test: super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                65.107    74.68       0.87  (OK)
[Parallel + Always Spawn]               127.065   119.119     1.07  (OK)
[Parallel + Thread Pool + Spin]         27.472    36.697      0.75  (OK)
================================================================================
Executing test: ping_pong_equal...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_equal
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1182.614  1322.057    0.89  (OK)
[Parallel + Always Spawn]               586.834   492.051     1.19  (OK)
[Parallel + Thread Pool + Spin]         433.277   458.771     0.94  (OK)
================================================================================
Executing test: ping_pong_unequal...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_unequal
                                        STUDENT   REFERENCE   PERF?
[Serial]                                2027.259  1984.712    1.02  (OK)
[Parallel + Always Spawn]               871.207   647.534     1.35  (NOT OK)
[Parallel + Thread Pool + Spin]         760.149   636.895     1.19  (OK)
================================================================================
Executing test: recursive_fibonacci...
Reference binary: ./runtasks_ref_linux
Results for: recursive_fibonacci
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1049.799  1613.957    0.65  (OK)
[Parallel + Always Spawn]               331.614   388.185     0.85  (OK)
[Parallel + Thread Pool + Spin]         382.631   437.818     0.87  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop
                                        STUDENT   REFERENCE   PERF?
[Serial]                                690.98    695.894     0.99  (OK)
[Parallel + Always Spawn]               843.248   749.797     1.12  (OK)
[Parallel + Thread Pool + Spin]         301.819   337.877     0.89  (OK)
================================================================================
```
이 이후의 test는 실행이 너무 오래 걸려 포기했다. [Parallel + Always Spawn]이 잘 동작함을 확인할 수 있다. 여기서 thread pool이 많은 경우 thread를 재사용함으로서 overhead를 줄임을 확인할 수 있었다. 