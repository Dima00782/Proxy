#include "proxy.hpp"
#include <iostream>

int main()
{
    Logger l;
    Proxy proxy(12345, l);
    proxy.start();

    return 0;
}
