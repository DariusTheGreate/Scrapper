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
    // new thread because it will call ioc.run() and block thread
    std::thread([this, &symbols](){ 
        establishConnectionsInternal(symbols);
    }).detach();
}

void WebSocketsManager::establishConnectionsInternal(const std::vector<std::string>& symbols)
{
    if(symbols.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        for (size_t i = 0; i < symbols.size(); ++i)
        {
            if(i >= _connectionsLimit)
            {
                spdlog::info("WARNING: Exceed connections limit (ulimit)");
                break;
            }

            addClient(symbols[i], ctx);
        }
    }

    _connectionsEstablished.endEvent(); // remember that we have established connections.
    ioc.run();
}

void WebSocketsManager::updateConnections(const std::vector<std::string>& symbols)
{
    spdlog::info("Update connections");

    std::lock_guard<std::mutex> lock(_clientsMutex);
    removeUnnecessaryConnections(symbols);
    for(const auto& i : symbols)
    {
        if(!isAbleToAddNewConnections())
        {
            spdlog::info("WARNING: Exceed connections limit (ulimit), zero new clients will be added");
            break;
        }
        WebSocketClient::failedConnections.remove(i);
        addClient(i, ctx);
    }
}

void WebSocketsManager::addClient(const std::string& symbol, boost::asio::ssl::context& ctx) 
{
    if(_clients.find(symbol) == _clients.end())
    {
        spdlog::info("Adding symbol: {}", symbol);
        static size_t indexInc = 0; // make it something else
        _clients[symbol] = std::make_unique<WebSocketClient>(ioc, ctx, "stream.binance.com", "443", symbol);
        _clients[symbol]->run();
    }
}

void WebSocketsManager::removeUnnecessaryConnections(const std::vector<std::string>& symbols)
{
    // Remove connections that are not satisfy current filtration or failed
    std::vector<std::string> clientsToRemove;
    for(const auto& [k,v] : _clients)
    {
        if(std::find(symbols.begin(), symbols.end(), k) == symbols.end() || (v && v->isFailed())) 
        {
            if(stopClient(k))
                clientsToRemove.push_back(k);
        }
    }

    for(const auto& k : clientsToRemove)
    {
        _clients.erase(k);
        spdlog::info("Removed WebSocketClient for symbol: {}", k);
    }
}

bool WebSocketsManager::stopClient(const std::string& symbol) 
{
    auto it = _clients.find(symbol);
    if (it != _clients.end() && it->second)
    {
        if(!it->second->isStopped() && !it->second->isStopping())
        {
            it->second->stop(); 
            spdlog::info("Stopping WebSocketClient for symbol: {}", symbol);
            return false;
        }
        else if(it->second->isStopped())
        {
            return true;
        }
    }
    spdlog::info("WebSocketClient not found for symbol: {}", symbol);
    return false;
}

void WebSocketsManager::checkConnectionsLimit()
{
    struct rlimit limit;
    if(getrlimit(RLIMIT_NOFILE, &limit) == 0)
    {
        spdlog::info("Current soft limit for number of open file descriptors: {}", limit.rlim_cur);
        spdlog::info("Maximum soft limit for number of open file descriptors: {}", limit.rlim_max);
        // Use only half of available file descriptors for now, for stablility

        const rlim_t newLimit = 8912;

        // Check if the new limit exceeds the maximum allowed limit
        if (newLimit > limit.rlim_max)
        {
            spdlog::warn("Requested limit {} exceeds maximum allowable limit {}. Adjusting to maximum.", newLimit, limit.rlim_max);
            _connectionsLimit = limit.rlim_max;
        }
        else
        {
            _connectionsLimit = newLimit;
        }

        limit.rlim_cur = _connectionsLimit;
        
        if (setrlimit(RLIMIT_NOFILE, &limit) == 0)
        {
            spdlog::info("Successfully set soft limit for descriptors to: {}", _connectionsLimit);
        }
        else
        {
            spdlog::error("Failed to set soft limit for descriptors: {}", strerror(errno));
        }

        _connectionsLimit = limit.rlim_cur / 2;
        spdlog::info("Current application limit for descriptors: {}", _connectionsLimit);
    }
}

void WebSocketsManager::stopSomeConnectionsAndDecreaseConnectionsLimit(size_t num)
{
    std::lock_guard<std::mutex> lock(_clientsMutex);
    for(const auto& [k,v] : _clients)
    {
        if(num-- > 0)
        {
            spdlog::info("Remove client number {}", num+1);
            stopClient(k);
            _connectionsLimit--;
        }
    }
}
