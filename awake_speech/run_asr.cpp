#include <iostream>
#include <windows.h>
#include <Mmsystem.h>
#include <conio.h>
#include <errno.h>
#include <process.h>
#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
//语音通用API
#include "../include/msp_cmn.h"
#include "../include/msp_errors.h"
#include "../include/msp_types.h"
//语音识别命令SDK
#include "../include/qisr.h"
//语音唤醒命令SDK
#include "../include/qivw.h"
//封装的麦克风接口文件
#include "./speech_recognize.h"
#include "./winrec.h"
#include "UDP_Send.h"
//加载讯飞SDK
#ifdef _WIN64
#pragma comment(lib,"../libs/msc_x64.lib") //x64
#else
#pragma comment(lib,"../libs/msc.lib") //x86
#endif
//构建离线语法识别网络生成数据保存路径
#ifdef _WIN64
const char* GRM_BUILD_PATH = "res/asr/GrmBuilld_x64";
#else
//构建离线语法识别网络生成数据保存路径
const char* GRM_BUILD_PATH = "res/asr/GrmBuilld";
#endif
//加载播放库
#pragma comment(lib,"winmm.lib")
//后台运行
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup")

//定义参数
#define FRAME_LEN	640 
#define	BUFFER_SIZE	4096
#define SAMPLE_RATE_16K     (16000)
#define SAMPLE_RATE_8K      (8000)
#define MAX_GRAMMARID_LEN   (32)
#define MAX_PARAMS_LEN      (1024)

enum {
	EVT_START = 0,
	EVT_STOP,
	EVT_QUIT,
	EVT_TOTAL
};

#define DEFAULT_FORMAT		\
{\
	WAVE_FORMAT_PCM,	\
	1,					\
	16000,				\
	32000,				\
	2,					\
	16,					\
	sizeof(WAVEFORMATEX)	\
}
//
#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0) //必要
//控制小枢开关
int is_closed = 0;
//唤醒状态flag，默认0未唤醒，1已换醒
int awkeFlag = 0;
//初始化录音对象
struct recorder* recorder;
//初始化录音状态
int record_state = MSP_AUDIO_SAMPLE_FIRST;
//oneshot专用，用来标识命令词识别结果是否已返回
int ISR_STATUS = 0;
//线程事件处理有关
static HANDLE events[EVT_TOTAL] = { NULL,NULL,NULL };
static COORD begin_pos = { 0, 0 };
static COORD last_pos = { 0, 0 };
//结果回调变量
static char* g_result = NULL;
static unsigned int g_buffersize = BUFFER_SIZE;
//离线语法识别资源路径
const char* ASR_RES_PATH = "fo|res/asr/common.jet";
//构建离线识别语法网络所用的语法文件
const char* GRM_FILE = "test.bnf";
//更新离线识别语法的contact槽（语法文件为此示例中使用的call.bnf）
const char* LEX_NAME = "contact";
//用户数据结构体
typedef struct _UserData {
	int     build_fini;  //标识语法构建是否完成
	int     update_fini; //标识更新词典是否完成
	int     errcode; //记录语法构建或更新词典回调错误码
	char    grammar_id[MAX_GRAMMARID_LEN]; //保存语法构建返回的语法ID
}UserData;
//构建语法表回调
int build_grm_cb(int ecode, const char* info, void* udata)
{
	UserData* grm_data = (UserData*)udata;

	if (NULL != grm_data) {
		grm_data->build_fini = 1;
		grm_data->errcode = ecode;
	}

	if (MSP_SUCCESS == ecode && NULL != info) {
		printf("构建语法成功！ 语法ID:%s\n", info);
		if (NULL != grm_data)
			_snprintf(grm_data->grammar_id, MAX_GRAMMARID_LEN - 1, info);
	}
	else
		printf("构建语法失败！%d\n", ecode);

	return 0;
}
//构建语法表
int build_grammar(UserData* udata)
{
	FILE* grm_file = NULL;
	char* grm_content = NULL;
	unsigned int grm_cnt_len = 0;
	char grm_build_params[MAX_PARAMS_LEN] = { NULL };
	int ret = 0;

	grm_file = fopen(GRM_FILE, "rb");
	if (NULL == grm_file) {
		printf("打开\"%s\"文件失败！[%s]\n", GRM_FILE, strerror(errno));
		return -1;
	}

	fseek(grm_file, 0, SEEK_END);
	grm_cnt_len = ftell(grm_file);
	fseek(grm_file, 0, SEEK_SET);

	grm_content = (char*)malloc(grm_cnt_len + 1);
	if (NULL == grm_content)
	{
		printf("内存分配失败!\n");
		fclose(grm_file);
		grm_file = NULL;
		return -1;
	}
	fread((void*)grm_content, 1, grm_cnt_len, grm_file);
	grm_content[grm_cnt_len] = '\0';
	fclose(grm_file);
	grm_file = NULL;

	_snprintf(grm_build_params, MAX_PARAMS_LEN - 1,
		"engine_type = local, \
		asr_res_path = %s, sample_rate = %d, \
		grm_build_path = %s, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH
	);
	ret = QISRBuildGrammar("bnf", grm_content, grm_cnt_len, grm_build_params, build_grm_cb, udata);

	free(grm_content);
	grm_content = NULL;

	return ret;
}
//更新离线回调
int update_lex_cb(int ecode, const char* info, void* udata)
{
	UserData* lex_data = (UserData*)udata;

	if (NULL != lex_data) {
		lex_data->update_fini = 1;
		lex_data->errcode = ecode;
	}

	if (MSP_SUCCESS == ecode)
		printf("更新词典成功！\n");
	else
		printf("更新词典失败！%d\n", ecode);

	return 0;
}
//更新离线识别函数
int update_lexicon(UserData* udata)
{
	const char* lex_content = "丁伟\n邓川\n小枢";
	unsigned int lex_cnt_len = strlen(lex_content);
	char update_lex_params[MAX_PARAMS_LEN] = { NULL };

	_snprintf_s(update_lex_params, MAX_PARAMS_LEN - 1,
		"engine_type = local, text_encoding = GB2312, \
		asr_res_path = %s, sample_rate = %d, \
		grm_build_path = %s, grammar_list = %s, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH,
		udata->grammar_id);
	return QISRUpdateLexicon(LEX_NAME, lex_content, lex_cnt_len, update_lex_params, update_lex_cb, udata);
}
//显示结果并传输数据
static void show_result(char* string, char is_over)
{
	COORD orig, current;
	CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE w = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(w, &info);
	current = info.dwCursorPosition;
	if (current.X == last_pos.X && current.Y == last_pos.Y) 
	{
		SetConsoleCursorPosition(w, begin_pos);
	}
	else 
	{
		/* changed by other routines, use the new pos as start */
		begin_pos = current;
	}
	if (is_over) SetConsoleTextAttribute(w, FOREGROUND_GREEN);
	printf("Result: [ %s ]\n", string);

	//发送数据
	xml_string resutl_str = any_xml(string);
	printf("Result: %d\n", resutl_str.flag);
	if (resutl_str.flag == 1)
	{
		send_simple(resutl_str);
	}
	else if(resutl_str.flag == 0)
	{
		is_closed = 1;
		PlaySound(TEXT("./sounds/unlog.wav"), NULL, SND_FILENAME | SND_SYNC);
	}

	if (is_over) SetConsoleTextAttribute(w, info.wAttributes);
	GetConsoleScreenBufferInfo(w, &info);
	last_pos = info.dwCursorPosition;
}
//语音识别通知回调
void on_result(const char* result, char is_last)
{
	if (result) {
		size_t left = g_buffersize - 1 - strlen(g_result);
		size_t size = strlen(result);
		if (left < size) {
			g_result = (char*)realloc(g_result, g_buffersize + BUFFER_SIZE);
			if (g_result)
				g_buffersize += BUFFER_SIZE;
			else {
				printf("mem alloc failed\n");
				return;
			}
		}
		strncat(g_result, result, size);
		PlaySound(TEXT("./sounds/alright.wav"), NULL, SND_FILENAME | SND_SYNC);
		show_result(g_result, is_last);
	}
}
void on_speech_begin()
{
	if (g_result)
	{
		free(g_result);
	}
	g_result = (char*)malloc(BUFFER_SIZE);
	g_buffersize = BUFFER_SIZE;
	memset(g_result, 0, g_buffersize);
	printf("Start Listening...\n");
}
void on_speech_end(int reason)
{
	if (reason == END_REASON_VAD_DETECT)
		printf("\nSpeaking done \n");
	else
		printf("\nRecognizer error %d\n", reason);
}
//唤醒状态消息提示，喂给回调函数QIVWRegisterNotify
int cb_ivw_msg_proc(const char* sessionID, int msg, int param1, int param2, const void* info, void* userData)
{
	if (MSP_IVW_MSG_ERROR == msg) //唤醒出错消息
	{
		printf("唤醒失败！");
		awkeFlag = 0;
	}
	else if (MSP_IVW_MSG_WAKEUP == msg) //唤醒成功消息
	{
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
		printf("唤醒成功！");
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
		awkeFlag = 1;
	}
	return 0;
}
//等待录音结束
static void wait_for_rec_stop(struct recorder* rec, unsigned int timeout_ms)
{
	while (!is_record_stopped(rec))
	{
		Sleep(1);
		if (timeout_ms != (unsigned int)-1)
			if (0 == timeout_ms--)
				break;
	}
}
//实时读取录音的内容
static void iat_cb(char* data, unsigned long len, void* user_para)
{
	int errcode;
	int ret = 0;
	const char* session_id = (const char*)user_para; //初始化本次识别的句柄。
	if (len == 0 || data == NULL)
		return;
	//读取data中的数据流
	errcode = QIVWAudioWrite(session_id, (const void*)data, len, record_state);
	//读取错误处理
	if (MSP_SUCCESS != errcode)
	{
		printf("QIVWAudioWrite failed! error code:%d\n", errcode);
		ret = stop_record(recorder);
		if (ret != 0) {
			printf("Stop failed! \n");
		}
		wait_for_rec_stop(recorder, (unsigned int)-1);
		QIVWAudioWrite(session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST);
		record_state = MSP_AUDIO_SAMPLE_LAST;
	}
	//判断录音初始
	if (record_state == MSP_AUDIO_SAMPLE_FIRST)
	{
		record_state = MSP_AUDIO_SAMPLE_CONTINUE;
	}
}
//唤醒函数
void run_ivw(const char* grammar_list, const char* session_begin_params)
{
	const char* session_id = NULL;
	int err_code = MSP_SUCCESS;
	//调用录音设备
	WAVEFORMATEX wavfmt = DEFAULT_FORMAT;
	//用于设置结束时显示的信息
	char sse_hints[128];
	int count = 0;
	//初始化唤醒session
	session_id = QIVWSessionBegin(grammar_list, session_begin_params, &err_code);
	if (err_code != MSP_SUCCESS)
	{
		printf("QIVWSessionBegin failed! error code:%d\n", err_code);
		goto exit;
	}
	//此处必须进行回调函数
	err_code = QIVWRegisterNotify(session_id, cb_ivw_msg_proc, NULL);
	if (err_code != MSP_SUCCESS)
	{
		_snprintf_s(sse_hints, sizeof(sse_hints), "QIVWRegisterNotify errorCode=%d", err_code);
		printf("QIVWRegisterNotify failed! error code:%d\n", err_code);
		goto exit;
	}
	//开始录音
	err_code = create_recorder(&recorder, iat_cb, (void*)session_id);
	err_code = open_recorder(recorder, get_default_input_dev(), &wavfmt);
	err_code = start_record(recorder);
	//循环监听
	while (record_state != MSP_AUDIO_SAMPLE_LAST)
	{
		//阻塞直到唤醒结果出现
		Sleep(200);
		if (awkeFlag == 1)
		{
			awkeFlag = 0;
			//恢复标志位方便下次唤醒
			break;
		}
		count++;
		//为了防止循环监听时写入到缓存中的数据过大
		if (count == 20)
		{
			//先释放当前录音资源
			stop_record(recorder);
			close_recorder(recorder);
			destroy_recorder(recorder);
			recorder = NULL;
			//printf("防止音频资源过大，重建\n");
			//struct recorder *recorder;
			//重建录音资源
			err_code = create_recorder(&recorder, iat_cb, (void*)session_id);
			err_code = open_recorder(recorder, get_default_input_dev(), &wavfmt);
			err_code = start_record(recorder);
			count = 0;
		}
	}
exit:
	if (recorder) {
		if (!is_record_stopped(recorder))
			stop_record(recorder);
		close_recorder(recorder);
		destroy_recorder(recorder);
		recorder = NULL;
	}
	if (NULL != session_id)
	{
		QIVWSessionEnd(session_id, sse_hints); //结束一次唤醒会话
	}
}
//展示按键
static void show_key_hints(void)
{
	printf("\n\
----------------------------\n\
Press F1 to start speaking\n\
Press F2 to end your speaking\n\
Press F3 to quit\n\
----------------------------\n");
}
//线程帮助器监听结果
static unsigned int  __stdcall helper_thread_proc(void* para)
{
	int key;
	int quit = 0;

	do {
		key = _getch();
		switch (key) {
		case 59:
			SetEvent(events[EVT_START]);
			break;
		case 60:
			SetEvent(events[EVT_STOP]);
			break;
		case 61:
			quit = 1;
			SetEvent(events[EVT_QUIT]);
			PostQuitMessage(0);
			break;
		default:
			break;
		}

		if (quit)
			break;
	} while (1);

	return 0;
}
static unsigned int  __stdcall helper_thread_proc_mouse(void* para)
{
	int key = 0;
	int quit = 0;
	if (KEY_DOWN(VK_LBUTTON))
	{
		key = VK_LBUTTON;
	}
	else if (KEY_DOWN(VK_RBUTTON))
	{
		key = VK_RBUTTON;
	}
	else if (KEY_DOWN(VK_MBUTTON))
	{
		key = VK_MBUTTON;
	}
	do {
		switch (key) {
		case 2:
			SetEvent(events[EVT_START]);
			break;
		case 1:
			SetEvent(events[EVT_STOP]);
			break;
		case 4:
			quit = 1;
			SetEvent(events[EVT_QUIT]);
			PostQuitMessage(0);
			break;
		default:
			break;
		}
		if (quit) break;
	} while (1);
	return 0;
}
//进程管理
static HANDLE start_helper_thread()
{
	HANDLE hdl;
	hdl = (HANDLE)_beginthreadex(NULL, 0, helper_thread_proc, NULL, 0, NULL);
	return hdl;
}
//麦克风语音监听，按键版
static void run_asr_mic(const char* session_begin_params)
{
	int errcode;
	int i = 0;
	HANDLE helper_thread = NULL;
	struct speech_rec asr;
	DWORD waitres;
	char isquit = 0;
	struct speech_rec_notifier recnotifier = {
		on_result,
		on_speech_begin,
		on_speech_end
	};
	errcode = sr_init(&asr, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier);
	for (i = 0; i < EVT_TOTAL; ++i) 
	{
		events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	helper_thread = start_helper_thread();
	if (helper_thread == NULL)
	{
		printf("create thread failed\n");
		goto exit;
	}
	//请选择功能语音
	PlaySound(TEXT("./sounds/choice.wav"), NULL, SND_FILENAME | SND_SYNC);
	show_key_hints();

	while (1) {
		waitres = WaitForMultipleObjects(EVT_TOTAL, events, FALSE, INFINITE);
		switch (waitres)
		{
		case WAIT_FAILED:
		case WAIT_TIMEOUT:
			printf("Why it happened !?\n");
			break;
		case WAIT_OBJECT_0 + EVT_START:
			if (errcode = sr_start_listening(&asr)) 
			{
				printf("start listen failed %d\n", errcode);
				isquit = 1;
			}
			break;
		case WAIT_OBJECT_0 + EVT_STOP:
			if (errcode = sr_stop_listening(&asr))
			{
				printf("stop listening failed %d\n", errcode);
				isquit = 1;
			}
			break;
		case WAIT_OBJECT_0 + EVT_QUIT:
			sr_stop_listening(&asr);
			isquit = 1;
			break;
		default:
			break;
		}
		if (isquit)
			break;
	}
exit:
	if (helper_thread != NULL) {
		WaitForSingleObject(helper_thread, INFINITE);
		CloseHandle(helper_thread);
	}

	for (i = 0; i < EVT_TOTAL; ++i) {
		if (events[i])
			CloseHandle(events[i]);
	}
	//停止监听
	sr_stop_listening(&asr);
	sr_uninit(&asr);
}
//麦克风语音监听，不按版
static void run_asr_mic_nokeys(const char* session_begin_params) 
{
	int errcode;
	int i = 0;
	struct speech_rec asr;
	DWORD waitres;
	char isquit = 0;
	struct speech_rec_notifier recnotifier = {
		on_result,
		on_speech_begin,
		on_speech_end
	};
	errcode = sr_init(&asr, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier);
	if (errcode = sr_start_listening(&asr)) {
		printf("start listen failed %d\n", errcode);
		isquit = 1;
	}
	Sleep(2000);
	if (errcode = sr_stop_listening_byvad(&asr)) {
		printf("stop listening failed %d\n", errcode);
		isquit = 1;
	}
	sr_stop_listening_byvad(&asr);
	sr_uninit(&asr);
}
//运行整套流程
int run_asr(UserData* udata)
{
	char asr_params[MAX_PARAMS_LEN] = { NULL };
	//唤醒参数设定
	const char* ssb_param = "ivw_threshold=0:1450,sst=wakeup,ivw_res_path =fo|res/ivw/wakeupresource.jet";
	const char* rec_rslt = NULL;
	//会话ID
	const char* session_id = NULL;
	const char* asr_audiof = NULL;
	FILE* f_pcm = NULL;
	char* pcm_data = NULL;
	long pcm_count = 0;
	long pcm_size = 0;
	int last_audio = 0;
	int aud_stat = MSP_AUDIO_SAMPLE_CONTINUE;
	int ep_status = MSP_EP_LOOKING_FOR_SPEECH;
	int rec_status = MSP_REC_STATUS_INCOMPLETE;
	int rss_status = MSP_REC_STATUS_INCOMPLETE;
	int errcode = -1;
	int aud_src = 0;
	const char res[] = "id=\"001\"";
	//离线语法参数设置
	_snprintf(asr_params, MAX_PARAMS_LEN - 1,
		"engine_type = local, \
		asr_res_path = %s, sample_rate = %d, \
		grm_build_path = %s, local_grammar = %s, \
		result_type = xml, result_encoding = gb2312, vad_eos=800 ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH,
		udata->grammar_id
	);
	//开始进行唤醒+识别
	if (1)
	{
		while (1)
		{
			is_closed = 0;
			printf("等待唤醒：\n");
			run_ivw(NULL, ssb_param);//唤醒函数
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
			printf("你好！小枢正在听……\n");
			PlaySound(TEXT("./sounds/awaken.wav"), NULL, SND_FILENAME | SND_SYNC);
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
			while (1)
			{
				if (is_closed == 0)
				{
					printf("语音识别状态：%d\n", is_closed);
					run_asr_mic_nokeys(asr_params);//识别函数
				}
				else if(is_closed == 1)
				{
					printf("语音识别状态：%d\n", is_closed);
					break;
				}
			}
		}
	}
	return 0;
}
//主函数
int main(int argc, char* argv[])
{
	//登录appid=5f1a38ec
	int         ret = MSP_SUCCESS;
	const char* lgi_param = "appid = 5f1a38ec, work_dir = .";
	//用户登录
	ret = MSPLogin(NULL, NULL, lgi_param);
	if (MSP_SUCCESS != ret)
	{
		printf("登录失败：%d\n", ret);
		//直接跳转到exit
		goto exit;
	}
	//分配用户数据
	UserData asr_data;
	//分配内存空间
	memset(&asr_data, 0, sizeof(UserData));
	printf("构建离线识别语法网络...\n");
	//返回构建结果
	ret = build_grammar(&asr_data);
	if (MSP_SUCCESS != ret)
	{
		printf("构建语法调用失败！\n");
		goto exit;
	}
	//查看是否构建完成
	while (1 != asr_data.build_fini)
		Sleep(300);
	//更新语法词典
	printf("请按任意键继续\n");
	//_getch();
	printf("更新离线语法词典...\n");
	ret = update_lexicon(&asr_data);
	//更新词典回调
	if (MSP_SUCCESS != ret)
	{
		printf("更新词典调用失败！\n");
		goto exit;
	}
	while (1 != asr_data.update_fini)
		Sleep(300);
	if (MSP_SUCCESS != asr_data.errcode)
		goto exit;
	//开始识别
	printf("更新离线语法词典完成，开始识别...\n");
	ret = run_asr(&asr_data);
	if (MSP_SUCCESS != ret) {
		printf("离线语法识别出错: %d \n", ret);
		goto exit;
	}
exit:
	MSPLogout();
	printf("请按任意键退出...\n");
	_getch();
	return 0;
}