using namespace std;

//���巵�ؽṹ��
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
//����һ��ȫ����Ϣ����
public:
	UDPSend();
	~UDPSend();
	void SendData(char* lpFrame, int length);
	void Close();
};
//��������
xml_string any_xml(char* xmlstr);
void send_simple(xml_string xmlstr);