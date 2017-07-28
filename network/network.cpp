// network.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "UdpM.h"
#include "TcpClient.h"
#include  "TcpServer.h"
#include "Input.h"

//���ڼ�¼Tcp����
std::vector<std::weak_ptr<CTcpClientBase>> g_vecClient;

class CUdpData : public CContextBase
{
public:

	CUdpData() {}
	~CUdpData() {}

	static void DataCB(
		std::shared_ptr<void> pContext,	//��������
		const char *pData,							//���յ�������ָ��
		int nLen,									//���յ��������ֽ���
		const std::string &strFromIP,				//����ԴIP
		short nFromPort								//����Դ�˿�
	)
	{
		//������յ���UDP���ݣ�����ٷ��أ��������������������Ĺ����߳�
		auto pThis = (CUdpData*)(pContext.get());
	}	
};

//��ʱ���ص�����
 void TimerCB(
		std::shared_ptr<void> pContext,	//��������
		int nTimer						//��ʱ��ID
	)
	{
		//��ʱ������������ٷ��أ��������������������Ĺ����߳�
	}
  

int DataCB(
	std::shared_ptr<CContextBase> pContext,		//��������
	Tool::TBuff<char> &Data,					//���յ������ݻ���
	std::weak_ptr<CContextBase> pTcp			//�����߱����ָ��
)
{
	//����Tcp���յ����ݣ�����ٷ��أ��������������������Ĺ����߳�
	return (int)Data.size();
}

bool ClientCB(
	std::shared_ptr<CContextBase> pContext,
	std::shared_ptr<CTcpClientBase> pClient,
	const std::string &strRemoteIP,
	int nRemotePort
)
{
	//�����µ�Tcp���ӣ�����ٷ��أ��������������������Ĺ����߳�
	pClient->RegCB(DataCB, nullptr, nullptr, nullptr);
	g_vecClient.push_back(pClient);
	return true;
}


int main()
{
	Input::CInput input;
	input.CaptureSignal();

	//
	//CUdpM��ʹ��ʾ��(CTcpClient��ʹ�÷�����֮���ƣ�����׸��)
	//
	
	//������յ���UDP���ݵĶ���
	auto pUdpData = CUdpData::Create<CUdpData>();
	
	//������������������ʼ��
	auto pMonitor = std::make_shared<CMonitor>();
	pMonitor->Init();
	int nTimer = pMonitor->AddTimer(1000, nullptr, TimerCB);

	//����udp��������
	auto pUdpM = CUdpM::Create<CUdpM>();
	pUdpM->RegDataRecv(CUdpData::DataCB, pUdpData);
	pUdpM->SetMonitor(pMonitor);
	pUdpM->Start(0, 0, 5000, "127.0.0.1");
	//pUdpM->Send(.......);

	//�����ȴ��û�����Ctrl+C
	LogN(111)("��ʾ������Ctrl+C������һ����������");
	input.Loop();

	//ֹͣudp����
	pUdpM->QStop();
	{
		std::weak_ptr<CUdpM> p = pUdpM;
		pUdpM.reset();
		CContextBase::WaitDestroy(p);
	}	

	//
	//CTcpServer���÷�ʾ��
	//

	//����CTcpServer
	auto pServer = CContextBase::Create<CTcpServer<CTcpClientBase>>();
	pServer->SetMonitor(pMonitor);
	pServer->RegClientCB(ClientCB, nullptr);
	pServer->Start("", 6000);

	//�����ȴ��û�����Ctrl+C
	LogN(111)("��ʾ������Ctrl+C����");
	input.Loop();

	//ֹͣCTcpServer
	pServer->QStop();
	{
		std::weak_ptr<CTcpServer<CTcpClientBase>> p = pServer;
		pServer.reset();
		CContextBase::WaitDestroy(p);
	}

	//ȷ�����еĿͻ��˹ر�
	for (auto p : g_vecClient)
	{
		if (!p.expired())
		{
			auto pClient = p.lock();
			if (nullptr != pClient)
			{
				pClient->QStop();
			}
		}
	}
	for (auto p: g_vecClient)
	{
		CContextBase::WaitDestroy(p);
	}

	//ɾ����ʱ��
	if (-1 != nTimer)
	{
		pMonitor->DelTimer(nTimer);
	}

	//����ʼ������������
	pMonitor->DeInit();

    return 0;
}

