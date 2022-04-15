#include <sys/types.h>
#include <sys/socket.h>
/* hton, ntoh и проч. */
#include <arpa/inet.h>
#include <memory.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <ctime>
#include <chrono>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "client.h"



#include "nvdsinfer_custom_impl.h"

#define CONNECTION_LOST 0
#define NO_CONNECTION -1
// #define ORDER_EMPTY -1

#define DIGITS_IN_MILLISECONDS 3

#define MAX_REPLY_SIZE 200
#define RECONNECT_TIME_MILLISECONDS 1000


using namespace std;

const char* zerosLine = "000";
vector <string> bboxJsons;



Client::Client(const NvDsSocket& params)
{

    _connected = false;
    _name = params.name;
    _port = params.port;
    _ip = params.ip;

    return;
}

Client::~Client(){
    close(_socket);
}

void Client::blockingMode(){
    
    fcntl(_socket , F_SETFL, _flags);
    int imode = 0;
    ioctl(_socket , FIONBIO, &imode);
}

void Client::streamMode(){
    fcntl(_socket , F_SETFL, _flags | O_NONBLOCK);
    int imode = 1;
    ioctl(_socket , FIONBIO, &imode);
}

void Client::connectToHost(){
    
    _socket = socket( AF_INET, SOCK_STREAM, 0 );
    if(_socket < 0)
    {
            perror( "Error calling socket" );
            return;
    }

    /*соединяемся по определённому порту с хостом*/
    _host.sin_family = AF_INET;
    _host.sin_port = htons(_port);
    _host.sin_addr.s_addr = inet_addr(_ip.c_str());
    
    _flags = fcntl(_socket , F_GETFL, 0);

    _lastConnectTime = std::chrono::steady_clock::now();
    int result = connect( _socket, ( struct sockaddr * )&_host, sizeof( _host ) );
    if( result )
    {
            return;
    }
    _connected = true;
    sendRaw("<head dataType=\"" + _name + "\" version=1 supplierTypeName=\"DeepStream\"/>");
    cout << _name << " connected (" << _ip << ":" << _port << ")" << endl;
    
}

RecvestResult Client::recvest(){
    char server_reply[MAX_REPLY_SIZE];

    static string recvbuf = "";
    static string inTag = "";
    static int order = 0;
    static bool isTagInside = false;

    RecvestResult recvRes;

    if(!_connected){
        recvRes.success = false;
        return recvRes;
    }

    int result = recv(_socket, server_reply , MAX_REPLY_SIZE - 1 , 0);
    cout << "recv: " << result << endl;
    if(result == CONNECTION_LOST || result == NO_CONNECTION){
        connectionLost();
        recvRes.success = false;
        return recvRes;
    }

    unsigned int length = result;
    server_reply[length] = 0;

    recvRes.success = true;
    recvRes.message = (string)(char*)(server_reply + 4);
    return recvRes;
    
    
    // string req = server_reply;
    // string startPart = "<getSourceImage frameNum=\"";
    // string endPart = "\"/>";
    // getSourceImage frameNum=1371219 /

}

void Client::connectionLost(){
    cerr << _name << " lost(" << _ip << ":" << _port << ")" << endl;
    close(_socket);
    _connected = false;
}


void Client::sendRaw(std::string message){

    if(!_connected){
        return;
    }
    int result = send( _socket, message.c_str(), message.size(), 0);
    if( result <= 0 )
    {
        connectionLost();
    }
}

void Client::reconnect(){
    if(_connected){
        return;
    }
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastConnectTime);

    if(duration.count() > RECONNECT_TIME_MILLISECONDS){
        // cout << "Reconnect" << endl;
        connectToHost();
        _lastConnectTime = std::chrono::steady_clock::now();
    }
    
}

vector<string> metaJsons;

void Client::addBBox(const BBbox& box){
    bboxJsons.push_back((string)"{" +
        "\"label\": " + "\"" + box.label + "\", " +
        " \"line_color\": \"null\", \"fill_color\": \"null\", " +
        "\"points\":[[" + to_string(box.left) + ", " + 
        to_string(box.top) + "], " + 
        "[" + to_string(box.left + box.width) + ", " +
        to_string(box.top + box.height) + "]], " + 
        "\"shape_type\": \"rectangle\", " + 
        "\"object_id\": " + to_string(box.id) + 
    "}");
}

void Client::addMetaTime(string key, const uint64_t value){
    
    char arcString [100];
    std::string strTmp;
    time_t frameTime = value / 1000000000;
    int milliseconds = (value / 1000000) % 1000;

    if (strftime(arcString, 100, "%Y-%m-%d %H:%M:%S", gmtime(&frameTime)) != 0)
    {
        strTmp = arcString;
    }
    else
    {
        perror("Time format error");
    }
    
    string millisecondsText = to_string(milliseconds);

    // Добавление нулей к миллисикундам
    millisecondsText = (zerosLine + millisecondsText.length()) + millisecondsText;

    strTmp += (string)"." + millisecondsText; 

    addMeta(key, strTmp);
}


void Client::addMeta(string key, int64_t value){
    metaJsons.push_back("\"" + key + "\" : " + to_string(value));
}

void Client::addMeta(string key, uint64_t value){
    metaJsons.push_back("\"" + key + "\" : " + to_string(value));
}

void Client::addMeta(string key, string value){
    metaJsons.push_back("\"" + key + "\" : \"" + value + "\"");
}

void Client::addMeta(string key){
    metaJsons.push_back("\"" + key + "\" : {}");
}

void Client::sendMessage(){

    string toSend = "{";

    for (string metaJson : metaJsons)
    {
        toSend += metaJson + ",\n";
    }

    toSend += "\"shapes\" : [";

    for (string bboxJson : bboxJsons)
    {
        toSend += bboxJson + ",\n";
    }
    if(bboxJsons.size() > 0){
        toSend.erase(toSend.length() - 2);
    }

    toSend += "]}\n";

    int len = toSend.length() + 4;
    char* bytelen = (char*)(&len);
    toSend = (string)"" + bytelen[0] + bytelen[1] + bytelen[2] + bytelen[3] + toSend; 

    sendRaw(toSend);
    metaJsons.clear();
    bboxJsons.clear();
}