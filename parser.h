#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct Config 
{
    std::vector<std::string> securities;
    size_t timer = 30;
    std::string filter;
};


using json = nlohmann::json;
class Parser 
{
public:
    static std::vector<std::string> parseSecurities(const std::string& json_file); 
    static Config parseTomlConfig(const std::string& filePath); 
};

