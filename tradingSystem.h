#pragma once

#include <deque>
#include <string>
#include <vector>

class TradingAlgorithm {
public:
    TradingAlgorithm(double sw = 5, double lw = 20, double initB = 1000000.0)
        : _shortWindow(sw), _longWindow(lw), _initialBalance(initB), _currentBalance(initB) {}

    std::string execute(const std::string& json_string); 
        
    double getMeanProfit() const; 
    
private: 
    void simulateTrade(const std::string& prediction, double price, double quantity); 

    double calculateMovingAverage(int windowSize); 

    std::string analyzeMovingAverage(double shortma, double longma);

private:
    double _shortWindow;
    double _longWindow;
    std::deque<double> _priceHistory;
    std::vector<double> _tradeProfits; 
    double _initialBalance;
    double _currentBalance;
    double _position;
    std::string _id;
};

