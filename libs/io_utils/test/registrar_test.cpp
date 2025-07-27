#include "registrar.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/epoll.h>

using namespace IO_Utils;

TEST(RegistrarTest, CreateSuccess)
{
    Registrar registrar;
    EXPECT_GE(registrar.get_epoll_fd(), 0);
}

TEST(RegistrarTest, RegisterDeregister)
{
    Registrar registrar;
    int pipe_fds[2];
    int res = pipe(pipe_fds);

    EXPECT_EQ(registrar.register_socket(pipe_fds[0], EPOLLIN), 0);
    EXPECT_EQ(registrar.deregister_socket(pipe_fds[0]), 0);

    res = close(pipe_fds[0]);
    res = close(pipe_fds[1]);
}

TEST(RegistrarTest, DoubleRegisterFails)
{
    Registrar registrar;
    int pipe_fds[2];
    int res = pipe(pipe_fds);

    EXPECT_EQ(registrar.register_socket(pipe_fds[0], EPOLLIN), 0);
    EXPECT_LT(registrar.register_socket(pipe_fds[0], 0), 0);

    registrar.deregister_socket(pipe_fds[0]);
    res = close(pipe_fds[0]);
    res = close(pipe_fds[1]);
}

TEST(RegistrarTest, RegisterInvalidSocket)
{
    Registrar registrar;
    ASSERT_LT(registrar.register_socket(-1, EPOLLIN), 0);
}

TEST(RegistrarTest, DeregisterInvalidSocket)
{
    Registrar registrar;
    ASSERT_LT(registrar.deregister_socket(-1), 0);
}

TEST(RegistrarTest, EpollWait)
{
    Registrar registrar;
    int pipe_fds[2];
    int res = pipe(pipe_fds);

    ASSERT_EQ(registrar.register_socket(pipe_fds[0], EPOLLIN), 0);

    write(pipe_fds[1], "test", 4);

    epoll_event events[1];
    int nfds = epoll_wait(registrar.get_epoll_fd(), events, 1, 100);
    EXPECT_EQ(nfds, 1);

    registrar.deregister_socket(pipe_fds[0]);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}