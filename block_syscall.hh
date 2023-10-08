#pragma once

#include <cerrno>
#include <coroutine>
#include <type_traits>

#include <iostream>

template <typename SyscallOpt, typename ReturnValue>
class BlockSyscall {
public:
    BlockSyscall()
        : haveSuspend_{false}
    {
    }

    bool await_ready() const noexcept { return false; }

    bool await_suspend(std::coroutine_handle<> awaitingCoroutine)
    {
        static_assert(std::is_base_of_v<BlockSyscall, SyscallOpt>);
        returnValue_ = static_cast<SyscallOpt*>(this)->syscall();
        haveSuspend_ = returnValue_ == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
        if (haveSuspend_)
            static_cast<SyscallOpt*>(this)->suspend(awaitingCoroutine);

        return haveSuspend_;
    }

    ReturnValue await_resume()
    {
        std::cout << "await_resume\n";
        if (haveSuspend_)
            returnValue_ = static_cast<SyscallOpt*>(this)->syscall();
        return returnValue_;
    }

protected:
    bool haveSuspend_;
    ReturnValue returnValue_;
};
