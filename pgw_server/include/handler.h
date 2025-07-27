#ifndef PGW_HANDLER
#define PGW_HANDLER

#include "imsi.h"
#include "session_storage.h"

#include <network_io.h>

#include <picohttpparser.h>
#include <quill/Logger.h>

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_set>

namespace PGW
{
    std::string vec_to_str(std::vector<uint8_t> data);

    class Handler
    {
    protected:
        virtual std::vector<uint8_t> create_response(std::string message);

    public:
        virtual ~Handler() = default;

        virtual std::unique_ptr<IO_Utils::Packet> handle_packet(std::unique_ptr<IO_Utils::Packet> packet);
    };

    class UDP_Handler : public Handler
    {
        quill::Logger* logger;
        std::unordered_set<IMSI> blacklist;
        std::shared_ptr<ISession_Storage> session_storage;

    public:
        UDP_Handler(std::unordered_set<IMSI> blacklist, std::shared_ptr<ISession_Storage> session_storage, quill::Logger* logger);

        std::unique_ptr<IO_Utils::Packet> handle_packet(std::unique_ptr<IO_Utils::Packet> packet) override;
    };
    class TCP_Handler : public Handler
    {
    public:
        virtual std::unique_ptr<IO_Utils::Packet> handle_packet(std::unique_ptr<IO_Utils::Packet> packet) override;
    };

    class HTTP_Handler : public TCP_Handler
    {
        std::vector<uint8_t> create_error_response(int status_code, const std::string &message);

        std::vector<uint8_t> process_request(
            std::string method,
            std::string path,
            int http_version,
            phr_header *headers,
            size_t num_headers,
            const char *body,
            size_t body_size);

        std::shared_ptr<ISession_Storage> session_storage;
        std::atomic<bool> &stop;
        quill::Logger* logger;

    public:
        static constexpr size_t MAX_HTTP_SIZE = 8192;

        HTTP_Handler(std::shared_ptr<ISession_Storage> session_storage, std::atomic<bool> &stop, quill::Logger* logger);

        std::unique_ptr<IO_Utils::Packet> handle_packet(std::unique_ptr<IO_Utils::Packet> packet) override;
    };
}

#endif // PGW_HANDLER