#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include <netinet/in.h>

struct BBbox
{
    float left, top, width, height, confidence;
    uint64_t id;
    std::string label;
};


class Client{

public:
    Client(std::string configPath);
    
    void connectToHost();

    void blockingMode();
    void streamMode();
    int imageRequest();

    void addMeta(std::string key);
    void addMeta(std::string key, std::string value);
    void addMeta(std::string key, int64_t value);
    void addMeta(std::string key, uint64_t value);
    void addMetaTime(std::string key, uint64_t value);
    void addBBox(const BBbox& box);

    void sendRaw(std::string message);

    void sendMessage();

private:
    int _socket;
    sockaddr_in _host;
    bool _connected;
    int _flags;
};




#endif