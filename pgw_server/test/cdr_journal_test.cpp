#include "cdr_journal.h"

#include "imsi.h"

#include <gtest/gtest.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/FileSink.h>

#include <filesystem>

static quill::Logger *main_logger;
class CDRJournalTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        std::cerr << "Prepairing test environment..." << std::endl;

        quill::Backend::start();

        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "test_log/cdr_journal_test.log",
            []()
            {
                quill::FileSinkConfig cfg;
                cfg.set_open_mode('w');
                cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
                return cfg;
            }(),
            quill::FileEventNotifier{});

        main_logger = quill::Frontend::create_or_get_logger("root", std::move(file_sink));
        main_logger->set_log_level(quill::LogLevel::Debug);
    }

    void SetUp() override
    {
        logger = main_logger;
        journal = std::make_unique<PGW::CDR_Journal>("test_cdr/test_cdr.csv", 10, logger);
    }

    void TearDown() override
    {
    }

    static void TearDownTestSuite()
    {
        std::cout << "Cleaning up..." << std::endl;

        main_logger->flush_log();

        main_logger = nullptr;
    }

    quill::Logger *logger;
    std::unique_ptr<PGW::CDR_Journal> journal;
};

TEST_F(CDRJournalTest, FileCreation)
{
    ASSERT_TRUE(journal->is_open());
}

TEST_F(CDRJournalTest, WriteEntry)
{
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("123456789");
    journal->write(imsi, "created");

    ASSERT_TRUE(journal->is_open());
}

TEST_F(CDRJournalTest, FileRotation)
{
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("123456789");

    // Подсчитать количество уже существующих журналов + текущий
    int ctr = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator("."))
    {
        if (entry.path().string().find("test_cdr_") != std::string::npos)
        {
            ctr++;
        }
    }

    // Задержка нужна, чтобы имя нового файла отличалось от прошлого и он не перезаписался
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Записать больше лимита
    for (int i = 0; i < 15; i++)
    {
        journal->write(imsi, "entry " + std::to_string(i));
    }

    // Должен создаться новый файл
    for (const auto &entry : std::filesystem::recursive_directory_iterator("."))
    {
        if (entry.path().string().find("test_cdr_") != std::string::npos)
        {
            ctr--;
        }
    }

    ASSERT_LT(ctr, 0);
}