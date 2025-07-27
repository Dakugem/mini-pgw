#ifndef PGW_CONFIG
#define PGW_CONFIG

#include <nlohmann/json.hpp>
#include <quill/core/LogLevel.h>

namespace PGW
{
    class Config
    {
        std::string config_path;
        std::unique_ptr<nlohmann::json> json_config;

        void load_unreloadable();

    public:
        std::string server_udp_ip;
        uint16_t server_udp_port;

        std::string log_file;
        quill::LogLevel log_level;

        Config(const std::string &config_path);
    };
}

#endif // PGW_CONFIG