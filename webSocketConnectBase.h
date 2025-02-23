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

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

struct WebSocketConnectBase
{
    WebSocketConnectBase(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, const std::string& host, const std::string& port) :
         resolver_(ioc)
       , host_(host)
       , port_(port)
    {}

    void run() 
    {
        resolver_.async_resolve(host_, port_,
        [this](beast::error_code ec, net::ip::tcp::resolver::results_type results) 
        {
            if (ec) 
            {
                fail(ec, "resolve");
                return;
            }
    
            onResolve(ec, results);
        });
    }

    void onResolve(beast::error_code ec, net::ip::tcp::resolver::results_type results) 
    {
        if (ec)
            return fail(ec, "resolve");

        boost::asio::async_connect(getSocket(), results, [this](beast::error_code ec, net::ip::tcp::endpoint) 
        {
            onConnect(ec);
        });
    }

    virtual void onConnect(beast::error_code ec) {};
    // HACK in order to be able to use objects inherited of WebSocketConnectBase as template parameters.
    virtual net::ip::tcp::socket& getSocket() { static boost::asio::io_context ioc; static net::ip::tcp::socket tmp{ioc}; return tmp; };
    virtual void fail(beast::error_code ec, char const* what) {};

    net::ip::tcp::resolver resolver_;
    std::string host_;
    std::string port_;
};
