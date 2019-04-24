#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#include "../include/easywsclient.hpp"
#include "../include/easylogging++.h"
#include "../include/request.h"
#include "../include/conversion.h"
#include "../include/definitions.h"
#include "../include/jsmn.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
//#pragma comment( lib, "ws2_32" )
/*#ifndef _WINSOCK2API
#include <WinSock2.h>
#endif
*/
#endif
#include <assert.h>
#include <string>
using easywsclient::WebSocket;


class WebSocketComm
{
    private:
        std::mutex io_mutex;
        WebSocket::pointer ws = NULL;
        info_t * info;
        std::string url;
        std::string lastMessage;
    public:      
        bool checkPK = false;

        WebSocketComm(const char * wsUrl, info_t * _info);

        ~WebSocketComm();

        int check();
        
       // void handle_message(const std::string & message);

        void send_solution( const char * to,
                            const char * pkstr,
                            const uint8_t * w,
                            const uint8_t * nonce,
                            const uint8_t * d);       
};

#endif