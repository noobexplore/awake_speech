/*实现.h的CPP文件*/
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "../include/msp_cmn.h"
#include "../include/msp_errors.h"
#include "../include/msp_types.h"
//语音识别命令SDK
#include "../include/qisr.h"
//语音唤醒命令SDK
#include "../include/qivw.h"
#include "./speech_recognize.h"
#include "./winrec.h"

//加载外部库，驱动录音功能
#ifdef _WIN64
#pragma comment(lib,"../libs/msc_x64.lib") //x64
#else
#pragma comment(lib,"../libs/msc.lib") //x86
#endif

//定义标识
#define SR_DBGON 0

#if SR_DBGON == 1
#	define sr_dbg printf
#else
#	define sr_dbg
#endif

#define DEFAULT_FORMAT {WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, sizeof(WAVEFORMATEX)}

//初始化状态
enum {
	SR_STATE_INIT,
	SR_STATE_STARTED
};
/* for debug. saving the recording to a file */
#ifdef __FILE_SAVE_VERIFY__
#define VERIFY_FILE_NAME "rec.pcm"
static int open_stored_file(const char* name);
static int loopwrite_to_file(char* data, size_t length);
static void safe_close_file();
#endif

#define SR_MALLOC malloc
#define SR_MFREE  free
#define SR_MEMSET memset

#ifdef __FILE_SAVE_VERIFY__
//音频文件
static FILE* fdwav = NULL;
//打开音频文件
static int open_stored_file(const char* name)
{
	fdwav = fopen(name, "wb+");
	if (fdwav == NULL) 
	{
		printf("error open file failed\n");
		return -1;
	}
	return 0;
}
//循环写入文件
static int loopwrite_to_file(char* data, size_t length)
{
	size_t wrt = 0, already = 0;
	int ret = 0;
	if (fdwav == NULL || data == NULL)
		return -1;
	while (1) {
		wrt = fwrite(data + already, 1, length - already, fdwav);
		if (wrt == (length - already))
			break;
		if (ferror(fdwav))
		{
			ret = -1;
			break;
		}
		already += wrt;
	}
	return ret;
}
//存储文件
static void safe_close_file()
{
	if (fdwav) {
		fclose(fdwav);
		fdwav = NULL;
	}
}
#endif

static void end_sr_on_error(struct speech_rec* sr, int errcode)
{
	if (sr->aud_src == SR_MIC)
		stop_record(sr->recorder);

	if (sr->session_id) 
	{
		if (sr->notif.on_speech_end)
			sr->notif.on_speech_end(errcode);

		QISRSessionEnd(sr->session_id, "err");
		sr->session_id = NULL;
	}
	sr->state = SR_STATE_INIT;
#ifdef __FILE_SAVE_VERIFY__
	safe_close_file();
#endif
}

static void end_sr_on_vad(struct speech_rec* sr)
{
	int errcode;
	int ret;
	const char* rslt;
	if (sr->aud_src == SR_MIC)
		stop_record(sr->recorder);
	sr->state = SR_STATE_INIT;
	ret = QISRAudioWrite(sr->session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &sr->ep_stat, &sr->rec_stat);
	if (ret != 0)
	{
		sr_dbg("write LAST_SAMPLE failed: %d\n", ret);
		QISRSessionEnd(sr->session_id, "write err");
	}
	sr->rec_stat = MSP_AUDIO_SAMPLE_CONTINUE;
	while (sr->rec_stat != MSP_REC_STATUS_COMPLETE) 
	{
		rslt = QISRGetResult(sr->session_id, &sr->rec_stat, 0, &errcode);
		if (rslt && sr->notif.on_result)
			sr->notif.on_result(rslt, sr->rec_stat == MSP_REC_STATUS_COMPLETE ? 1 : 0);
		/* for cpu occupy, should sleep here */
		Sleep(100); 
	}
	if (sr->session_id) 
	{
		if (sr->notif.on_speech_end)
			sr->notif.on_speech_end(END_REASON_VAD_DETECT);
		QISRSessionEnd(sr->session_id, "VAD Normal");
		sr->session_id = NULL;
	}
	sr->state = SR_STATE_INIT;
#ifdef __FILE_SAVE_VERIFY__
	safe_close_file();
#endif
}
/* the record call back */
static void asr_cb(char* data, unsigned long len, void* user_para)
{
	int errcode;
	struct speech_rec* sr;

	if (len == 0 || data == NULL)
		return;
	sr = (struct speech_rec*)user_para;
	if (sr == NULL || sr->ep_stat >= MSP_EP_AFTER_SPEECH)
		return;
#ifdef __FILE_SAVE_VERIFY__
	loopwrite_to_file(data, len);
#endif
	errcode = sr_write_audio_data(sr, data, len);
	if (errcode) 
	{
		end_sr_on_error(sr, errcode);
		return;
	}
}
//跳过空格
static const char* skip_space(const char* s)
{
	while (s && *s != ' ' && *s != '\0')
		s++;
	return s;
}

static int update_format_from_sessionparam(const char* session_para, WAVEFORMATEX* wavefmt)
{
	const char* s;
	if ((s = strstr(session_para, "sample_rate")))
	{
		if (s = strstr(s, "="))
		{
			s = skip_space(s);
			if (s && *s)
			{
				wavefmt->nSamplesPerSec = atoi(s);
				wavefmt->nAvgBytesPerSec = wavefmt->nBlockAlign * wavefmt->nSamplesPerSec;
			}
		}
		else
			return -1;
	}
	else
	{
		return -1;
	}
	return 0;
}

void wait_for_rec_stop(struct recorder* rec, unsigned int timeout_ms)
{
	while (!is_record_stopped(rec))
	{
		Sleep(1);
		if (timeout_ms != (unsigned int)-1)
			if (0 == timeout_ms--)
				break;
	}
}

/* 
 * devid will be ignored if aud_src is not SR_MIC ; if devid == -1, then
 * the default input device will be used.
 */

int sr_init(struct speech_rec* sr, const char* session_begin_params, enum sr_audsrc aud_src, int devid, struct speech_rec_notifier* notify)
{
	int errcode;
	size_t param_size;
	WAVEFORMATEX wavfmt = DEFAULT_FORMAT;
	// 如果设备数量没有
	if (aud_src == SR_MIC && get_input_dev_num() == 0)
	{
		return -E_SR_NOACTIVEDEVICE;
	}
	if (!sr)
		return -E_SR_INVAL;
	//分配内存
	SR_MEMSET(sr, 0, sizeof(struct speech_rec));
	sr->state = SR_STATE_INIT;
	sr->aud_src = aud_src;
	sr->ep_stat = MSP_EP_LOOKING_FOR_SPEECH;
	sr->rec_stat = MSP_REC_STATUS_SUCCESS;
	sr->audio_status = MSP_AUDIO_SAMPLE_FIRST;

	param_size = strlen(session_begin_params) + 1;
	sr->session_begin_params = (char*)SR_MALLOC(param_size);
	if (sr->session_begin_params == NULL) 
	{
		sr_dbg("mem alloc failed\n");
		return -E_SR_NOMEM;
	}

	strncpy(sr->session_begin_params, session_begin_params, param_size);
	sr->notif = *notify;

	if (aud_src == SR_MIC)
	{
		errcode = create_recorder(&sr->recorder, asr_cb, (void*)sr);
		if (sr->recorder == NULL || errcode != 0)
		{
			sr_dbg("create recorder failed: %d\n", errcode);
			errcode = -E_SR_RECORDFAIL;
			goto fail;
		}

		update_format_from_sessionparam(session_begin_params, &wavfmt);

		errcode = open_recorder(sr->recorder, devid, &wavfmt);
		if (errcode != 0)
		{
			sr_dbg("recorder open failed: %d\n", errcode);
			errcode = -E_SR_RECORDFAIL;
			goto fail;
		}
	}

	return 0;

fail:
	if (sr->recorder)
	{
		destroy_recorder(sr->recorder);
		sr->recorder = NULL;
	}

	if (sr->session_begin_params)
	{
		SR_MFREE(sr->session_begin_params);
		sr->session_begin_params = NULL;
	}
	SR_MEMSET(&sr->notif, 0, sizeof(sr->notif));

	return errcode;
}


int sr_start_listening(struct speech_rec* sr)
{
	int ret;
	const char* session_id = NULL;
	int errcode = MSP_SUCCESS;

	if (sr->state >= SR_STATE_STARTED)
	{
		sr_dbg("already STARTED.\n");
		return -E_SR_ALREADY;
	}
	//听写不需要语法，第一个参数为NULL
	session_id = QISRSessionBegin(NULL, sr->session_begin_params, &errcode);
	if (MSP_SUCCESS != errcode)
	{
		sr_dbg("\nQISRSessionBegin failed! error code:%d\n", errcode);
		return errcode;
	}
	sr->session_id = session_id;
	sr->ep_stat = MSP_EP_LOOKING_FOR_SPEECH;
	sr->rec_stat = MSP_REC_STATUS_SUCCESS;
	sr->audio_status = MSP_AUDIO_SAMPLE_FIRST;
	if (sr->aud_src == SR_MIC) 
	{
		ret = start_record(sr->recorder);
		if (ret != 0) 
		{
			sr_dbg("start record failed: %d\n", ret);
			QISRSessionEnd(session_id, "start record fail");
			sr->session_id = NULL;
			return -E_SR_RECORDFAIL;
		}
#ifdef __FILE_SAVE_VERIFY__
		open_stored_file(VERIFY_FILE_NAME);
#endif
	}
	sr->state = SR_STATE_STARTED;
	if (sr->notif.on_speech_begin)
		sr->notif.on_speech_begin();
	return 0;
}

int sr_stop_listening(struct speech_rec* sr)
{
	int ret = 0;
	const char* rslt = NULL;

	if (sr->state < SR_STATE_STARTED) 
	{
		sr_dbg("Not started or already stopped.\n");
		return 0;
	}

	if (sr->aud_src == SR_MIC) {
		ret = stop_record(sr->recorder);
#ifdef __FILE_SAVE_VERIFY__
		safe_close_file();
#endif
		if (ret != 0) 
		{
			sr_dbg("Stop failed! \n");
			return -E_SR_RECORDFAIL;
		}
		wait_for_rec_stop(sr->recorder, (unsigned int)-1);
	}
	sr->state = SR_STATE_INIT;
	ret = QISRAudioWrite(sr->session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &sr->ep_stat, &sr->rec_stat);
	if (ret != 0)
	{
		sr_dbg("write LAST_SAMPLE failed: %d\n", ret);
		QISRSessionEnd(sr->session_id, "write err");
		return ret;
	}

	while (sr->rec_stat != MSP_REC_STATUS_COMPLETE) 
	{
		rslt = QISRGetResult(sr->session_id, &sr->rec_stat, 0, &ret);
		if (MSP_SUCCESS != ret) 
		{
			sr_dbg("\nQISRGetResult failed! error code: %d\n", ret);
			end_sr_on_error(sr, ret);
			return ret;
		}
		if (NULL != rslt && sr->notif.on_result)
			sr->notif.on_result(rslt, sr->rec_stat == MSP_REC_STATUS_COMPLETE ? 1 : 0);
		Sleep(100);
	}

	QISRSessionEnd(sr->session_id, "normal");
	sr->session_id = NULL;
	return 0;
}

int sr_stop_listening_byvad(struct speech_rec* sr)
{
	int ret = 0;
	const char* rslt = NULL;
	char rslt_str[2048] = {'\0'};
	int count = 0;
	//如果未开始录音就错误调用了本函数那么返回报错
	if (sr->state < SR_STATE_STARTED) {
		sr_dbg("Not started or already stopped.\n");
		return 0;
	}
	//进程阻塞
	while (1)
	{
		//VAD检测为音频末尾
		if (sr->ep_stat == MSP_EP_AFTER_SPEECH)
		{
			break;
		}
	}
	//采用录音参数为NULL，0，MSP_AUDIO_SAMPLE_LAST
	ret = QISRAudioWrite(sr->session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &sr->ep_stat, &sr->rec_stat);
	if (ret != 0) 
	{
		sr_dbg("write LAST_SAMPLE failed: %d\n", ret);
		QISRSessionEnd(sr->session_id, "write err");
		return ret;
	}
	//反复调用结果函数，直至获取结果
	while (sr->rec_stat != MSP_REC_STATUS_COMPLETE) 
	{
		rslt = QISRGetResult(sr->session_id, &sr->rec_stat, 0, &ret);
		if (MSP_SUCCESS != ret) {
			sr_dbg("\nQISRGetResult failed! error code: %d\n", ret);
			end_sr_on_error(sr, ret);
			return ret;
		}
		if (NULL != rslt && sr->notif.on_result)
			sr->notif.on_result(rslt, sr->rec_stat == MSP_REC_STATUS_COMPLETE ? 1 : 0);
		Sleep(100);
	}
	
	//结束本次录音，释放资源
	if (sr->aud_src == SR_MIC)
	{
		ret = stop_record(sr->recorder);
#ifdef __FILE_SAVE_VERIFY__
		safe_close_file();
#endif
		if (ret != 0) 
		{
			sr_dbg("Stop failed! \n");
			return -E_SR_RECORDFAIL;
		}
		wait_for_rec_stop(sr->recorder, (unsigned int)-1);
	}
	sr->state = SR_STATE_INIT;
	QISRSessionEnd(sr->session_id, "normal");
	sr->session_id = NULL;
	if (rslt != NULL)
	{
		rslt = NULL;
	}
	return 0;
}

int sr_write_audio_data(struct speech_rec* sr, char* data, unsigned int len)
{
	const char* rslt = NULL;
	int ret = 0;
	if (!sr)
		return -E_SR_INVAL;
	if (!data || !len)
		return 0;
	ret = QISRAudioWrite(sr->session_id, data, len, sr->audio_status, &sr->ep_stat, &sr->rec_stat);
	if (ret) 
	{
		end_sr_on_error(sr, ret);
		return ret;
	}
	sr->audio_status = MSP_AUDIO_SAMPLE_CONTINUE;

	if (MSP_EP_AFTER_SPEECH == sr->ep_stat)
	{
		end_sr_on_vad(sr);
	}
	return 0;
}

void sr_uninit(struct speech_rec* sr)
{
	if (sr->recorder) 
	{
		if (!is_record_stopped(sr->recorder))
			stop_record(sr->recorder);
		close_recorder(sr->recorder);
		destroy_recorder(sr->recorder);
		sr->recorder = NULL;
	}
	if (sr->session_begin_params)
	{
		SR_MFREE(sr->session_begin_params);
		sr->session_begin_params = NULL;
	}
}