#include <chrono>
#include <vector>
#include <optional>
#include <coroutine>
#include "debug.hpp"
#include <deque>
#include <queue>
#include <thread>

struct Loop{
    std::deque<std::coroutine_handle<>> mReadyQueue;

    // std::deque<std::coroutine_handle<>> mWaitingQueue;
     
    struct TimerEntry {
        std::chrono::system_clock::time_point expireTime;
        std::coroutine_handle<> coroutine; 

        bool operator<(TimerEntry const & that) const noexcept{ // 小于号运算是大于号运算，用于将priority_queue的大顶堆变为小顶堆，将时间近的放在最上方
            return expireTime > that.expireTime;
        }
    };
  
    // std::vector<TimerEntry> timerTable;
    std::priority_queue<TimerEntry> mTimerHeap;
 
    void addTask(std::coroutine_handle<> task){
        mReadyQueue.push_front(task);
    }

    void addTimer(std::chrono::system_clock::time_point expireTime, std::coroutine_handle<> t){
        mTimerHeap.push({expireTime, t});
    }

    void runAll(){
        while (!mTimerHeap.empty() || !mReadyQueue.empty()){
            while (!mReadyQueue.empty()){
                auto readyTask = mReadyQueue.front();
                mReadyQueue.pop_front();
                readyTask.resume();
            }
            if (!mTimerHeap.empty()){
                auto nowTime = std::chrono::system_clock::now();
                auto  timer = std::move(mTimerHeap.top());
                if (timer.expireTime < nowTime){
                    mTimerHeap.pop();
                    mReadyQueue.push_back(timer.coroutine);
                } else {
                    std::this_thread::sleep_until(timer.expireTime);
                }
            }
        }
    }

    Loop &operator=(Loop &&) = delete; // 单例模式，防止拷贝
};

Loop& getLoop(){
    static Loop loop; //单例模式
    return loop;
}




template<class T>
struct Promise {
    Promise()noexcept{};
    Promise(Promise &&) = delete;
    auto initial_suspend() noexcept { return std::suspend_always(); }

    auto final_suspend() noexcept { return std::suspend_always(); }

    void unhandled_exception() noexcept{
        mException = std::current_exception();
    }

    auto yield_value(T ret) {
        mRetvalue = ret;
        return std::suspend_always();
    }

    void return_value(T ret) noexcept{
        mRetvalue = ret;
    }

    std::coroutine_handle<Promise> get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    T result(){
        if (mException){
            std::rethrow_exception(mException); // 有异常就再次抛出，没有就返回值
        }

        // T ret = std::move(mResult);
        return mRetvalue.value();
    }
    std::exception_ptr mException{};

    // T mRetvalue;
    std::optional<T> mRetvalue;
};

template<>
struct Promise<void> {
    Promise() = default;
    Promise(Promise &&) = delete;
    ~Promise() = default;
    auto initial_suspend() noexcept { return std::suspend_always(); }

    auto final_suspend() noexcept { return std::suspend_always(); }

    void unhandled_exception() noexcept{
        mException = std::current_exception();
    }

    // auto yield_value(T ret) {
    //     mRetvalue = ret;
    //     return std::suspend_always();
    // }

    void return_void() {  }

    std::coroutine_handle<Promise> get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    void result(){
        if (mException){
            std::rethrow_exception(mException); // 有异常就再次抛出，没有就返回值
        }

        // return mRetvalue.value();
    }
    std::exception_ptr mException{};

    // std::optional<T> mRetvalue;
};

template<class T>
struct Task {
    using promise_type = Promise<T>;
    std::coroutine_handle<promise_type> mCoroution;

    Task(std::coroutine_handle<promise_type> coroutine)
        : mCoroution(coroutine) {}

    Task(Task &&) = delete;

    struct Awaiter {
        bool await_ready() const noexcept { return false; }

        // std::coroutine_handle 类似于指向promise_type的原始指针，destory就是delete

        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> coroutine) { // std::coroutine_handle<> = std::coroutine_handle<void> 特化版本 类型擦除
            return mCoroution;
        }

        auto await_resume() const {
            // return mCoroution.promise().
            return mCoroution.promise().result();
        }

        std::coroutine_handle<promise_type> mCoroution;
    };

    auto operator co_await() const noexcept { return Awaiter(mCoroution); }

    // auto await_transform(std::coroutine_handle<promise_type> coroutine){
    //     return std::suspend_always();
    // }

    operator std::coroutine_handle<>() const noexcept { return mCoroution; }

    ~Task() { mCoroution.destroy(); };
};

struct SleepAwaiter{
    bool await_ready(){
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine){
        getLoop().addTimer(mExpireTime, coroutine);        
        return std::noop_coroutine();
    }

    void await_resume(){

    }

    std::chrono::system_clock::time_point mExpireTime;
};

Task<void> sleep_until(std::chrono::system_clock::time_point expireTime) {
    co_await SleepAwaiter(expireTime);
    co_return;
}

Task<void> sleep_for(std::chrono::system_clock::duration duration) {
    co_await SleepAwaiter(std::chrono::system_clock::now() + duration);
    co_return;
}

Task<int> world() {
    debug(), "world";
    co_return 0;
}

Task<int> hello() {
    debug(), "hello";
    Task<int> son = world();
    co_yield 42;
    int a = co_await son;
    debug(), a;
    co_return 0;
}



int main() {
    Task<int> corou1 = hello();
    while (!corou1.mCoroution.done()) {
        debug(), "in";
        corou1.mCoroution.resume();
    }
    return 0;
}