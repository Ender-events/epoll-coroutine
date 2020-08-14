#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class Socket;

class SocketAcceptOperation : public BlockSyscall<SocketAcceptOperation, int>
{
public:
    SocketAcceptOperation(Socket* socket);
    ~SocketAcceptOperation();

    int syscall();
    void suspend();
private:
    Socket* socket;
    void* buffer_;
    std::size_t len_;
};

