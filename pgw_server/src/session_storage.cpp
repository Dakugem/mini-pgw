#include "session_storage.h"

#include "cdr_journal.h"

#include <quill/LogMacros.h>

namespace PGW
{
    size_t Session_Storage::get_shard_index(const IMSI &imsi) const
    {
        return std::hash<IMSI>{}(imsi) % amount_of_shards;
    }

    void Session_Storage::cleanup(std::atomic<bool> &stop)
    {
        LOG_DEBUG(logger, "Session storage cleanup thread started");

        while (!stop.load())
        {
            std::chrono::seconds timeout{session_timeout_in_seconds.load()};

            for (auto &shard : shards)
            {               
                std::unique_lock lock(shard.mutex);

                auto current_time = std::chrono::steady_clock::now();

                auto it = shard.sessions.begin();
                while (it != shard.sessions.end())
                {
                    if (current_time - it->second.last_activity >= timeout)
                    {
                        LOG_DEBUG(logger, "Session with IMSI {} deleted on timeout", it->first.get_IMSI_to_str());
                        cdr_log.write(it->first, "delete_session_on_timeout");
                        it = shard.sessions.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        LOG_DEBUG(logger, "Session storage cleanup thread stopped");
    }

    void Session_Storage::delete_sessions_gracefully()
    {
        LOG_DEBUG(logger, "Session storage gracefull offload started");

        for (size_t i = 0; i < amount_of_shards; ++i)
        {
            Shard &shard = shards[i];
            std::unique_lock lock(shard.mutex);

            auto current_time = std::chrono::steady_clock::now();

            auto it = shard.sessions.begin();
            while (it != shard.sessions.end())
            {
                LOG_DEBUG(logger, "Session with IMSI {} deleted on offload", it->first.get_IMSI_to_str());
                cdr_log.write(it->first, "delete_session_on_offload");
                it = shard.sessions.erase(it);

                if (it != shard.sessions.end())
                {
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / graceful_shutdown_rate.load()));
                    lock.lock();
                }
            }
        }

        LOG_DEBUG(logger, "Session storage gracefull offload end");
    }

    Session_Storage::Session_Storage(
        std::atomic<size_t> &session_timeout_in_seconds,
        std::atomic<size_t> &graceful_shutdown_rate,
        CDR_Journal &cdr_log,
        std::unordered_set<IMSI> blacklist,
        quill::Logger* logger,
        std::atomic<bool> &stop) : session_timeout_in_seconds(session_timeout_in_seconds),
                                   graceful_shutdown_rate(graceful_shutdown_rate),
                                   cdr_log(cdr_log),
                                   blacklist(blacklist),
                                   logger(logger)
    {
        LOG_DEBUG(logger, "Session storage created");
        cleanup_thread = std::thread{&Session_Storage::cleanup, this, std::ref(stop)};
    }

    bool Session_Storage::_create(IMSI imsi, Session session)
    {
        if (blacklist.contains(imsi))
        {
            // Чтобы как-то ограничить число таких записей в CDR журнал
            static IMSI last_received_imsi_from_blacklist;
            if (last_received_imsi_from_blacklist != imsi)
            {
                cdr_log.write(imsi, "rejected, IMSI blacklisted");

                LOG_DEBUG(logger, "Create session rejected: IMSI {} blacklisted", imsi.get_IMSI_to_str());

                last_received_imsi_from_blacklist = imsi;
            }

            return false;
        }

        Shard &shard = shards[get_shard_index(imsi)];

        // На момент записи шард блокируется для остальных операций
        std::unique_lock lock(shard.mutex);

        if(shard.sessions.contains(imsi)){
            return _update(imsi, session);
        }

        shard.sessions[imsi] = session;

        if (!shard.sessions.contains(imsi))
        {
            LOG_ERROR(logger, "Create session rejected on error in session_storage for IMSI {}", imsi.get_IMSI_to_str());
            cdr_log.write(imsi, "rejected, error while session creating");

            return false;
        }

        LOG_DEBUG(logger, "Create session success for IMSI {}", imsi.get_IMSI_to_str());
        cdr_log.write(imsi, "created");

        return true;
    }

    bool Session_Storage::_read(IMSI imsi, Session &session)
    {
        Shard &shard = shards[get_shard_index(imsi)];

        // Другим потокам позволяется читать паралельно с этим в этом же шарде
        std::shared_lock lock(shard.mutex);

        if (shard.sessions.contains(imsi))
        {
            session = Session(shard.sessions.at(imsi));

            LOG_DEBUG(logger, "Find session for IMSI {} success", imsi.get_IMSI_to_str());

            return true;
        }

        //Сомнительный механизм, чтобы не засорять логи уровня DEBUG повторяющимися сообщениями на поиск того-же IMSI
        static IMSI last_received_not_founded_imsi;
        if(last_received_not_founded_imsi != imsi){
            LOG_DEBUG(logger, "Can't find session for IMSI {} ", imsi.get_IMSI_to_str());
            last_received_not_founded_imsi = imsi;
        }

        return false;
    }

    bool Session_Storage::_update(IMSI imsi, Session session)
    {
        Shard &shard = shards[get_shard_index(imsi)];

        // На момент записи шард блокируется для остальных операций
        std::unique_lock lock(shard.mutex);

        if (shard.sessions.contains(imsi))
        {
            auto current_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> duration = current_time - shard.sessions.at(imsi).last_activity;

            // Сессию нельзя обновлять чаще чем раз в 0.5 секунды
            if (duration.count() >= 0.5)
            {
                shard.sessions[imsi].last_activity = current_time;
                cdr_log.write(imsi, "updated");

                LOG_DEBUG(logger, "Successfull update for IMSI {}", imsi.get_IMSI_to_str());

                return true;
            }

            return false;
        }

        LOG_DEBUG(logger, "Attempt to update session for IMSI {} which not exist", imsi.get_IMSI_to_str());

        return false;
    }

    bool Session_Storage::_delete(IMSI imsi)
    {
        Shard &shard = shards[get_shard_index(imsi)];

        // На момент удаления шард блокируется для остальных операций
        std::unique_lock lock(shard.mutex);

        LOG_DEBUG(logger, "Attempt to delete session for IMSI {}", imsi.get_IMSI_to_str());

        cdr_log.write(imsi, "delete_session_manually");

        return shard.sessions.erase(imsi) > 0;
    }

    Session_Storage::~Session_Storage()
    {
        cleanup_thread.join();

        delete_sessions_gracefully();
    }
}