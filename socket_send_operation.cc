#include "socket_send_operation.hh"

#include <iostream>

#include "socket.hh"

SocketSendOperation::SocketSendOperation(Socket* socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchWrite(socket);
    std::cout << "socket_send_operation\n";
}

SocketSendOperation::~SocketSendOperation()
{
    socket->io_context_.unwatchWrite(socket);
    std::cout << "~socket_send_operation\n";
}

ssize_t SocketSendOperation::syscall()
{
    std::cout << "send(" << socket->fd_ << ", buffer_, len_, 0)\n";
    return send(socket->fd_, buffer_, len_, 0);
}

void SocketSendOperation::suspend()
{
    socket->coroSend_ = awaitingCoroutine_;
}


