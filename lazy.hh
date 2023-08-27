#include <iostream>
#pragma once
////////////////////////////////////////////////////////////////
// Reference implementation of std::lazy proposal D2506R0 https://wg21.link/p2506r0.
// https://godbolt.org/z/dxxavazPa

#include <cassert>
#include <coroutine>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <ranges>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#define _EMPTY_BASES __declspec(empty_bases)
#ifdef __clang__
#define _NO_UNIQUE_ADDRESS
#else
#define _NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#endif
#else
#define _EMPTY_BASES
#define _NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace std {
struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) _Aligned_block {
    unsigned char _Pad[__STDCPP_DEFAULT_NEW_ALIGNMENT__];
};

template <class _Alloc>
using _Rebind = typename allocator_traits<_Alloc>::template rebind_alloc<_Aligned_block>;

template <class _Allocator = void>
class _Promise_allocator { // statically specified allocator type
private:
    using _Alloc = _Rebind<_Allocator>;

    static void* _Allocate(_Alloc _Al, const size_t _Size)
    {
        if constexpr (default_initializable<_Alloc> && allocator_traits<_Alloc>::is_always_equal::value) {
            // do not store stateless allocator
            const size_t _Count = (_Size + sizeof(_Aligned_block) - 1) / sizeof(_Aligned_block);
            return _Al.allocate(_Count);
        } else {
            // store stateful allocator
            static constexpr size_t _Align = (::std::max)(alignof(_Alloc), sizeof(_Aligned_block));
            const size_t _Count = (_Size + sizeof(_Alloc) + _Align - 1) / sizeof(_Aligned_block);
            void* const _Ptr = _Al.allocate(_Count);
            const auto _Al_address = (reinterpret_cast<uintptr_t>(_Ptr) + _Size + alignof(_Alloc) - 1) & ~(alignof(_Alloc) - 1);
            ::new (reinterpret_cast<void*>(_Al_address)) _Alloc(::std::move(_Al));
            return _Ptr;
        }
    }

public:
    static void* operator new(const size_t _Size)
        requires default_initializable<_Alloc>
    {
        return _Allocate(_Alloc{}, _Size);
    }

    template <class _Alloc2, class... _Args>
        requires convertible_to<_Alloc2, _Allocator>
    static void* operator new(const size_t _Size, allocator_arg_t, _Alloc2&& _Al, _Args&...)
    {
        return _Allocate(static_cast<_Alloc>(static_cast<_Allocator>(::std::forward<_Alloc2>(_Al))),
            _Size);
    }

    template <class _This, class _Alloc2, class... _Args>
        requires convertible_to<_Alloc2, _Allocator>
    static void* operator new(const size_t _Size, _This&, allocator_arg_t, _Alloc2&& _Al, _Args&...)
    {
        return _Allocate(static_cast<_Alloc>(static_cast<_Allocator>(::std::forward<_Alloc2>(_Al))),
            _Size);
    }

    static void operator delete(void* const _Ptr, const size_t _Size) noexcept
    {
        if constexpr (default_initializable<_Alloc> && allocator_traits<_Alloc>::is_always_equal::value) {
            // make stateless allocator
            _Alloc _Al{};
            const size_t _Count = (_Size + sizeof(_Aligned_block) - 1) / sizeof(_Aligned_block);
            _Al.deallocate(static_cast<_Aligned_block*>(_Ptr), _Count);
        } else {
            // retrieve stateful allocator
            const auto _Al_address = (reinterpret_cast<uintptr_t>(_Ptr) + _Size + alignof(_Alloc) - 1) & ~(alignof(_Alloc) - 1);
            auto& _Stored_al = *reinterpret_cast<_Alloc*>(_Al_address);
            _Alloc _Al{::std::move(_Stored_al)};
            _Stored_al.~_Alloc();

            static constexpr size_t _Align = (::std::max)(alignof(_Alloc), sizeof(_Aligned_block));
            const size_t _Count = (_Size + sizeof(_Alloc) + _Align - 1) / sizeof(_Aligned_block);
            _Al.deallocate(static_cast<_Aligned_block*>(_Ptr), _Count);
        }
    }
};

template <>
class _Promise_allocator<void> { // type-erased allocator
private:
    using _Dealloc_fn = void (*)(void*, size_t);

    template <class _Alloc2>
    static void* _Allocate(_Alloc2 _Al2, size_t _Size)
    {
        using _Alloc = _Rebind<_Alloc2>;
        auto _Al = static_cast<_Alloc>(_Al2);

        if constexpr (default_initializable<_Alloc> && allocator_traits<_Alloc>::is_always_equal::value) {
            // don't store stateless allocator
            const _Dealloc_fn _Dealloc = [](void* const _Ptr, const size_t _Size) {
                _Alloc _Al{};
                const size_t _Count = (_Size + sizeof(_Dealloc_fn) + sizeof(_Aligned_block) - 1) / sizeof(_Aligned_block);
                _Al.deallocate(static_cast<_Aligned_block*>(_Ptr), _Count);
            };

            const size_t _Count = (_Size + sizeof(_Dealloc_fn) + sizeof(_Aligned_block) - 1) / sizeof(_Aligned_block);
            void* const _Ptr = _Al.allocate(_Count);
            ::memcpy(static_cast<char*>(_Ptr) + _Size, &_Dealloc, sizeof(_Dealloc));
            return _Ptr;
        } else {
            // store stateful allocator
            static constexpr size_t _Align = (::std::max)(alignof(_Alloc), sizeof(_Aligned_block));

            const _Dealloc_fn _Dealloc = [](void* const _Ptr, size_t _Size) {
                _Size += sizeof(_Dealloc_fn);
                const auto _Al_address = (reinterpret_cast<uintptr_t>(_Ptr) + _Size + alignof(_Alloc) - 1) & ~(alignof(_Alloc) - 1);
                auto& _Stored_al = *reinterpret_cast<const _Alloc*>(_Al_address);
                _Alloc _Al{::std::move(_Stored_al)};
                _Stored_al.~_Alloc();

                const size_t _Count = (_Size + sizeof(_Al) + _Align - 1) / sizeof(_Aligned_block);
                _Al.deallocate(static_cast<_Aligned_block*>(_Ptr), _Count);
            };

            const size_t _Count = (_Size + sizeof(_Dealloc_fn) + sizeof(_Al) + _Align - 1) / sizeof(_Aligned_block);
            void* const _Ptr = _Al.allocate(_Count);
            ::memcpy(static_cast<char*>(_Ptr) + _Size, &_Dealloc, sizeof(_Dealloc));
            _Size += sizeof(_Dealloc_fn);
            const auto _Al_address = (reinterpret_cast<uintptr_t>(_Ptr) + _Size + alignof(_Alloc) - 1) & ~(alignof(_Alloc) - 1);
            ::new (reinterpret_cast<void*>(_Al_address)) _Alloc{::std::move(_Al)};
            return _Ptr;
        }

#if defined(__GNUC__) && !defined(__clang__)
        // avoid false positive "control reaches end of non-void function"
        __builtin_unreachable();
#endif // defined(__GNUC__) && !defined(__clang__)
    }

public:
    static void* operator new(const size_t _Size)
    { // default: new/delete
        void* const _Ptr = ::operator new[](_Size + sizeof(_Dealloc_fn));
        const _Dealloc_fn _Dealloc = [](void* const _Ptr, const size_t _Size) {
            ::operator delete[](_Ptr, _Size + sizeof(_Dealloc_fn));
        };
        ::memcpy(static_cast<char*>(_Ptr) + _Size, &_Dealloc, sizeof(_Dealloc_fn));
        return _Ptr;
    }

    template <class _Alloc, class... _Args>
    static void* operator new(const size_t _Size, allocator_arg_t, const _Alloc& _Al, _Args&...)
    {
        return _Allocate(_Al, _Size);
    }

    template <class _This, class _Alloc, class... _Args>
    static void* operator new(const size_t _Size, _This&, allocator_arg_t, const _Alloc& _Al,
        _Args&...)
    {
        return _Allocate(_Al, _Size);
    }

    static void operator delete(void* const _Ptr, const size_t _Size) noexcept
    {
        _Dealloc_fn _Dealloc;
        ::memcpy(&_Dealloc, static_cast<const char*>(_Ptr) + _Size, sizeof(_Dealloc_fn));
        _Dealloc(_Ptr, _Size);
    }
};

template <class, template <class...> class>
inline constexpr bool __is_specialization_of = false;
template <class... _Args, template <class...> class _Templ>
inline constexpr bool __is_specialization_of<_Templ<_Args...>, _Templ> = true;

template <class _Ty>
concept _Await_suspend_result = same_as<_Ty, void> || same_as<_Ty, bool> || __is_specialization_of<_Ty, coroutine_handle>;

template <class _Ty, class _Promise = void>
concept simple_awaitable = requires(_Ty& _Val, const coroutine_handle<_Promise>& _Coro) {
    {
        _Val.await_ready()
    } -> convertible_to<bool>;
    {
        _Val.await_suspend(_Coro)
    } -> _Await_suspend_result;
    _Val.await_resume();
};

template <class _Ty, class _Promise = void>
concept _Has_member_co_await = requires(_Ty&& _Val) {
    {
        static_cast<_Ty&&>(_Val).operator co_await()
    } -> simple_awaitable<_Promise>;
};

template <class _Ty, class _Promise = void>
concept _Has_ADL_co_await = requires(_Ty&& _Val) {
    {
        operator co_await(static_cast<_Ty&&>(_Val))
    } -> simple_awaitable<_Promise>;
};

template <class _Ty, class _Promise = void>
concept awaitable = _Has_member_co_await<_Ty, _Promise> || _Has_ADL_co_await<_Ty, _Promise> || simple_awaitable<_Ty&, _Promise>;

static_assert(simple_awaitable<suspend_always>);
static_assert(simple_awaitable<suspend_never>);

template <class _Ty = void, class _Allocator = void>
class [[nodiscard]] lazy;

template <class _Ty>
class _Lazy_promise_base {
public:
    _Lazy_promise_base() noexcept
    {
    }

    _Lazy_promise_base(_Lazy_promise_base&& _That) noexcept(is_nothrow_move_constructible_v<_Ty>)
        : _Disc{_That._Disc}
    {
        switch (_Disc) {
        case _Discriminator::_Empty:
            break;
        case _Discriminator::_Data:
            ::std::construct_at(::std::addressof(_Data), ::std::move(_That._Data));
            ::std::destroy_at(::std::addressof(_That._Data_));
            break;
        case _Discriminator::_Exception:
            ::std::construct_at(::std::addressof(_Except), ::std::move(_That._Except));
            ::std::destroy_at(::std::addressof(_That._Except));
            break;
        }

        _That._Disc = _Discriminator::_Empty;
    }

    ~_Lazy_promise_base()
    {
        switch (_Disc) {
        case _Discriminator::_Empty:
            break;
        case _Discriminator::_Data:
            ::std::destroy_at(::std::addressof(_Data));
            break;
        case _Discriminator::_Exception:
            ::std::destroy_at(::std::addressof(_Except));
            break;
        }
    }
#ifndef _MSC_VER // Avoid false positive
    [[nodiscard]]
#endif
    suspend_always
    initial_suspend() noexcept
    {
        return {};
    }

    [[nodiscard]] auto final_suspend() noexcept
    {
        return _Final_awaiter{};
    }

    void return_value(_Ty _Val) noexcept
        requires is_reference_v<_Ty>
    {
        switch (_Disc) {
        case _Discriminator::_Exception:
            break;
        case _Discriminator::_Empty:
            // _Data is a pointer, we can begin its lifetime by assigning
        case _Discriminator::_Data:
            _Data = ::std::addressof(_Val);
            _Disc = _Discriminator::_Data;
            break;
        }
    }

    // clang-format off
    template <class _Uty>
        requires (!is_reference_v<_Ty> && convertible_to<_Uty, _Ty> && constructible_from<_Ty, _Uty>)
    void return_value(_Uty&& _Val) noexcept(is_nothrow_constructible_v<_Uty, _Ty>) {
        // clang-format on
        switch (_Disc) {
        case _Discriminator::_Exception:
            break;
        case _Discriminator::_Data:
            ::std::destroy_at(::std::addressof(_Data));
            _Disc = _Discriminator::_Empty;
            [[fallthrough]];
        case _Discriminator::_Empty:
            ::std::construct_at(::std::addressof(_Data), ::std::forward<_Uty>(_Val));
            _Disc = _Discriminator::_Data;
        }
    }

    void unhandled_exception()
    {
        switch (_Disc) {
        case _Discriminator::_Exception:
            break;
        case _Discriminator::_Data:
            ::std::destroy_at(::std::addressof(_Data));
            _Disc = _Discriminator::_Empty;
            [[fallthrough]];
        case _Discriminator::_Empty:
            ::std::construct_at(::std::addressof(_Except), ::std::current_exception());
            _Disc = _Discriminator::_Exception;
            break;
        }
    }

private:
    template <class, class>
    friend class lazy;

    struct _Final_awaiter {
        [[nodiscard]] bool await_ready() noexcept
        {
            return false;
        }

        template <class _Promise>
        [[nodiscard]] coroutine_handle<> await_suspend(coroutine_handle<_Promise> _Coro) noexcept
        {
#ifdef __cpp_lib_is_pointer_interconvertible
            static_assert(is_pointer_interconvertible_base_of_v<_Lazy_promise_base, _Promise>);
#endif // __cpp_lib_is_pointer_interconvertible

            _Lazy_promise_base& _Current = _Coro.promise();
            return _Current._Cont ? _Current._Cont : ::std::noop_coroutine();
        }

        void await_resume() noexcept
        {
        }
    };

    struct _Awaiter {
        coroutine_handle<_Lazy_promise_base> _Coro;

        [[nodiscard]] bool await_ready() noexcept
        {
            return !_Coro;
        }

        [[nodiscard]] coroutine_handle<_Lazy_promise_base>
        await_suspend(coroutine_handle<> _Cont) noexcept
        {
            _Coro.promise()._Cont = _Cont;
            return _Coro;
        }

        _Ty await_resume()
        {
            auto& _Promise = _Coro.promise();
            switch (_Promise._Disc) {
            case _Discriminator::_Data:
                if constexpr (is_reference_v<_Ty>) {
                    return static_cast<_Ty>(*_Promise._Data);
                } else {
                    return ::std::move(_Promise._Data);
                }
#if defined(__GNUC__) && !defined(__clang__)
                // avoid false positive "fallthrough"
                __builtin_unreachable();
#endif // defined(__GNUC__) && !defined(__clang__)
            case _Discriminator::_Exception:
                assert(_Promise._Except && "This can't happen?");
                ::std::rethrow_exception(::std::move(_Promise._Except));
            default:
            case _Discriminator::_Empty:
                assert(false && "This can't happen?");
                ::std::terminate();
            }
        }
    };

    enum class _Discriminator : unsigned char {
        _Empty,
        _Exception,
        _Data
    };
    union {
        exception_ptr _Except;
        conditional_t<is_reference_v<_Ty>, add_pointer_t<_Ty>, _Ty> _Data;
    };
    _Discriminator _Disc = _Discriminator::_Empty;
    coroutine_handle<> _Cont;
};

template <class _Ty>
    requires is_void_v<_Ty>
class _Lazy_promise_base<_Ty> {
public:
    _Lazy_promise_base() noexcept
    {
    }

    _Lazy_promise_base(_Lazy_promise_base&& _That) noexcept(is_nothrow_move_constructible_v<_Ty>)
        : _Disc{_That._Disc}
    {
        if (_Disc == _Discriminator::_Exception) {
            ::std::construct_at(::std::addressof(_Except), ::std::move(_That._Except));
            ::std::destroy_at(::std::addressof(_That._Except));
            _That._Disc = _Discriminator::_Empty;
        }
    }

    ~_Lazy_promise_base()
    {
        if (_Disc == _Discriminator::_Exception) {
            ::std::destroy_at(::std::addressof(_Except));
        }
    }

    [[nodiscard]] suspend_always initial_suspend() noexcept
    {
        return {};
    }

    [[nodiscard]] auto final_suspend() noexcept
    {
        return _Final_awaiter{};
    }

    void return_void() noexcept
    {
    }

    void unhandled_exception()
    {
        if (_Disc == _Discriminator::_Empty) {
            ::std::construct_at(::std::addressof(_Except), ::std::current_exception());
            _Disc = _Discriminator::_Exception;
        }
    }

private:
    template <class, class>
    friend class lazy;

    struct _Final_awaiter {
        [[nodiscard]] bool await_ready() noexcept
        {
            return false;
        }

        template <class _Promise>
        [[nodiscard]] coroutine_handle<> await_suspend(coroutine_handle<_Promise> _Coro) noexcept
        {
#ifdef __cpp_lib_is_pointer_interconvertible
            static_assert(is_pointer_interconvertible_base_of_v<_Lazy_promise_base, _Promise>);
#endif // __cpp_lib_is_pointer_interconvertible

            _Lazy_promise_base& _Current = _Coro.promise();
            return _Current._Cont ? _Current._Cont : ::std::noop_coroutine();
        }

        void await_resume() noexcept
        {
        }
    };

    struct _Awaiter {
        coroutine_handle<_Lazy_promise_base> _Coro;

        [[nodiscard]] bool await_ready() noexcept
        {
            return !_Coro;
        }

        [[nodiscard]] coroutine_handle<_Lazy_promise_base>
        await_suspend(coroutine_handle<> _Cont) noexcept
        {
            _Coro.promise()._Cont = _Cont;
            return _Coro;
        }

        void await_resume()
        {
            auto& _Promise = _Coro.promise();
            if (_Promise._Disc == _Discriminator::_Exception) {
                ::std::rethrow_exception(::std::move(_Promise._Except));
            }
        }
    };

    enum class _Discriminator : unsigned char {
        _Empty,
        _Exception
    };
    union {
        exception_ptr _Except;
    };
    _Discriminator _Disc = _Discriminator::_Empty;
    coroutine_handle<> _Cont;
};

template <class _Ty, class _Allocator>
class [[nodiscard]] lazy {
public:
    static_assert(is_void_v<_Ty> || is_reference_v<_Ty> || (is_object_v<_Ty> && is_move_constructible_v<_Ty>),
        "lazy's first template argument must be void, a reference type, or a "
        "move-constructible object type");

    struct _EMPTY_BASES promise_type : _Promise_allocator<_Allocator>, _Lazy_promise_base<_Ty> {
        [[nodiscard]] lazy get_return_object() noexcept
        {
            return lazy{coroutine_handle<promise_type>::from_promise(*this)};
        }
    };

    lazy(lazy&& _That) noexcept
        : _Coro(::std::exchange(_That._Coro, {}))
    {
    }

    ~lazy()
    {
        if (_Coro) {
            _Coro.destroy();
        }
    }

    [[nodiscard]] typename _Lazy_promise_base<_Ty>::_Awaiter operator co_await()
    {
        // Pre: _Coro refers to a coroutine suspended at its initial suspend point
        assert(_Coro && !_Coro.done() && "co_await requires the lazy object to be associated with a coroutine "
                                         "suspended at its initial suspend point");

        auto& _Promise_base = static_cast<_Lazy_promise_base<_Ty>&>(_Coro.promise());
        return typename _Lazy_promise_base<_Ty>::_Awaiter{
            coroutine_handle<_Lazy_promise_base<_Ty>>::from_promise(_Promise_base)};
    }

    [[nodiscard]] _Ty sync_await()
    {
        // Pre: _Coro refers to a coroutine suspended at its initial suspend point
        assert(_Coro && !_Coro.done() && "sync_await requires the lazy object to be associated with a coroutine "
                                         "suspended at its initial suspend point");

        simple_awaitable<noop_coroutine_handle> auto _Simple = operator co_await();

        assert(!_Simple.await_ready() && "sync_await requires the lazy object to be associated with a coroutine "
                                         "suspended at its initial suspend point");

        _Simple.await_suspend(::std::noop_coroutine()).resume();

        return _Simple.await_resume();
    }

private:
    friend _Lazy_promise_base<_Ty>;

    explicit lazy(coroutine_handle<promise_type> _Coro_) noexcept
        : _Coro(_Coro_)
    {
    }

    coroutine_handle<promise_type> _Coro = nullptr;
};
} // namespace std
