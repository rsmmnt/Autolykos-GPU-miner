#include "../include/easywsclient.hpp"
#include "../include/websocket.h"
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
#pragma comment( lib, "ws2_32" )

#endif
#include <assert.h>
#include <string>
using easywsclient::WebSocket;




WebSocketComm :: WebSocketComm(const char * wsUrl, info_t * _info)
{
    #ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc) {
        printf("WSAStartup Failed.\n");
    }
    #endif

    url = std::string(wsUrl);
    ws = WebSocket::from_url(url);
    if(!ws)
    {
        LOG(ERROR) << "Can't connect to websocket";
    }
    assert(ws);
    info = _info;
    //ws.send("GETBLOCK");
}

WebSocketComm :: ~WebSocketComm()
{
    ws->close();
    delete ws;
    #ifdef _WIN32
        WSACleanup();
    #endif  

}


int WebSocketComm :: check()
{
    io_mutex.lock();
    if(ws)
    {
        if(ws->getReadyState() != WebSocket::CLOSED)
        {
            ws->poll();
         //ws->dispatch(handle_message);
            //VLOG(1) << "WebSocket polling";
        }
    }
    else
    {
        LOG(INFO) << "WebSocket disconnected, trying to reconnect...";
        delete ws;
        ws = WebSocket::from_url(url);
        if(!ws)
        {
            io_mutex.unlock();
            return EXIT_FAILURE;
        }
        // ws->send("GETBLOCK");
        ws->poll();
    }
    
    // can't pass pointer to this to lambda unfortunately
    std::string _lastMessage = lastMessage;
    
    info_t* _info = info;
    bool _checkPK = checkPK;
    ws->dispatch([&_info, &_lastMessage,&_checkPK](const std::string& message)
    {
        VLOG(1) << "Websocket got message:" << message;
        json_t newreq(0, REQ_LEN);
        jsmn_parser parser;
        int changed = 0;
        int boundChanged = 0;
        if(message != _lastMessage)
        {
        //lastMessage = message;
            strcpy(newreq.ptr,message.c_str());
            VLOG(1)  << " Message c string" << newreq.ptr;
            ToUppercase(newreq.ptr);
            jsmn_init(&parser);
            jsmn_parse(&parser, newreq.ptr, newreq.len, newreq.toks, REQ_LEN);

            if(_checkPK)
            {   
                if (strncmp(_info->pkstr, newreq.GetTokenStart(PK_POS), PK_SIZE_4))
                {
                
                LOG(ERROR) << "Generated and received public keys do not match\n";
                
                
                fprintf(
                stderr, "ABORT:  Public key derived from your secret key:\n"
                "        0x%.2s",
                _info->pkstr
                );

                for (int i = 2; i < PK_SIZE_4; i += 16)
                {
                    fprintf(stderr, " %.16s", _info->pkstr + i);
                }
            
                fprintf(
                    stderr, "\n""        is not equal to the expected public key:\n"
                    "        0x%.2s", newreq.GetTokenStart(PK_POS)
                );

                for (int i = 2; i < PK_SIZE_4; i += 16)
                {
                    fprintf(stderr, " %.16s", newreq.GetTokenStart(PK_POS) + i);
                }

                fprintf(stderr, "\n");
                }
            }

            int len = newreq.GetTokenLen(BOUND_POS);

            _info->info_mutex.lock();
            
            HexStrToBigEndian(
            newreq.GetTokenStart(MES_POS), newreq.GetTokenLen(MES_POS),
            _info->mes_h, NUM_SIZE_8
            );

            char buf[NUM_SIZE_4 + 1];

            DecStrToHexStrOf64(newreq.GetTokenStart(BOUND_POS), len, buf);
            HexStrToLittleEndian(buf, NUM_SIZE_4, _info->bound_h, NUM_SIZE_8);

            _info->info_mutex.unlock();

            ++(_info->blockId);
            LOG(INFO) << "Got new block in main thread";

            _lastMessage = message;         
        }





    });

    lastMessage = _lastMessage;
    
    io_mutex.unlock();
    return EXIT_SUCCESS;
}
            


void WebSocketComm :: send_solution( const char * to,
                    const char * pkstr,
                    const uint8_t * w,
                    const uint8_t * nonce,
                    const uint8_t * d)
{
    uint32_t len;
    uint32_t pos = 0;

    char request[JSON_CAPACITY];

    //====================================================================//
    //  Form message to post
    //====================================================================//
    strcpy(request + pos, "{\"pk\":\"");
    pos += 7;

    strcpy(request + pos, pkstr);
    pos += PK_SIZE_4;

    strcpy(request + pos, "\",\"w\":\"");
    pos += 7;

    BigEndianToHexStr(w, PK_SIZE_8, request + pos);
    pos += PK_SIZE_4;

    strcpy(request + pos, "\",\"n\":\"");
    pos += 7;

    LittleEndianToHexStr(nonce, NONCE_SIZE_8, request + pos);
    pos += NONCE_SIZE_4;

    strcpy(request + pos, "\",\"d\":");
    pos += 6;

    LittleEndianOf256ToDecStr(d, request + pos, &len);
    pos += len;

    strcpy(request + pos, "e0}\0");    
    std::string data(request);
    io_mutex.lock();
    ws->send(data);
    io_mutex.unlock();
}

        


