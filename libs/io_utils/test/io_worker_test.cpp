#include "io_worker.h"

#include "queue.h"
#include "registrar.h"
#include "network_io.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/FileSink.h>

#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <vector>

using namespace IO_Utils;

class IO_WorkerTest : public ::testing::Test
{
protected:
    static quill::Logger *main_logger;
    static std::shared_ptr<HTTP_Socket> http_socket;
    static std::shared_ptr<UDP_Socket> udp_socket;

    static IO_Worker *worker;
    static std::thread *worker_thread;

    static Queue<Packet> udp_in_queue;
    static Queue<Packet> udp_out_queue;
    static Queue<Packet> http_in_queue;
    static Queue<Packet> http_out_queue;

    static std::unique_ptr<HTTP_Connection> http_connection;
    static std::unique_ptr<UDP_Connection> udp_connection;

    static std::atomic<bool> stop;

    static void SetUpTestSuite()
    {
        quill::Backend::start();
        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "test_log/IO_Worker_test.log",
            []()
            {
                quill::FileSinkConfig cfg;
                cfg.set_open_mode('w');
                return cfg;
            }());
        main_logger = quill::Frontend::create_or_get_logger("root", std::move(file_sink));
        main_logger->set_log_level(quill::LogLevel::Debug);

        udp_in_queue = std::move(Queue<Packet>(10));
        udp_out_queue = std::move(Queue<Packet>(10));
        http_in_queue = std::move(Queue<Packet>(10));
        http_out_queue = std::move(Queue<Packet>(10));

        uint32_t temp_ip;
        Socket::make_ip_address("0.0.0.0", temp_ip);
        http_socket = std::make_shared<HTTP_Socket>(temp_ip, 65500);
        Socket::make_ip_address("127.0.0.1", temp_ip);
        udp_socket = std::make_shared<UDP_Socket>(temp_ip, 0);

        stop.store(false);

        worker = new IO_Utils::IO_Worker(
            "0.0.0.0", 65500,
            "0.0.0.0", 65500,
            main_logger);

        worker_thread = new std::thread(
            &IO_Utils::IO_Worker::run, std::ref(*worker),
            std::ref(stop),
            std::ref(http_in_queue), std::ref(udp_in_queue),
            std::ref(http_out_queue), std::ref(udp_out_queue));

        std::this_thread::sleep_for(std::chrono::seconds(1));

        int http_fd = http_socket->connect_socket();
        if (http_fd <= 0)
            std::cout << "Wrong http fd" << std::endl;
        int udp_fd = udp_socket->listen_or_bind();
        if (udp_fd <= 0)
            std::cout << "Wrong udp fd" << std::endl;
        http_connection = std::make_unique<HTTP_Connection>(http_fd);
        udp_connection = std::make_unique<UDP_Connection>(udp_fd);
    }

    void SetUp() override
    {
        uint32_t temp_ip;
        Socket::make_ip_address("0.0.0.0", temp_ip);
        std::shared_ptr<Socket> temp_socket = std::make_shared<UDP_Socket>(temp_ip, 65500);
        udp_packet = new UDP_Packet(temp_socket);

        temp_socket = std::make_shared<HTTP_Socket>(temp_ip, 65500);
        http_packet = new HTTP_Packet(temp_socket);
    }

    void TearDown() override
    {
        delete udp_packet, http_packet;
    }

    static void TearDownTestSuite()
    {
        stop.store(true);

        worker_thread->join();
        delete worker;

        main_logger->flush_log();
    }

    UDP_Packet *udp_packet;
    HTTP_Packet *http_packet;
};

quill::Logger *IO_WorkerTest::main_logger = nullptr;
std::shared_ptr<HTTP_Socket> IO_WorkerTest::http_socket = nullptr;
std::shared_ptr<UDP_Socket> IO_WorkerTest::udp_socket = nullptr;

IO_Worker *IO_WorkerTest::worker = nullptr;
std::thread *IO_WorkerTest::worker_thread = nullptr;

Queue<Packet> IO_WorkerTest::udp_in_queue(1);
Queue<Packet> IO_WorkerTest::udp_out_queue(1);
Queue<Packet> IO_WorkerTest::http_in_queue(1);
Queue<Packet> IO_WorkerTest::http_out_queue(1);

std::unique_ptr<HTTP_Connection> IO_WorkerTest::http_connection = nullptr;
std::unique_ptr<UDP_Connection> IO_WorkerTest::udp_connection = nullptr;

std::atomic<bool> IO_WorkerTest::stop{false};

TEST_F(IO_WorkerTest, ReceiveUDPPackets)
{
    udp_packet->data = {0x00, 0x11, 0x22};
    udp_connection->send_packet(*udp_packet);

    size_t ctr = 0;
    std::unique_ptr<Packet> received_udp_packet;
    // Ожидание получения пакета (но не дольше лимита в 3 c)
    while (ctr < 100 && (received_udp_packet = udp_in_queue.pop()) == nullptr)
    {
        ctr++;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    ASSERT_NE(received_udp_packet, nullptr);
    EXPECT_EQ(received_udp_packet->data, udp_packet->data);
}

TEST_F(IO_WorkerTest, ReceiveHTTPPackets)
{
    http_packet->data = {0x00, 0x11, 0x22};
    http_connection->send_packet(*http_packet);

    size_t ctr = 0;
    std::unique_ptr<Packet> received_http_packet;
    // Ожидание получения пакета (но не дольше лимита в 3 c)
    while (ctr < 100 && (received_http_packet = http_in_queue.pop()) == nullptr)
    {
        ctr++;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    ASSERT_NE(received_http_packet, nullptr);
    EXPECT_EQ(received_http_packet->data, http_packet->data);
}