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