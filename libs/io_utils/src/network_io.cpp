#include "network_io.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

namespace IO_Utils{
    bool Socket::operator==(const Socket& other){
        return this->ip == other.ip && this->port == other.port;
    }

    std::string Socket::socket_to_str(){
        
        char ipv4_str[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &ip, ipv4_str, INET_ADDRSTRLEN);

        std::string result{ipv4_str};
        result += ":";
        result += std::to_string(port);
        return result;
    }
    
    int Socket::make_ip_address(std::string IP, uint32_t& ip){
        if(inet_pton(AF_INET, IP.c_str(), &ip) != 1){
            return -1;
        }
        return 0;
    }

    static int set_nonblocking(int fd){
        int flags = fcntl(fd, F_GETFL, 0);
        if(flags == -1) return -1;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    int TCP_Socket::accept_socket(int listening_fd){
        sockaddr_in address;
        socklen_t addrlen = sizeof(address);
        int fd = accept(listening_fd, (sockaddr*)&address, &addrlen);
        if(fd == -1){
            close(fd);
            return -1;
        }

        if(set_nonblocking(fd) == -1){
            close(fd);
            return -2;
        }

        this->ip = address.sin_addr.s_addr;
        this->port = ntohs(address.sin_port);

        return fd;
    }

    int TCP_Socket::connect_socket(){
        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = this->ip;
        address.sin_port = htons(this->port);

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd == -1){
            close(fd);
            return -2;
        }

        
        if(connect(fd, (sockaddr*)&address, sizeof(address)) == -1){
            close(fd);
            return -3;
        }

        return fd;
    }

    int UDP_Connection::send_packet(const Packet& packet){
        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = packet.get_socket()->ip;
        address.sin_port = htons(packet.get_socket()->port);

        int send_bytes = sendto(fd, packet.data.data(), packet.data.size(), 0, (sockaddr*)&address, sizeof(address));

        if(send_bytes > 0){
            if((size_t)send_bytes < packet.data.size()){
                return -2;
            }
        }else{
            return -1;
        }

        return 0;
    }
    int UDP_Connection::recv_packet(Packet& packet){
        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        socklen_t addrlen = sizeof(address);

        char buffer[BUFF_SIZE];

        int recv_bytes = recvfrom(fd, buffer, BUFF_SIZE, 0, (sockaddr*)&address, &addrlen);

        if(recv_bytes >= 0){
            packet.data.clear();

            for(size_t i = 0; i < (size_t)recv_bytes; ++i){
                packet.data.push_back(buffer[i]);
            }

            UDP_Socket socket{address.sin_addr.s_addr, ntohs(address.sin_port)};
            packet.set_socket(std::make_shared<UDP_Socket>(socket));
        }else{
            return -1;
        }

        return 0;
    }

    int TCP_Connection::send_packet(const Packet& packet){
        int send_bytes = send(fd, packet.data.data(), packet.data.size(), 0);

        if(send_bytes > 0){
            if((size_t)send_bytes < packet.data.size()){
                return -2;
            }
        }else{
            return -1;
        }

        return 0;
    }    

    int TCP_Connection::recv_packet(Packet& packet){
        char buffer[BUFF_SIZE];

        int recv_bytes = recv(fd, buffer, BUFF_SIZE, 0);

        if(recv_bytes >= 0){
            packet.data.clear();

            for(size_t i = 0; i < (size_t)recv_bytes; ++i){
                packet.data.push_back(buffer[i]);
            }

            return 0;
        }else{
            return -1;
        }
    }

    int UDP_Socket::listen_or_bind(){
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(fd == -1){
            close(fd);
            return -1;
        }

        int reuse = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
            close(fd);
            return -2;
        }

        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = this->ip;
        address.sin_port = htons(this->port);

        if(bind(fd, (sockaddr*)&address, sizeof(address)) == -1){
            close(fd);
            return -3;
        }

        if(set_nonblocking(fd) == -1){
            close(fd);
            return -4;
        }
        
        return fd;
    }

    int TCP_Socket::listen_or_bind(){
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd == -1){
            close(fd);
            return -1;
        }

        int reuse = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
            close(fd);
            return -2;
        }

        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = this->ip;
        address.sin_port = htons(this->port);

        if(bind(fd, (sockaddr*)&address, sizeof(address)) == -1){
            close(fd);
            return -3;
        }

        if(set_nonblocking(fd) == -1){
            close(fd);
            return -4;
        }

        if(listen(fd, 5) == -1){
            close(fd);
            return -5;
        }

        return fd;
    }
}