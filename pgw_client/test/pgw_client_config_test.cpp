#include "pgw_client_config.h"

#include <gtest/gtest.h>

#include <fstream>
#include <filesystem>

class ClientConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::ofstream config("test_client_config.json");
        config << R"({
            "server_udp_ip": "192.168.1.100",
            "server_udp_port": 54321,
            "log_file": "client_test.log",
            "log_level": "INFO"
        })";
        config.close();
    }
    
    void TearDown() override {
        std::remove("test_client_config.json");
    }
};

TEST_F(ClientConfigTest, LoadValidConfig) {
    PGW::Config config("test_client_config.json");
    EXPECT_EQ(config.server_udp_ip, "192.168.1.100");
    EXPECT_EQ(config.server_udp_port, 54321);
    EXPECT_EQ(config.log_file, "client_test.log");
    EXPECT_EQ(config.log_level, quill::LogLevel::Info);
}

TEST_F(ClientConfigTest, InvalidIPAddress) {
    std::ofstream config("invalid_config.json");
    config << R"({"server_udp_ip": "invalid_ip"})";
    config.close();
    
    EXPECT_THROW({
        try {
            PGW::Config config("invalid_config.json");
        }
        catch(const std::invalid_argument& e) {
            EXPECT_STREQ(e.what(), "UDP_IP invalid");
            throw;
        }
    }, std::invalid_argument);
    
    std::remove("invalid_config.json");
}

TEST_F(ClientConfigTest, MissingConfigFile) {
    EXPECT_ANY_THROW(PGW::Config config("nonexistent_config.json"));
}

TEST_F(ClientConfigTest, InvalidLogLevel) {
    std::ofstream config("invalid_config.json");
    config << R"({"log_level": "INVALID_LEVEL"})";
    config.close();
    
    EXPECT_ANY_THROW(PGW::Config config("invalid_config.json"));
    
    std::remove("invalid_config.json");
}