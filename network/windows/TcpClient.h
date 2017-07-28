#pragma once
#include "Monitor.h"
#include <mutex>
#include <atomic>
#include "../Tool/TBuff.h"

//Tcp客户端类
class CTcpClient :	public CContextBase
{
public:	

	//构造函数
	CTcpClient();
	
	//析构
	virtual ~CTcpClient();

	//连接状态输出回调函数
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//环境变量
		bool bConnected								//true链接 false断开
		);

	//接收到数据的回调函数，返回已经处理的数据长度（说明：已经被处理的数据，将在被对象内部删除）
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//环境变量
		 Tool::TBuff<char> &Data							//接收到的数据缓冲
		);

	//注册接收到数据的回调函数
	void RegCallback(
		CT_Data pfnData,						//接收到数据的回调函数指针
		std::shared_ptr<CContextBase> pContextData,		//接收到数据的回调函数环境变量
		CT_Connect pfnConnect,					//连接成功的回调函数指针
		std::shared_ptr<CContextBase> pContextConnect	//连接成功的回调函数环境变量
	);

	//创建Socket并绑定地址
	bool PreSocket(
		const std::string &strLocalIP,			//要使用的本地IP
		unsigned short nLocalPort				//要使用的本地端口
	);

	//开始连接
	bool Start(
		const std::string &strServerIP,			//服务器IP
		unsigned short nServerPort,				//服务器端口
		int nRecvBuf = 0,						//接收缓冲区大小，0标识使用默认值
		int nSendBuf = 0,						//发送缓冲区大小，0标识使用默认值
		const std::string &strLocalIP = "",		//本地IP
		unsigned short nLocalPort = 0			//本地端口
	);

	//通知停止连接
	bool QStop();

	//投递发送数据
	int Send(const char *pData, int nLen);

	//获取本地端口
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//获取本地IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//套接字
	SOCKET m_socket;

	//套接字是否被shutdown了
	std::atomic<bool> m_bShutdown;

	//发送相关的变量及保护锁
	std::mutex m_mutextContext;				//保护锁
	CMonitor::SendContext *m_pContextSend;	//投递Send成功后应该置为NULL

	//链接标识
	std::string m_strFlag;

	//接收到数据的输出回到函数和环境变量
	CT_Data m_pfnData;
	std::shared_ptr<CContextBase> m_pContextData;

	//连接状态的输出回调函数和环境变量
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pContextConnect;

	//服务器地址
	sockaddr_in m_addrServer;

	//本地IP地址和端口
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;

	//接收缓冲区
	Tool::TBuff<char> m_buffRecv;

	//发送缓冲区
	Tool::TBuff<char> m_buffSend;

protected:
	
	//实际投递发送， 需要调用本函数者锁定发送保护锁
	int ExeSend(const char *pData, int nLen);

	//处理连接成功
	virtual void OnConnected();

	//处理接收到的数
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//可发送的处理函数
	virtual void OnSendable();

	//发生错误退出
	virtual void OnError();	

	//连接服务器成功的回调函数
	static void ConnectedCB(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s								//连接服务器的套接字
		);

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
		std::shared_ptr<void> pContext,						//环境变量
		SOCKET s,								//发生错误的套接字
		int nErrCode							//错误代码，同GetlastErr()的返回值
	);

	//关闭一个套接字
	void CloseSocket(SOCKET &s);

	
};

