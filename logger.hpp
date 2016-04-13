#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <ostream>
#include <memory>
#include <string>

class Logger
{
public:
    enum class LOG_LEVEL
    {
        INFO
    };

public:
    Logger();

    Logger(std::shared_ptr<std::ostream>& stream);

    std::ostream& get_stream(LOG_LEVEL);

private:
    std::shared_ptr<std::ostream> out;
};

#endif // LOGGER_HPP
