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
	printf("Sending data >>>> %s\n\n", lpFrame);
	sendto(SendSocket, lpFrame, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
}

//关闭连接
void UDPSend::Close()
{
	printf("finished sending, close socket.\n\n");
	closesocket(SendSocket);
	printf("Exting.\n\n");
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
	TiXmlNode* traffic = NULL;
	TiXmlNode* education = NULL;
	TiXmlNode* business = NULL;
	TiXmlNode* park = NULL;
	TiXmlNode* hospital = NULL;
	TiXmlNode* administrative = NULL;
	TiXmlNode* cinema = NULL;
	TiXmlNode* other = NULL;
	TiXmlNode* housetype = NULL;
	//数值节点
	TiXmlNode* num_pre = NULL;
	TiXmlNode* nums1 = NULL;
	TiXmlNode* nums2 = NULL;
	TiXmlNode* ten = NULL;
	//构造两个节点
	TiXmlNode* action_Node = NULL;
	TiXmlNode* location_Node = NULL;

	//开始迭代查找
	motion = object_Node->IterateChildren("motion", motion);
	menu = object_Node->IterateChildren("menu", menu);
	properties = object_Node->IterateChildren("properties", properties);
	traffic = object_Node->IterateChildren("traffic", traffic);
	education = object_Node->IterateChildren("education", education);
	business = object_Node->IterateChildren("business", business);
	park = object_Node->IterateChildren("park", park);
	hospital = object_Node->IterateChildren("hospital", hospital);
	administrative = object_Node->IterateChildren("administrative", administrative);
	cinema = object_Node->IterateChildren("cinema", cinema);
	other = object_Node->IterateChildren("other", other);
	housetype = object_Node->IterateChildren("housetype", housetype);
	//数值
	nums1 = object_Node->IterateChildren("nums", nums1);
	if (nums1 != NULL) num_pre = nums1->PreviousSibling("ten");
	if (nums1 != NULL) nums2 = nums1->NextSibling("nums");
	ten = object_Node->IterateChildren("ten", ten);
	//判断action
	if (motion != NULL) action_Node = motion;
	if (menu != NULL) action_Node = menu;
	//判断location
	if (properties != NULL) location_Node = properties;
	if (traffic != NULL) location_Node = traffic;
	if (education != NULL) location_Node = education;
	if (business != NULL) location_Node = business;
	if (park != NULL) location_Node = park;
	if (hospital != NULL) location_Node = hospital;
	if (administrative != NULL) location_Node = administrative;
	if (cinema != NULL) location_Node = cinema;
	if (other != NULL) location_Node = other;
	if (housetype != NULL) location_Node = housetype;

	//处理楼层数值
	string num_id = "";
	if (nums1 != NULL)
	{
		if (nums2 != NULL)
		{
			if (ten != NULL)
			{
				string nums1_id = nums1->ToElement()->Attribute("id");
				string nums2_id = nums2->ToElement()->Attribute("id");
				string ten_id = ten->ToElement()->Attribute("id");
				num_id = nums1_id + ten_id + nums2_id;
			}
			else
			{
				string nums1_id = nums1->ToElement()->Attribute("id");
				string nums2_id = nums2->ToElement()->Attribute("id");
				num_id = nums1_id + "10" + nums2_id;
			}
		}
		else
		{
			if (num_pre != NULL)
			{
				string nums1_id = nums1->ToElement()->Attribute("id");
				string ten_id = ten->ToElement()->Attribute("id");
				num_id = "1" + ten_id + nums1_id;
			}
			else
			{
				if (ten != NULL)
				{
					string nums1_id = nums1->ToElement()->Attribute("id");
					string ten_id = ten->ToElement()->Attribute("id");
					num_id = nums1_id + ten_id + "0";
				}
				else
				{
					string nums1_id = nums1->ToElement()->Attribute("id");
					num_id = nums1_id;
				}
			}
		}
	}
	else
	{
		if (ten != NULL)
		{
			string ten_id = ten->ToElement()->Attribute("id");
			num_id = ten_id;
		}
	}

	//先判断有没有closed
	if (closed_Node != NULL)
	{
		xml_action_loc.action_Node_id = "";
		xml_action_loc.location_Node_id = "";
		xml_action_loc.confidence_Node_id = "";
		xml_action_loc.rawtext_Node_text = "";
		return xml_action_loc;
	}
	else if (num_id != "")
	{
		xml_action_loc.flag = 1;
		xml_action_loc.action_Node_id = num_id;
		xml_action_loc.location_Node_id = "";
		xml_action_loc.confidence_Node_id = confidence_Node->ToElement()->GetText();
		xml_action_loc.rawtext_Node_text = rawtext_Node->ToElement()->GetText();
		return xml_action_loc;
	}
	else
	{
		xml_action_loc.flag = 1;
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
			xml_action_loc.action_Node_id = action_Node->ToElement()->Attribute("id");
			xml_action_loc.location_Node_id = location_Node->ToElement()->Attribute("id");
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
	int id_int = atoi(xmlstr.action_Node_id.c_str());
	if (intStr <= 20)
	{
		status_id = 0;
		PlaySound(TEXT("./sounds1.1/nounderstand.wav"), NULL, SND_FILENAME | SND_ASYNC);
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
		//这里播放"好的"音频
		PlaySound(TEXT("./sounds1.1/alright.wav"), NULL, SND_FILENAME | SND_ASYNC);
		if (id_int < 3000000)
		{
			udpsend = new UDPSend();//发送UDP
			udpsend->SendData((char*)asr_params, MAX_LEN);
			udpsend->Close();//发送完之后一定要释放
		}
		else
		{
			PlaySound(TEXT("./sounds1.1/apologize.wav"), NULL, SND_FILENAME | SND_ASYNC);
		}
	}
}