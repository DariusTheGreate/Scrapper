#pragma once
//todo: remove unnecessary includes.
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
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <algorithm>

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

struct Event;

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace websocket = boost::beast::websocket;
using tcp = net::ip::tcp;

class BinanceSession {
public:
    BinanceSession(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, const std::string& host, const std::string& port, const std::string& filename, Event* eventPtr)
        : resolver_(ioc)
        , stream_(ioc, ctx)
        , host_(host)
        , port_(port)
        , _filename(filename)
        , _eventToNotify(eventPtr)
    {
    }

    void run(); 
    
private:
    void onResolve(beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results); 
    
    void onConnect(boost::beast::error_code ec);
    
    void onHandshake(boost::beast::error_code ec); 
    
    void onWrite(boost::beast::error_code ec, std::size_t bytes_transferred); 

    void fail(boost::beast::error_code ec, char const* what); 

private:
    http::request<http::string_body> _req{ http::verb::get, "/api/v3/exchangeInfo", 11 };
    net::ip::tcp::resolver resolver_;
    beast::ssl_stream<net::ip::tcp::socket> stream_;
    boost::asio::streambuf buffer_;
    http::response<http::dynamic_body> res_;
    std::string host_;
    std::string port_;
    std::string _filename = "exchange_info.json";
    Event* _eventToNotify = nullptr;
};


//class OtherSession {}