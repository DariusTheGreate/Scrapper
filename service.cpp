#include "service.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <algorithm>
#include <unordered_set>

#include "webSocketConnection.h"
#include "securitiesManager.h"

void Service::run()
{
    try
    {
        updateConfig(); //make it updateSymbols and establish connections immidiatly in case we have exchangeinfo file on start
        auto downloadSecurities = std::async(std::launch::async, [this]()
        {
            try
            {
                _downloadedEvent.downloadExchangeInfo();
            }
            catch(const std::exception& e)
            {
                spdlog::error("Exception {} when trying to download exchange info. Probably due to using to much web socket connections. Trying resolve problem by removing some connections..", e.what());
                _connectionsManager.stopSomeConnectionsAndDecreaseConnectionsLimit(50);
            }
            
        });
        update();  
    } 
    catch (std::exception const& e) 
    {
        spdlog::error("Uncaught exception: ", e.what());
    }
}

void Service::update()
{
    while(1)
    {
        if(_downloadedEvent.isDone())
        {
            updateSymbols();
            _connectionsManager.update(getSymbols());
            _downloadedEvent.restartEvent();
        }
    }
}

void Service::updateConfig()
{
    if(!isFileExists("config.toml"))
    {
        spdlog::info("Config file is not available, use default config.");
    }

    _serviceConfiguration = Parser::parseTomlConfig("config.toml");
    _downloadedEvent.setTimeOut(_serviceConfiguration.timer);
    spdlog::info("Config: timer: {}, filter: {}", _serviceConfiguration.timer, _serviceConfiguration.filter);
}

void Service::updateSymbols()
{
    updateConfig();
    if(!isFileExists(_exchangeInfoFilePath))
    {
        spdlog::error("Json file {} is not available.", _exchangeInfoFilePath);
        return;
    }
    _symbols = Parser::parseSecurities(_exchangeInfoFilePath);
    _symbols = findIntersection(_symbols, _serviceConfiguration.securities, _serviceConfiguration.filter);
}

std::vector<std::string> Service::findIntersection(std::vector<std::string>& v, std::vector<std::string>& filter, const std::string& predicateFilter) 
{
    if(filter.empty())
    {
        std::vector<std::string> result;
        std::copy_if(v.begin(), v.end(), std::back_inserter(result), [&predicateFilter](auto& v){ return v.find(predicateFilter) != std::string::npos;});
        return result;
    }
    std::vector<std::string> result;
    std::sort(v.begin(), v.end());
    std::sort(filter.begin(), filter.end());

    std::vector<std::string> intersection;
    std::set_intersection(v.begin(), v.end(), filter.begin(), filter.end(), std::back_inserter(intersection));

    std::copy_if(intersection.begin(), intersection.end(), std::back_inserter(result), [&predicateFilter](auto& v){ return v.find(predicateFilter) != std::string::npos;});

    return result;
}

bool Service::isFileExists(const std::string& filename) 
{
    std::ifstream file(filename);
    return file.good();
}

const std::vector<std::string>& Service::getSymbols() const
{
    return _symbols;
}

const std::string& Service::getSymbolsPath() const
{
    return _exchangeInfoFilePath;
}

