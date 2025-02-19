#pragma once
#include <atomic>

#include "parser.h"
#include "webSocketsManager.h"

class Service
{
public:
    Service() {}
    Service(const std::string& fp) : _exchangeInfoFilePath(fp) 
    {
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.load_verify_file("cacert.pem"); // Download cacert.pem from:  https://curl.se/docs/caextract.html
    }

    void run();
private:

    void update();
    
    void downloadExchangeInfo();
    
    void run_binance_session(); 
    // todo: may combine establishConnections and updateConnections
    void establishConnections();
    
    void updateConnections();
    
    void parseSecurities();
    
    std::vector<std::string> findIntersection(std::vector<std::string>& v, std::vector<std::string>& filter, const std::string& predicateFilter = ""); 
    
    bool jsonFileExists(const std::string& filename);
    
private:
    Config _serviceConfiguration;
    WebSocketsManager _connectionsManager;
    std::vector<std::string> _symbols;
    std::string _exchangeInfoFilePath = "exchange_info.json";
    boost::asio::ssl::context ctx{ ssl::context::tlsv12_client };

    std::atomic_flag _exchangeDataUpdate;
    std::atomic_flag _symbolsUpdate;
    std::atomic_flag _connectionsInitiated;
};

