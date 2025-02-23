#include "parser.h"

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

std::vector<std::string> Parser::parseSecurities(const std::string& json_file) 
{
    std::vector<std::string> symbols_vector;
    try 
    {
        std::ifstream file(json_file);
        if (!file.is_open()) 
        {
            spdlog::error("Failed to open file!");
            return {};
        }
        json data;
        file >> data;
        if (data.contains("symbols") && data["symbols"].is_array()) 
        {
            for (const auto& symbol_data : data["symbols"]) 
            {
                if (symbol_data.contains("symbol") && symbol_data["symbol"].is_string()) 
                {
                    auto sec = symbol_data["symbol"].get<std::string>();
                    std::for_each(sec.begin(), sec.end(), [](char& c){ c = std::tolower(c); });
                    symbols_vector.push_back(std::move(sec));
                }
            }
        } 
        else 
        {
            spdlog::error("JSON structure does not contain 'symbols' array or is not in the expected format");
            return {};
        }

    } 
    catch (const json::parse_error& e) 
    {
        spdlog::error("JSON parse error");
        return {}; 
    }
    catch (const std::exception& e) 
    {
        spdlog::error("Std exception: ");
        return {}; 
    }
    return std::move(symbols_vector);
}

Config Parser::parseTomlConfig(const std::string& filePath) 
{
    Config config;
    try 
    {
        auto tomlData = toml::parse_file(filePath);
        if (tomlData.contains("main") && tomlData["main"].is_table()) 
        {
            if (auto* mainTable = tomlData["main"].as_table(); mainTable)
            {
                if (auto* securitiesArray = mainTable->get_as<toml::array>("securities")) 
                {
                    for (const auto& item : *securitiesArray) 
                    {
                        if (item.is_string()) 
                        {
                            auto sec = item.as_string()->get();
                            std::for_each(sec.begin(), sec.end(), [](char& c){ c = std::tolower(c); });
                            config.securities.emplace_back(std::move(sec));
                        }
                    }
                }
                if (auto timer = mainTable->get("timer"); timer && timer->is_integer()) 
                {
                    config.timer = timer->as_integer()->get();
                }
                if (auto timer = mainTable->get("filter"); timer && timer->is_string()) 
                {
                    config.filter = timer->as_string()->get();
                    if(config.filter.size() > 0)
                        std::for_each(config.filter.begin(), config.filter.end(), [](char& c){ c = std::tolower(c); });
                }
           }

        }
    }
    catch (const toml::parse_error& err) 
    {
        spdlog::error("Error parsing TOML file: {} line: {} column: {}", err.description(), err.source().begin.line, err.source().begin.column);
    }

    return config;
}
