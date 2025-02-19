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
        auto downloadSecurities = std::async(std::launch::async, [this]()
        {
            downloadExchangeInfo();
        });
        auto mainLoop = std::async(std::launch::async, [this]()
        {
            update();  
        }); 
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
        if(_exchangeDataUpdate.test())
        {
            parseSecurities();
        }
        if(_symbolsUpdate.test())
        {
            if(!_connectionsInitiated.test())
                establishConnections();
            else
                updateConnections();
        }
    }
}

void Service::downloadExchangeInfo()
{
    boost::asio::io_context ioc;
    boost::asio::steady_timer timer(ioc, std::chrono::seconds(0)); // Initial delay = 0
    std::function<void(const boost::beast::error_code&)>timer_callback = [this, &timer, &timer_callback](const boost::beast::error_code& error) 
    {
        if (!error) 
        {
            run_binance_session();
            timer.expires_at(timer.expiry() + std::chrono::seconds(_serviceConfiguration.timer));
            timer.async_wait(timer_callback);
        }
        else
        {
             spdlog::error("Timer error: {}", error.message());
        }
    };
    timer.async_wait(timer_callback); 
    ioc.run();
}

void Service::run_binance_session() 
{
    try
    {
        boost::asio::io_context ioc;
        BinanceSession session(ioc, ctx, "api.binance.com", "443");
        spdlog::info("Try to get exchangeinfo");
        session.run(&_exchangeDataUpdate);
        ioc.run(); // Blocks session
    }
    catch (const std::exception& e) 
    {
        spdlog::error("Exception in run_binance_session {}. Probably due to the fact that we handle to much webSockets connections. Try to remove connections and update connections limit", e.what());
        _connectionsManager.removeSomeConnectionsAndDecreaseConnectionsLimit(100);
    }
}

// todo: may combine establishConnections and updateConnections
void Service::establishConnections()
{
    spdlog::info("Establish websocket connections to securities");
    if(!jsonFileExists(_exchangeInfoFilePath))
    {
        spdlog::error("Json file {} is not available.", _exchangeInfoFilePath);
        _symbolsUpdate.clear();
        _connectionsInitiated.test_and_set();
        return;
    }
    _connectionsInitiated.test_and_set();
    std::thread([this](){
        _connectionsManager.establishConnections(_symbols);
    }).detach();
    _connectionsInitiated.test_and_set();

    _symbolsUpdate.clear();
}

void Service::updateConnections()
{
    spdlog::info("Update websocket connections. MaxConnections: {}", _connectionsManager.getConnectionsLimit());
    if(!jsonFileExists(_exchangeInfoFilePath))
    {
        spdlog::error("Json file {} is not available.", _exchangeInfoFilePath);
        _symbolsUpdate.clear();
        return;
    }

    _symbols = Parser::parseSecurities(_exchangeInfoFilePath);
    _symbols = findIntersection(_symbols, _serviceConfiguration.securities, _serviceConfiguration.filter);

    _connectionsManager.removeUnnecessaryConnections(_symbols);
    if(!_connectionsManager.isAbleToAddNewConnections())
    {
        spdlog::info("WARNING: Exceed connections limit (ulimit), zero new clients will be added");
        _symbolsUpdate.clear();
        return;
    }
    
    for(const auto& i : _symbols)
    {
        if(!_connectionsManager.isAbleToAddNewConnections())
            break;
        _connectionsManager.addNewClient(i);
    }

    _symbolsUpdate.clear();
}

void Service::parseSecurities()
{
    spdlog::info("Parse new securities");
    _serviceConfiguration = Parser::parseTomlConfig("config.toml");
    spdlog::info("Timer: {}", _serviceConfiguration.timer);

    if(!jsonFileExists(_exchangeInfoFilePath))
    {
        spdlog::error("Json file {} is not available.", _exchangeInfoFilePath);
        _symbolsUpdate.test_and_set();
        _exchangeDataUpdate.clear();
        return;
    }

    _symbols = Parser::parseSecurities(_exchangeInfoFilePath);
    _symbols = findIntersection(_symbols, _serviceConfiguration.securities, _serviceConfiguration.filter);
    _symbolsUpdate.test_and_set();
    _exchangeDataUpdate.clear();
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

bool Service::jsonFileExists(const std::string& filename) 
{
    std::ifstream file(filename);
    return file.good();
}

