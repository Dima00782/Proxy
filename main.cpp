#include "proxy.hpp"
#include <iostream>

int main()
{
    Logger l;
    Proxy proxy(7777, l);
    proxy.start();

    return 0;
}
