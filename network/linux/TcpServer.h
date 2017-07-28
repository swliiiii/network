#pragma once
#include "Monitor.h"
#include <string>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "Tool/TBuff.h"
#include "Tool/TLog.h"

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1 
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef SOCKET
#define SOCKET int
#endif // !SOCKET

//处理一个Tcp连接的基类
class CTcpClientBase : public CContextBase
{
public:

	//连接状态通知回调函数
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//环境变量
		bool bConnect						//连接标识
		);

	//接收到数据的回调函数，返回已经处理的数据长度（说明：已经被处理的数据，将在被对象内部删除）
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//环境变量
		Tool::TBuff<char> &Data,					//接收到的数据缓冲
		std::weak_ptr<CContextBase> pTcp	//接收者本身的指针
		);


	//构造
	CTcpClientBase(int s, const std::string &strRemoteIP, unsigned short nRemotePort);

	//析构
	virtual ~CTcpClientBase();

	//注册接收到数据的回调函数
	void RegCB(CT_Data pfn, std::shared_ptr<CContextBase> pContext, CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect);

	//开始收发数据
	virtual bool Start(
		int nRecvBuf = 0,							//接收缓冲区大小，0标识使用默认值
		int nSendBuf = 0							//发送缓冲区大小，0标识使用默认值
		);

	//通知关闭连接
	virtual bool QStop();

	//设置缓冲区大小
	bool SetSocketBuff(
		int nRecvBuf = 0,							//接收缓冲区大小，0标识使用默认值
		int nSendBuf = 0							//发送缓冲区大小，0标识使用默认值
	);

	//发送数据
	int Send(const char *pData, int nLen);

	//获取远程IP和端口
	std::string & GetRemoteIP() { return m_strRemoteIP; }
	unsigned short GetRemotePort() { return m_nRemotePort; }

protected:

	//套接字
	int m_socket;

	//远程的IP和端口
	std::string m_strRemoteIP;
	unsigned short m_nRemotePort;

	//停止标识
	bool m_bStop;

	//接收缓冲区
	Tool::TBuff<char> m_buffRecv;

	//接收的数据输出回调函数
	CT_Data m_pfnRecvData;
	std::shared_ptr<CContextBase> m_pRecvData;

	//连接通断输出回调函数
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pConnect;

protected:

	//接收到数据的实际处理函数
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//可发送的处理函数
	virtual void OnSendable();

	//即将退出时的处理
	virtual void OnError();

	//接收到数据的通知回调函数
	static void RecvDataCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd						//发生事件的文件描述符
	);

	//可以发送数据的通知回调函数
	static void SendableCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd						//发生事件的文件描述符
	);

	//发生错误的通知回调函数
	static void ErrCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd,					//发生事件的文件描述符
		int nError					//错误信息
	);

	//关闭socket
	void CloseSocket(int &s);

};


#define LogTS LogN(104)

//Tcp服务器类
template<class T>
class CTcpServer : public CContextBase
{
public:

	//构造，禁止外部直接创建对象，应该调用Create接口
	CTcpServer()
		: m_socket(INVALID_SOCKET)
		, m_pfnClient(nullptr)

	{
		LogTS("CTcpServer");
	}

	//析构
	virtual ~CTcpServer()
	{
		LogTS("~CTcpServer");
		CloseSocket(m_socket);
	}


	//创建客户端对象通知回调函数，返回true表示自动启动该连接，返回false表示不要自动启动该链接
	using CT_Client = bool(*)(
		std::shared_ptr<CContextBase> pContext,
		std::shared_ptr<T> pClient,
		const std::string &strRemoteIP,
		int nRemotePort
		);

	//注册创建客户端的通知回调函数
	void RegClientCB(CT_Client pfn, std::shared_ptr<CContextBase>  pContext)
	{
		m_pfnClient = pfn;
		m_pContext = pContext;
	}

	//获取设置的本地地址
	std::string LocalIP() { return m_strLocalIP; }
	unsigned short LocalPort() { return m_nLocalPort; }
	
	bool PreSocket(const std::string &strIP, unsigned short nPort)
	{
		//创建socket
		m_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (INVALID_SOCKET == m_socket)
		{
			LogTS("[CTcpServer::Start] create socket failed! -- error <%d - %s>",
				errno, strerror(errno));
			return false;
		}

		//绑定本机地址
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(nPort);
		inet_pton(AF_INET, strIP.c_str(), (void*)&addr.sin_addr);
		if (-1 == bind(m_socket, (sockaddr*)&addr, sizeof addr))
		{
			LogTS("[CTcpServer::Start] <%s:%d> bind failed! -- error<%d - %s>",
				strIP.c_str(), nPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}

		//设置为非阻塞的
		if (!SetNonblocking(m_socket))
		{
			CloseSocket(m_socket);
			return false;
		}
		m_strLocalIP = strIP;
		m_nLocalPort = nPort;
		return true;
	}
			
	//启动服务器
	bool Start(
		const std::string &strIP = "",						//侦听的IP地址
		unsigned short nPort = 0							//侦听的端口号
	)
	{
		if (nullptr == GetMonitor())
		{
			LogTS("[CTcpServr::Start] failed -- NO Monitor yet!");
			return false;
		}
		if (INVALID_SOCKET == m_socket && !PreSocket(strIP, nPort))
		{
			return false;
		}

		//启动侦听
		if (-1 == listen(m_socket, 20))
		{
			LogTS("[CTcpServer::Start] listen at <%s:%d>  failed! -- error<%d - %s>",
				m_strLocalIP.c_str(), m_nLocalPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}

		//添加到CMonitor
		if (!GetMonitor()->Add(m_socket, EPOLLIN, ClientCB, m_pThis.lock(), nullptr, m_pThis.lock(), ErrCB, m_pThis.lock()))
		{
			LogTS("[CTcpServer::Start] <%s:%d> Add to CMonitor failed! -- error<%d - %s>",
				m_strLocalIP.c_str(), m_nLocalPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}
		LogTS("[CTcpServr::Start]  Listen at <%s:%d>", m_strLocalIP.c_str(), m_nLocalPort);
		return true;
	}

	//停止服务器
	bool QStop()
	{
		LogTS("[CTcpServer::QStop] socket<%d>", m_socket);
		if (INVALID_SOCKET != m_socket)
		{
			shutdown(m_socket, SHUT_RDWR);		
		}
	}

	//打印内部信息
	void Print()
	{
	}

protected:
	
	//客户端连接的处理回调函数
	static void ClientCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd						//发生事件的文件描述符
	)
	{
		LogTS("[CTcpServer::ClientCB]");
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}

		while (true)
		{
			sockaddr_in addrPeer;
			socklen_t nAddrLen = sizeof(addrPeer);
			int s = accept(pThis->m_socket, (sockaddr*)&addrPeer, &nAddrLen);
			if (-1 == s)
			{
				if (EAGAIN == errno)
				{
					return;
				}
				LogTS("[CTcpServer::ClientCB] accept fiale -- Errno<%d - %s>", errno, strerror(errno));
				continue;				 
			}
			if (!pThis->SetNonblocking(s))
			{
				LogTS("[CTcpServer::ClientCB] setNonblocking failed, close the socket!");
				pThis->CloseSocket(s);
				continue;;
			}
			char buff[32]{ 0 };
			inet_ntop(AF_INET, (void*)&(addrPeer.sin_addr), buff, 64);	
			auto p = std::make_shared<T>(s, buff, htons(addrPeer.sin_port));
			p->SetMonitor(pThis->GetMonitor());
			p->SetThisPtr(p);
			bool bStart = true;
			if (nullptr != pThis->m_pfnClient)
			{
				bStart = pThis->m_pfnClient(pThis->m_pContext, p, buff, htons(addrPeer.sin_port));
			}	
			if (bStart)
			{
				p->Start();
			}
		}
	}

	//发生错误的处理回调函数
	static void ErrCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd,					//发生事件的文件描述符
		int nError					//错误信息
	)
	{
		LogTS("[CTcpServer::ErrCB]");
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}
		pThis->CloseSocket(pThis->m_socket);
	}
	
	//将一个socket设置为非阻塞的
	bool SetNonblocking(int nFd)
	{
		int nFlags = fcntl(nFd, F_GETFL);
		if (-1 == nFlags)
		{
			//不应该发生的事件
			LogTS("[CTcpServer::SetNonblocking] getfl failed, must be care -- errno<%d - %s>",
				errno, strerror(errno));
			return false;
		}
		nFlags |= O_NONBLOCK;
		if (-1 == fcntl(nFd, F_SETFL, nFlags))
		{
			LogTS("[CTcpServer::SetNonblocking] setfl failed, must be care -- errno<%d - %s>",
				errno, strerror(errno));
			return false;
		}
		return true;
	}

	//关闭socket
	void CloseSocket(int &s)
	{
		if (INVALID_SOCKET != s)
		{
			LogTS("close socket <%d>", s);
			close(s);
			s = INVALID_SOCKET;
		}	
	}
	
protected:

	//侦听的套接字
	int m_socket;

	//创建客户端对象的回调函数和环境变量
	CT_Client m_pfnClient;
	std::shared_ptr<CContextBase> m_pContext;

	//本地IP和端口
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;
};
