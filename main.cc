#include "io_context.hh"
#include "socket.hh"

#include "lazy.hh"
#include <exception>

std::lazy<bool> inside_loop(Socket& socket)
{
    char buffer[42] = {0};
    ssize_t nbRecv = co_await socket.recv(buffer, sizeof(buffer));
    ssize_t nbSend = 0;
    while (nbSend < nbRecv) {
        ssize_t res = co_await socket.send(buffer, sizeof buffer);
        if (res <= 0)
            co_return false;
        nbSend += res;
    }
    std::cout << "DONE (" << nbRecv << "):" << '\n';
    if (nbRecv <= 0)
        co_return false;
    printf("%s\n", buffer);
    co_return true;
}

std::lazy<> echo_socket(std::shared_ptr<Socket> socket)
{
    bool run = true;
    while (run) {
        std::cout << "BEGIN\n";
        run = co_await inside_loop(*socket);
        std::cout << "END\n";
    }
}

std::lazy<> accept(Socket& listen)
{
    try {
        while (true) {
            auto task = listen.accept();
            std::cout << "co_await task\n";
            auto socket = co_await task;
            auto t = echo_socket(socket);
            listen.getContext().spawn(std::move(t));
        }
    } catch (const std::exception& e) {
        std::cout << "exception (accept): " << e.what() << '\n';
    }
}

int main()
{
    IOContext io_context{};
    Socket listen{"3490", io_context};
    auto t = accept(listen);
    // t.sync_await();
    io_context.spawn(std::move(t));

    io_context.run();
}
