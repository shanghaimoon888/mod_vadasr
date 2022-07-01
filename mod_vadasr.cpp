/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005/2012, Anthony Minessale II <anthm@freeswitch.org>
*
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C) 
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Anthony Minessale II <anthm@freeswitch.org>
* Neal Horman <neal at wanlink dot com>
*
*
* mod_vadasr.c -- Freeswitch asr Module
*
*/
#define DR_WAV_IMPLEMENTATION

#include <switch.h>
#include "dr_wav.h"
#include "opusvad.h"
#include "queue.h"
#include "xfasr.h"

#define VAD_EVENT_START "vad::start"
#define VAD_EVENT_STOP "vad::stop"
#define VAD_EVENT_ASR "vad::asr"

static switch_bool_t robot_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type);

#define MAX_VOICE_LEN 240000
#define MAX_VOICE_LEN_BASE64 645000
#define MAXFILES 8
#define TTS_MAX_SIZE 900
#define MAX_HZ_SIZE 240
#define VAD_VOICE_FRAMES 5 
#define VAD_SILINCE_FRAMES 50
#define VAD_HIS_LEN 100
#define VAD_ADD_FRAME_SIZE 5

static struct {
	char* appid;
	char* appkey;
} globals;


typedef struct robot_session_info {
	int index;
	int filetime;
	int fileplaytime;
	int nostoptime;
	int asrtimeout;
	int asr;
	int play, pos;
	int sos, eos, ec, count;
	int eos_silence_threshold;
	int final_timeout_ms;
	int silence_threshold; 
	int harmonic;
	int monitor;
	int lanid;
	int vadvoicems;
	int vadsilencems;
	int nslevel;
	switch_core_session_t *session;
	char taskid[32];
	char groupid[32];
	char telno[32];
	char userid[64];
	char callid[64];
	char orgi[64];
	char extid[64];
	char uuid[64];
	char uuidbak[64];
	char recordfilename[128];
	char para1[256];
	char para2[256];
	char para3[256];
	char filename[TTS_MAX_SIZE];
	char vadfilename[TTS_MAX_SIZE];
	short buffer[MAX_VOICE_LEN];
	drwav *fwav;
	drwav *fvadwav; 
	int state; // 0:silence 1:voice
	queue *vadqueue;
	int16_t *vadbuffer;
	int16_t framecount;
	switch_audio_resampler_t  *resampler;
	asr_session_t *asrsession;

} robot_session_info_t;


SWITCH_BEGIN_EXTERN_C

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vadasr_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_vadasr_load);
SWITCH_MODULE_DEFINITION(mod_vadasr, mod_vadasr_load, mod_vadasr_shutdown, NULL);
SWITCH_STANDARD_APP(robotasr_start_function);

SWITCH_MODULE_LOAD_FUNCTION(mod_vadasr_load)
{

	switch_application_interface_t *app_interface;
	char *cf = "asr.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	globals.appid = NULL;
	globals.appkey = NULL;

	if (switch_event_reserve_subclass(VAD_EVENT_START) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Robot Couldn't register subclass %s!\n",
			VAD_EVENT_START);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(VAD_EVENT_STOP) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Robot Couldn't register subclass %s!\n",
			VAD_EVENT_STOP);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(VAD_EVENT_ASR) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Robot Couldn't register subclass %s!\n",
			VAD_EVENT_ASR);
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
	}
	else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *)switch_xml_attr_soft(param, "name");
				char *val = (char *)switch_xml_attr_soft(param, "value");
				if (!strcmp(var, "appid")) {
					globals.appid = val;
				}
				if (!strcmp(var, "appkey")) {
					globals.appkey = val;
				}
			}
		}

		switch_xml_free(xml);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Robot enabled,appid=%s,appkey=%s\n", globals.appid, globals.appkey);

	// 为此模块增加app，调用名称即为 vad
	SWITCH_ADD_APP(app_interface, "vad", "vad", "ai robot", robotasr_start_function, "[<ACTION ><VAD_VOICE_FRAMES> <VAD_SILINCE_FRAMES> <NS_LEVEL>]", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

//  Called when the system shuts down
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vadasr_shutdown)
{
	switch_event_free_subclass(VAD_EVENT_START);
	switch_event_free_subclass(VAD_EVENT_STOP);
	switch_event_free_subclass(VAD_EVENT_ASR);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "myapplication disabled\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(robotasr_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel;
	robot_session_info_t *robot_info;

	// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "robot_start_function start\n");
	if (session == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			"FreeSWITCH is NULL! Please report to developers\n");
		return;
	}
	channel = switch_core_session_get_channel(session);
	if (channel == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			"No channel for FreeSWITCH session! Please report this "
			"to the developers.\n");
		return;
	}

	/* Is this channel already set? */
	bug = (switch_media_bug_t *)switch_channel_get_private(channel, "_robot_");

	/* If yes */

	if (bug != NULL) {

		/* If we have a stop remove audio bug */
		if (strcasecmp(data, "stop") == 0) {
			// robot_info = (robot_session_info_t *)switch_channel_get_private(channel, "_robotinfo_");
			switch_channel_set_private(channel, "_robot_", NULL);
			// process_close(robot_info);
			switch_core_media_bug_remove(session, &bug);
			return;
		}
		/* We have already started */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
			"Robot Cannot run 2 at once on the same channel!\n");
		return;
	}

	const char *action = NULL, *vadvoicems = NULL, *vadsilencems = NULL, *nslevel = NULL;
	char *argv[4] = { 0 };
	char *mycmd = NULL;

	if (!zstr(data)) {
		mycmd = switch_core_session_strdup(session, data);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0]) action = argv[0];
	if (argv[1]) vadvoicems = argv[1];
	if (argv[2]) vadsilencems = argv[2];
	if (argv[3]) nslevel = argv[3];

	if (!action || !vadvoicems || !vadsilencems || !nslevel) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "-ERR Missing Arguments\n");
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
		"action %s vadvoicems %s vadsilencems %s nslevel %s\n", action, vadvoicems, vadsilencems,
		nslevel);

	// 初始化变量， 一定记得要 free掉
	robot_info = (robot_session_info_t *)malloc(sizeof(robot_session_info_t));
	if (robot_info == NULL) return;
	robot_info->session = session;
	strcpy(robot_info->uuid, switch_core_session_get_uuid(robot_info->session));
	robot_info->vadvoicems = atoi(vadvoicems);
	robot_info->vadsilencems = atoi(vadsilencems);
	robot_info->nslevel = atoi(nslevel);

	status = switch_core_media_bug_add(session, "vmd", NULL, robot_callback, robot_info, 0, SMBF_READ_REPLACE, &bug);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Robot Failure hooking to stream\n");
		return;
	}
	switch_channel_set_private(channel, "_robot_", bug);
}

SWITCH_END_EXTERN_C


static switch_bool_t process_close(robot_session_info_t *rh)
{
	switch_channel_t *channel;

	rh->uuid[0] = 0;
	rh->index = -1;
	if (NULL != rh->fwav) { drwav_uninit(rh->fwav); }
	if (NULL != rh->fvadwav) { drwav_uninit(rh->fvadwav); }
	destroy_queue(rh->vadqueue);
	channel = switch_core_session_get_channel(rh->session);
	switch_channel_set_private(channel, "_robot_", NULL);
	delete rh->asrsession;
	free(rh);
	return SWITCH_TRUE;
}



void handle_event(const std::string & message, void *arg)
{
	switch_event_t *event;
	switch_status_t status;
	switch_event_t *event_copy;
	switch_channel_t *channel;

	robot_session_info_t *robot_info = (robot_session_info_t *)arg;
	channel = switch_core_session_get_channel(robot_info->session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "eventAsrText:%s\n", message.c_str());

	status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VAD_EVENT_ASR);
	if (status != SWITCH_STATUS_SUCCESS) { return; }

	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Asr-Text", message.c_str());
	switch_channel_event_set_data(channel, event);
	switch_event_fire(&event);
}

void handle_message(const std::string & message, void *arg)
{
	char middleText[500] = { 0 };
	//printf(">>> %s\n", message.c_str());
	cJSON* cjson_test = NULL;
	cJSON* cjson_action = NULL;
	cJSON* cjson_code = NULL;
	cJSON* cjson_data = NULL;
	cJSON* cjson_desc = NULL;
	cJSON* cjson_sid = NULL;
	cJSON* cjson_text = NULL;
	cJSON* cjson_segid = NULL;
	cJSON* cjson_cn = NULL;
	cJSON* cjson_st = NULL;
	cJSON* cjson_rt = NULL;
	cJSON* cjson_rt_item = NULL;
	cJSON* cjson_cw_item = NULL;
	cJSON* cjson_w_item = NULL;
	cJSON* cjson_type = NULL;
	cJSON* cjson_ws = NULL;
	cJSON* cjson_cw = NULL;
	cJSON* cjson_w = NULL;

	asr_session_t *asr = (asr_session_t *)arg;

	cjson_test = cJSON_Parse(message.c_str());
	cjson_action = cJSON_GetObjectItem(cjson_test, "action");
	cjson_code = cJSON_GetObjectItem(cjson_test, "code");
	cjson_data = cJSON_GetObjectItem(cjson_test, "data");
	cjson_desc = cJSON_GetObjectItem(cjson_test, "desc");
	cjson_sid = cJSON_GetObjectItem(cjson_test, "sid");

	if (strcmp(cjson_action->valuestring, "result") == 0 && strcmp(cjson_code->valuestring, "0") == 0 && strlen(cjson_data->valuestring) > 0)
	{
		cjson_text = cJSON_Parse(cjson_data->valuestring);
		cjson_segid = cJSON_GetObjectItem(cjson_text, "seg_id");
		cjson_cn = cJSON_GetObjectItem(cjson_text, "cn");
		cjson_st = cJSON_GetObjectItem(cjson_cn, "st");
		cjson_rt = cJSON_GetObjectItem(cjson_st, "rt");
		cjson_type = cJSON_GetObjectItem(cjson_st, "type");

		if (strcmp(cjson_type->valuestring, "0") == 0)
		{
			int rt_array_size = cJSON_GetArraySize(cjson_rt);
			//printf("rt_array_size:%d", rt_array_size);
			for (int i = 0; i < rt_array_size; i++)
			{
				cjson_rt_item = cJSON_GetArrayItem(cjson_rt, i);
				cjson_ws = cJSON_GetObjectItem(cjson_rt_item, "ws");

				int ws_array_size = cJSON_GetArraySize(cjson_ws);
				for (int j = 0; j < ws_array_size; j++)
				{
					cjson_cw_item = cJSON_GetArrayItem(cjson_ws, j);
					cjson_cw = cJSON_GetObjectItem(cjson_cw_item, "cw");

					int cw_array_size = cJSON_GetArraySize(cjson_cw);
					for (int k = 0; k < cw_array_size; k++)
					{
						cjson_w_item = cJSON_GetArrayItem(cjson_cw, k);
						cjson_w = cJSON_GetObjectItem(cjson_w_item, "w");
						//printf("w:%s", cjson_w->valuestring);
						if (strlen(asr->asr_text) <= BFLEN - 20)
						{
							strcat(asr->asr_text, cjson_w->valuestring);
						}
						else
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "content too long!!!!!!\n");
						}

					}

				}

			}		
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asrFinalResult:%s\n", asr->asr_text);

		}
		else
		{
			int rt_array_size = cJSON_GetArraySize(cjson_rt);
			//printf("rt_array_size:%d", rt_array_size);
			for (int i = 0; i < rt_array_size; i++)
			{
				cjson_rt_item = cJSON_GetArrayItem(cjson_rt, i);
				cjson_ws = cJSON_GetObjectItem(cjson_rt_item, "ws");

				int ws_array_size = cJSON_GetArraySize(cjson_ws);
				for (int j = 0; j < ws_array_size; j++)
				{
					cjson_cw_item = cJSON_GetArrayItem(cjson_ws, j);
					cjson_cw = cJSON_GetObjectItem(cjson_cw_item, "cw");

					int cw_array_size = cJSON_GetArraySize(cjson_cw);
					for (int k = 0; k < cw_array_size; k++)
					{
						cjson_w_item = cJSON_GetArrayItem(cjson_cw, k);
						cjson_w = cJSON_GetObjectItem(cjson_w_item, "w");
						strcat(middleText, cjson_w->valuestring);

					}
				}
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asrTempResult:%s\n", middleText);
		}
	}
	else if (strcmp(cjson_action->valuestring, "error") == 0 )
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asrErrorInfo:%s\n", cjson_desc->valuestring);

	}

}

static switch_bool_t robot_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	robot_session_info_t *robot_info;
	//	switch_codec_t *read_codec;
	switch_frame_t *frame;
	int flag;
	drwav_data_format format;// = { 0 };
	int16_t len;
	int voiceflagcount;
	int silenceflagcount;
	int nslevel;
	switch_event_t *event;
	switch_status_t status;
	switch_event_t *event_copy;
	char *recorddir = NULL;
	switch_codec_implementation_t read_impl;
	switch_channel_t *channel;

	
	
	robot_info = (robot_session_info_t *)user_data;
	if (robot_info == NULL) { return SWITCH_FALSE; }

	channel = switch_core_session_get_channel(robot_info->session);

	voiceflagcount = robot_info->vadvoicems / 20;
	silenceflagcount = robot_info->vadsilencems / 20;
	nslevel = robot_info->nslevel;

	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = 1;
	format.sampleRate = (drwav_uint32)8000;
	format.bitsPerSample = 16;

	recorddir = switch_core_get_variable_dup("record_prefix");

	switch (type) {

	case SWITCH_ABC_TYPE_INIT:
		sprintf(robot_info->filename, "%s%s.wav", recorddir, robot_info->uuid);
		robot_info->fwav = drwav_open_file_write(robot_info->filename, &format);
		if (!robot_info->fwav) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "full record openfile error %s\n",
				robot_info->filename);
		}

		SetConsoleOutputCP(CP_UTF8); //解决windows控制台输出中文乱码

		robot_info->vadqueue = create_queue();
		robot_info->state = 0;
		robot_info->framecount = 0;
		robot_info->fvadwav = NULL;

		//初始话语音识别
		robot_info->asrsession = new asr_session_t();
		robot_info->asrsession->handle_message = handle_message;		
		robot_info->asrsession->handle_event = handle_event;
		robot_info->asrsession->event_arg = robot_info;

		switch_core_session_get_read_impl(robot_info->session, &read_impl);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Read imp %u %u.\n", read_impl.samples_per_second, read_impl.number_of_channels);
		status = switch_resample_create(&robot_info->resampler, read_impl.actual_samples_per_second, 16000, 640, SWITCH_RESAMPLE_QUALITY, 1);
		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to allocate resampler\n");
		}

		break; 

	case SWITCH_ABC_TYPE_READ_REPLACE:
		
		if (robot_info->uuid[0] == 0) break;

		//获取语音数据
		frame = switch_core_media_bug_get_read_replace_frame(bug);

		//静音检测
		flag = silk_VAD_Get((const short*)frame->data);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "vad result %d\n", flag);

		//静音标志缓冲
		len = get_queue_length(robot_info->vadqueue);
		if (len == VAD_HIS_LEN) { delete_queue(robot_info->vadqueue); }
		insert_queue(robot_info->vadqueue, flag, NULL, 0);


		//语音检测
		if (getvadflagcount(robot_info->vadqueue, voiceflagcount, 1) && robot_info->state == 0) {

			robot_info->state = 1;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "+++++Speech Detected!!!+++++\n");

			//开启语音识别
			init_asr((char*)globals.appid, (char*)globals.appkey, robot_info->asrsession);

			sprintf(robot_info->vadfilename, "%s%s_%d.wav", recorddir, robot_info->uuid, robot_info->framecount);
			robot_info->fvadwav = drwav_open_file_write(robot_info->vadfilename, &format);
			if (!robot_info->fvadwav) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "vad open file error %s\n",
					robot_info->vadfilename);
				strcpy(robot_info->vadfilename, "");
				//break;
			}
						
			
			status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VAD_EVENT_START);
			if (status != SWITCH_STATUS_SUCCESS) { break; }
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Vad-Status", "start");
			switch_channel_event_set_data(channel, event);	
			/*if ((switch_event_dup(&event_copy, event)) != SWITCH_STATUS_SUCCESS) { break; }
			switch_core_session_queue_event(robot_info->session, &event);
			switch_event_fire(&event_copy);*/
			switch_event_fire(&event);
		}

		//静音检测
		if (getvadflagcount(robot_info->vadqueue, silenceflagcount, 0) && robot_info->state == 1) {
			robot_info->state = 0;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
				"-----Silence Detected,Stop Recording!!! FileName:%s.-----\n", robot_info->vadfilename);
			if (robot_info->fvadwav) { drwav_uninit(robot_info->fvadwav); }
			robot_info->fvadwav = NULL;

			status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VAD_EVENT_STOP);
			if (status != SWITCH_STATUS_SUCCESS) { break; }
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Vad-Status", "stop");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Vad-RecordFile", robot_info->vadfilename);
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		    
			//发送Asr结束标记
			send_end(robot_info->asrsession);
		}

		//录音-vad部分
		if (robot_info->fvadwav) { drwav_write_pcm_frames(robot_info->fvadwav, frame->samples, frame->data); }
		//完整部分
		if (robot_info->fwav){ drwav_write_pcm_frames(robot_info->fwav, frame->samples, frame->data); }
		robot_info->framecount++;
		
		//检测到语音时发送语音数据包
		if(robot_info->state == 1)
		{
			//上采样至16K
			switch_resample_process(robot_info->resampler, (int16_t *)frame->data, frame->datalen);
			send_data(robot_info->asrsession, (char*)robot_info->resampler->to, robot_info->resampler->to_len);
		}
		break;

	case SWITCH_ABC_TYPE_CLOSE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SWITCH_ABC_TYPE_CLOSE\n");
		send_end(robot_info->asrsession);
		thrd_join(robot_info->asrsession->thr, NULL);
		thrd_detach(robot_info->asrsession->thr);
		mtx_destroy(&robot_info->asrsession->mutex);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "the asr thread closed!!!\n");

		if (robot_info->resampler)
		{
			switch_resample_destroy(&robot_info->resampler);
		}
		process_close(robot_info);
		break;
	default:
		break;
	}

	switch_safe_free(recorddir);
	return SWITCH_TRUE;
}

