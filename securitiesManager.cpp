#include "securitiesManager.h"
#include <spdlog/spdlog.h>

// todo: atomic flag pointer - not best solution
void BinanceSession::run(std::atomic_flag* flg) 
{
    _dataReady = flg;
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

bool BinanceSession::isDataAvailable() 
{
    if(!_dataReady)
        return false;
    return _dataReady->test();
}

void BinanceSession::onResolve(beast::error_code ec, net::ip::tcp::resolver::results_type results) 
{
    if (ec)
        return fail(ec, "resolve");

    boost::asio::async_connect(stream_.next_layer(), results, [this](beast::error_code ec, net::ip::tcp::endpoint) 
    {
        onConnect(ec);
    });
}

void BinanceSession::onConnect(beast::error_code ec) 
{
    if (ec)
        return fail(ec, "connect");

    stream_.async_handshake(ssl::stream_base::client, [this](beast::error_code ec) 
    {
        onHandshake(ec);
    });
}

void BinanceSession::onHandshake(beast::error_code ec) 
{
    if (ec)
        return fail(ec, "handshake");

    _req.set(http::field::host, host_);
    _req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    http::async_write(stream_, _req, [this](beast::error_code ec, std::size_t bytes_transferred) 
    {
        onWrite(ec, bytes_transferred);
    });
}

void BinanceSession::onWrite(beast::error_code ec, std::size_t bytes_transferred) 
{
    if (ec)
        return fail(ec, "write");

    http::response_parser<http::file_body> parser;
    constexpr size_t c_exchangeInfoResponseLimit = 1024 * 1024 * 64;
    parser.body_limit(c_exchangeInfoResponseLimit);
    beast::error_code ec_file;
    parser.get().body().open(_filename.c_str(), beast::file_mode::write, ec_file);
    if (ec_file) 
    {
        spdlog::error("Error during openning of a file:{}", _filename);
        return;
    }

    beast::flat_buffer buffer;
    bool done = false;
    while (!done) 
    {
        http::read(stream_, buffer, parser, ec);
        if (ec == net::error::eof) 
            ec = {}; 
        if(ec) 
        {
            spdlog::error("HTTP read error: ", ec.message());
            stream_.shutdown(ec);
            if (ec) 
                spdlog::error("Stream shutdown error: ", ec.message());
            return;
        }
        done = parser.is_done(); 
    }
    if(parser.get().result() != http::status::ok)
    {
        spdlog::error("HTTP request failed: ");
        return;
    }
    auto ecTmp = beast::error_code();
    stream_.shutdown(ecTmp); 
    spdlog::info("Sucessfully received exchangeinfo.");

    if(_dataReady)
        _dataReady->test_and_set();
}

void BinanceSession::fail(beast::error_code ec, char const* what) 
{
    spdlog::error("BinanceSession error: {}: {}. Program will try again in given timeout(see config.toml)", what, ec.message());
}


