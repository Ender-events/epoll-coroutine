#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class Socket;

class SocketSendOperation : public BlockSyscall<SocketSendOperation, ssize_t>
{
public:
    SocketSendOperation(Socket* socket, void* buffer, std::size_t len);
    ~SocketSendOperation();

    ssize_t syscall();
    void suspend();
private:
    Socket* socket;
    void* buffer_;
    std::size_t len_;
};

