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

//加载播放库
#pragma comment(lib,"winmm.lib")

WSADATA wsaData;//初始化
SOCKET SendSocket;
sockaddr_in RecvAddr;//服务器地址
int Port = 12000;//服务器监听地址
char SendBuf[1024];//发送数据的缓冲区
int BufLen = 1024;//缓冲区大小
//类方法实现
UDPSend::UDPSend()
{
	//初始化Socket
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//创建Socket对象
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//设置服务器地址
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
}
//类的析构函数
UDPSend::~UDPSend(){}
//发送函数
void UDPSend::SendData(char* lpFrame, int length)
{
	printf("Sending data >>>> %s\n", lpFrame);
	sendto(SendSocket, lpFrame, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
}
//关闭连接
void UDPSend::Close()
{
	printf("finished sending, close socket.\n");
	closesocket(SendSocket);
	printf("Exting.\n");
	WSACleanup();
}
//解析xml
xml_string any_xml(char* xmlstr)
{
	//初始化结构体
	xml_string xml_action_loc;
	TiXmlDocument* myDocument = new TiXmlDocument();
	//使用Parse函数进行格式化xmlstring
	myDocument->Parse(xmlstr);
	//获取根节点
	TiXmlElement* rootElement = myDocument->RootElement();
	//如果解析失败
	if (rootElement == NULL || strcmp(rootElement->Value(), "nlp")) return xml_action_loc;
	//获取版本节点
	TiXmlElement* element = rootElement->FirstChildElement();
	if (element == NULL || strcmp(element->Value(), "version")) return xml_action_loc;
	//直接查找对应的某节点
	TiXmlNode* result_Node = NULL;
	result_Node = rootElement->IterateChildren("result", result_Node);
	//rawtext节点
	TiXmlNode* rawtext_Node = NULL;
	rawtext_Node = rootElement->IterateChildren("rawtext", rawtext_Node);
	//找到confidence节点，获取总体的置性值
	TiXmlNode* confidence_Node = NULL;
	confidence_Node = rootElement->IterateChildren("confidence", confidence_Node);
	//找到object节点
	TiXmlNode* object_Node = NULL;
	object_Node = result_Node->IterateChildren("object", object_Node);
	//一定要获取is_closed参数
	TiXmlNode* closed_Node = NULL;
	closed_Node = object_Node->IterateChildren("close", closed_Node);
	//再依次获取下面的motion、menu、properties、school、museum、business、administrative、hospital
	TiXmlNode* motion = NULL;
	TiXmlNode* menu = NULL;
	TiXmlNode* properties = NULL;
	TiXmlNode* school = NULL;
	TiXmlNode* museum = NULL;
	TiXmlNode* business = NULL;
	TiXmlNode* administrative = NULL;
	TiXmlNode* hospital = NULL;
	//构造两个节点
	TiXmlNode* action_Node = NULL;
	TiXmlNode* location_Node = NULL;
	//开始迭代查找
	motion = object_Node->IterateChildren("motion", closed_Node);
	menu = object_Node->IterateChildren("menu", closed_Node);
	properties = object_Node->IterateChildren("properties", closed_Node);
	school = object_Node->IterateChildren("school", closed_Node);
	museum = object_Node->IterateChildren("museum", closed_Node);
	business = object_Node->IterateChildren("business", closed_Node);
	administrative = object_Node->IterateChildren("administrative", closed_Node);
	hospital = object_Node->IterateChildren("hospital", closed_Node);
	//判断action
	if (motion != NULL) action_Node = motion;
	if (menu != NULL) action_Node = menu;
	//判断location
	if (properties != NULL) location_Node = properties;
	if (school != NULL) location_Node = school;
	if (museum != NULL) location_Node = museum;
	if (business != NULL) location_Node = business;
	if (administrative != NULL) location_Node = administrative;
	if (hospital != NULL) location_Node = hospital;
	
	//先判断有没有closed
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
		//这里需要判断是否解析成功
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
			//分别获取action和location id text
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
//简单发送函数
void send_simple(xml_string xmlstr)
{
	UDPSend* udpsend;
	int status_id;
	char asr_params[MAX_LEN] = { NULL };
	int intStr = atoi(xmlstr.confidence_Node_id.c_str());
	if (intStr < 5)
	{
		status_id = 0;
		PlaySound(TEXT("./sounds/dont_understand1.wav"), NULL, SND_FILENAME | SND_SYNC);
		//封装字符串
		_snprintf(asr_params, MAX_LEN - 1, "{\"status_id\":\"%d\", \"action_id\":\"\", \"location\":\"\"}", status_id);
	}
	else
	{
		status_id = 1;
		//封装字符串
		_snprintf(asr_params, MAX_LEN - 1, "{\"status_id\":\"%d\", \"action_id\":\"%s\", \"location\":\"%s\"}",
			status_id, xmlstr.action_Node_id.c_str(), xmlstr.location_Node_id.c_str());
		printf("action id: %s location id: %s raw_text text: %s confidence value: %s\n\n",
			xmlstr.action_Node_id.c_str(),
			xmlstr.location_Node_id.c_str(),
			xmlstr.rawtext_Node_text.c_str(),
			xmlstr.confidence_Node_id.c_str());
	}
	//发送UDP
	udpsend = new UDPSend();
	udpsend->SendData((char*)asr_params, MAX_LEN);
	//发送完之后一定要释放
	udpsend->Close();
}