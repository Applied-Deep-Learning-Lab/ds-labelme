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

#include <fcntl.h>
#include <sys/ioctl.h>

#include "client.h"

#include "nvdsinfer_custom_impl.h"





using namespace std;

vector <string> bboxJsons;

Client::Client(std::string configPath)
{

    ifstream configFile(configPath);

    _connected = false;

    if (!configFile)
    {
        perror( "No \"ip.config\" file" );
        return;
    }
    string ip;
    int port;

    

    getline(configFile, ip, ':');
    configFile >> port;
    cout << "Connect to host " << ip << ":" << port << endl;
    /*объявляем сокет*/
    _socket = socket( AF_INET, SOCK_STREAM, 0 );
    if(_socket < 0)
    {
            perror( "Error calling socket" );
            return;
    }

    /*соединяемся по определённому порту с хостом*/
    _host.sin_family = AF_INET;
    _host.sin_port = htons(port);
    _host.sin_addr.s_addr = inet_addr(ip.c_str());
    
    _flags = fcntl(_socket , F_GETFL, 0);

    return;
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
    int result = connect( _socket, ( struct sockaddr * )&_host, sizeof( _host ) );
    if( result )
    {
            perror( "Error calling connect" );
            return;
    }
    _connected = true;
    
}

int Client::imageRequest(){
    char server_reply[200];
    // if( recv(_socket, server_reply , 6000 , 0) < 0)
	// {
	// 	puts("recv failed");
	// }

    int len = recv(_socket, server_reply , 200 , 0);
    if(len <= 0){
        return -1;
    }


    server_reply[len] = 0;
    // string req = server_reply;
    // string startPart = "<getSourceImage frameNum=\"";
    // string endPart = "\"/>";



    return 1;
}



void Client::sendRaw(std::string message){

    // if(!_connected){
    //     //connectToHost();
    // }
    int result = send( _socket, message.c_str(), message.size(), 0);
    if( result <= 0 )
    {
        //_connected = false;
        perror( "Error calling send" );
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

    strTmp += (string)"." + to_string(milliseconds);

    addMeta(key, strTmp);
}

// void Client::addMetaTime(string key, const int64_t value){
    
//     char arcString [100];
//     std::string strTmp;

//     if (strftime (arcString, 100, "%Y-%m-%d %H:%M:%S", gmtime(&value)) != 0)
//     {
//         strTmp = arcString;
//     }
//     else
//     {
//         perror("Time format error");
//     }
    
//     addMeta(key, strTmp);
// }



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