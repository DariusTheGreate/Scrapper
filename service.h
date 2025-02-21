#pragma once
#include <atomic>

#include "parser.h"
#include "webSocketsManager.h"
#include "downloadTimerEvent.h"

class Service
{
public:
    Service(const std::string& fp) : _exchangeInfoFilePath(fp) {}

    void run();
private:

    void update();

    void updateConfig();

    void updateSymbols();

    std::vector<std::string> findIntersection(std::vector<std::string>& v, std::vector<std::string>& filter, const std::string& predicateFilter);

    bool isFileExists(const std::string& filename);

    const std::vector<std::string>& getSymbols() const;

    const std::string& getSymbolsPath() const;

private:
    WebSocketsManager _connectionsManager;
    DownloadOnTimerEvent<BinanceSession> _downloadedEvent;
    Config _serviceConfiguration;
    std::vector<std::string> _symbols;
    std::string _exchangeInfoFilePath = "exchange_info.json";
};

