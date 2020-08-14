#pragma once

#include <set>
#include <stdexcept>
#include <sys/epoll.h>

#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"

/* Just an epoll wrapper */
class Socket;

class IOContext
{
public:
    IOContext()
        : fd_{epoll_create1(0)}
    {
        if (fd_ == -1)
            throw std::runtime_error{"epoll_create1"};
    }

    void run();
private:
    constexpr static std::size_t max_events = 10;
    const int fd_;

    // Fill it by watchRead / watchWrite
    std::set<Socket*> processedSockets;

    friend Socket;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    void attach(Socket* socket);
    void watchRead(Socket* socket);
    void unwatchRead(Socket* socket);
    void watchWrite(Socket* socket);
    void unwatchWrite(Socket* socket);
    void detach(Socket* socket);
};
