#pragma once
#include <string>
#include <thread>
#include "Monitor.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#ifndef SOCKET
#define SOCKET int
#endif // !SOCKET

//UDP封装类
class CUdpM : public CContextBase
{
public:

	//接收到数据的回调函数定义
	using CT_DataRecv = void(*) (
		std::shared_ptr<void> pContext,	//环境变量
		const char *pData,							//接收到的数据指针
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
		unsigned short nPort = 0,				//本地端口
		const std::string &strLocalIP = "",		//本地IP
		const std::string &strMultiIP = "",		//组播IP
		bool bReuse = true						//是否允许端口复用
	);

	//通知停止工作
	bool QStop();

	//发送数据
	virtual bool Send(const char *pData, int nLen, const std::string &strToIP = "", unsigned short nToPort = 0);

	//获取本地端口
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//获取本地IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//处理接收到的数据的示例代码
	virtual void OnRecvData(char *pData, int nLen, const std::string &strFromIP, unsigned short nFromPort);

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
		int nError					//错误信息
	);

	//关闭socket
	void CloseSocket(int &s);

	//设置为非阻塞的
	bool SetNonblocking(int nFd);

protected:

	//套接字
	int m_socket;

	//绑定的本地IP
	std::string m_strLocalIP;

	//绑定的本地端口
	unsigned short m_nLocalPort;

	//组播地址
	std::string m_strMultiIP;

	//停止标识
	bool m_bStop;

	//接收数据的缓冲区
	char *m_pBuff;

	//接收到数据的输出回到函数和环境变量
	CT_DataRecv m_pfnDataRecv;
	std::shared_ptr<void> m_pRecvDataContext;

	//上次发送的目的地址
	sockaddr_in m_addrSendTo;

};
