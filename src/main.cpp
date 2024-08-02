#include <chrono>
#include <coroutine>
#include "debug.hpp"

template<class T>
struct Promise {
    auto initial_suspend() noexcept { return std::suspend_always(); }

    auto final_suspend() noexcept { return std::suspend_always(); }

    void unhandled_exception() { throw; }

    auto yield_value(T ret) {
        mRetvalue = ret;
        return std::suspend_always();
    }

    void return_void() { mRetvalue = 0; }

    std::coroutine_handle<Promise> get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    T mRetvalue;
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

        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> coroutine) {
            return mCoroution;
        }

        int await_resume() const {
            // return mCoroution.promise().
            return 0;
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

Task<int> world() {
    debug(), "world";
    co_return;
}

Task<int> hello() {
    debug(), "hello";
    Task<int> son = world();
    int a = co_await son;
    debug(), a;
    co_return;
}

int main() {
    Task<int> corou1 = hello();
    while (!corou1.mCoroution.done()) {
        debug(), "in";
        corou1.mCoroution.resume();
    }
    return 0;
}