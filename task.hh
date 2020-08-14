#pragma once

// A simple version of cppcoro::task working with g++10

#include <coroutine>
#include <iostream>

namespace std
{
    template <typename T> struct task;
    namespace detail
    {

        template <typename T>
        struct promise_type_base
        {
            coroutine_handle<> waiter; // who waits on this coroutine
            task<T> get_return_object();
            suspend_always initial_suspend() { return {}; }
            struct final_awaiter {
                bool await_ready() { return false; }
                void await_resume() {}

                template <typename promise_type>
                    void await_suspend(coroutine_handle<promise_type> me) {
                        if (me.promise().waiter)
                            me.promise().waiter.resume();
                    }
            };
            auto final_suspend() {
                return final_awaiter{};
            }
            void unhandled_exception() {}
        };
        template <typename T>
        struct promise_type final : promise_type_base<T>
        {
            T result;
            void return_value(T value)
            {
                result = value;
            }
            T await_resume()
            {
                return result;
            }
            task<T> get_return_object();
        };
        template <>
        struct promise_type<void> final : promise_type_base<void>
        {
            void return_void() {}
            void await_resume() {}
            task<void> get_return_object();
        };

    }

template <typename T = void>
struct task
{
    using promise_type = detail::promise_type<T>;
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
    T await_resume();
    void await_suspend(coroutine_handle<> waiter) {
        handle_.promise().waiter = waiter;
        handle_.resume();
    }

    void resume() {
        handle_.resume();
    }
    coroutine_handle<promise_type> handle_;
};

template <typename T>
T task<T>::await_resume() {
    return handle_.promise().result;
}
template <>
inline void task<void>::await_resume() {}
    namespace detail
    {
        template <typename T>
        task<T> promise_type<T>::get_return_object()
        {
            return task<T>{coroutine_handle<promise_type<T>>::from_promise(*this)};
        }
        inline task<void> promise_type<void>::get_return_object()
        {
            return task<void>{coroutine_handle<promise_type<void>>::from_promise(*this)};
        }
    }
}
