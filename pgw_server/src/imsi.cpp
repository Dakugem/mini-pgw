#include "imsi.h"

namespace PGW
{
    bool IMSI::check_IMSI_format(const std::vector<uint8_t> &data) const
    {
        // Длина обязательных полей IE
        if (data.size() < 4)
            return false;

        // Тип IE с IMSI -> Type = 1
        if (data.at(0) != 1)
            return false;

        // Проверка соответствия размера полезных данных тому размеру, что указан в обязательном поле Length
        size_t length = data.at(1) << 8 | data.at(2);
        if (length != data.size() - 4)
            return false;

        // Поле Spare игнорируется получателем, поэтому проверятся не будет
        // Поле Instance тоже игнорируется, так как в данном случае он не имеет смысла

        // Все цифры IMSI должны быть не больше 0b1001
        // Последняя цифра в IE может быть филлером и иметь значение 0b1111, если длина IMSI не кратна двум
        for (size_t i = 0; i < length * 2; ++i)
        {
            uint8_t number = (data.at(i / 2 + 4) & (0xF << ((i % 2) * 4))) >> ((i % 2) * 4);

            if (i == length * 2 - 1)
            {
                if (number > 0b1001 && number != 0b1111)
                    return false;
            }
            else
            {
                if (number > 0b1001)
                    return false;
            }
        }

        return true;
    }

    bool IMSI::set_IMSI_from_str(std::string imsi_str)
    {
        if (imsi_str.size() > 15 || imsi_str.size() < 1)
            return false;

        for (char c : imsi_str)
            if (!std::isdigit(c))
                return false;

        imsi = imsi_str;
        return true;
    }

    bool IMSI::set_IMSI_from_IE(std::vector<uint8_t> imsi_ie)
    {
        if (!check_IMSI_format(imsi_ie))
            return false;

        std::string imsi_str = "";
        size_t length = imsi_ie.at(1) << 8 | imsi_ie.at(2);

        for (size_t i = 0; i < length * 2; ++i)
        {
            uint8_t number = (imsi_ie[i / 2 + 4] >> ((i % 2) * 4)) & 0xF;

            if (number == 0xF && i == length * 2 - 1)
                break;

            imsi_str += ('0' + number);
        }

        return set_IMSI_from_str(imsi_str);
    }

    std::string IMSI::get_IMSI_to_str() const
    {
        return imsi;
    }

    std::vector<uint8_t> IMSI::get_IMSI_to_IE() const
    {
        std::vector<uint8_t> imsi_ie;
        // Type = 1
        imsi_ie.push_back(0x01);

        // Length определяется по IMSI
        uint16_t length = (imsi.length() / 2) + (imsi.length() % 2);
        imsi_ie.push_back(length >> 8);
        imsi_ie.push_back(length);

        // Spare & Instance = 0
        imsi_ie.push_back(0x00);

        size_t i = 0;
        for (; i < length - (imsi.length() % 2); ++i)
        {
            uint8_t two_nums = (imsi[i * 2] - '0') | ((imsi[i * 2 + 1] - '0') << 4);
            imsi_ie.push_back(two_nums);
        }

        // Добавление последней цифры с филлером, если длина IMSI не кратна 2
        if (i * 2 < imsi.length())
        {
            uint8_t two_nums = (imsi[i * 2] - '0') | (0xF << 4);
            imsi_ie.push_back(two_nums);
        }

        return imsi_ie;
    }

    bool IMSI::operator==(const IMSI &other) const
    {
        return this->imsi == other.imsi;
    }

}

namespace std
{
    size_t hash<PGW::IMSI>::operator()(const PGW::IMSI &imsi) const noexcept
    {
        return hash<string>{}(imsi.get_IMSI_to_str());
    }
}