#ifndef HTTPAPI_H
#define HTTPAPI_H

#include "httplib.h"
#include <vector>
#include <string>
#include <nvml.h>

void HttpApiThread(std::vector<double>* hashrates);


#endif