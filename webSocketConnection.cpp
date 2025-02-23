#include "webSocketConnection.h"

#include <chrono>
#include <thread>

void WebSocketClient::run() 
{
    stopped_ = false;
    if (stopping_) return; 

    resolver_.async_resolve(host_, port_,
        [this](beast::error_code ec, net::ip::tcp::resolver::results_type results) 
        {
            if (ec) 
                return fail(ec, "resolve");
            onResolve(results);
        });
}

void WebSocketClient::stop()
{
    stopping_ = true;
    //cancelSSL();
    if (ws_.is_open())
    {
        try
        {
            closeConnectionAsync();
        }
        catch (const std::exception& e)
        {
            spdlog::error("Exception while initiating async_close for WebSocket {}: {}", id_, e.what());
        }
    }
    else
    {
        spdlog::info("WebSocket {} is already closed.", id_);
    }
}

void WebSocketClient::cancelSSL()
{
    try
    {
        ws_.next_layer().next_layer().cancel(); // Cancel SSL operations
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception while cancelling SSL operations for WebSocket {}: {}", id_, e.what());
        // .. what can we do ?..
    }
}

void WebSocketClient::closeConnectionAsync()
{
    beast::error_code ec;
    ws_.async_close(beast::websocket::close_code::normal,
        [this](beast::error_code ec)
        {
            onClose(ec);
        });
    spdlog::info("end of closeConnectionAsync()");
}

void WebSocketClient::onClose(beast::error_code ec)
{
    if (ec && ec != net::error::eof && ec != boost::asio::error::operation_aborted && ec != boost::asio::ssl::error::stream_truncated)
    {
        spdlog::error("WebSocket {} failed to close: {}", id_, ec.message());
        return;
    }
    spdlog::info("WebSocket {} closed successfully.", id_);

    stopping_ = false; // dont like this
    stopped_ = true;
}

void WebSocketClient::onResolve(net::ip::tcp::resolver::results_type results) 
{
    net::async_connect(ws_.next_layer().next_layer(), results,
        [this](beast::error_code ec, net::ip::tcp::endpoint ep) 
        {
            if (ec) 
                return fail(ec, "connect");
            onConnect(ep);
        });
}

void WebSocketClient::onConnect(net::ip::tcp::endpoint ep) 
{
    ws_.next_layer().async_handshake(ssl::stream_base::client,
        [this](beast::error_code ec) 
        {
            if (ec) 
                return fail(ec, "ssl_handshake");
            onSslHandshake();
        });
}

void WebSocketClient::onSslHandshake() 
{
    ws_.async_handshake(host_, endpoint_,
        [this](beast::error_code ec) 
        {
            if (ec) 
                return fail(ec, "handshake");
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
            if(stopping_ || stopped_)
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

            //Unocomment to fast test stability in case of disconnect
            /*static int c = 0;
            static int failC = 50;
            if(symbol_ == "btcusdt" && c++ == failC)
                fail({}, "random error");
            */

            const char* dataPtr = boost::asio::buffer_cast<const char*>(buffer_.data());
            std::string tradeData = std::string(dataPtr, buffer_.size());
            //spdlog::info(tradeData);
            algorithm_.execute(tradeData); // async for this to since its block whole websocket?

            buffer_.consume(bytes_transferred);
            readMessage(); // TODO: can this lead to stack overflow if read is too fast?.. 
        });
}

void WebSocketClient::fail(beast::error_code ec, const char* what)
{
    // Nice to add immediate reconnections attempts using some exponential backoff strategy
    spdlog::error("WebSocket {} {}: {}. Adding this symbol {} to failedConnections list.", id_, what, ec.message(), symbol_);
    WebSocketClient::failedConnections.add(symbol_); 
    return;
}

FailedConnectionsContainer WebSocketClient::failedConnections;

