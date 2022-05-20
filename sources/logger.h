#include <string>
#include <iostream>


using namespace std;

class Logger
{
private:
    bool _isEnable;
public:
    Logger(bool isEnable = true);
    ~Logger();

    void print(ostream& writeTo, std::string text);
    void printLog(std::string text);
    void printWarning(std::string text);
    void printError(std::string text);
    void forwardPrint(std::string text);
    void enable();
    void disable();
};

