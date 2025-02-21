#pragma once
#include <atomic>

// todo better nameing
struct Event
{
    bool isDone()
    {
        return _done.test();
    }

    void endEvent()
    {
        _done.test_and_set();
    }

    void restartEvent()
    {
        _done.clear();
    }

    std::atomic_flag _done;
};

