#include "pgw_client_config.h"

#include <stdexcept>
#include <fstream>
#include <unordered_map>
#include <arpa/inet.h>

namespace PGW
{
    bool check_ip_address(const std::string &ip) noexcept
    {
        uint32_t temp_ip;
        if (inet_pton(AF_INET, ip.c_str(), &temp_ip) != 1)
        {
            return false;
        }
        return true;
    }

    Config::Config(const std::string &config_path) : config_path(config_path)
    {
        std::ifstream config_file(config_path);

        if (!config_file.is_open())
            throw std::invalid_argument("Can't open config file");

        nlohmann::json json_config_temp;

        config_file >> json_config_temp;

        json_config = std::make_unique<nlohmann::json>(json_config_temp);

        load_unreloadable();
    }

    void Config::load_unreloadable()
    {

        std::string temp_server_udp_ip = json_config->at("server_udp_ip");
        if (!check_ip_address(temp_server_udp_ip))
            throw std::invalid_argument("UDP_IP invalid");

        uint16_t temp_server_udp_port = json_config->at("server_udp_port");

        std::string temp_log_file = json_config->at("log_file");

        std::string temp_log_level = json_config->at("log_level");
        static std::unordered_map<std::string, quill::LogLevel> log_levels{
            {"DEBUG", quill::LogLevel::Debug},
            {"INFO", quill::LogLevel::Info},
            {"WARNING", quill::LogLevel::Warning},
            {"ERROR", quill::LogLevel::Error},
            {"CRITICAL", quill::LogLevel::Critical}};

        if (!log_levels.contains(temp_log_level))
            throw std::invalid_argument("Wrong log level");


        server_udp_ip = temp_server_udp_ip;
        server_udp_port = temp_server_udp_port;
        log_file = temp_log_file;
        log_level = log_levels.at(temp_log_level);
    }
}