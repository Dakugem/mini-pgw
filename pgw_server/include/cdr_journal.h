#ifndef PGW_CDR_JOURNAL
#define PGW_CDR_JOURNAL

#include <fstream>
#include <mutex>

#include <quill/Logger.h>

namespace PGW
{
    class IMSI;
    
    class CDR_Journal
    {
        std::string filename;
        std::ofstream file;
        std::mutex m;

        quill::Logger* logger;

        // Счетчик чтобы раз в некоторое число строк сбрасывать буфер в файл, конкретно каждые 50 строк
        size_t ctr = 0;
        // Максимальный размер CDR журнала в строках, после исчерпания создается новый файл
        size_t cdr_max_length_lines;

        //Создает CDR журнал с указанным именем и временной меткой добавленной к в нему
        bool create_file();
    public:
        CDR_Journal(const std::string filename, size_t cdr_max_length_lines, quill::Logger* logger);

        //Записи в журнале определенного формата Timestamp, IMSI, Action
        //Timestamp определяется в момент записи самой строки в журнал
        virtual void write(IMSI imsi, std::string action);

        bool is_open();

        ~CDR_Journal();

        CDR_Journal(const CDR_Journal& other) = delete;
        CDR_Journal& operator=(const CDR_Journal& other) = delete;
    };
}

#endif // PGW_CDR_JOURNAL