#include "io_worker.h"

#include <quill/LogMacros.h>

#include <sys/epoll.h>
#include <stdexcept>
#include <cerrno>

namespace IO_Utils
{
    IO_Worker::IO_Worker(
        std::string udp_ip, uint16_t udp_port,
        std::string http_ip, uint16_t http_port,
        quill::Logger *logger) : logger(logger)
    {
        uint32_t _http_ip, _udp_ip;

        registrar = std::make_unique<Registrar>();

        if (registrar->get_epoll_fd() < 0)
        {
            LOG_ERROR(logger, "Registrar failure epoll_fd = {}", registrar->get_epoll_fd());
            throw std::runtime_error("Can't create registrar correctly");
        }

        int res = Socket::make_ip_address(http_ip, _http_ip);
        if (res == -1)
        {
            LOG_ERROR(logger, "HTTP ip wrong");
            throw std::invalid_argument("Wrong http ip");
        }

        res = Socket::make_ip_address(udp_ip, _udp_ip);
        if (res == -1)
        {
            LOG_ERROR(logger, "UDP ip wrong");
            throw std::invalid_argument("Wrong udp ip");
        }

        http_server = std::make_shared<HTTP_Socket>(_http_ip, http_port);
        udp_server = std::make_shared<UDP_Socket>(_udp_ip, udp_port);

        errno = 0;
        http_server_fd = http_server->listen_or_bind();
        if (http_server_fd <= 0)
        {
            LOG_ERROR(logger, "Failure while binding http server fd, fd = {}, errno = {}", http_server_fd, errno);
            throw std::runtime_error("Listen http server failure");
        }

        errno = 0;
        udp_server_fd = udp_server->listen_or_bind();
        if (udp_server_fd <= 0)
        {
            LOG_ERROR(logger, "Failure while binding udp server fd, fd = {}, errno = {}", udp_server_fd, errno);
            throw std::runtime_error("Bind udp server failure");
        }

        udp_server_connection = std::make_unique<UDP_Connection>(udp_server_fd);

        errno = 0;
        res = registrar->register_socket(http_server_fd, EPOLLIN);
        if (res < 0)
        {
            LOG_ERROR(logger, "HTTP server register wrong, epoll_fd = {}, server_fd = {}, errno = {}", registrar->get_epoll_fd(), http_server_fd, errno);
            throw std::runtime_error("Can't register http server");
        }

        errno = 0;
        res = registrar->register_socket(udp_server_fd, EPOLLIN | EPOLLOUT);
        if (res < 0)
        {
            LOG_ERROR(logger, "UDP server register wrong, epoll_fd = {}, server_fd = {}, errno = {}", registrar->get_epoll_fd(), udp_server_fd, errno);
            throw std::runtime_error("Can't register udp server");
        }
    }

    void IO_Worker::run(
        std::atomic<bool> &stop,
        Queue<Packet> &http_in_queue, Queue<Packet> &udp_in_queue,
        Queue<Packet> &http_out_queue, Queue<Packet> &udp_out_queue)
    {
        int res;
        epoll_event events[MAX_EVENTS];
        std::unique_ptr<Packet> http_packet_to_send = nullptr;
        size_t ctr = 0;
        while (ctr < 10)
        {
            // После поступления сигнала на остановку проитерируемся еще 10 раз, чтобы разослать оставшиеся пакеты или хотя бы их часть
            if (stop.load())
                ctr++;

            errno = 0;
            int nfds = epoll_wait(registrar->get_epoll_fd(), events, MAX_EVENTS, TIMEOUT);
            if (nfds < 0)
            {
                LOG_ERROR(logger, "Epoll_wait error, epoll_fd = {}, errno = {}", registrar->get_epoll_fd(), errno);
            }

            for (int i = 0; i < nfds; ++i)
            {
                int fd = events[i].data.fd;

                if (fd == http_server_fd && events[i].events & EPOLLIN)
                {
                    std::shared_ptr<HTTP_Socket> client_socket = std::make_shared<HTTP_Socket>();

                    errno = 0;
                    int client_fd = client_socket->accept_socket(fd);
                    if (client_fd < 0)
                    {
                        LOG_WARNING(logger, "Client accept wrong, epoll_fd = {}, client_fd = {}, server_fd = {}, errno = {}", registrar->get_epoll_fd(), client_fd, fd, errno);

                        continue;
                    }

                    errno = 0;
                    res = registrar->register_socket(client_fd, EPOLLIN | EPOLLOUT);
                    if (res == -1)
                    {
                        LOG_WARNING(logger, "Client register wrong, epoll_fd = {}, client_fd = {}, server_fd = {}, errno = {}", registrar->get_epoll_fd(), client_fd, fd, errno);

                        continue;
                    }

                    client_sockets[client_fd] = client_socket;
                    connections[client_fd] = std::make_unique<HTTP_Connection>(client_fd);
                }
                else if (fd == udp_server_fd)
                {
                    if (events[i].events & EPOLLIN)
                    {
                        UDP_Packet packet(nullptr);

                        errno = 0;
                        res = udp_server_connection->recv_packet(packet);
                        if (res < 0)
                        {
                            LOG_WARNING(logger, "Trouble with receiving UDP packets, server_fd = {}, errno = {}", udp_server_fd, errno);
                        }
                        else if (packet.data.size() == 0)
                        {
                        }
                        else
                        {
                            std::unique_ptr<UDP_Packet> temp_packet = std::make_unique<UDP_Packet>(packet);
                            res = udp_in_queue.push(std::move(temp_packet));
                            if (res == -1)
                            {
                                LOG_WARNING(logger, "UDP in_queue is FULL, drop the packet from {}", packet.get_socket()->socket_to_str());
                            }
                            else
                            {
                            }
                        }
                    }
                    if (events[i].events & EPOLLOUT)
                    {
                        std::unique_ptr<Packet> packet = udp_out_queue.pop();

                        if (packet != nullptr)
                        {
                            errno = 0;
                            res = udp_server_connection->send_packet(*packet.release());
                            if (res < 0)
                            {
                                LOG_WARNING(logger, "Trouble with sending UDP packets, server_fd = {}, errno = {}", udp_server_fd, errno);
                            }
                        }
                        else
                        {
                        }
                    }
                }
                else
                {
                    if (events[i].events & EPOLLIN)
                    {
                        if (!connections.contains(fd) || !client_sockets.contains(fd))
                        {
                            LOG_INFO(logger, "Unregistered correctly fd = {}", fd);

                            continue;
                        }

                        HTTP_Packet packet{nullptr};
                        packet.set_socket(client_sockets.at(fd));

                        errno = 0;
                        res = connections.at(fd)->recv_packet(packet);
                        if (res < 0)
                        {
                            LOG_WARNING(logger, "Trouble with receiving HTTP packet from {}, server_fd = {}, client_fd = {}, errno = {}", packet.get_socket()->socket_to_str(), http_server_fd, fd, errno);
                        }
                        else if (packet.data.size() == 0)
                        {
                        }
                        else
                        {
                            std::unique_ptr<Packet> temp_packet = std::make_unique<HTTP_Packet>(packet);
                            res = http_in_queue.push(std::move(temp_packet));
                            if (res == -1)
                            {
                                LOG_WARNING(logger, "HTTP in_queue is FULL, drop the packet from {}", packet.get_socket()->socket_to_str());
                            }
                            else
                            {
                            }
                        }
                    }
                    if (events[i].events & EPOLLOUT)
                    {
                        if (!connections.contains(fd) || !client_sockets.contains(fd))
                        {
                            LOG_INFO(logger, "Unregistered correctly fd = {}", fd);

                            continue;
                        }

                        if (http_packet_to_send == nullptr)
                            http_packet_to_send = http_out_queue.pop();

                        if (http_packet_to_send == nullptr)
                        {
                        }
                        else if (client_sockets.at(fd) == http_packet_to_send->get_socket())
                        {

                            LOG_DEBUG(logger, "Sending HTTP response to client {}", client_sockets.at(fd)->socket_to_str());

                            errno = 0;
                            res = connections.at(fd)->send_packet(*http_packet_to_send.release());
                            if (res < 0)
                            {
                                LOG_WARNING(logger, "Trouble with sending HTTP packets to {}, client_fd = {}, server_fd = {}, errno = {}", client_sockets.at(fd)->socket_to_str(), fd, udp_server_fd, errno);
                            }
                        }
                    }
                    if (events[i].events & EPOLLHUP || events[i].events & EPOLLRDHUP)
                    {
                        if (!client_sockets.contains(fd) || !connections.contains(fd))
                        {
                            LOG_INFO(logger, "Attempt to deregister socket with fd = {} that does not exist", fd);

                            continue;
                        }

                        LOG_DEBUG(logger, "Deregister socket {} with fd = {}", client_sockets.at(fd)->socket_to_str(), fd);

                        errno = 0;
                        registrar->deregister_socket(fd);
                        if (errno != 0)
                        {
                            LOG_INFO(logger, "Can't deregister socket {} with fd = {}", client_sockets.at(fd)->socket_to_str(), fd);

                            continue;
                        }

                        client_sockets.erase(fd);
                        connections.erase(fd);
                    }
                }
            }
        }

        for (auto pair : client_sockets)
        {
            errno = 0;
            registrar->deregister_socket(pair.first);
            if (errno != 0)
            {
                LOG_INFO(logger, "Can't deregister socket {} with fd = {}", pair.second->socket_to_str(), pair.first);
            }
        }
    }

    IO_Worker::~IO_Worker()
    {
        errno = 0;
        registrar->deregister_socket(udp_server_fd);
        if (errno != 0)
        {
            LOG_INFO(logger, "Can't deregister socket {} with fd = {}", udp_server->socket_to_str(), udp_server_fd);
        }

        errno = 0;
        registrar->deregister_socket(http_server_fd);
        if (errno != 0)
        {
            LOG_INFO(logger, "Can't deregister socket {} with fd = {}", http_server->socket_to_str(), http_server_fd);
        }
    }
}