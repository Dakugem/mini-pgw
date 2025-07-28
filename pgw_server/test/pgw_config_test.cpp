#include "pgw_config.h"

#include <gtest/gtest.h>

#include <fstream>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Tестовый конфиг
        std::ofstream config("test_config.json");
        config << R"({
            "udp_ip": "127.0.0.1",
            "udp_port": 65000,
            "http_ip": "192.168.1.1",
            "http_port": 8080,
            "session_timeout_sec": 30,
            "gracefull_shutdown_rate": 1000,
            "cdr_file": "cdr.csv",
            "cdr_file_max_lines": 1000,
            "log_file": "log.txt",
            "log_level": "DEBUG",
            "blacklist": ["12345", "67890"]
        })";
        config.close();
    }
    
    void TearDown() override {
        std::remove("test_config.json");
    }
};

TEST_F(ConfigTest, LoadConfig) {
    PGW::Config config("test_config.json");
    ASSERT_EQ(config.udp_ip, "127.0.0.1");
    ASSERT_EQ(config.http_port, 8080);
    ASSERT_EQ(config.session_timeout_sec, 30);
    ASSERT_EQ(config.blacklist.size(), 2);
}

TEST_F(ConfigTest, InvalidIPAddress) {
    std::ofstream config("invalid_config.json");
    config << R"({"udp_ip": "invalid_ip"})";
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

TEST_F(ConfigTest, MissingConfigFile) {
    EXPECT_ANY_THROW(PGW::Config config("nonexistent_config.json"));
}

TEST_F(ConfigTest, InvalidLogLevel) {
    std::ofstream config("invalid_config.json");
    config << R"({"log_level": "INVALID_LEVEL"})";
    config.close();
    
    EXPECT_ANY_THROW(PGW::Config config("invalid_config.json"));
    
    std::remove("invalid_config.json");
}

TEST_F(ConfigTest, HotReload) {
    PGW::Config config("test_config.json");

    // Поправка, чтобы слишком быстро работающий тест не ломался (если время создания и изменения файла одинаковое, то тест не сработает)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Модифицируем конфиг в процессе работы
    std::ofstream config_modified("test_config.json");
    config_modified << R"({
            "udp_ip": "127.0.0.1",
            "udp_port": 65000,
            "http_ip": "192.168.1.1",
            "http_port": 8080,
            "session_timeout_sec": 60,
            "gracefull_shutdown_rate": 100,
            "cdr_file": "cdr.csv",
            "cdr_file_max_lines": 1000,
            "log_file": "log.txt",
            "log_level": "INFO",
            "blacklist": ["12345", "67890"]
        })";
    config_modified.close();
    
    ASSERT_TRUE(config.try_reload());
    EXPECT_EQ(config.session_timeout_sec, 60);
    EXPECT_EQ(config.gracefull_shutdown_rate, 100);
    EXPECT_EQ(config.log_level, quill::LogLevel::Info);
}
