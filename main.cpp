#include "proxy.hpp"
#include "scheduler.hpp"

int main()
{
    Logger l;
    auto scheduler = std::make_unique<Scheduler>();
    Proxy proxy(5555, l, std::move(scheduler));
    proxy.start();

    return 0;
}
