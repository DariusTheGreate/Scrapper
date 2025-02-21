#include "webSocketsManager.h"

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
#include <atomic>

#include <unistd.h>
#include <sys/resource.h>
#include <errno.h>

#include "parser.h"
#include "securitiesManager.h"

WebSocketsManager::WebSocketsManager() 
{
    ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    try
    {
        ctx.load_verify_file("cacert.pem");
    }
    catch(const std::exception& e)
    {
        spdlog::info("Problem during SSL sertificate load. Ensure you have cacert.pem for ssl handshake");
    }
    checkConnectionsLimit();
}

void WebSocketsManager::update(const std::vector<std::string>& symbols)
{
    if(!_connectionsEstablished.isDone())
        establishConnections(symbols);
    else
        updateConnections(symbols);
}

void WebSocketsManager::establishConnections(const std::vector<std::string>& symbols)
{
    std::thread([this, &symbols](){ //dangle!!
        establishConnectionsInternal(symbols);
    }).detach();
}

void WebSocketsManager::establishConnectionsInternal(const std::vector<std::string>& symbols)
{
    if(symbols.empty())
        return;
    for (size_t i = 0; i < symbols.size(); ++i) 
    {
        if(i >= _connectionsLimit)
        {
            spdlog::info("WARNING: Exceed connections limit (ulimit)");
            break;
        }

        addClient(symbols[i], ctx, i);
    }
    _connectionsEstablished.endEvent();
    ioc.run();
}

void WebSocketsManager::updateConnections(const std::vector<std::string>& symbols)
{
    removeUnnecessaryConnections(symbols);
    if(!isAbleToAddNewConnections())
    {
        spdlog::info("WARNING: Exceed connections limit (ulimit), zero new clients will be added");
        return;
    }
    
    for(const auto& i : symbols)
    {
        if(!isAbleToAddNewConnections())
            break;
        addNewClient(i);
    }
}

void WebSocketsManager::updateConnectionsInternal(const std::vector<std::string>& symbols)
{
    if(symbols.empty())
        return;
    for (size_t i = 0; i < symbols.size(); ++i) 
    {
        if(i  >= _connectionsLimit)
        {
            spdlog::info("WARNING: Exceed connections limit (ulimit)");
            break;
        }

        addClient(symbols[i], ctx, i);
    }
    ioc.run();
}

void WebSocketsManager::addClient(const std::string& symbol, boost::asio::ssl::context& ctx, size_t index) 
{
    std::lock_guard<std::mutex> lock(_clientsMutex);
    if (!containsSymbol(symbol)) 
    {
        spdlog::info("Adding symbol: {}", symbol);
        _clients[symbol] = std::make_unique<WebSocketClient>(ioc, ctx, "stream.binance.com", "443", symbol, index);
        _clients[symbol]->run();
    }
}

void WebSocketsManager::addNewClient(const std::string& symbol) 
{
    if(_clients.size()  >= _connectionsLimit)
    {
        spdlog::info("WARNING: Exceed connections limit (ulimit), zero new clients will be added");
        return;
    }
    std::thread([this, symbol]()
    {
       addClient(symbol, ctx, _clients.size());
    }).detach();
}

void WebSocketsManager::removeClient(const std::string& symbol) {
    std::thread([this, symbol]()
    {
        removeClientInternal(symbol);
    }).detach();
}

void WebSocketsManager::removeClientInternal(const std::string& symbol) 
{
    std::lock_guard<std::mutex> lock(_clientsMutex);
    auto it = _clients.find(symbol);
    if (it != _clients.end())
    {
        it->second->stop(); 
        _clients.erase(it);
        spdlog::info("Removed WebSocketClient for symbol: {}", symbol);
    }
    else
    {
        spdlog::info("WebSocketClient not found for symbol: {}", symbol);
    }
}

bool WebSocketsManager::containsSymbol(const std::string& symbol)
{
    return _clients.find(symbol) != _clients.end();
}

size_t WebSocketsManager::getNumOfClients() const
{
    return _clients.size();
}

void WebSocketsManager::removeUnnecessaryConnections(const std::vector<std::string>& symbols)
{
    // Remove connections that are failed or stalled or anything else.
    {
        std::unique_lock lk(WebSocketClient::failedConnections.mutex);
        auto& failedSockets = WebSocketClient::failedConnections.symbols;
        for(auto& s : failedSockets)
        {
            removeClient(s);
        }
        failedSockets.clear();
    }

    // Remove connections that are not satisfy current filtration
    for(const auto& [k,v] : _clients)
    {
        if(std::find(symbols.begin(), symbols.end(), k) == symbols.end())
        {
            removeClient(k);
        }
    }
}

void WebSocketsManager::checkConnectionsLimit()
{
    struct rlimit limit;
    if(getrlimit(RLIMIT_NOFILE, &limit) == 0)
    {
        spdlog::info("Current soft limit for number of open file descriptors: {}", limit.rlim_cur);
        spdlog::info("Maximum soft limit for number of open file descriptors: {}", limit.rlim_max);
        // Use only half of available file descriptors for now, for stablility
        _connectionsLimit = limit.rlim_cur / 2;
        spdlog::info("Current application limit for descriptors: {}", _connectionsLimit);
    }
}

size_t WebSocketsManager::getConnectionsLimit() const
{
    return _connectionsLimit;
}

bool WebSocketsManager::isAbleToAddNewConnections()
{
    return _clients.size() < _connectionsLimit;
}

void WebSocketsManager::removeSomeConnectionsAndDecreaseConnectionsLimit(size_t num)
{
    for(const auto& [k,v] : _clients)
    {
        if(num-- > 0)
        {
            spdlog::info("Remove client number {}", num+1);
            removeClient(k);
            _connectionsLimit--;
        }
    }
}

