#ifndef PGW_SESSION_STORAGE
#define PGW_SESSION_STORAGE

#include "imsi.h"

#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <atomic>
#include <thread>

#include <quill/Logger.h>

namespace PGW
{
    struct Session
    {
        IMSI imsi;
        std::chrono::steady_clock::time_point last_activity;
    };

    class ISession_Storage
    {
    public:
        virtual bool _create(IMSI, Session) = 0;
        virtual bool _read(IMSI, Session &) = 0;
        virtual bool _update(IMSI, Session) = 0;
        virtual bool _delete(IMSI) = 0;

        virtual ~ISession_Storage() = default;
    };

    class CDR_Journal;

    class Session_Storage : public ISession_Storage
    {
        // Шард в данном случае как 'осколок' всего хранилища, чтобы
        // изолировать операции с ним от остального хранилища и не мешать вести их там параллельно
        // Такая схема здесь скорее всего будет излишней, но в случае масштабирования может подойти
        struct Shard
        {
            std::unordered_map<IMSI, Session> sessions;
            std::shared_mutex mutex;
        };

        static constexpr size_t amount_of_shards = 16;
        std::vector<Shard> shards{amount_of_shards};

        std::atomic<size_t> &session_timeout_in_seconds;
        std::atomic<size_t> &graceful_shutdown_rate;
        std::unique_ptr<std::ofstream> CDR_file;

        std::unordered_set<IMSI> blacklist;

        quill::Logger* logger;

        std::thread cleanup_thread;

        // Хэш-функция для определения номера шарда
        size_t get_shard_index(const IMSI &imsi) const;

        // Функция осуществляющая периодическую очистку хранилища сессий от устаревших записей.
        // Действует в отдельном потоке, создаваемом в конструкторе, итерация каждые 0.5 секунды
        void cleanup(std::atomic<bool> &stop);

        // Удаляет сессии со скоростью graceful_shutdown_rate сессий в секунду
        void delete_sessions_gracefully();

    public:
        CDR_Journal &cdr_log;
        Session_Storage(
            std::atomic<size_t> &session_timeout_in_seconds,
            std::atomic<size_t> &graceful_shutdown_rate,
            CDR_Journal &cdr_log,
            std::unordered_set<IMSI> blacklist,
            quill::Logger* logger,
            std::atomic<bool> &stop);

        // Перезапишет сессию даже если она существует
        // Но если использовать в связке с предварительным _read и _update в случае нахождения, все нормально
        bool _create(IMSI imsi, Session session) override;

        bool _read(IMSI imsi, Session &session) override;

        // На данный момент просто обновляет last_activity
        bool _update(IMSI imsi, Session session) override;

        bool _delete(IMSI imsi) override;

        ~Session_Storage();
    };
}

#endif // PGW_SESSION_STORAGE