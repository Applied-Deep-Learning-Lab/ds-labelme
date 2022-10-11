#include "logger.h"

#include <chrono>

const string colorYellow = "\e[33m";
const string colorGreen = "\e[32m";
const string colorRed = "\e[31m";
const string colorDefault = "\e[0m";

Logger::Logger(bool isEnable)
{
    _isEnable = isEnable;
}

Logger::~Logger()
{

}



void Logger::print(ostream &writeTo, std::string text){
    
    if(_isEnable){
        char buf[100];
        auto time_now = chrono::system_clock::now();

        time_t time_now_t = chrono::system_clock::to_time_t(time_now);

        strftime(buf, 100, "(%Y-%m-%d %H:%M:%S): ", gmtime(&time_now_t));

        writeTo << buf << text << colorDefault << std::endl;
    }
}

void Logger::printLog(std::string text){
    print(cout, colorGreen + "LOG: " + text);
}

void Logger::printError(std::string text){
    print(cerr, colorRed + "ERROR: " + text);
}

void Logger::printWarning(std::string text){
    print(cerr, colorYellow + "WARNING: " + text);
}

void Logger::enable(){
    _isEnable = true;
}

void Logger::disable(){
    _isEnable = false;
}