#include <cctype>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <typeinfo>
#include <iomanip>

#include "pgw_config.h"
#include "cdr_journal.h"
#include "session_storage.h"
#include "handler.h"

#include <io_worker.h>
#include <network_io.h>
#include <queue.h>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/RotatingFileSink.h>

using namespace PGW;

void process(std::atomic<bool> &stop,
             IO_Utils::Queue<IO_Utils::Packet> &http_in_queue,
             IO_Utils::Queue<IO_Utils::Packet> &udp_in_queue,
             IO_Utils::Queue<IO_Utils::Packet> &http_out_queue,
             IO_Utils::Queue<IO_Utils::Packet> &udp_out_queue,
             const std::unordered_set<IMSI> blacklist,
             std::shared_ptr<ISession_Storage> session_storage,
             quill::Logger *logger)
{

    Handler handler{};
    UDP_Handler udp_handler{blacklist, session_storage, logger};
    HTTP_Handler http_handler{session_storage, stop, logger};

    bool res = false;
    // А этот цикл остановим сразу, чтобы не порождал еще ответы на запросы после /stop
    while (!stop.load())
    {
        std::unique_ptr<IO_Utils::Packet> packet = udp_in_queue.pop();

        if (packet != nullptr)
        {
            LOG_DEBUG(logger, "Received UDP packet\n{}", vec_to_str(packet->data));

            if (typeid(*packet.get()) == typeid(IO_Utils::UDP_Packet))
            {
                packet = udp_handler.handle_packet(std::move(packet));

                res = udp_out_queue.push(std::move(packet));
                if (!res)
                {
                    LOG_WARNING(logger, "The UDP out_queue is FULL");
                }
            }
            else
            {
                packet = handler.handle_packet(std::move(packet));

                res = udp_out_queue.push(std::move(packet));
                if (!res)
                {
                    LOG_DEBUG(logger, "The UDP out_queue is FULL");
                }
            }
        }

        packet = http_in_queue.pop();

        if (packet != nullptr)
        {
            if (typeid(*packet.get()) == typeid(IO_Utils::HTTP_Packet))
            {
                packet = http_handler.handle_packet(std::move(packet));

                res = http_out_queue.push(std::move(packet));
                if (!res)
                {
                    LOG_WARNING(logger, "The HTTP out_queue is FULL");
                }
            }
            else
            {
                packet = handler.handle_packet(std::move(packet));

                res = http_out_queue.push(std::move(packet));
                if (!res)
                {
                    LOG_DEBUG(logger, "The HTTP out_queue is FULL");
                }
            }
        }

        // Ограничение на скорость обработки пакетов
        auto start = clock();
        auto end = start;
        while (end - start < CLOCKS_PER_SEC / 1000000)
            end = clock();
    }
}

int main()
{
    std::unique_ptr<Config> server_config;
    try
    {
        server_config = std::make_unique<Config>("pgw_server_config.json");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        // Отключаю все потому, что программа еще не запустилась нормально и здесь это не так важно,
        // сначала нужно исправить конфигурацию
        // Хотя можно конечно использовать дефолтную, но зачем?
        return -1;
    }

    std::atomic<size_t> session_timeout_sec;
    session_timeout_sec.store(server_config->session_timeout_sec);
    std::atomic<size_t> gracefull_shutdown_rate;
    gracefull_shutdown_rate.store(server_config->gracefull_shutdown_rate);
    std::atomic<quill::LogLevel> log_level;
    log_level.store(server_config->log_level);

    quill::Backend::start();

    // Создается rotation file sink, чтобы создавать новые лог файлы при переполнении старых (1 МБ) или по истечении таймаута (1 час)
    auto rotating_file_sink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(
        server_config->log_file,
        []()
        {
            quill::RotatingFileSinkConfig cfg;
            cfg.set_open_mode('w');
            cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
            cfg.set_rotation_frequency_and_interval('H', 1);
            cfg.set_rotation_max_file_size(1024 * 1024);

            return cfg;
        }());

    quill::Logger *logger = quill::Frontend::create_or_get_logger(
        "root", std::move(rotating_file_sink),
        quill::PatternFormatterOptions{"%(time) [%(thread_id)] LOG_%(log_level:<9) "
                                       "%(message)",
                                       "%Y-%m-%d %H:%M:%S.%Qms", quill::Timezone::LocalTime});

    logger->set_log_level(server_config->log_level);

    // Это для того, чтобы потом использовать при смене уровня логирования
    // Уровни кроме указанных в ТЗ не используются, они просто не прочитаются из конфига
    const size_t log_level_strings_size = 11;
    const std::string log_level_strings[]{
        "TRACE_L3",
        "TRACE_L2",
        "TRACE_L1",
        "DEBUG",
        "INFO",
        "NOTICE",
        "WARNING",
        "ERROR",
        "CRITICAL",
        "BACKTRACE",
        "NONE"};

    std::unordered_set<IMSI> blacklist;
    for (const auto &imsi_str : server_config->blacklist)
    {
        IMSI temp_imsi;
        if (temp_imsi.set_IMSI_from_str(imsi_str))
        {
            blacklist.insert(temp_imsi);
        }
        else
        {
            // INFO потому, что пропущенные IMSI влияют на оказание сервиса этим абонентам (он им оказывается хотя не должен), но ничего серьезного не произошло
            LOG_INFO(logger, "Invalid IMSI in blacklist will be skipped: {}", imsi_str);
        }
    }

    // Кажется это называется Lock-Free SPSC Queue, момент в том, что пользоваться такой очередью должны только два потока, один читает, а второй - пишет
    IO_Utils::Queue<IO_Utils::Packet> http_in_queue{1000};
    IO_Utils::Queue<IO_Utils::Packet> udp_in_queue{10000};
    IO_Utils::Queue<IO_Utils::Packet> http_out_queue{1000};
    IO_Utils::Queue<IO_Utils::Packet> udp_out_queue{10000};

    std::atomic<bool> stop = false;

    IO_Utils::IO_Worker *io_worker;
    try
    {
        io_worker = new IO_Utils::IO_Worker(
            server_config->udp_ip, server_config->udp_port,
            server_config->http_ip, server_config->http_port,
            logger);
    }
    catch (const std::exception &e)
    {
        // Все выводы сообщений уже сделаны в IO_Worker конструкторе
        return -1;
    }

    std::thread io_worker_thread(
        &IO_Utils::IO_Worker::run, std::ref(io_worker),
        std::ref(stop),
        std::ref(http_in_queue), std::ref(udp_in_queue),
        std::ref(http_out_queue), std::ref(udp_out_queue));

    // Если журнал не создастся, выдаст запись в лог с уровнем INFO
    CDR_Journal cdr_log{server_config->cdr_file, server_config->cdr_file_max_lines, logger};

    std::shared_ptr<ISession_Storage> session_storage = std::make_shared<Session_Storage>(
        session_timeout_sec, gracefull_shutdown_rate,
        cdr_log, blacklist, logger, stop);

    std::thread process_thread(
        process,
        std::ref(stop),
        std::ref(http_in_queue), std::ref(udp_in_queue),
        std::ref(http_out_queue), std::ref(udp_out_queue),
        blacklist,
        std::ref(session_storage),
        logger);

    while (!stop.load())
    {
        try
        {
            if (server_config->try_reload())
            {
                LOG_DEBUG(logger, "Configuration change:\nSession_timeout = {}\nGracefull_shutdown_rate = {}\nLog_level = {}",
                         server_config->session_timeout_sec,
                         server_config->gracefull_shutdown_rate,
                         quill::detail::log_level_to_string(server_config->log_level, log_level_strings, log_level_strings_size));
                session_timeout_sec.store(server_config->session_timeout_sec);
                gracefull_shutdown_rate.store(server_config->gracefull_shutdown_rate);
                log_level.store(server_config->log_level);

                logger->flush_log();
                logger->set_log_level(server_config->log_level);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    process_thread.join();
    io_worker_thread.join();

    delete io_worker;

    return 0;
}