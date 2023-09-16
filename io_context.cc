#include "io_context.hh"

#include <bits/types/sigset_t.h>
#include <csignal>
#include <exception>
#include <stdexcept>
#include <sys/signalfd.h>

#include "socket.hh"

void IOContext::run()
{
    sigset_t mask;
    if (sigemptyset(&mask) == -1) {
        throw std::runtime_error{"sigemptyset"};
    }
    if (sigaddset(&mask, SIGINT)) {
        throw std::runtime_error{"sigaddset"};
    }
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        throw std::runtime_error{"sigprocmask"};
    }
    auto sigint_fd = signalfd(-1, &mask, 0);
    if (sigint_fd == -1) {
        throw std::runtime_error{"signalfd"};
    }
    {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sigint_fd;
        if (epoll_ctl(fd_, EPOLL_CTL_ADD, sigint_fd, &ev) == -1) {
            throw std::runtime_error{"epoll_ctl: attach signal"};
        }
    }

    struct epoll_event ev, events[max_events];
    bool loop_run = true;

    while (loop_run) {
        auto nfds = epoll_pwait(fd_, events, max_events, -1, &mask);
        std::cout << "toto: " << nfds << '\n';
        if (nfds == -1)
            throw std::runtime_error{"epoll_wait"};

        for (int n = 0; n < nfds; ++n) {
            const auto& event = events[n];
            if (event.data.fd == sigint_fd) {
                loop_run = false;
                continue;
            }
            auto socket = static_cast<Socket*>(event.data.ptr);

            if (event.events & EPOLLIN)
                socket->resumeRecv();
            if (event.events & EPOLLOUT)
                socket->resumeSend();
        }
        for (auto* socket : processedSockets) {
            auto io_state = socket->io_new_state_;
            if (socket->io_state_ == io_state)
                continue;
            ev.events = io_state;
            ev.data.ptr = socket;
            if (epoll_ctl(fd_, EPOLL_CTL_MOD, socket->fd_, &ev) == -1)
                throw std::runtime_error{"epoll_ctl: mod"};
            socket->io_state_ = io_state;
        }
    }
    cleanIO();
}

void IOContext::spawn(std::lazy<>&& task)
{
    [](std::lazy<> t, IOContext* ctx) -> oneway_task {
        try {
            co_await t;
        } catch (const std::exception& e) {
            ctx->delayDestructor.push_back(std::move(t));
        }
    }(std::move(task), this);
}

void IOContext::attach(Socket* socket)
{
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
}

void IOContext::watchRead(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLIN;
    processedSockets.insert(socket);
}

void IOContext::unwatchRead(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLIN;
    processedSockets.insert(socket);
}

void IOContext::watchWrite(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLOUT;
    processedSockets.insert(socket);
}

void IOContext::unwatchWrite(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLOUT;
    processedSockets.insert(socket);
}

void IOContext::detach(Socket* socket)
{
    if (epoll_ctl(fd_, EPOLL_CTL_DEL, socket->fd_, nullptr) == -1) {
        perror("epoll_ctl: detach");
        exit(EXIT_FAILURE);
    }
    processedSockets.erase(socket);
}

void IOContext::cleanIO()
{
    delayDestructor.reserve(processedSockets.size());
    // stole processedSockets to avoid iterator invalidation
    for (auto* socket : std::move(processedSockets)) {
        socket->cancel();
    }
    delayDestructor.clear();
}
