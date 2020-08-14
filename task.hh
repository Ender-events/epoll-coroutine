#pragma once

#include <coroutine>
#include <iostream>

namespace std
{
template <typename T = void>
struct task
{
    struct promise_type
    {
        coroutine_handle<> waiter; // who waits on this coroutine
        task<T> get_return_object() {
            return task<T>{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() { return {}; }
        auto final_suspend() {
            struct final_awaiter {
                bool await_ready() { return false; }
                void await_resume() {}
                void await_suspend(coroutine_handle<promise_type> me) {
                    std::cerr << "resghsg\n";
                    if (me.promise().waiter)
                        me.promise().waiter.resume();
                }
            };
            return final_awaiter{};
        }
        void return_void() {}
        void unhandled_exception() {}
    };
    task()
      : handle_{nullptr}
    {}
    task(coroutine_handle<promise_type> handle)
      : handle_{handle}
    {}
    /*
    ~task()
    {
        if (handle_)
            handle_.destroy();
    }
    */

    bool await_ready() { return false; }
    T await_resume() {  
        /*
        auto& result = handle_.promise().result;
        if (result.index() == 1) return get<1>(result);
        rethrow_exception(get<2>(result));
        */
    }
    void await_suspend(coroutine_handle<> waiter) {
        handle_.promise().waiter = waiter;
        handle_.resume();
    }

    void resume() {
        handle_.resume();
    }
    coroutine_handle<promise_type> handle_;
};
}

