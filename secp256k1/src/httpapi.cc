#include "../include/httpapi.h"
#include <sstream>
using namespace httplib;

void HttpApiThread(std::vector<double>* hashrates)
{
    Server svr;

    svr.Get("/", [&](const Request& req, Response& res) {
        std::stringstream strBuf;
        strBuf << "{ \"gpus\":" << (*hashrates).size() << " , ";
        strBuf << "\"hashrates\": [ ";
        double totalHr = 0;
        for(int i = 0; i < (*hashrates).size(); i++)
        {
            strBuf << (*hashrates)[i] << " , ";
            totalHr += (*hashrates)[i];
        } 
        strBuf << " ] , ";
        strBuf << "\"total\": " << totalHr << " }";
        std::string str = strBuf.str();
        res.set_content(str.c_str(), "text/plain");
    });    


    svr.listen("0.0.0.0", 32067);
}