#pragma once
#include "Monitor.h"
#include <thread>
#include <sstream>
#include <mutex>
#include <WS2tcpip.h>
#include <atomic>
#include "../Tool/TBuff.h"
#include "../Tool/TLog.h"

//处理一个Tcp连接的基类
class CTcpClientBase : public CContextBase
{
public:

	//连接状态通知回调函数
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//环境变量
		bool bConnect								//连接标识
		);

	//接收到数据的回调函数，返回已经处理的数据长度（说明：已经被处理的数据，将在被对象内部删除）
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//环境变量
		Tool::TBuff<char> &Data,					//接收到的数据缓冲
		std::weak_ptr<CContextBase> pTcp			//接收者本身的指针
		);


	//构造
	CTcpClientBase(
		SOCKET s,									//本次连接的套接字
		const std::string &strRemoteIP,				//远端IP 
		unsigned short nRemotePort					//远端端口
	);

	//析构
	virtual ~CTcpClientBase();

	//注册接收到数据的回调函数
	void RegCB(
		CT_Data pfnData,
		std::shared_ptr<CContextBase> pContextData,
		CT_Connect pfnConnect, 
		std::shared_ptr<CContextBase> pContextConnect
	);

	//开始收发数据
	virtual bool Start(
		int nRecvBuf = 0,							//接收缓冲区大小，0标识使用默认值
		int nSendBuf = 0							//发送缓冲区大小，0标识使用默认值
	);

	//通知停止连接
	virtual bool QStop();

	//设置缓冲区大小
	bool SetSocketBuff(
		int nRecvBuf = 0,							//接收缓冲区大小，0标识使用默认值
		int nSendBuf = 0							//发送缓冲区大小，0标识使用默认值
	);

	//投递发送数据
	int Send(
		const char *pData,			//待发送的数据指针
		int nLen					//待发送的数据长度
	);

	//获取远程IP和端口
	std::string & GetRemoteIP() { return m_strRemoteIP; }
	unsigned short GetRemotePort() { return m_nRemotePort; }

protected:

	//套接字
	SOCKET m_socket;

	//socket是否被shutdown
	std::atomic<bool> m_bShutdown;

	//远程的IP和端口
	std::string m_strRemoteIP;
	unsigned short m_nRemotePort;

	//发送相关变量及访问保护
	std::mutex m_mutextContext;				//保护锁
	CMonitor::SendContext *m_pContextSend;	//投递的环境变量

	//接收缓冲区
	Tool::TBuff<char> m_buffRecv;

	//接收的数据输出回调函数
	CT_Data m_pfnRecvData;
	std::shared_ptr<CContextBase> m_pRecvData;

	//连接通断输出回调函数
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pConnect;

	//发送缓冲区
	Tool::TBuff<char> m_buffSend;

protected:

	//实际投递发送， 需要调用本函数者锁定发送保护锁
	int ExeSend(const char *pData, int nLen);

	//处理接收到的数据
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//处理发送成功
	virtual void OnSendable();

	//发生错误的处理
	virtual void OnError();

	//发送结果回调函数
	static void SentCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//发送数据的socket
		int nSent,								//实际发送的字节数
		CMonitor::SendContext *pContextSend		//发送的信息结构体，外部通过临时保存该变量后，给本变量赋值然后调用Send接口发送
	);

	//接收到数据的处理回调函数
	static void RecvDataCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//接收到数据的套接字
		char *pData,							//接收到的数据指针
		int nLen								//接收到的数据字节
	);

	//发生错误的处理回调函数
	static void ErrCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//发生错误的套接字
		int nErrCode							//错误代码，同GetlastErr()的返回值
	);

	//关闭一个socket
	void CloseSocket(SOCKET &s);
};

#define LogTS LogN(104)

//Tcp服务器类
template<class T>
class CTcpServer : public CContextBase
{
public:

	//构造
	CTcpServer()
		: m_sock(INVALID_SOCKET)
		, m_pfnClient(nullptr)

	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	//析构
	virtual ~CTcpServer()
	{
		LogTS("~CTcpServer");
		CloseSocket(m_sock);
		WSACleanup();
	}

	//创建客户端对象通知回调函数，返回true表示自动启动该连接，返回false表示不要自动启动该链接
	using CT_Client = bool(*)(
		std::shared_ptr<CContextBase> pContext,
		std::shared_ptr<T> pClient,
		const std::string &strRemoteIP,
		int nRemotePort
		);

	//注册创建客户端的通知回调函数
	void RegClientCB(CT_Client pfn, std::shared_ptr<CContextBase> pContext)
	{
		m_pfnClient = pfn;
		m_pContext = pContext;
	}

	//获取设置的本地地址
	std::string LocalIP() { return m_strLocalIP; }
	unsigned short LocalPort() { return m_nLocalPort; }

	//尝试绑定地址
	bool PreSocket(const std::string &strIP, unsigned short nPort)
	{
		//创建并开始侦听socket
		m_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == m_sock)
		{
			LogTS("[%s]创建socket失败!", __FUNCTION__);
			return false;
		}
		sockaddr_in addr{ 0 };
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, strIP.c_str(), (void*)&addr.sin_addr);
		addr.sin_port = htons(nPort);
		if (SOCKET_ERROR == bind(m_sock, (sockaddr*)&addr, sizeof(addr)))
		{
			LogTS("[%s]绑定地址失败<%s:%d> -- <%d>", __FUNCTION__, strIP.c_str(), nPort, GetLastError());
			QStop();
			return false;
		}
		m_strLocalIP = strIP;
		m_nLocalPort = nPort;
		return true;
	}

	//开始侦听
	bool Start(
		const std::string &strIP = "",			//侦听的IP地址
		unsigned short nPort = 0				//侦听的端口号
	)
	{
		if (nullptr == GetMonitor())
		{
			LogTS("[%s]还没有设置网络驱动器！", __FUNCTION__);
			return false;
		}

		if (INVALID_SOCKET == m_sock && !PreSocket(strIP, nPort))
		{
			return false;
		}
	
		if (SOCKET_ERROR == listen(m_sock, SOMAXCONN))
		{
			LogTS("[%s]启动侦听失败<%s:%d>", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
			QStop();
			return false;
		}
		if (!GetMonitor()->Attach(m_sock))
		{
			LogTS("[%s]添加到完成端口失败<%s:%d>", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
			QStop();
			return false;
		}

		//将侦听socket和完成端口绑定
		if (!GetMonitor()->PostAcceptEx(m_sock, ClientCB, m_pThis.lock(), ErrCB, m_pThis.lock(), 5))
		{
			LogTS("[%s]投递AcceptEx失败<%s:%d>", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
			QStop();
			return false;
		}
		LogTS("[%s]开始在 <%s:%d> 侦听！", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
		return true;
	}

	//停止服务器及所有的客户端
	bool QStop()
	{
		if (INVALID_SOCKET != m_sock)
		{
			//listen的socket似乎是不能被shutdown，只能Closesocket了
			//但是这样是有close该socket后，该sokcet立即重用，导致完成端口
			//上记录的回调信息错乱的可能性，虽然一般在程序退出时最后关闭lisentsocket而不会出现问题
			//但是当有两个lisent的服务器时，尤其要注意这种问题，可以将两个服务器注册到两个完成端口。
			int nRet = shutdown(m_sock, SD_RECEIVE);//此处执行失败
			CloseSocket(m_sock);
		}
		return true;
	}

protected:


	//客户端连接的处理回调函数
	static void ClientCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//客户端连接socket，调用者在使用完毕该socket后，应当关闭该socket
		const std::string &strRemoteIP,			//客户端IP
		int nRetmotePort						//客户端端口
	)
	{
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}
		setsockopt(s,
			SOL_SOCKET,
			SO_UPDATE_ACCEPT_CONTEXT,
			(char*)&pThis->m_sock,
			sizeof(pThis->m_sock)
			);
		auto p = std::make_shared<T>(s, strRemoteIP, nRetmotePort);
		p->SetMonitor(pThis->GetMonitor());
		p->SetThisPtr(p);
		bool bStart = true;
		if (nullptr != pThis->m_pfnClient)
		{
			bStart = pThis->m_pfnClient(pThis->m_pContext, p, strRemoteIP, nRetmotePort);
		}
		if (bStart)
		{
			p->Start();
		}
	}

	//发生错误的处理回调函数
	static void ErrCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//发生错误的套接字
		int nErrCode							//错误代码，同GetlastErr()的返回值
	)
	{
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}
		if (INVALID_SOCKET != pThis->m_sock)
		{
			shutdown(pThis->m_sock, SD_BOTH);
		}
	}

	//关闭一个套接字
	void CloseSocket(SOCKET &s)
	{
		if (INVALID_SOCKET != s)
		{
			closesocket(s);
			s = INVALID_SOCKET;
		}
	}

protected:

	//侦听的套接字
	SOCKET m_sock;

	//创建客户端对象的回调函数和环境变量
	CT_Client m_pfnClient;
	std::shared_ptr<CContextBase> m_pContext;

	//本地IP和端口
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;
};
