#pragma once
#include "webSocketConnection.h"
#include "Event.h"

class WebSocketsManager 
{
public:
    WebSocketsManager();

    void update(const std::vector<std::string>& symbols);

    void stopSomeConnectionsAndDecreaseConnectionsLimit(size_t num);
private:
    void establishConnections(const std::vector<std::string>& symbols);

    void establishConnectionsInternal(const std::vector<std::string>& symbols);

    void updateConnections(const std::vector<std::string>& symbols);

    void addClient(const std::string& symbol, boost::asio::ssl::context& ctx); 
    
    bool stopClient(const std::string& symbol);

    bool containsSymbol(const std::string& symbol) const { return _clients.find(symbol) != _clients.end(); }

    size_t getNumOfClients() const { return _clients.size(); }

    void removeUnnecessaryConnections(const std::vector<std::string>& symbols);

    void checkConnectionsLimit();

    size_t getConnectionsLimit() const { return _connectionsLimit; }

    bool isAbleToAddNewConnections()const { return _clients.size() < _connectionsLimit; }

private:
    std::mutex _clientsMutex;
    size_t _connectionsLimit = 0;
    std::unordered_map<std::string, std::unique_ptr<WebSocketClient>> _clients;
    std::vector<std::unique_ptr<WebSocketClient>> _bufferForClosedConnections;
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx{ boost::asio::ssl::context::tlsv12_client };
    Event _connectionsEstablished;
};


