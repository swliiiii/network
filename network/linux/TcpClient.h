#pragma once
#include <set>
#include "Monitor.h"
#include <arpa/inet.h>
#include <atomic>
#include "Tool/TBuff.h"

#ifndef SOCKET
#define SOCKET int
#endif // !SOCKET


class CTcpClient : public CContextBase
{
public:

	//构造函数
	CTcpClient();

	//析构
	virtual ~CTcpClient(void);

	//连接状态输出回调函数
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//环境变量
		bool bConnected									//true链接 false断开
		);

	//接收到数据的回调函数
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//环境变量
		Tool::TBuff<char> &Data								//接收到的数据
		);

	//注册接收到数据的回调函数
	void RegCallback(
		CT_Data pfnData,								//接收到数据的回调函数指针
		std::shared_ptr<CContextBase> pContextData,		//接收到数据的回调函数环境变量
		CT_Connect pfnConnect,							//连接成功的回调函数指针
		std::shared_ptr<CContextBase> pContextConnect	//连接成功的回调函数环境变量
	);

	//创建Socket并绑定地址
	bool PreSocket(
		const std::string &strLocalIP,				//要使用的本地IP
		unsigned short nLocalPort					//要使用的本地端口
	);

	//开始连接服务器
	bool Start(
		const std::string &strServerIP,				//服务器的IP
		unsigned short nServerPort,					//服务器的端口
		int nRecvBuf = 0,							//接收缓冲区大小，0标识使用默认值
		int nSendBuf = 0,							//发送缓冲区大小，0标识使用默认值
		const std::string &strLocalIP = "",			//要使用的本地IP
		unsigned short nLocalPort = 0				//要使用的本地端口
		);

	//通知停止连接
	bool QStop();

	//发送数据
	int Send(const char *pData, int nLen);

	//获取本地端口
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//获取本地IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//客户端套接字
	int m_socket;

	//停止标识
	bool m_bStop;

	//是否链接的标识， 0标识未连接，大于0标识已连接
	std::atomic<int> m_nConnectFlag;

	//本对象的标识
	std::string m_strFlag;

	//接收到数据的输出回到函数和环境变量
	CT_Data m_pfnData;
	std::shared_ptr<CContextBase> m_pContextData;

	//连接状态的输出回调函数和环境变量
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pContextConnect;

	//服务器地址
	sockaddr_in m_addrServer;

	//本机绑定地址
	sockaddr_in m_addrLocal;

	//本地IP地址和端口
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;

	//接收缓冲区
	Tool::TBuff<char> m_buffRecv;

protected:

	//连接成功的处理函数
	virtual void OnConnected();

	//处理接收到的数据的示例代码
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//可发送的处理函数
	virtual void OnSendable();

	//即将退出时的处理
	virtual void OnError();

	//接收到数据的通知回调函数
	static  void RecvDataCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd				//发生事件的文件描述符
		);

	//可以发送数据的通知回调函数
	static void SendableCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd				//发生事件的文件描述符
		);

	//发生错误的通知回调函数
	static void ErrCB(
		std::shared_ptr<void> pContext,		//环境变量
		int nfd,					//发生事件的文件描述符
		int nError					//错误号
		);


	//接收数据的处理函数
	void RecvData();

	//将一个socket设置为非阻塞的
	bool SetNonblocking(int nFd);

	//关闭socket并置为无效值
	void CloseSocket(int &s);
};

