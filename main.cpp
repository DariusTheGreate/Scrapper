#include "service.h"

#include <csignal>
#include <execinfo.h>
#include <iostream>

void signalHandler(int sig) {
    void* array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    std::cerr << "Error: signal " << sig << std::endl;
    backtrace_symbols_fd(array, size, STDERR_FILENO);
}

int main()
{
    /*if (signal(SIGSEGV, signalHandler) == SIG_ERR) {
        std::cerr << "Failed to install SIGSEGV handler" << std::endl;
    }*/

    Service s("exchange_info.json");
    s.run();
    return 0;
}

