#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "tradingSystem.h"
#include "securitiesManager.h"


namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

struct FailedConnectionsContainer
{
    std::vector<std::string> symbols;
    std::mutex mutex;
};

class WebSocketClient
{
public:
    WebSocketClient(net::io_context& ioc, ssl::context& ctx, const std::string& host, const std::string& port, const std::string& symbol, int id)
        : ioc_(ioc)
        , ctx_(ctx)
        , resolver_(ioc_)
        , ws_(ioc_, ctx_)
        , host_(host)
        , port_(port)
        , symbol_(symbol)
        , id_(id)
        , stopping_(false)
    {
        endpoint_ = "/ws/" + symbol_ + "@aggTrade";
    }

    void run(); 
    
    void stop();
    
private:
    
    void onResolve(net::ip::tcp::resolver::results_type results);
    
    void onConnect(net::ip::tcp::endpoint ep);
    
    void onSslHandshake(); 
    
    void onHandshake(); 
    
    void readMessage(); 
    
    void fail(beast::error_code ec, const char* what);
    
private:
    TradingAlgorithm algorithm_;
    net::io_context& ioc_;
    ssl::context& ctx_;
    net::ip::tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<net::ip::tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::string port_;
    std::string symbol_;
    std::string endpoint_;
    int id_;
    bool stopping_ = false;
public:
    static FailedConnectionsContainer failedConnections;
};


