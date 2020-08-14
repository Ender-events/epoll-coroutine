#include "socket_recv_operation.hh"

#include <iostream>

#include "socket.hh"

SocketRecvOperation::SocketRecvOperation(Socket* socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchRead(socket);
    std::cout << "socket_recv_operation\n";
}

SocketRecvOperation::~SocketRecvOperation()
{
    socket->io_context_.unwatchRead(socket);
    std::cout << "~socket_recv_operation\n";
}

ssize_t SocketRecvOperation::syscall()
{
    std::cout << "recv(" << socket->fd_ << ", buffer_, len_, 0)\n";
    return recv(socket->fd_, buffer_, len_, 0);
}

void SocketRecvOperation::suspend()
{
    socket->coroRecv_ = awaitingCoroutine_;
}
