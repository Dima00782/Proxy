## Proxy server

### build:
first of all you must build sfml library (https://github.com/SFML/SFML) and then just run:

$ g++ *.cpp -g -std=c++14 -Wall -I/path/to/SFML/include -L/path/to/SFML/build/lib -o proxy -lsfml-system -lsfml-network

### run:
$ LD_LIBRARY_PATH=/path/to/SFML/build/lib ./proxy

### usage and test:
You can test proxy server with browser and command line

#### For firefox browser:
1) type in the address line : about:config
2) then choose network.http.version
3) and set it to "1.0" because the proxy works with http version 1.0

4) go to about:preferences#advanced and choose tab Network
5) click on Setting and choose "Manual proxy configuration" and fill address and port of the proxy
6) All complete! Now you can see old simple site which works with HTTP 1.0

#### For command line:
You can test proxy with curl
Just type in console:

$ http_proxy=PROXY_ADDRESS:PORT curl -i -0 -H "Accept: application/json" -H "Content-Type: application/json" -X GET http://ya.ru

P.s. ya.ru for example
