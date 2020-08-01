using namespace std;

//定义返回结构体
typedef struct xml_action_location
{
	int flag = 0;
	string action_Node_id;
	string location_Node_id;
	string confidence_Node_id;
	string rawtext_Node_text;
}xml_string;

class UDPSend
{
//定义一个全局消息变量
public:
	UDPSend();
	~UDPSend();
	void SendData(char* lpFrame, int length);
	void Close();
};
//函数声明
xml_string any_xml(char* xmlstr);
void send_simple(xml_string xmlstr);