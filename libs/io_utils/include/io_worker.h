#ifndef IO_UTILS_WORKER
#define IO_UTILS_WORKER

#include "registrar.h"
#include "network_io.h"
#include "queue.h"

#include <quill/Logger.h>

#include <unordered_map>

class IO_WorkerTest;

namespace IO_Utils
{
    class IO_Worker
    {
        std::shared_ptr<Socket> http_server, udp_server;
        int http_server_fd, udp_server_fd;
        quill::Logger *logger;

        std::unique_ptr<IRegistrar> registrar;
        std::unique_ptr<UDP_Connection> udp_server_connection;
        std::unordered_map<int, std::unique_ptr<Connection>> connections;
        std::unordered_map<int, std::shared_ptr<Socket>> client_sockets;

    public:
        IO_Worker(
            std::string udp_ip, uint16_t udp_port,
            std::string http_ip, uint16_t http_port,
            quill::Logger *logger);

        void run(
            std::atomic<bool> &stop,
            Queue<Packet> &http_in_queue, Queue<Packet> &udp_in_queue,
            Queue<Packet> &http_out_queue, Queue<Packet> &udp_out_queue);

        ~IO_Worker();
    };
}

#endif // IO_UTILS_WORKER