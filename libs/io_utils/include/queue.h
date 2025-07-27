#ifndef IO_UTILS_QUEUE
#define IO_UTILS_QUEUE

#include <atomic>
#include <memory>

namespace IO_Utils{
    //Важно чтобы к очереди имели доступ только два потока
    template<typename T>
    class Queue{
        const size_t capacity;
        std::unique_ptr<std::unique_ptr<T>[]> buffer;

        alignas(64) std::atomic<size_t> head{0};
        alignas(64) std::atomic<size_t> tail{0};
    public:
        explicit Queue(size_t size) : 
            capacity(size),
            buffer(new std::unique_ptr<T>[size]) {}

        //Если очередь переполнена, новые элементы отбрасываются
        bool push(std::unique_ptr<T> elem) noexcept {
            static_assert(sizeof(T) > 0, "T должен быть полным типом");
            const size_t current_head = head.load(std::memory_order_acquire);
            //Поможет ли установить сюда acquire чтобы читать и писать очередь из более чем двух потоков?
            const size_t current_tail = tail.load(std::memory_order_relaxed);

            if (current_tail - current_head >= capacity) return false;

            buffer[current_tail % capacity] = std::move(elem);
            tail.store(current_tail + 1, std::memory_order_release);

            return true;
        }

        //Если очередь пуста, возвращается nullptr
        std::unique_ptr<T> pop() noexcept {
            static_assert(sizeof(T) > 0, "T должен быть полным типом");
            const size_t current_tail = tail.load(std::memory_order_acquire);
            //Поможет ли установить сюда acquire чтобы читать и писать очередь из более чем двух потоков?
            const size_t current_head = head.load(std::memory_order_relaxed);

            if (current_head == current_tail) return nullptr;

            auto elem = std::move(buffer[current_head % capacity]);
            head.store(current_head + 1, std::memory_order_release);

            return elem;
        }

        ~Queue() {
            static_assert(sizeof(T) > 0, "T должен быть полным типом");
        }

        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;
    };
}

#endif //IO_UTILS_QUEUE