#include "pgw_config.h"

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
        std::filesystem::path temp_config_path = config_path;
        last_write_time = std::filesystem::last_write_time(temp_config_path);

        std::ifstream config_file(config_path);

        if (!config_file.is_open())
            throw std::invalid_argument("Can't open config file");

        nlohmann::json json_config_temp;

        config_file >> json_config_temp;

        json_config = std::make_unique<nlohmann::json>(json_config_temp);

        load_unreloadable();
        load_reloadable();
    }

    void Config::load_unreloadable()
    {

        std::string temp_udp_ip = json_config->at("udp_ip");
        if (!check_ip_address(temp_udp_ip))
            throw std::invalid_argument("UDP_IP invalid");

        uint16_t temp_udp_port = json_config->at("udp_port");

        std::string temp_http_ip = json_config->at("http_ip");
        if (!check_ip_address(temp_http_ip))
            throw std::invalid_argument("HTTP_IP invalid");

        uint16_t temp_http_port = json_config->at("http_port");

        std::string temp_cdr_file = json_config->at("cdr_file");
        size_t temp_cdr_file_max_lines = json_config->at("cdr_file_max_lines");
        if (temp_cdr_file_max_lines < 1000)
            throw std::invalid_argument("CDR journal too short (min 1000 lines)");

        std::string temp_log_file = json_config->at("log_file");

        std::vector<std::string> temp_blacklist = json_config->at("blacklist").get<std::vector<std::string>>();

        // Это для того, чтобы в случае проблем при чтении конфигурации они не повлияли на существующую конфигурацию
        // Актуально для функции load_reloadable вызываемой try_reload
        udp_ip = temp_udp_ip;
        udp_port = temp_udp_port;
        http_ip = temp_http_ip;
        http_port = temp_http_port;
        cdr_file = temp_cdr_file;
        cdr_file_max_lines = temp_cdr_file_max_lines;
        log_file = temp_log_file;
        blacklist = temp_blacklist;
    }

    void Config::load_reloadable()
    {
        size_t temp_session_timeout_sec = json_config->at("session_timeout_sec");
        if (temp_session_timeout_sec == 0)
            throw std::invalid_argument("Zero session timeout");
        if (temp_session_timeout_sec > 24 * 60 * 60)
            throw std::invalid_argument("Session timeout too long (max 1 day)");

        size_t temp_gracefull_shutdown_rate = json_config->at("gracefull_shutdown_rate");
        if (temp_gracefull_shutdown_rate == 0)
            throw std::invalid_argument("Zero shutdown rate");

        std::string temp_log_level = json_config->at("log_level");
        static std::unordered_map<std::string, quill::LogLevel> log_levels{
            {"DEBUG", quill::LogLevel::Debug},
            {"INFO", quill::LogLevel::Info},
            {"WARNING", quill::LogLevel::Warning},
            {"ERROR", quill::LogLevel::Error},
            {"CRITICAL", quill::LogLevel::Critical}};

        if (!log_levels.contains(temp_log_level))
            throw std::invalid_argument("Wrong log level");

        // Это для того, чтобы в случае проблем при чтении конфигурации, они не повлияли на существующую конфигурацию
        // Актуально при вызове этой функции через try_reload
        session_timeout_sec = temp_session_timeout_sec;
        gracefull_shutdown_rate = temp_gracefull_shutdown_rate;
        log_level = log_levels.at(temp_log_level);
    }

    bool Config::try_reload()
    {
        std::filesystem::path temp_config_path = config_path;
        auto current_last_write_time = std::filesystem::last_write_time(temp_config_path);

        if (current_last_write_time != last_write_time)
        {
            std::ifstream config_file(config_path);

            if (!config_file.is_open())
                throw std::invalid_argument("Can't open config file");

            nlohmann::json json_config_temp;

            config_file >> json_config_temp;

            json_config = std::make_unique<nlohmann::json>(json_config_temp);

            size_t temp_session_timeout_sec = session_timeout_sec;
            size_t temp_gracefull_shutdown_rate = gracefull_shutdown_rate;
            quill::LogLevel temp_log_level = log_level;

            load_reloadable();

            if (temp_session_timeout_sec != session_timeout_sec ||
                temp_gracefull_shutdown_rate != gracefull_shutdown_rate ||
                temp_log_level != log_level
            )
            {
                last_write_time = current_last_write_time;

                return true;
            }
        }

        return false;
    }
}