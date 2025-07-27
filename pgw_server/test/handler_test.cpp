#include "handler.h"

#include "session_storage.h"

#include <network_io.h>

#include <gtest/gtest.h>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/FileSink.h>

class MockSessionStorage : public PGW::ISession_Storage
{
    std::unordered_map<PGW::IMSI, PGW::Session> sessions;

public:
    bool _create(PGW::IMSI imsi, PGW::Session session) override
    {
        if (sessions.contains(imsi))
        {
            return _update(imsi, session);
        }
        else
        {
            sessions[imsi] = session;
            return true;
        }
    }
    bool _read(PGW::IMSI imsi, PGW::Session &session) override {
        if(sessions.contains(imsi)) return true;

        return false;
    }
    bool _update(PGW::IMSI imsi, PGW::Session session) override {
        if (sessions.contains(imsi))
        {
            return true;
        }

        return false;
    }
    bool _delete(PGW::IMSI imsi) override {
        if(sessions.contains(imsi)){
            sessions.erase(imsi);

            return true;
        }

        return false;
    }
};

static quill::Logger *main_logger;

class HandlerTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        std::cerr << "Prepairing test environment..." << std::endl;

        quill::Backend::start();

        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "test_log/handler_test.log",
            []()
            {
                quill::FileSinkConfig cfg;
                cfg.set_open_mode('w');
                cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
                return cfg;
            }(),
            quill::FileEventNotifier{});

        main_logger = quill::Frontend::create_or_get_logger("root", std::move(file_sink));
        main_logger->set_log_level(quill::LogLevel::Debug);
    }

    void SetUp() override
    {
        logger = main_logger;
        storage = std::make_shared<MockSessionStorage>();

        http_socket = std::make_shared<IO_Utils::HTTP_Socket>(0, 0);
        udp_socket = std::make_shared<IO_Utils::UDP_Socket>(0, 0);
    }

    void TearDown() override
    {
    }

    static void TearDownTestSuite()
    {
        std::cout << "Cleaning up..." << std::endl;

        main_logger->flush_log();

        main_logger = nullptr;
    }

    quill::Logger *logger;
    std::shared_ptr<MockSessionStorage> storage;
    std::shared_ptr<IO_Utils::HTTP_Socket> http_socket;
    std::shared_ptr<IO_Utils::UDP_Socket> udp_socket;
};

TEST_F(HandlerTest, UDPHandlerValidPacket)
{
    PGW::UDP_Handler handler({}, storage, logger);
    auto packet = std::make_unique<IO_Utils::UDP_Packet>(udp_socket);

    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("123456789");
    packet->data = imsi.get_IMSI_to_IE();

    auto response = handler.handle_packet(std::move(packet));
    std::string res_str(response->data.begin(), response->data.end());
    ASSERT_EQ(res_str, "created");
}

TEST_F(HandlerTest, HTTPHandlerCheckSubscriber)
{
    std::atomic<bool> stop(false);
    PGW::HTTP_Handler handler(storage, stop, logger);
    auto packet = std::make_unique<IO_Utils::HTTP_Packet>(http_socket);

    std::string request =
        "GET /check_subscriber HTTP/1.1\r\n"
        "IMSI: 123456789\r\n"
        "\r\n";
    packet->data.assign(request.begin(), request.end());

    auto response = handler.handle_packet(std::move(packet));
    std::string res_str(response->data.begin(), response->data.end());
    ASSERT_NE(res_str.find("active"), std::string::npos);
}

TEST_F(HandlerTest, HTTPHandlerStop)
{
    std::atomic<bool> stop(false);
    PGW::HTTP_Handler handler(storage, stop, logger);
    auto packet = std::make_unique<IO_Utils::HTTP_Packet>(http_socket);

    std::string request =
        "GET /stop HTTP/1.1\r\n"
        "\r\n";
    packet->data.assign(request.begin(), request.end());

    auto response = handler.handle_packet(std::move(packet));
    std::string res_str(response->data.begin(), response->data.end());
    ASSERT_NE(res_str.find("offload started"), std::string::npos);
}