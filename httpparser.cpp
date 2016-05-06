#include "httpparser.hpp"
#include <sstream>

HttpParser::Header HttpParser::parse(const std::vector<char>& request)
{
    std::string str(request.data());
    std::stringstream ss(str);

    Header header;

    if (ss >> str)
    {
        if (str == "GET")
        {
            header.method = Method::GET;
        }
        else if (str == "HEAD")
        {
            header.method = Method::HEAD;
        }
        else if (str == "POST")
        {
            header.method = Method::POST;
        }
        else
        {
            if (str.size() >= 3)
            {
                header.method = Method::UNKNOWN;
            }

            return header;
        }

        if (ss >> str)
        {
            str = str.substr(7); // erase http://
            if (str.back() == '/')
            {
                str.pop_back(); // erase the last slash
            }
            header.URI = str;
        }
        else
        {
            return header;
        }

        if (ss >> str)
        {
            if (str == "HTTP/1.0")
            {
                header.version = Version::HTTP_1_0;
            }
            else if (str == "HTTP/1.1")
            {
                header.version = Version::HTTP_1_1;
            }
            else
            {
                return header;
            }
        }
    }

    return header;
}
