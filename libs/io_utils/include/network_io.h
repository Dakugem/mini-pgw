#ifndef IO_UTILS_NETWORK_IO
#define IO_UTILS_NETWORK_IO

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace IO_Utils{
    constexpr size_t BUFF_SIZE = 1024;

    class Packet;
    class Socket{
    public:
        uint32_t ip;
        uint16_t port;

        Socket() : ip(0), port(0){}
        Socket(uint32_t ip, uint16_t port) : ip(ip), port(port){}

        virtual int listen_or_bind() = 0; 

        virtual std::string socket_to_str();
        virtual bool operator==(const Socket& other);
        virtual ~Socket() = default;

        static int make_ip_address(std::string IP, uint32_t& ip);
    };

    class UDP_Socket : public Socket{
    public:
        UDP_Socket() : Socket(){}
        UDP_Socket(uint32_t ip, uint16_t port) : Socket(ip, port){}

        int listen_or_bind() override;
    };

    class TCP_Socket : public Socket{
    public:
        TCP_Socket() : Socket(){}
        TCP_Socket(uint32_t ip, uint16_t port) : Socket(ip, port){}

        int listen_or_bind() override;
        virtual int accept_socket(int listening_fd);
        virtual int connect_socket();
    };

    class HTTP_Socket : public TCP_Socket{
    public:
        HTTP_Socket() : TCP_Socket(){}
        HTTP_Socket(uint32_t ip, uint16_t port) : TCP_Socket(ip, port){}

        //Используем функции родительского класса TCP_Socket
        //int listen_or_bind() override {return TCP_Socket::listen_or_bind();}

        //int accept_socket(int listening_fd);
        //int connect_socket();
    };

    class Connection{
    public:
        Connection(int fd) : fd(fd){}

        virtual int send_packet(const Packet& packet) = 0;
        virtual int recv_packet(Packet& packet) = 0;

        int fd;
    };

    class UDP_Connection : public Connection{
    public:
        UDP_Connection(int fd) : Connection::Connection(fd){}

        int send_packet(const Packet& packet) override;
        int recv_packet(Packet& packet) override;
    };

    class TCP_Connection : public Connection{
    public:
        TCP_Connection(int fd) : Connection::Connection(fd){}

        int send_packet(const Packet& packet) override;
        int recv_packet(Packet& packet) override;
    };

    class HTTP_Connection : public TCP_Connection{
    public:
        HTTP_Connection(int fd) : TCP_Connection::TCP_Connection(fd){}

        int send_packet(const Packet& packet) override{
            return TCP_Connection::send_packet(packet);
        }
        int recv_packet(Packet& packet) override{
            return TCP_Connection::recv_packet(packet);
        }
    };

    class Packet{
    protected:
        std::shared_ptr<Socket> socket;
    public:
        Packet(std::shared_ptr<Socket> socket) : socket(socket){}

        virtual std::shared_ptr<Socket> get_socket() const { return socket; }
        virtual void set_socket(std::shared_ptr<Socket> socket) {this->socket = socket;}

        std::vector<uint8_t> data;
    };

    class UDP_Packet : public Packet{
    public:
        UDP_Packet(std::shared_ptr<Socket> socket) : Packet::Packet(socket){}
    };

    class TCP_Packet : public Packet{
    public:
        TCP_Packet(std::shared_ptr<Socket> socket) : Packet::Packet(socket){}
    };

    class HTTP_Packet : public TCP_Packet{
    public:
        HTTP_Packet(std::shared_ptr<Socket> socket) : TCP_Packet::TCP_Packet(socket){}
    };
}

#endif //IO_UTILS_NETWORK_IO