#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#include "io_context.hh"
#include "lazy.hh"
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"

class Socket : std::enable_shared_from_this<Socket> {
public:
    /* Listen tcp non blocking socket */
    Socket(std::string_view port, IOContext& io_context);
    Socket(const Socket&) = delete;
    Socket(Socket&& socket) noexcept;

    ~Socket();

    std::lazy<std::shared_ptr<Socket>> accept();

    SocketRecvOperation recv(void* buffer, std::size_t len);
    SocketSendOperation send(void* buffer, std::size_t len);

    bool resumeRecv()
    {
        if (!coroRecv_)
            return false;
        coroRecv_.resume();
        return true;
    }

    bool resumeSend()
    {
        if (!coroSend_)
            return false;
        coroSend_.resume();
        return true;
    }

    IOContext& getContext() { return io_context_; }

private:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    IOContext& io_context_;
    int fd_ = -1;
    struct addrinfo* addr_res = nullptr;
    friend IOContext;
    uint32_t io_state_ = 0;
    uint32_t io_new_state_ = 0;
    bool canceled = false;

    explicit Socket(int fd, IOContext& io_context);
    std::coroutine_handle<> coroRecv_ = nullptr;
    std::coroutine_handle<> coroSend_ = nullptr;

    void cancel()
    {
        canceled = true;
        if (coroRecv_ && !coroRecv_.done()) {
            coroRecv_.resume();
        }
        if (coroSend_ && !coroSend_.done()) {
            coroSend_.resume();
        }
    }
};
