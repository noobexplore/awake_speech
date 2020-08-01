#include <iostream>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <Mmsystem.h>
#include "UDP_Send.h"
#include "./tinyxml/tinystr.h"
#include "./tinyxml/tinyxml.h"

#define MAX_LEN      (1024)
#pragma comment(lib,"WS2_32.lib")
#ifdef _DEBUG 
#pragma comment(lib,"tinyxml.lib")
#else
#pragma comment(lib,"tinyxml.lib")
#endif

//���ز��ſ�
#pragma comment(lib,"winmm.lib")

WSADATA wsaData;//��ʼ��
SOCKET SendSocket;
sockaddr_in RecvAddr;//��������ַ
int Port = 12000;//������������ַ
char SendBuf[1024];//�������ݵĻ�����
int BufLen = 1024;//��������С
//�෽��ʵ��
UDPSend::UDPSend()
{
	//��ʼ��Socket
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//����Socket����
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//���÷�������ַ
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
}
UDPSend::~UDPSend()
{

}
void UDPSend::SendData(char* lpFrame, int length)
{
	printf("Sending data====%s\n", lpFrame);
	sendto(SendSocket, lpFrame, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
}
void UDPSend::Close()
{
	printf("finished sending, close socket.\n");
	closesocket(SendSocket);
	printf("Exting.\n");
	WSACleanup();
}

/*const char* xmlstr =
	"<?xml version='1.0' encoding='gb2312' standalone='yes' ?>\
	<nlp>\
	<version>1.1 </version>\
	<rawtext>�Ŵ�ͥ������</rawtext>\
	<confidence>94</confidence >\
	<engine>local</engine>\
	<result>\
	<focus>action|localtion</focus>\
	<confidence>97|93</confidence >\
	<object>\
	<action id='1'>�Ŵ�</action>\
	<localtion id='4'>ͥ������</localtion>\
	</object>\
	</result>\
	</nlp>";*/

	//����xml
xml_string any_xml(char* xmlstr)
{
	//��ʼ���ṹ��
	xml_string xml_action_loc;
	TiXmlDocument* myDocument = new TiXmlDocument();
	//ʹ��Parse�������и�ʽ��xmlstring
	myDocument->Parse(xmlstr);
	//��ȡ���ڵ�
	TiXmlElement* rootElement = myDocument->RootElement();
	if (rootElement == NULL || strcmp(rootElement->Value(), "nlp")) return xml_action_loc;
	//printf("%s\n", rootElement->Value());
	//��һ��
	TiXmlElement* element = rootElement->FirstChildElement();
	if (element == NULL || strcmp(element->Value(), "version")) return xml_action_loc;
	//printf("%s:\t%s\n", element->Value(), element->GetText());
	//ֱ�Ӳ��Ҷ�Ӧ��ĳ�ڵ�
	TiXmlNode* result_Node = NULL;
	result_Node = rootElement->IterateChildren("result", result_Node);
	//rawtext�ڵ�
	TiXmlNode* rawtext_Node = NULL;
	rawtext_Node = rootElement->IterateChildren("rawtext", rawtext_Node);
	//�ҵ�confidence�ڵ�
	TiXmlNode* confidence_Node = NULL;
	confidence_Node = rootElement->IterateChildren("confidence", confidence_Node);
	//�ҵ�object�ڵ�
	TiXmlNode* object_Node = NULL;
	object_Node = result_Node->IterateChildren("object", object_Node);
	//һ��Ҫ��ȡis_closed����
	TiXmlNode* closed_Node = NULL;
	closed_Node = object_Node->IterateChildren("close", closed_Node);
	//�����λ�ȡ�����action��location
	TiXmlNode* action_Node = NULL;
	action_Node = object_Node->IterateChildren("action", action_Node);
	TiXmlNode* location_Node = NULL;
	location_Node = object_Node->IterateChildren("location", location_Node);
	//���ж���û��closed
	if (closed_Node != NULL)
	{
		xml_action_loc.action_Node_id = "";
		xml_action_loc.location_Node_id = "";
		xml_action_loc.confidence_Node_id = "";
		xml_action_loc.rawtext_Node_text = "";
		return xml_action_loc;
	}
	else
	{
		xml_action_loc.flag = 1;
		//������Ҫ�ж��Ƿ�����ɹ�
		if (action_Node == NULL && location_Node != NULL)
		{
			xml_action_loc.action_Node_id = location_Node->ToElement()->Attribute("id");
			xml_action_loc.location_Node_id = "";
			xml_action_loc.confidence_Node_id = confidence_Node->ToElement()->GetText();
			xml_action_loc.rawtext_Node_text = rawtext_Node->ToElement()->GetText();
		}
		else if (action_Node != NULL && location_Node == NULL)
		{
			xml_action_loc.action_Node_id = action_Node->ToElement()->Attribute("id");
			xml_action_loc.location_Node_id = "";
			xml_action_loc.confidence_Node_id = confidence_Node->ToElement()->GetText();
			xml_action_loc.rawtext_Node_text = rawtext_Node->ToElement()->GetText();
		}
		else if (action_Node == NULL && location_Node == NULL)
		{
			xml_action_loc.action_Node_id = "";
			xml_action_loc.location_Node_id = "";
			xml_action_loc.confidence_Node_id = confidence_Node->ToElement()->GetText();
			xml_action_loc.rawtext_Node_text = rawtext_Node->ToElement()->GetText();
		}
		else
		{
			//�ֱ��ȡaction��location id text
			xml_action_loc.action_Node_id = action_Node->ToElement()->Attribute("id");
			xml_action_loc.location_Node_id = location_Node->ToElement()->Attribute("id");
			xml_action_loc.action_Node_id = "0" + xml_action_loc.action_Node_id;
			xml_action_loc.location_Node_id = "0" + xml_action_loc.location_Node_id;
			xml_action_loc.action_Node_id = xml_action_loc.action_Node_id + xml_action_loc.location_Node_id;
			xml_action_loc.confidence_Node_id = confidence_Node->ToElement()->GetText();
			xml_action_loc.rawtext_Node_text = rawtext_Node->ToElement()->GetText();
		}
		return xml_action_loc;
	}
}
//�򵥷��ͺ���
void send_simple(xml_string xmlstr)
{
	UDPSend* udpsend;
	int status_id;
	char asr_params[MAX_LEN] = { NULL };
	int intStr = atoi(xmlstr.confidence_Node_id.c_str());
	if (intStr < 5)
	{
		status_id = 0;
		PlaySound(TEXT("./sounds/dont_understand.wav"), NULL, SND_FILENAME | SND_SYNC);
		//��װ�ַ���
		_snprintf(asr_params, MAX_LEN - 1, "{\"status_id\":\"%d\", \"action_id\":\"\", \"location\":\"\"}", status_id);
	}
	else
	{
		status_id = 1;
		//��װ�ַ���
		_snprintf(asr_params, MAX_LEN - 1, "{\"status_id\":\"%d\", \"action_id\":\"%s\", \"location\":\"%s\"}",
			status_id, xmlstr.action_Node_id.c_str(), xmlstr.location_Node_id.c_str());
		printf("action id: %s\tlocation id: %s\traw_text text: %s\tconfidence value: %s\n\n",
			xmlstr.action_Node_id.c_str(),
			xmlstr.location_Node_id.c_str(),
			xmlstr.rawtext_Node_text.c_str(),
			xmlstr.confidence_Node_id.c_str());
	}
	//����UDP
	udpsend = new UDPSend();
	udpsend->SendData((char*)asr_params, MAX_LEN);
	//������֮��һ��Ҫ�ͷ�
	udpsend->Close();
}