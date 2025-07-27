#include "handler.h"

#include <quill/LogMacros.h>

#include <algorithm>

namespace PGW
{
    std::string vec_to_str(std::vector<uint8_t> data)
    {
        std::string str = "";

        for (size_t i = 0; i < data.size(); ++i)
        {
            str += (char)data.at(i);
        }

        return str;
    }

    std::vector<uint8_t> Handler::create_response(std::string message)
    {
        return {message.begin(), message.end()};
    }

    std::unique_ptr<IO_Utils::Packet> Handler::handle_packet(std::unique_ptr<IO_Utils::Packet> packet)
    {
        packet->data = create_response("Unknown packet type");
        return packet;
    }

    UDP_Handler::UDP_Handler(std::unordered_set<IMSI> blacklist, std::shared_ptr<ISession_Storage> session_storage, quill::Logger *logger) : blacklist(blacklist), session_storage(session_storage), logger(logger) {}

    std::unique_ptr<IO_Utils::Packet> UDP_Handler::handle_packet(std::unique_ptr<IO_Utils::Packet> packet)
    {
        IMSI imsi;

        if (!imsi.set_IMSI_from_IE(packet->data))
        {
            packet->data = create_response("rejected, not IMSI IE");

            LOG_DEBUG(logger, "Received message without IMSI IE\n{}", vec_to_str(packet->data));

            return packet;
        }

        Session session{imsi};
        if (session_storage->_read(imsi, session))
        {
            if (session_storage->_update(imsi, session))
            {
                packet->data = create_response("updated");
            }
            else
            {
                packet->data = create_response("rejected, the last update was too recent");
            }

            return packet;
        }

        auto current_time = std::chrono::steady_clock::now();
        session.last_activity = current_time;
        if (!session_storage->_create(imsi, session))
        {
            packet->data = create_response("rejected, IMSI blacklisted or error creating session");
        }
        else
        {
            packet->data = create_response("created");
        }

        return packet;
    }

    std::unique_ptr<IO_Utils::Packet> TCP_Handler::handle_packet(std::unique_ptr<IO_Utils::Packet> packet)
    {
        packet->data = create_response("TCP_request_response");

        return packet;
    }

    std::vector<uint8_t> HTTP_Handler::create_error_response(int status_code, const std::string &message)
    {
        std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + message + "\r\n";
        response += "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n";
        response += message;

        return {response.begin(), response.end()};
    }

    std::vector<uint8_t> HTTP_Handler::process_request(
        std::string method,
        std::string path,
        int http_version,
        phr_header *headers,
        size_t num_headers,
        const char *body,
        size_t body_size)
    {
        std::string content = "";
        std::string response = "HTTP/1.1 200 OK\r\n";

        LOG_DEBUG(logger, "Method = {}\nPath = {}\nHttp version = {}", method, path, http_version);

        if (path == "/check_subscriber")
        {
            response = "HTTP/1.1 400 Bad Request\r\n";
            std::string header_with_IMSI = "";
            if(num_headers > 0){
                LOG_DEBUG(logger, "Headers:");
            }
            for (size_t i = 0; i < num_headers; ++i)
            {
                static std::string imsi_header = "IMSI";
                std::string header{headers[i].name, headers[i].name_len};
                std::string value{headers[i].value, headers[i].value_len};

                LOG_DEBUG(logger, "{}: {}", header, value);

                if (std::equal(header.begin(), header.end(), imsi_header.begin(), imsi_header.end(),
                               [](char a, char b)
                               {
                                   return std::tolower(a) == std::tolower(b);
                               }))
                {
                    size_t index = value.find("\\");
                    if (index == std::string::npos)
                        index = headers[i].value_len;
                    header_with_IMSI = value.substr(0, index);
                }
            }

            IMSI imsi;
            Session session;
            if (header_with_IMSI != "" && imsi.set_IMSI_from_str(header_with_IMSI))
            {
                LOG_DEBUG(logger, "Check session existence for IMSI {}", imsi.get_IMSI_to_str());
                if (session_storage->_read(imsi, session))
                {
                    content = "active";
                }
                else
                {
                    content = "not active";
                }

                response = "HTTP/1.1 200 OK\r\n";
            }
        }
        else if (path == "/stop")
        {
            // По факту тут происходит не столько gracefull_offload, сколько отключение всего вообще
            stop.store(true);

            LOG_DEBUG(logger, "Start offload");

            content = "offload started";
        }

        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
        response += content;

        LOG_DEBUG(logger, "Response on that packet:\n{}", response);

        return {response.begin(), response.end()};
    }

    HTTP_Handler::HTTP_Handler(std::shared_ptr<ISession_Storage> session_storage, std::atomic<bool> &stop, quill::Logger *logger) : session_storage(session_storage), stop(stop), logger(logger) {}

    std::unique_ptr<IO_Utils::Packet> HTTP_Handler::handle_packet(std::unique_ptr<IO_Utils::Packet> packet)
    {
        LOG_DEBUG(logger, "Received HTTP packet from {}", packet->get_socket()->socket_to_str());

        // Слишком длинное сообщение
        if (packet->data.size() > MAX_HTTP_SIZE)
        {
            LOG_WARNING(logger, "The received HTTP packet was too long");
            packet->data = create_error_response(400, "Bad Request");
            return packet;
        }

        const char *method;
        size_t method_len;
        const char *path;
        size_t path_len;
        int minor_version;
        phr_header headers[16];
        size_t num_headers = 16;

        // Парсинг HTTP-запроса
        int parsed = phr_parse_request(
            (char *)packet->data.data(),
            packet->data.size(),
            &method, &method_len,
            &path, &path_len,
            &minor_version,
            headers, &num_headers,
            0);

        // Ответ на запрос который не получилось распарсить
        if (parsed <= 0)
        {
            LOG_WARNING(logger, "The received http packet could not be processed");

            packet->data = create_error_response(400, "Bad Request");
            return packet;
        }

        // Извлечение информации о запросе
        std::string method_str(method, method_len);
        std::string path_str(path, path_len);

        // Проверка наличия тела запроса
        // Но на данный момент это вообше не нужно
        bool has_body = false;
        size_t content_length = 0;
        for (size_t i = 0; i < num_headers; i++)
        {
            static std::string content_length_header = "Content-Length";
            std::string header(headers[i].name, headers[i].name_len);
            header += std::string(headers[i].value, headers[i].value_len);

            if (std::equal(header.begin(), header.end(), content_length_header.begin(), content_length_header.end(),
                           [](char a, char b)
                           {
                               return std::tolower(a) == std::tolower(b);
                           }))
            {
                content_length = std::strtoul(headers[i].value, nullptr, 10);
                has_body = true;

                break;
            }
        }

        if (has_body)
        {
            size_t body_start = parsed;
            size_t body_size = packet->data.size() - body_start;

            // Обработка тела сообщения если потребуется
        }

        // Обработка запроса и формирование ответа
        packet->data = process_request(
            method_str,
            path_str,
            minor_version,
            headers,
            num_headers,
            has_body ? (char *)packet->data.data() + parsed : nullptr,
            has_body ? content_length : 0);

        return packet;
    }
}