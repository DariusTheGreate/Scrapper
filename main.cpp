#include "service.h"

int main()
{
    Service s("exchange_info.json");
    s.run();
    return 0;
}

