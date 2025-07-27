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
TEST(IO_WorkerTest, ReceiveUDPPacket)
{
}

class IO_WorkerTest : public ::testing::Test
{
protected:
    static quill::Logger *main_logger;
    static std::unique_ptr<HTTP_Socket> http_socket;
    static std::unique_ptr<UDP_Socket> udp_socket;

    static IO_Worker *worker;
    static std::thread *worker_thread;

    static Queue<Packet> *udp_in_queue;
    static Queue<Packet> *udp_out_queue;
    static Queue<Packet> *http_in_queue;
    static Queue<Packet> *http_out_queue;

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

        uint32_t temp_ip;
        Socket::make_ip_address("127.0.0.1", temp_ip);
        http_socket = std::make_unique<HTTP_Socket>(temp_ip, 0);
        udp_socket = std::make_unique<UDP_Socket>(temp_ip, 0);

        stop.store(false);

        worker = new IO_Utils::IO_Worker(
            "0.0.0.0", 65500,
            "0.0.0.0", 65500,
            main_logger);

        worker_thread = new std::thread(
            &IO_Utils::IO_Worker::run, std::ref(*worker),
            std::ref(stop),
            std::ref(*http_in_queue), std::ref(*udp_in_queue),
            std::ref(*http_out_queue), std::ref(*udp_out_queue));

        std::this_thread::sleep_for(std::chrono::seconds(1));

        http_connection = std::make_unique<HTTP_Connection>(http_socket->connect_socket());
        udp_connection = std::make_unique<UDP_Connection>(udp_socket->listen_or_bind());
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

quill::Logger *main_logger = nullptr;
std::unique_ptr<HTTP_Socket> http_socket = nullptr;
std::unique_ptr<UDP_Socket> udp_socket = nullptr;

IO_Worker *worker = nullptr;
std::thread *worker_thread = nullptr;

Queue<Packet> *udp_in_queue = nullptr;
Queue<Packet> *udp_out_queue = nullptr;
Queue<Packet> *http_in_queue = nullptr;
Queue<Packet> *http_out_queue = nullptr;

std::unique_ptr<HTTP_Connection> http_connection = nullptr;
std::unique_ptr<UDP_Connection> udp_connection = nullptr;

std::atomic<bool> stop{false};

TEST_F(IO_WorkerTest, ReceiveUDPPackets)
{
    udp_packet->data = {0x00, 0x11, 0x22};
    udp_connection->send_packet(*udp_packet);

    size_t ctr = 0;
    std::unique_ptr<Packet> received_udp_packet;
    // Ожидание получения пакета (но не дольше лимита в 1 c)
    while (ctr < 100 && (received_udp_packet = udp_out_queue->pop()) == nullptr)
    {
        ctr++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_EQ(received_udp_packet->data, udp_packet->data);
}
/*
// Тест отправки HTTP ответов
TEST_F(IO_WorkerTest, SendsHTTPResponses)
{
    std::atomic<bool> stop(false);
    Queue<Packet> http_in_queue{10};
    Queue<Packet> udp_in_queue{10};
    Queue<Packet> http_out_queue{10};
    Queue<Packet> udp_out_queue{10};

    // Добавляем тестовый HTTP ответ в очередь
    auto http_packet = std::make_unique<HTTP_Packet>(http_socket);
    http_packet->data = {'r', 'e', 's', 'p'};
    http_out_queue.push(std::move(http_packet));

    // Создаем фейкового клиента
    worker->client_sockets[http_socket->accepted_fd] = http_socket;
    worker->connections[http_socket->accepted_fd] =
        std::make_unique<MockHTTPConnection>(http_socket->accepted_fd);

    // Запускаем воркер
    std::thread worker_thread([&]
                              { worker->run(stop, http_in_queue, udp_in_queue, http_out_queue, udp_out_queue); });

    // Даем время на обработку
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Проверяем результат
    auto mock_conn = static_cast<MockHTTPConnection *>(
        worker->connections[http_socket->accepted_fd].get());
    EXPECT_EQ(mock_conn->sent_data, std::vector<uint8_t>({'r', 'e', 's', 'p'}));

    stop.store(true);
    worker_thread.join();
}

// Тест обработки нового HTTP подключения
TEST_F(IO_WorkerTest, AcceptsNewHTTPConnections)
{
    std::atomic<bool> stop(false);
    Queue<Packet> http_in_queue{10};
    Queue<Packet> udp_in_queue{10};
    Queue<Packet> http_out_queue{10};
    Queue<Packet> udp_out_queue{10};

    // Запускаем воркер
    std::thread worker_thread([&]
                              { worker->run(stop, http_in_queue, udp_in_queue, http_out_queue, udp_out_queue); });

    // Имитируем событие нового подключения
    EXPECT_TRUE(registrar->registered_fds.count(http_socket->fd) > 0);

    // Проверяем что сокет клиента зарегистрирован
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(registrar->registered_fds.count(http_socket->accepted_fd) > 0);

    stop.store(true);
    worker_thread.join();
}*/