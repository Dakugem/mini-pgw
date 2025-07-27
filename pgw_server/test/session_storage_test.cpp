#include "session_storage.h"

#include "cdr_journal.h"

#include <gtest/gtest.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/FileSink.h>

#include <unordered_set>

class SessionStorageTest : public ::testing::Test
{
protected:
    static quill::Logger *main_logger;
    static PGW::CDR_Journal *main_cdr;

    static void SetUpTestSuite()
    {
        std::cerr << "Prepairing test environment..." << std::endl;

        quill::Backend::start();

        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "test_log/session_storage_test.log",
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
        main_cdr = new PGW::CDR_Journal("test_cdr/test_storage_cdr.csv", 100, main_logger);
    }

    void SetUp() override
    {
        std::atomic<size_t> timeout(30);
        std::atomic<size_t> rate(100);
        PGW::IMSI blacklisted_imsi;
        blacklisted_imsi.set_IMSI_from_str("0123456789");
        std::unordered_set<PGW::IMSI> blacklist{blacklisted_imsi};
        stop.store(false);

        storage = std::make_unique<PGW::Session_Storage>(
            timeout, rate, *main_cdr, blacklist, main_logger, stop);
    }

    void TearDown() override
    {
        stop.store(true);
        storage.release();
    }

    static void TearDownTestSuite()
    {
        std::cout << "Cleaning up..." << std::endl;

        SessionStorageTest::main_logger->flush_log();

        SessionStorageTest::main_logger = nullptr;

        delete SessionStorageTest::main_cdr;
    }

    std::atomic<bool> stop{false};
    std::unique_ptr<PGW::Session_Storage> storage;
};

quill::Logger *SessionStorageTest::main_logger = nullptr;
PGW::CDR_Journal *SessionStorageTest::main_cdr = nullptr;

TEST_F(SessionStorageTest, CreateSession)
{
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("123456789");
    PGW::Session session{imsi, std::chrono::steady_clock::now()};

    ASSERT_TRUE(storage->_create(imsi, session));

    imsi.set_IMSI_from_str("0123456789");
    session.imsi = imsi;
    session.last_activity = std::chrono::steady_clock::now();

    ASSERT_FALSE(storage->_create(imsi, session));
}

TEST_F(SessionStorageTest, UpdateSession)
{
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("123456789");
    PGW::Session session{imsi, std::chrono::steady_clock::now()};
    session.last_activity -= std::chrono::seconds(1);
    storage->_create(imsi, session);

    // Сессия должна обновится
    ASSERT_TRUE(storage->_update(imsi, session));
}

TEST_F(SessionStorageTest, SessionCleanup)
{
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("1234567890");

    // Новая сессия
    ASSERT_TRUE(storage->_create(imsi, {imsi, std::chrono::steady_clock::now()}));

    PGW::Session session;
    ASSERT_TRUE(storage->_read(imsi, session));

    // Создание устаревшей сессии
    session.last_activity -= std::chrono::seconds(60);

    // Чтобы его точно не было и он не обновлялся
    ASSERT_TRUE(storage->_delete(imsi));

    // Так в хранилище попадет сессия с устаревшей меткой
    storage->_create(imsi, session);

    bool session_removed = false;
    for (int i = 0; i < 30; ++i)
    {
        PGW::Session result;
        if (!storage->_read(imsi, result))
        {
            session_removed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_TRUE(session_removed);
}