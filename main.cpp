#include "proxy.hpp"
#include <iostream>

int main()
{
    Logger l;
    Proxy proxy(5555, l);
    proxy.start();

    return 0;
}
