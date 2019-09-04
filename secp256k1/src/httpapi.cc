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
            strBuf << (*hashrates)[i];
            if(i < (*hashrates).size() - 1) strBuf << " , ";
            totalHr += (*hashrates)[i];
        } 
        strBuf << " ] , ";
        strBuf << "\"total\": " << totalHr ;
        
        // NVML data if available
        nvmlReturn_t result;
        result = nvmlInit();
        if (result == NVML_SUCCESS)
        { 
            std::stringstream temps;
            std::stringstream wattages;
            bool first = true;
            for(int i = 0; i < (*hashrates).size(); i++)
            {
                nvmlDevice_t device;
                result = nvmlDeviceGetHandleByIndex(i, &device);
                if(result == NVML_SUCCESS)
                {
                    unsigned int temp;
                    unsigned int power;
                    result = nvmlDeviceGetPowerUsage ( device, &power );
                    result = nvmlDeviceGetTemperature ( device, 0, &temp );
                    if(first)
                    {
                        first = false;
                    }
                    else
                    {
                        temps << " , ";
                        wattages << " , ";        
                    }
                    temps << temp;
                    wattages << power;
                }
            }

            strBuf << " , \"temps\": [ " << temps.str() << " ] ,";
            strBuf << " \"wattages\": [ " << wattages.str() << " ] ";


            result = nvmlShutdown();
        }
        else
        {
            strBuf << " } ";
        }


        std::string str = strBuf.str();
        res.set_content(str.c_str(), "text/plain");
    });
    


    svr.listen("0.0.0.0", 32067);
}