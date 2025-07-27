#ifndef PGW_IMSI
#define PGW_IMSI

#include <vector>
#include <string>
#include <cstdint>

namespace PGW
{
    class IMSI
    {
        std::string imsi;

        // Проверка соответствия UDP пакета формату IE содержащего IMSI
        bool check_IMSI_format(const std::vector<uint8_t> &data) const;

    public:
        bool set_IMSI_from_str(std::string imsi_str);

        bool set_IMSI_from_IE(std::vector<uint8_t> imsi_ie);

        std::string get_IMSI_to_str() const;

        std::vector<uint8_t> get_IMSI_to_IE() const;

        bool operator==(const IMSI &other) const;
    };
}

namespace std
{
    // Определение хеш-функции для класса IMSI, чтобы можно было использовать его как ключ для unordered_set и unordered_map
    template <>
    struct hash<PGW::IMSI>
    {
        size_t operator()(const PGW::IMSI &imsi) const noexcept;
    };
}

#endif // PGW_IMSI