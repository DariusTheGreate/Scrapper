#include "webSocketConnection.h"

#include <chrono>
#include <thread>

void WebSocketClient::run() 
{
    if (stopping_) return; 

    resolver_.async_resolve(host_, port_,
        [this](beast::error_code ec, net::ip::tcp::resolver::results_type results) 
        {
            if (ec) 
            {
                fail(ec, "resolve");
                return;
            }
            onResolve(results);
        });
}

void WebSocketClient::stop()
{
    stopping_ = true;
    try
    {
        ws_.next_layer().next_layer().cancel(); // Cancel SSL operations
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception while cancelling SSL operations for WebSocket {}: {}", id_, e.what());
    }

    if (ws_.is_open())
    {
        try
        {
            beast::error_code ec;
            ws_.async_close(beast::websocket::close_code::normal,
            [this](beast::error_code ec)
            {
                if (ec && ec != net::error::eof && ec != boost::asio::error::operation_aborted && ec != boost::asio::ssl::error::stream_truncated)
                {
                    spdlog::error("WebSocket {} failed to close: {}", id_, ec.message());
                }
                else
                {
                    spdlog::info("WebSocket {} closed successfully.", id_);
                }
                stopping_ = false;
            });
        }
        catch (const std::exception& e)
        {
            spdlog::error("Exception while initiating async_close for WebSocket {}: {}", id_, e.what());
            stopping_ = false; 
        }
    }
    else
    {
        spdlog::info("WebSocket {} is already closed.", id_);
    }
    spdlog::info("Stopping WebSocket client for symbol: {}", symbol_);
}

void WebSocketClient::onResolve(net::ip::tcp::resolver::results_type results) 
{
    net::async_connect(ws_.next_layer().next_layer(), results,
        [this](beast::error_code ec, net::ip::tcp::endpoint ep) 
        {
            if (ec) 
            {
                fail(ec, "connect");
                return;
            }
            onConnect(ep);
        });
}

void WebSocketClient::onConnect(net::ip::tcp::endpoint ep) 
{
    ws_.next_layer().async_handshake(ssl::stream_base::client,
        [this](beast::error_code ec) 
        {
            if (ec) 
            {
                fail(ec, "ssl_handshake");
                return;
            }
            onSslHandshake();
        });
}

void WebSocketClient::onSslHandshake() 
{
    ws_.async_handshake(host_, endpoint_,
        [this](beast::error_code ec) 
        {
            if (ec) 
            {
                fail(ec, "handshake");
                return;
            }
            onHandshake();
        });
}

void WebSocketClient::onHandshake() 
{
    spdlog::info("WebSocket {}, connected to {}", id_, endpoint_);
    readMessage(); 
}

void WebSocketClient::readMessage() 
{
    ws_.async_read(buffer_,
        [this](beast::error_code ec, std::size_t bytes_transferred) 
        {
            if(stopping_)
                return;

            if (ec) 
            {
                if (ec == websocket::error::closed) 
                {
                    spdlog::info("WebSocket {} closed clearly", id_);
                }
                else 
                {
                    fail(ec, "read");
                }

                buffer_.clear();
                return;
            }

            //Unocomment to test stability in case of disconnect
            /*static int c = 0;
            static int failC = 50;
            if(symbol_ == "btcusdt" && c++ == failC)
                fail({}, "random error");
            */

            const char* data_ptr = boost::asio::buffer_cast<const char*>(buffer_.data());
            size_t data_len = buffer_.size();

            std::string tradeData = std::string(data_ptr, data_len);
            //spdlog::info("{}", tradeData);
            algorithm_.execute(tradeData);

            buffer_.consume(bytes_transferred);
            readMessage(); 
        });
}

void WebSocketClient::fail(beast::error_code ec, const char* what)
{
    spdlog::error("WebSocket {} {}: {}. Adding this symbol {} to failedConnections list.", id_, what, ec.message(), symbol_);
    std::lock_guard<std::mutex> lock(failedConnections.mutex);
    failedConnections.symbols.push_back(symbol_);
    return;
}

FailedConnectionsContainer WebSocketClient::failedConnections;

