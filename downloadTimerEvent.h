#pragma once
#include "Event.h"
#include "webSocketsManager.h"

template<typename TSession>
struct DownloadOnTimerEvent : Event
{
    void downloadExchangeInfo(size_t timeOut, const std::string& symbolsPath, const std::string& host, const std::string& port)
    {
        boost::asio::io_context ioc;
        boost::asio::steady_timer timer(ioc, std::chrono::seconds(0)); // Initial delay = 0
        std::function<void(const boost::beast::error_code&)>timer_callback = [this, timeOut, &symbolsPath, &host, &port, &timer, &timer_callback](const boost::beast::error_code& error) 
        {
            if (!error) 
            {
                runBinanceSession(symbolsPath, host, port);
                timer.expires_at(timer.expiry() + std::chrono::seconds(timeOut));
                timer.async_wait(timer_callback);
            }
            else
            {
                 spdlog::error("Timer error: {}", error.message());
            }
        };
        timer.async_wait(timer_callback); 
        ioc.run();
    }

    void runBinanceSession(const std::string& symbolsPath, const std::string& host, const std::string& port) 
    {
        try
        {
            boost::asio::io_context ioc;

            boost::asio::ssl::context ctx{ ssl::context::tlsv12_client };
            ctx.set_verify_mode(ssl::verify_peer);
            ctx.load_verify_file("cacert.pem"); // Download cacert.pem from:  https://curl.se/docs/caextract.html

            // Create new session each time executed
            TSession session(ioc, ctx, host, port, symbolsPath, this);// pass "this" as event to update
            spdlog::info("Try to get exchangeinfo");
            session.run();
            ioc.run(); // Blocks session
        }
        catch (const std::exception& e) 
        {
            spdlog::error("Exception in run_binance_session {}. Probably due to the fact that we handle to much webSockets connections. Try to remove connections and update connections limit", e.what());
            //_connectionsManager.removeSomeConnectionsAndDecreaseConnectionsLimit(100);
        }
    }
};


