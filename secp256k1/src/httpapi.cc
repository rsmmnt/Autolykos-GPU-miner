#include "../include/httpapi.h"
#include <sstream>
using namespace httplib;

void HttpApiThread(std::vector<double>* hashrates, std::vector<std::pair<int,int>>* props)
{
    Server svr;

    svr.Get("/", [&](const Request& req, Response& res) {
        
        std::unordered_map<std::pair<int,int>, double> hrMap;
        for(int i = 0; i < (*hashrates).size() ; i++)
        {
            hrMap[(*props)[i]] = (*hashrates)[i];
        }
        
        
        
        std::stringstream strBuf;
        strBuf << "{ \"gpus\":" << (*hashrates).size() << " , ";
        strBuf << " \"devices\" : { " ;
        /*
        strBuf << "\"hashrates\": [ ";
        double totalHr = 0;
        for(int i = 0; i < (*hashrates).size(); i++)
        {
            strBuf << (*hashrates)[i];
            if(i < (*hashrates).size() - 1) strBuf << " , ";
            totalHr += (*hashrates)[i];
        } 
        strBuf << " ] , ";
        */
        //strBuf << "\"total\": " << totalHr << " , " ;
        
        // NVML data if available
        double totalHr = 0;
        nvmlReturn_t result;
        result = nvmlInit();
        if (result == NVML_SUCCESS)
        { 
            unsigned int devcount;
            result = nvmlDeviceGetCount(&devcount);
            //std::stringstream temps;
            //std::stringstream wattages;
            bool first = true;
            for(int i = 0; i < devcount; i++)
            {
                std::stringstream deviceInfo;
                nvmlDevice_t device;
                result = nvmlDeviceGetHandleByIndex(i, &device);
                if(result == NVML_SUCCESS)
                {
                    

                    nvmlPciInfo_t pciInfo;
                    result = nvmlDeviceGetPciInfo ( nvmlDevice_t device, &pciInfo );
                    if(result != NVML_SUCCESS) { continue; }

                    if(first)
                    {
                        first = false;
                    }
                    else
                    {
                        deviceInfo << " , ";        
                    }

                    deviceInfo << " \"gpu" << i << "\" : { ";
                    deviceInfo << " \"pciid\" : \"" << pciInfo.bus << "." << pciInfo.device << "\" , ";
                    double hrate;
                    if( hrMap[std::make_pair(pciInfo.bus, pciInfo.device)] != nullptr)
                    {
                        hrate = hrMap[std::make_pair(pciInfo.bus, pciInfo.device)];
                        deviceInfo << " \"hashrate\" : " << hrate << " , ";
                        totalHr += hrate;
                    }

                    unsigned int temp;
                    unsigned int power;
                    result = nvmlDeviceGetPowerUsage ( device, &power );
                    result = nvmlDeviceGetTemperature ( device, NVML_TEMPERATURE_GPU, &temp );
                    deviceInfo << " \"power\" : " << power/1000 << " , ";
                    deviceInfo << " \"temperature\" : " << temp << " }";
                    strBuf << deviceInfo.str();
                }
            }

            strBuf << " } , \"total\": " << totalHr  ;


            result = nvmlShutdown();
        }

        strBuf << " } ";


        std::string str = strBuf.str();
        res.set_content(str.c_str(), "text/plain");
    });
    


    svr.listen("0.0.0.0", 32067);
}