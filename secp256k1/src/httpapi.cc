#include "../include/httpapi.h"

using namespace httplib;

void HttpApiThread(std::vector<double>* hashrates)
{
    Server svr;

    svr.Get("/", [&](const Request& req, Response& res) {
        res.set_content("Main page", "text/plain");
    });    

    svr.listen("0.0.0.0", 32067);
}