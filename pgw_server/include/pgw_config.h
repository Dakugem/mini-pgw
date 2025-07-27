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
        std::chrono::time_point<std::chrono::file_clock> last_write_time;

        void load_unreloadable();
        void load_reloadable();

    public:
        std::string udp_ip;
        uint16_t udp_port;
        std::string http_ip;
        uint16_t http_port;

        size_t session_timeout_sec;
        size_t gracefull_shutdown_rate;

        std::string cdr_file;
        size_t cdr_file_max_lines;

        std::string log_file;
        quill::LogLevel log_level;

        std::vector<std::string> blacklist;

        Config(const std::string &config_path);

        // Горячая смена перезагружаемой части конфигурации
        bool try_reload();
    };
}

#endif // PGW_CONFIG