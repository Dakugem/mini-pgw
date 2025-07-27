#include "imsi.h"
#include "pgw_client_config.h"

#include <io_worker.h>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/RotatingFileSink.h>

#include <iostream>
#include <thread>
#include <unordered_map>
#include <sstream>
#include <iomanip>

using namespace PGW;

std::string vec_to_str(std::vector<uint8_t> data)
{
    std::string str = "";

    for (size_t i = 0; i < data.size(); ++i)
    {
        str += (char)data.at(i);
    }

    return str;
}

int main(int argc, char *argv[])
{
    std::unique_ptr<Config> client_config;
    try
    {
        client_config = std::make_unique<Config>("pgw_client_config.json");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        // Отключаю все потому, что программа еще не запустилась нормально и здесь это не так важно,
        // сначала нужно исправить конфигурацию
        // Хотя можно конечно использовать дефолтную, но зачем?
        return -1;
    }

    quill::Backend::start();

    // Создается rotation file sink, чтобы создавать новые лог файлы при переполнении старых (8 МБ) или по истечении таймаута (1 час)
    auto rotating_file_sink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(
        client_config->log_file,
        []()
        {
            quill::RotatingFileSinkConfig cfg;
            cfg.set_open_mode('w');
            cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
            cfg.set_rotation_frequency_and_interval('H', 1);
            cfg.set_rotation_max_file_size(1024 * 1024 * 8);

            return cfg;
        }());

    quill::Logger *logger = quill::Frontend::create_or_get_logger(
        "root", std::move(rotating_file_sink),
        quill::PatternFormatterOptions{"%(time) [%(thread_id)] LOG_%(log_level:<9) "
                                       "%(message)",
                                       "%Y-%m-%d %H:%M:%S.%Qms", quill::Timezone::LocalTime});

    logger->set_log_level(client_config->log_level);

    IMSI imsi;
    int imsi_amount = 1;
    for (int i = 0; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-M") && argc > i + 1)
        {
            i++;
            if (!imsi.set_IMSI_from_str(argv[i]))
            {
                LOG_ERROR(logger, "Wrong IMSI {} in arguments", argv[i]);
                return -1;
            }
        }
        if (!strcmp(argv[i], "-N") && argc > i + 1)
        {
            i++;
            int temp = std::stoi(argv[i]);

            if (temp > 0)
            {
                imsi_amount = temp;
            }
            else
            {
                LOG_INFO(logger, "Wrong Number of IMSI to generate, use N = 1");
            }
        }
    }

    if (imsi.get_IMSI_to_str().size() < 1)
    {
        LOG_ERROR(logger, "Zero IMSI");
        logger->flush_log();

        return -1;
    }

    LOG_DEBUG(logger, "Start with IMSI {}", imsi.get_IMSI_to_str());
    LOG_DEBUG(logger, "Number of IMSI to generate = {}", imsi_amount);

    IO_Utils::Queue<IO_Utils::Packet> http_in_queue{1};
    IO_Utils::Queue<IO_Utils::Packet> udp_in_queue{10000};
    IO_Utils::Queue<IO_Utils::Packet> http_out_queue{1};
    IO_Utils::Queue<IO_Utils::Packet> udp_out_queue{10000};

    std::atomic<bool> stop = false;

    IO_Utils::IO_Worker *io_worker;
    try
    {
        io_worker = new IO_Utils::IO_Worker(
            "0.0.0.0", 0,
            "0.0.0.0", 0,
            logger);
    }
    catch (const std::exception &e)
    {
        // Все выводы сообщений уже сделаны в IO_Worker конструкторе
        return -1;
    }

    LOG_DEBUG(logger, "IO_Worker object created");

    std::thread io_worker_thread(
        &IO_Utils::IO_Worker::run, std::ref(io_worker),
        std::ref(stop),
        std::ref(http_in_queue), std::ref(udp_in_queue),
        std::ref(http_out_queue), std::ref(udp_out_queue));

    LOG_DEBUG(logger, "IO_Worker thread started");

    // В этом векторе будут хранится все IMSI успешно созданные в цикле ниже
    std::vector<IMSI> imsis;

    std::stringstream ss;
    unsigned long long imsi_ull = std::stoull(imsi.get_IMSI_to_str());

    uint32_t server_ip = IO_Utils::Socket::make_ip_address(client_config->server_udp_ip, server_ip);
    std::shared_ptr<IO_Utils::UDP_Socket> server_socket = std::make_shared<IO_Utils::UDP_Socket>(server_ip, client_config->server_udp_port);

    for (size_t i = imsi_ull; i < imsi_ull + imsi_amount; ++i)
    {
        IMSI temp_imsi;
        ss << std::setw(imsi.get_IMSI_to_str().size()) << std::setfill('0') << std::to_string(i);
        if (!temp_imsi.set_IMSI_from_str(ss.str()))
        {
            LOG_WARNING(logger, "Can't create IMSI from {}", std::to_string(i));
        }
        else
        {
            LOG_DEBUG(logger, "Create IMSI {}", temp_imsi.get_IMSI_to_str());

            std::unique_ptr<IO_Utils::UDP_Packet> packet = std::make_unique<IO_Utils::UDP_Packet>(server_socket);
            packet->data = temp_imsi.get_IMSI_to_IE();

            if (udp_out_queue.push(std::move(packet)))
            {
                imsis.push_back(temp_imsi);

                LOG_INFO(logger, "Send IE with IMSI {}", temp_imsi.get_IMSI_to_str());
            }
            else
            {
                LOG_WARNING(logger, "Can't send IMSI IE, queue is FULL");
            }
        }

        ss.str("");
        ss.clear();
    }

    size_t amount_of_responses = 0;
    size_t amount_of_unexpected_responses = 0;
    while (amount_of_responses < imsis.size())
    {
        std::unique_ptr<IO_Utils::Packet> packet = udp_in_queue.pop();

        if (packet != nullptr)
        {
            std::cout << "For IMSI " << imsis[amount_of_responses].get_IMSI_to_str() << " response [" << amount_of_responses << "]: " << vec_to_str(packet->data) << std::endl;

            LOG_INFO(logger, "Receive for IMSI {} UDP packet [{}]\n{}", imsis[amount_of_responses].get_IMSI_to_str(), amount_of_responses, vec_to_str(packet->data));

            if (vec_to_str(packet->data) != "created" &&
                vec_to_str(packet->data) != "updated" &&
                // В данном случае ожидается именно blacklisted, а не error
                vec_to_str(packet->data) != "rejected, IMSI blacklisted or error creating session")
            {
                LOG_WARNING(logger, "Server send unexpected response");

                amount_of_unexpected_responses++;
            }

            amount_of_responses++;
        }
    }

    LOG_DEBUG(logger, "Amount of expected responses = {}\nAmount on all responses = {}\nTheir ratio = {:.2f}", 
        amount_of_responses - amount_of_unexpected_responses, amount_of_responses,
        (float)(amount_of_responses - amount_of_unexpected_responses) / amount_of_responses);

    stop.store(true);

    io_worker_thread.join();

    delete io_worker;

    return 0;
}