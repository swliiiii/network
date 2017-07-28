// network.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "UdpM.h"
#include "TcpClient.h"
#include  "TcpServer.h"
#include "Input.h"

//用于记录Tcp链接
std::vector<std::weak_ptr<CTcpClientBase>> g_vecClient;

class CUdpData : public CContextBase
{
public:

	CUdpData() {}
	~CUdpData() {}

	static void DataCB(
		std::shared_ptr<void> pContext,	//环境变量
		const char *pData,							//接收到的数据指针
		int nLen,									//接收到的数据字节数
		const std::string &strFromIP,				//数据源IP
		short nFromPort								//数据源端口
	)
	{
		//处理接收到的UDP数据，请快速返回，以免阻塞网络驱动器的工作线程
		auto pThis = (CUdpData*)(pContext.get());
	}	
};

//定时器回调函数
 void TimerCB(
		std::shared_ptr<void> pContext,	//环境变量
		int nTimer						//定时器ID
	)
	{
		//定时器函数，请快速返回，以免阻塞网络驱动器的工作线程
	}
  

int DataCB(
	std::shared_ptr<CContextBase> pContext,		//环境变量
	Tool::TBuff<char> &Data,					//接收到的数据缓冲
	std::weak_ptr<CContextBase> pTcp			//接收者本身的指针
)
{
	//处理Tcp接收到数据，请快速返回，以免阻塞网络驱动器的工作线程
	return (int)Data.size();
}

bool ClientCB(
	std::shared_ptr<CContextBase> pContext,
	std::shared_ptr<CTcpClientBase> pClient,
	const std::string &strRemoteIP,
	int nRemotePort
)
{
	//处理新的Tcp连接，请快速返回，以免阻塞网络驱动器的工作线程
	pClient->RegCB(DataCB, nullptr, nullptr, nullptr);
	g_vecClient.push_back(pClient);
	return true;
}


int main()
{
	Input::CInput input;
	input.CaptureSignal();

	//
	//CUdpM的使用示例(CTcpClient的使用方法与之类似，不再赘述)
	//
	
	//处理接收到的UDP数据的对象
	auto pUdpData = CUdpData::Create<CUdpData>();
	
	//创建网络驱动器并初始化
	auto pMonitor = std::make_shared<CMonitor>();
	pMonitor->Init();
	int nTimer = pMonitor->AddTimer(1000, nullptr, TimerCB);

	//创建udp对象并启动
	auto pUdpM = CUdpM::Create<CUdpM>();
	pUdpM->RegDataRecv(CUdpData::DataCB, pUdpData);
	pUdpM->SetMonitor(pMonitor);
	pUdpM->Start(0, 0, 5000, "127.0.0.1");
	//pUdpM->Send(.......);

	//阻塞等待用户输入Ctrl+C
	LogN(111)("提示：输入Ctrl+C进入下一个处理流程");
	input.Loop();

	//停止udp对象
	pUdpM->QStop();
	{
		std::weak_ptr<CUdpM> p = pUdpM;
		pUdpM.reset();
		CContextBase::WaitDestroy(p);
	}	

	//
	//CTcpServer的用法示例
	//

	//创建CTcpServer
	auto pServer = CContextBase::Create<CTcpServer<CTcpClientBase>>();
	pServer->SetMonitor(pMonitor);
	pServer->RegClientCB(ClientCB, nullptr);
	pServer->Start("", 6000);

	//阻塞等待用户输入Ctrl+C
	LogN(111)("提示：输入Ctrl+C结束");
	input.Loop();

	//停止CTcpServer
	pServer->QStop();
	{
		std::weak_ptr<CTcpServer<CTcpClientBase>> p = pServer;
		pServer.reset();
		CContextBase::WaitDestroy(p);
	}

	//确保所有的客户端关闭
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

	//删除定时器
	if (-1 != nTimer)
	{
		pMonitor->DelTimer(nTimer);
	}

	//反初始化网络驱动器
	pMonitor->DeInit();

    return 0;
}

