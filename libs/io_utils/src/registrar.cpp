#include "registrar.h"

#include <unistd.h>
#include <sys/epoll.h>

namespace IO_Utils
{

    Registrar::Registrar()
    {
        epoll_fd = epoll_create1(0);
    }

    Registrar::~Registrar()
    {
        if (epoll_fd >= 0)
        {
            close(epoll_fd);
        }
    }

    int Registrar::register_socket(int fd, uint32_t events)
    {
        epoll_event _events;
        _events.events = events;
        _events.data.fd = fd;

        int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &_events);
        return res;
    }

    int Registrar::deregister_socket(int fd)
    {
        if (epoll_fd < 0) return -1;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1)
        {
            if (close(fd) == -1)
            {
                return -2;
            }
            return -3;
        }
        if (close(fd) == -1)
        {
            return -4;
        }
        return 0;
    }

    int Registrar::get_epoll_fd()
    {
        return epoll_fd;
    };
}