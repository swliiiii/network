#pragma once
#include "Monitor.h"
#include <mutex>
#include <atomic>
#include <deque>
#include "../Tool/TRingBuff.h"
#include "../Tool/TBuff.h"

//UDP通讯类
class CUdpM : public CContextBase
{
public:

	//接收到数据的回调函数
	using CT_DataRecv = void(*) (
		std::shared_ptr<void> pContext,	//环境变量
		const char *pData,								//接收到的数据指针
		int nLen,									//接收到的数据字节数
		const std::string &strFromIP,				//数据源IP
		short nFromPort								//数据源端口
		);
	
	//构造函数
	CUdpM();

	//析构
	virtual ~CUdpM();

	//注册接收到数据的回调函数
	void RegDataRecv(CT_DataRecv pfn, std::shared_ptr<void> pContex);

	/*
	注意：本地IP地址不允许设置为空，这是本类的一个设计缺陷，由于使用了IOCP，当想要停止工作时，还没有找到
	比较合理有效的方式通知IOCP（disconnectEx、shutdown对UDP是无意义的，closesocket则可能引入socket被立即
	重用导致IOCP中的socket信息被覆盖的问题，所以都不可用），所以设计为：使用本socket向本地址发送数据，
	当接收到本地址发给自己的数据时，认为用户要停止工作，不再继续投递操作。基于判断数据源是否来自本socket所在
	地址的需要，必须设置本地IP。
	*/
	
	//创建Socket并绑定地址
	bool PreSocket(
		const std::string &strLocalIP,			//要使用的本地IP
		unsigned short nLocalPort,				//要使用的本地端口
		bool bReuse = true						//是否允许端口复用
	);

	//开始工作
	bool Start(
		int nRecvBuf = 0,
		int nSendBuf = 0,
		unsigned short nPort = 0,
		const std::string &strLocalIP = "", 
		const std::string &strMultiIP = "",
		bool bReuse = false
	);

	//停止工作
	bool QStop();

	//发送数据
	bool Send(const char *pData, int nLen, const std::string &strToIP = "", unsigned short nToPort = 0);

	//获取本地端口
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//获取本地IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//套接字
	SOCKET m_socket;

	//是否要退出了
	std::atomic<bool> m_bStop;

	//接收操作是否停止了
	std::atomic<bool> m_bStopRecv;

	//绑定的本地IP
	std::string m_strLocalIP;

	//绑定的本地端口
	unsigned short m_nLocalPort;

	//组播地址
	std::string m_strMultiIP;

	//发送相关的变量及访问保护
	std::mutex m_mutextContext;					//保护锁
	CMonitor::SendToContext *m_pContextSend;	//投递Send成功后应该置为NULL

	//接收到数据的输出回到函数和环境变量
	CT_DataRecv m_pfnDataRecv;
	std::shared_ptr<void> m_pRecvDataContext;

	//发送数据节点
	struct BuffNode
	{
		std::string strDstIP;
		unsigned short nDstPort;
		Tool::TBuff<char> buff;

		BuffNode() : nDstPort(0){}

		void swap(BuffNode &node)
		{
			buff.swap(node.buff);
			strDstIP.swap(node.strDstIP);
			std::swap(nDstPort, node.nDstPort);
		}
	};

	Tool::TRingBuff<BuffNode> m_collBuff;

protected:

	//发送数据, 调用本接口一定要外部自己锁定发送互斥体
	void ExeSend(const char *pData, int nLen, const std::string &strToIP = "", unsigned short nToPort = 0);

	//处理接收到的数据
	virtual void OnRecvData(char *pData, int nLen, const std::string &strFromIP, unsigned short nFromPort);

	//处理发送成功
	virtual void OnSent();

	//处理错误
	virtual void OnErr();

	//错误发生的回调函数
	static void ErrCB(
		std::shared_ptr<void>  pContext,					//环境变量
		SOCKET s,								//发生错误的套接字
		int nErrCode							//错误代码，同GetlastErr()的返回值
		);

	//接收到数据的回调函数
	static bool RecvFromCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//接收到数据的套接字
		char *pData,							//接收到的数据指针
		int nLen,								//接收到的数据字节								
		const std::string strFromIP,			//数据源IP
		unsigned short nFromPort							//数据源端口
		);

	//发送数据结果的回调函数
	static void SentToCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//发送数据的socket
		int nSent,								//实际发送的字节数
		CMonitor::SendToContext *pContextSend	//发送的信息结构体，外部获得本上下文值后，即取得了控制权，如果不再使用，请使用delete删除，或者通过PostSendTo（SendContext*）接口继续发送数据,交出控制权。
		);

	//退出前清理状态和资源
	void Clear();

	//关闭socket
	void CloseSocket(SOCKET &s);

};


