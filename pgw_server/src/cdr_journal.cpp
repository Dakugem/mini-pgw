#include "cdr_journal.h"

#include "imsi.h"

#include <quill/LogMacros.h>

namespace PGW
{
    CDR_Journal::CDR_Journal(std::string filename, size_t cdr_max_length_lines, quill::Logger *logger) : filename(filename), cdr_max_length_lines(cdr_max_length_lines), logger(logger)
    {
        create_file();
    };

    bool CDR_Journal::create_file()
    {
        if (file.is_open())
        {
            file << std::flush;
            file.close();
        }

        std::string current_filename;

        size_t index = filename.find_last_of(".");

        if (index != std::string::npos)
        {
            current_filename = filename.substr(0, index);
        }
        else
        {
            current_filename = filename;
        }

        std::time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm time_info;
        localtime_r(&timestamp, &time_info);

        current_filename += "_";
        current_filename += std::to_string(time_info.tm_year + 1900);
        current_filename += "-";
        current_filename += std::to_string(time_info.tm_mon);
        current_filename += "-";
        current_filename += std::to_string(time_info.tm_mday);
        current_filename += "_";
        current_filename += std::to_string(time_info.tm_hour);
        current_filename += ":";
        current_filename += std::to_string(time_info.tm_min);
        current_filename += ":";
        current_filename += std::to_string(time_info.tm_sec);

        if (index != std::string::npos)
        {
            current_filename += filename.substr(index, filename.size() - index);
        }
        else
        {
            current_filename += ".csv";
        }

        file.open(current_filename, std::ios::out);

        if (!file.is_open())
        {
            // Даже если не ведется CDR журнал сессии все равно можно создавать, хранить и удалять, функции выполняются потому INFO
            // Поставил бы ERROR но в примере с лекции все что выше INFO идет только в связи с потерей сервиса, а он вроде как предоставляется
            LOG_ERROR(logger, "Can't create CDR Journal with name {}", current_filename);
            return false;
        }

        LOG_DEBUG(logger, "Created CDR Journal with name {}", current_filename);

        return true;
    }

    void CDR_Journal::write(IMSI imsi, std::string action)
    {
        std::lock_guard<std::mutex> lock(m);

        if (ctr >= cdr_max_length_lines || !file.is_open())
        {
            ctr = 0;

            if (!create_file())
            {
                return;
            }
        }

        std::time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm time_info;
        localtime_r(&timestamp, &time_info);

        std::string str = "\"";

        str += std::to_string(time_info.tm_year + 1900);
        str += "-";
        str += std::to_string(time_info.tm_mon);
        str += "-";
        str += std::to_string(time_info.tm_mday);
        str += " ";
        str += std::to_string(time_info.tm_hour);
        str += ":";
        str += std::to_string(time_info.tm_min);
        str += ":";
        str += std::to_string(time_info.tm_sec);

        str += "\",\"";

        str += imsi.get_IMSI_to_str();

        str += "\",\"";

        str += action;

        str += "\"\r\n";

        file << str;

        if (ctr % 50 == 0 && ctr != 0)
        {
            file << std::flush;
        }

        ctr++;
    }

    bool CDR_Journal::is_open()
    {
        std::lock_guard<std::mutex> lock(m);

        return file.is_open();
    }

    CDR_Journal::~CDR_Journal()
    {
        if (file.is_open())
        {
            file << std::flush;
        }
    }
}