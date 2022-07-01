#include "xfasr.h"
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include "md5.h"
#include "url.h"
#include "base64.h"
#include "tinycthread.h"



int ws_thread(void * arg)
{
	asr_session_t* asr = (asr_session_t*)arg;
	
	while (asr->ws->getReadyState() != WebSocket::CLOSED) {
		asr->ws->poll();
		asr->ws->dispatch(asr->handle_message);
	}

	asr->handle_event(asr->asr_text, asr->event_arg);
	deinit_asr(asr);

	
}

WebSocket::pointer init_asr(char* appid, char* key, asr_session_t* asr)
{
	WebSocket::pointer ws = NULL;
	char tempStr[200];
	unsigned char digest[EVP_MAX_MD_SIZE] = { '\0' };
	unsigned int digest_len = 0;
	char hex[36];
	char url[200];
	char outdata[200] = { 0 };
	int outlen;

	int64_t timeStamp = time(NULL);
	std::sprintf(tempStr, "%s%d", appid, timeStamp);
	md5((unsigned char*)tempStr, hex);
	HMAC(EVP_sha1(), key, strlen(key), (unsigned char*)hex, strlen(hex), digest, &digest_len);

	base64_encode_block((char*)digest, digest_len, (char*)outdata, &outlen);
	urlencode(outdata);
	std::sprintf(url, "ws://rtasr.xfyun.cn/v1/ws?appid=%s&ts=%d&signa=%s", appid, timeStamp, outdata);
	std::string wsUrl = url;
	ws = WebSocket::from_url(wsUrl, "", asr);
	if (ws)
	{
		asr->ws = ws;
		asr->asr_text = new char[BFLEN]{0};
		memset(asr->asr_text, 0, sizeof(asr->asr_text));
		if (thrd_create(&asr->thr, ws_thread, (void*)asr) == thrd_success){}
		mtx_init(&asr->mutex, mtx_plain);
	}

	return ws;
}

int send_data(asr_session_t* asr, char* buf, int buflen)
{
	mtx_lock(&asr->mutex);
	if (!asr->ws || buf==NULL)
	{
		mtx_unlock(&asr->mutex);
		return -1;
	}
	
	std::vector<uint8_t> data(buf, buf + buflen);
	
	if ((asr->ws->getReadyState() != WebSocket::CLOSED))
	{
		asr->ws->sendBinary(data);
	}
	else
	{
		mtx_unlock(&asr->mutex);
		return -1;
	}

	mtx_unlock(&asr->mutex);
	return buflen;
}

int send_end(asr_session_t* asr)
{
	char *endbuf = "{\"end\": true}";

	return send_data(asr, endbuf, strlen(endbuf));
}

void deinit_asr(asr_session_t* asr)
{
	try {
		mtx_lock(&asr->mutex);
		asr->ws->close();
		delete asr->ws;
		asr->ws = nullptr;
		delete asr->asr_text;
		mtx_unlock(&asr->mutex);
	}
	catch(...)
	{
		printf("get exception\n");
	}

}