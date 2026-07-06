#include <crc.h>

#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

int Crc::CalcCrc(std::vector<uint8_t> &bytes, ModeCrc mode) {
    int res = 0;
    switch(mode)
    {
    /**
    *   использует полином = 0x1021
    *   для вычисления CRC16
    */
    case ModeCrc::func:
    {
        int crc = 0, poly = 0x1021;
        for (size_t i=0;i<bytes.size();++i) {
            crc ^= bytes[i] << 8;
            for (size_t j=0;j<8;++j) {
                if (crc & 0x8000)
                    crc = (crc << 1) ^ poly;
                else
                    crc <<= 1;
                crc &= 0xFFFF; // Ограничение до 16 бит
            }
        }
        res = crc;
        break;
    }
    /**
     * использует файл с таблицей для
     * быстрого вычисления CRC16
     */
    case ModeCrc::table:
    {
        /**
         * unpack crc table from crc.dat
         */
        std::vector<uint16_t> tbl;     // uint16_t, т.к. crc16
        std::string s;
        std::ifstream fsz("crc.dat");
        while (std::getline(fsz, s)) {
            if (s.empty()) continue;

            uint16_t num = static_cast<uint16_t>(std::stoul(s, nullptr, 16));
            tbl.push_back(num);
        }
        fsz.close();

        if (tbl.size() < 256) {
            std::cout << "Размер таблицы:" << tbl.size() << std::endl;
            throw std::runtime_error("Ошибка: таблица CRC слишком мала!");
        }

        uint16_t crc = 0;  // Начальное значение
        for (uint8_t byte : bytes) {
            uint8_t index = (crc >> 8) ^ byte;  // Индекс в таблице
            crc = (crc << 8) ^ tbl[index];      // Быстрое обновление
            crc &= 0xFFFF;                      // Ограничение 16 бит
        }
        res = crc;
        break;
    }
    }
    return res;
}

std::vector<uint8_t> Crc::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

void Crc::testCrc(std::string strHexBytes) {
    try {
        std::vector<uint8_t> bytes = Crc::hexToBytes(strHexBytes);
        std::cout << std::hex << Crc::CalcCrc(bytes, ModeCrc::func) << std::endl;
        std::cout << std::hex << Crc::CalcCrc(bytes, ModeCrc::table) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
