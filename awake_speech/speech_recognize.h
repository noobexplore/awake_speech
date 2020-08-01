/*
* ����¼���ӿڲ���¼��ʶ��ģ��
*/

//ö��Ԫ��
enum sr_audsrc
{
	SR_MIC,	/* write data from mic */
	SR_USER	/* write data from user by calling API */
};
//����һЩ����
#define E_SR_NOACTIVEDEVICE		1
#define E_SR_NOMEM				2
#define E_SR_INVAL				3
#define E_SR_RECORDFAIL			4
#define E_SR_ALREADY			5
#define DEFAULT_INPUT_DEVID     (-1)
//detected speech done
#define END_REASON_VAD_DETECT	0
//��������֪ͨ�ṹ��
struct speech_rec_notifier 
{
	void (*on_result)(const char* result, char is_last);
	void (*on_speech_begin)();
	/*0 if VAD. others, error : see E_SR_xxx and msp_errors.h*/
	void (*on_speech_end)(int reason);
};
struct speech_rec 
{
	/* from mic or manual stream write */
	enum sr_audsrc aud_src;
	struct speech_rec_notifier notif;
	const char* session_id;
	int ep_stat;
	int rec_stat;
	int audio_status;
	struct recorder* recorder;
	volatile int state;
	char* session_begin_params;
};

#ifdef __cplusplus
extern "C" {
#endif
/*
* must init before start . devid = -1, then the default device will be used.
* devid will be ignored if the aud_src is not SR_MIC
*/
//������֮ǰ���뾭��һ�²���
int sr_init(struct speech_rec* sr, const char* session_begin_params, enum sr_audsrc aud_src, int devid, struct speech_rec_notifier* notifier);
int sr_start_listening(struct speech_rec* sr);
int sr_stop_listening(struct speech_rec* sr);
int sr_stop_listening_byvad(struct speech_rec* sr);
/* ֻ���ֶ��ķ�ʽ */
int sr_write_audio_data(struct speech_rec* sr, char* data, unsigned int len);
/* ��ʹ�õ�ʱ�����ʹ��uninit */
void sr_uninit(struct speech_rec* sr);
#ifdef __cplusplus
} /* extern "C" */
#endif /* C++ */