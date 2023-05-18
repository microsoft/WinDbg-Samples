#pragma once
#include <string>
#include <map>
#include "GdbSrvControllerLib.h"


typedef std::string(*FnExecuteCommand)(_In_ const char* pCommand);

static std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();
    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

static std::string hex_to_string(const std::string& input)
{
    std::string output;
    for (size_t i = 0; i < input.length(); i += 2)
    {
        std::string byte = input.substr(i, 2);
        char chr = (char)(int)strtol(byte.c_str(), NULL, 16);
        output.push_back(chr);
    }
    return output;
}

static void QRcmdRegistor(GdbSrvController* ctx, const std::string& input, std::map<std::string, std::string>& maps)
{
    std::string qstr = std::string("qRcmd,").append(string_to_hex(input));
    std::string rstr = ctx->ExecuteCommand(qstr.c_str());
    if (!rstr.empty()
        && rstr.size() > 2
        && rstr.c_str()[0] == 'O'
        && rstr.c_str()[rstr.size() - 1] == 'a')
    {
        char buffer[100] = { 0 };
        rstr.copy(buffer, rstr.size() - 2, 1);
        std::string sstr = hex_to_string(buffer);
        size_t index = sstr.find('=');
        if (index > 0 && index != (size_t)(-1))
        {
            std::string key = sstr.substr(0, index);
            std::string val = sstr.substr(index + 1, rstr.size() - index);
            if (val.c_str()[1] == 'x' || val.c_str()[1] == 'X')
                val = sstr.substr(index + 3, rstr.size() - index - 2);

            maps[key] = val;
        }
    }
}

static void QRcmdRegistor2(GdbSrvController* ctx, const std::string& input, std::map<std::string, std::string>& maps)
{
    std::string qstr = std::string("qRcmd,").append(string_to_hex(input));
    std::string rstr = ctx->ExecuteCommand(qstr.c_str());

    if (!rstr.empty()
        && rstr.size() > 2
        && rstr.c_str()[0] == 'O'
        && rstr.c_str()[rstr.size() - 1] == 'a')
    {
        char buffer[100] = { 0 };
        rstr.copy(buffer, rstr.size() - 2, 1);
        std::string sstr = hex_to_string(buffer);

        size_t tag1index = sstr.find("base=0x");
        size_t tag2index = sstr.find(" limit=0x");
        if (tag1index > 0 && tag1index != (size_t)(-1) &&
            tag2index > 0 && tag2index != (size_t)(-1))
        {
            std::string value1 = sstr.substr(tag1index + 7, sstr.size() - tag1index - 7 - (sstr.size() - tag2index));
            std::string value2 = sstr.substr(tag2index + 9, sstr.size() - tag2index - 9);

            std::string key1 = std::string(sstr.substr(0, 4)).append("base");
            std::string key2 = std::string(sstr.substr(0, 4)).append("limit");

            maps[key1] = value1;
            maps[key2] = value2;
        }
    }
}

static void QuerySpecialRegistor(GdbSrvController* ctx, std::map<std::string, std::string>& maps)
{
    //pfn("qRcmd,7220637233");
    QRcmdRegistor(ctx, "r cr0", maps);
    QRcmdRegistor(ctx, "r cr2", maps);
    QRcmdRegistor(ctx, "r cr3", maps);
    QRcmdRegistor(ctx, "r cr4", maps);
    QRcmdRegistor(ctx, "r cr8", maps);

    QRcmdRegistor2(ctx, "r idtr", maps);
    QRcmdRegistor2(ctx, "r gdtr", maps);
}