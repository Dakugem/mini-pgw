#ifndef IO_UTILS_REGISTRAR
#define IO_UTILS_REGISTRAR

#include <cstdint>

namespace IO_Utils
{
    constexpr int MAX_EVENTS = 32;
    constexpr int TIMEOUT = 1000;

    class IRegistrar
    {
    public:
        virtual ~IRegistrar() = default;

        virtual int register_socket(int fd, uint32_t events) = 0;
        virtual int deregister_socket(int fd) = 0;

        virtual int get_epoll_fd() = 0;
    };
    class Registrar : public IRegistrar
    {
        int epoll_fd;

    public:
        Registrar();
        ~Registrar();

        int register_socket(int fd, uint32_t events) override;
        int deregister_socket(int fd) override;

        int get_epoll_fd() override;

        Registrar(const Registrar &) = delete;
        Registrar &operator=(const Registrar &) = delete;
    };
}
#endif // IO_UTILS_REGISTRAR