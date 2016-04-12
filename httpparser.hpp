#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <vector>

class HttpParser
{
public:
    enum class Method
    {
        GET,
        HEAD,
        POST,
        UNKNOWN
    };

    enum class Version
    {
        HTTP_1_1,
        HTTP_1_0,
        UNKNOWN
    };

    struct Header
    {
        Header()
            : method(Method::UNKNOWN)
            , version(Version::UNKNOWN)
        {}

        operator bool() const
        {
            return method != Method::UNKNOWN && version != Version::UNKNOWN && !URI.empty();
        }

        Method method;
        Version version;
        std::string URI;
    };

public:
    static Header parse(const std::vector<char>& request);
};

#endif // HTTP_PARSER_HPP
