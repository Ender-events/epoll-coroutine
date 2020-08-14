# C++ coroutine with epoll

Use C++20 coroutine to have async accept / recv / send operation

Exemple echo server with in the main.cc

## Purpose

Learning coroutine.

Inspired by:
* https://github.com/lewissbaker/cppcoro

## TODO / Improvement

* catch sigint + destroy coroutine
* move semantic on socket accept (RVO ?)
* handling error
* clean (remove logging, ...)

