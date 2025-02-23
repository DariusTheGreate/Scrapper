#include "tradingSystem.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <cctype>
#include <random>

#include "spdlog/spdlog.h"
#include <nlohmann/json.hpp>

std::string TradingAlgorithm::execute(const std::string& json_string) 
{
    nlohmann::json json_data;
    try
    {
        json_data = nlohmann::json::parse(json_string);
    }
    catch (const std::exception& e) 
    {
        spdlog::error("Failed to parse JSON: {}", e.what());
        return "ERROR: Invalid JSON";
    }

    std::string event_type = json_data["e"];
    double price = std::stod(json_data["p"].get<std::string>());
    double quantity = std::stod(json_data["q"].get<std::string>());
    _id = json_data["s"];

    _priceHistory.push_back(price);
    if (_priceHistory.size() > _longWindow) 
    {
        _priceHistory.pop_front();
    }

    if (_priceHistory.size() >= _longWindow) 
    {
        double short_ma = calculateMovingAverage(_shortWindow);
        double long_ma = calculateMovingAverage(_longWindow);
        std::string prediction = analyzeMovingAverage(short_ma, long_ma);

        simulateTrade(prediction, price, quantity);
        return prediction;
    }

    return "WAIT";
}

double TradingAlgorithm::getMeanProfit() const 
{
    if (_tradeProfits.empty()) 
        return 0.0;
    double sum = 0.0;
    for (double profit : _tradeProfits) 
    {
        sum += profit;
    }
    return sum;
}

void TradingAlgorithm::simulateTrade(const std::string& prediction, double price, double quantity) 
{
    double profit = 0.0;

    if (prediction == "LONG" && _currentBalance >= price * quantity) 
    {
        _position += quantity;
        _currentBalance -= price * quantity;
        spdlog::info("{}: Bought {} at price {}. Current Balance: {}. Current Profit {}.", _id, quantity, price, _currentBalance, getMeanProfit());
    }
    else if (prediction == "SHORT" && _position >= quantity) 
    {
        profit = price * quantity; 
        _currentBalance += profit;
        _position -= quantity; 
        spdlog::info("{}: Sold {} at price {}. Profit: {}, Current Balance: {}. Current Profit {}.", _id, quantity, price, profit, _currentBalance, getMeanProfit());
    }
    _tradeProfits.push_back(profit);
}

double  TradingAlgorithm::calculateMovingAverage(int windowSize) 
{
    double sum = 0.0;
    for (size_t i = _priceHistory.size() - windowSize; i < _priceHistory.size(); ++i)
        sum += _priceHistory[i];
    return sum / windowSize;
}

std::string TradingAlgorithm::analyzeMovingAverage(double shortma, double longma) 
{
    if (shortma > longma)
        return "LONG";
    else if (shortma < longma) 
        return "SHORT"; 
    return "HOLD"; 
}

