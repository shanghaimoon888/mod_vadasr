#ifndef XFASR_H
#define XFASR_H

#include "easywsclient.hpp"
#include "tinycthread.h"

using easywsclient::WebSocket;

#ifdef __cplusplus
extern "C"
{
#endif


#define BFLEN 500

typedef void(*on_message)(const std::string& message, void *arg);
typedef void(*on_close)(const std::string& asrtext, void *arg);

typedef struct asr_session {

    WebSocket::pointer ws;
    on_message handle_message;
	on_close handle_event;
	void *event_arg;//有问题不用
	thrd_t thr;
	mtx_t mutex;
	char *asr_text;

} asr_session_t;


WebSocket::pointer init_asr(char* appid, char* key, asr_session_t* asr);
int send_data(asr_session_t* asr, char* buf, int buflen);
int send_end(asr_session_t* asr);

void deinit_asr(asr_session_t* asr);



#ifdef __cplusplus
}
#endif

#endif