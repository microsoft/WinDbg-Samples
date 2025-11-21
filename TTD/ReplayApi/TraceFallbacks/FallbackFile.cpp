// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "Fallbacks.h"
#include "InstructionDecoder.h"

#include <TTD/IReplayEngine.h>

#include <ReplayHelpers.h>
#include <Formatters.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <system_error>
#include <vector>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace TTD::Replay;

// Write the fallback statistics to a JSON file.
std::error_code WriteFallbackStatsFile(std::vector<FallbackInfo> const& fallbackInfo, std::filesystem::path const& outputFile)
{
    nlohmann::json jsonArray = nlohmann::json::array();
    
    for (auto const& info : fallbackInfo)
    {
        nlohmann::json obj;
        obj["Type"]                  = info.Type == FallbackType::FullFallback ? "Full" : "Synthetic";
        obj["Count"]                 = info.Count;
        obj["Position"]              = std::format("{}", info.Position);
        obj["InstructionBytes"]      = TTD::GetBytesString<_countof(info.Instruction.bytes)>(info.Instruction.size, info.Instruction.bytes);
        obj["DecodedInstruction"]    = info.DecodedInstruction;
        obj["NormalizedInstruction"] = info.NormalizedInstruction;
        
        jsonArray.push_back(obj);
    }
    
    std::ofstream out(outputFile);
    if (!out.is_open())
    {
        return std::make_error_code(std::errc::io_error);
    }
    
    out << jsonArray.dump(2) << "\n";
    if (!out)
    {
        return std::make_error_code(std::errc::io_error);
    }
    
    out.close();
    if (out.fail())
    {
        return std::make_error_code(std::errc::io_error);
    }
    
    return std::error_code();
}

// Read the fallback statistics from a JSON file.
std::error_code ReadFallbackStatsFile(std::filesystem::path const& inputFile, std::vector<FallbackInfo>& fallbackInfo)
{
    std::ifstream in(inputFile);
    if (!in.is_open())
    {
        std::cerr << std::format("Error: Unable to open input file: {}\n", inputFile.string());
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }
    
    nlohmann::json jsonArray;
    try
    {
        in >> jsonArray;
    }
    catch (nlohmann::json::parse_error const& e)
    {
        std::cerr << std::format("Error: Failed to parse JSON file: {}\n", e.what());
        return std::make_error_code(std::errc::invalid_argument);
    }
    
    if (!jsonArray.is_array())
    {
        std::cerr << "Error: JSON file does not contain an array at the root level\n";
        return std::make_error_code(std::errc::invalid_argument);
    }
    
    fallbackInfo.clear();
    fallbackInfo.reserve(jsonArray.size());
    
    size_t entryIndex = 0;
    for (auto const& obj : jsonArray)
    {
        try
        {
            FallbackInfo info;
            
            std::string typeStr = obj.at("Type").get<std::string>();
            info.Type = (typeStr == "Full") ? FallbackType::FullFallback : FallbackType::SyntheticInstruction;
            
            info.Count = obj.at("Count").get<uint64_t>();

            std::string posStr = obj.at("Position").get<std::string>();
            std::wstring widePosStr = std::wstring(posStr.begin(), posStr.end());
            info.Position = TryParsePositionFromString(widePosStr.c_str(), Position::Invalid);

            std::string hexBytes = obj.at("InstructionBytes").get<std::string>();
            if (!ParseHexBytes(hexBytes, info.Instruction))
            {
                std::cerr << std::format("Error: Failed to parse instruction bytes at entry {}: '{}'\n", entryIndex, hexBytes);
                return std::make_error_code(std::errc::invalid_argument);
            }
            
            info.DecodedInstruction = obj.at("DecodedInstruction").get<std::string>();
            info.NormalizedInstruction = obj.at("NormalizedInstruction").get<std::string>();
            
            fallbackInfo.push_back(info);
            ++entryIndex;
        }
        catch (nlohmann::json::out_of_range const& e)
        {
            std::cerr << std::format("Error: Missing required field in JSON entry {}: {}\n", entryIndex, e.what());
            return std::make_error_code(std::errc::invalid_argument);
        }
        catch (nlohmann::json::type_error const& e)
        {
            std::cerr << std::format("Error: Invalid type for field in JSON entry {}: {}\n", entryIndex, e.what());
            return std::make_error_code(std::errc::invalid_argument);
        }
    }
    
    return std::error_code();
}
