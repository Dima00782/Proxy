#include "httpparser.hpp"
#include <sstream>

HttpParser::Header HttpParser::parse(const std::string& request)
{
    std::string str;
    std::stringstream ss(request);

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

bool HttpParser::query_is_end(const std::string& request)
{
    auto pos = request.find("Content-Length: ");
    if (pos != std::string::npos) {
        pos += 16; // sizeof "Content-Length: "
        auto end_of_content_length = request.find("\r\n", pos);
        if (end_of_content_length != std::string::npos) {
            auto length_string = request.substr(pos, end_of_content_length - pos);
            auto content_length = std::stoul(length_string);

            auto end_of_http_header = request.find("\r\n\r\n");
            if (end_of_http_header != std::string::npos
                    && ((request.size() - end_of_http_header - 4 /* sizeof "\r\n\r\n" */) >= content_length)) {
                return true;
            }
        }
    }

    bool is_end_by_default = (request.size() > 4
                              && request[request.size() - 4] == '\r'
                              && request[request.size() - 3] == '\n'
                              && request[request.size() - 2] == '\r'
                              && request[request.size() - 1] == '\n');

    bool is_end_by_html = (request.size() > 7
                           && request[request.size() - 1] == '>'
                           && request[request.size() - 2] == 'l'
                           && request[request.size() - 3] == 'm'
                           && request[request.size() - 4] == 't'
                           && request[request.size() - 5] == 'h'
                           && request[request.size() - 6] == '/'
                           && request[request.size() - 7] == '<');

    bool is_end_by_html_with_eol = (request.size() > 9
                                    && request[request.size() - 1] == '\n'
                                    && request[request.size() - 2] == '\r'
                                    && request[request.size() - 3] == '>'
                                    && request[request.size() - 4] == 'L'
                                    && request[request.size() - 5] == 'M'
                                    && request[request.size() - 6] == 'T'
                                    && request[request.size() - 7] == 'H'
                                    && request[request.size() - 8] == '/'
                                    && request[request.size() - 9] == '<');

    return is_end_by_default || is_end_by_html || is_end_by_html_with_eol;
}
