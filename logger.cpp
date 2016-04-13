#include "logger.hpp"
#include <iostream>
#include <string>

Logger::Logger()
    : out(nullptr)
{}

Logger::Logger(std::shared_ptr<std::ostream> &stream)
    : out(stream)
{}

std::ostream& Logger::get_stream(LOG_LEVEL) {
    if (out == nullptr)
    {
        return std::cout;
    }

    return *out;
}
