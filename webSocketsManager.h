#pragma once
#include "webSocketConnection.h"

class WebSocketsManager
{
public:
    WebSocketsManager();

    void establishConnections(const std::vector<std::string>& symbols);

    void addClient(const std::string& symbol, boost::asio::ssl::context& ctx, size_t index); 
    
    void addNewClient(const std::string& symbol);

    void removeClient(const std::string& symbol);

    void removeClientInternal(const std::string& symbol);

    bool containsSymbol(const std::string& symbol);

    size_t getNumOfClients() const;

    void removeUnnecessaryConnections(const std::vector<std::string>& symbols);

    void checkConnectionsLimit();

    size_t getConnectionsLimit() const;

    bool isAbleToAddNewConnections();

    void removeSomeConnectionsAndDecreaseConnectionsLimit(size_t num);

private:
    std::mutex _clientsMutex;
    size_t _connectionsLimit;
    std::unordered_map<std::string, std::unique_ptr<WebSocketClient>> _clients;
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx{ boost::asio::ssl::context::tlsv12_client };
};


