#pragma once
#include "Event.h"
#include "webSocketsManager.h"

template<typename TSession>
struct DownloadOnTimerEvent : Event
{
    DownloadOnTimerEvent() = default;
    DownloadOnTimerEvent(size_t timeOut, const std::string& symbolsPath, const std::string& host, const std::string& port) 
    : _timeOut(timeOut)
    , _symbolsPath(symbolsPath)
    , _host(host)
    , _port(port) 
    {
        _ctx.set_verify_mode(ssl::verify_peer);
        _ctx.load_verify_file("cacert.pem"); // Download cacert.pem from:  https://curl.se/docs/caextract.html
    }

    void downloadExchangeInfo()
    {
        boost::asio::io_context ioc;
        boost::asio::steady_timer timer(ioc, std::chrono::seconds(0)); // Initial delay = 0
        std::function<void(const boost::beast::error_code&)>timer_callback = [this, &timer, &timer_callback](const boost::beast::error_code& error) // note dangle may occure if not carefull
        {
            if (!error) 
            {
                runBinanceSession();
                timer.expires_at(timer.expiry() + std::chrono::seconds(_timeOut));
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

    void runBinanceSession() 
    {
        boost::asio::io_context ioc;

        // Create new session each time executed, since it is new session..
        TSession session(ioc, _ctx, _host, _port, _symbolsPath, this); // pass "this" as event to update
        spdlog::info("Try to get exchangeinfo");
        session.run();
        ioc.run(); // Blocks session
    }

    void setTimeOut(size_t sec)
    {
        _timeOut = sec;
    }

private:
    size_t _timeOut = 0;

    std::string _host;
    std::string _port;
    std::string _symbolsPath;

    boost::asio::ssl::context _ctx{ ssl::context::tlsv12_client };
};
