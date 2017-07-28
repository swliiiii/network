#include "stdafx.h"
#include "UdpM.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "Tool/TLog.h"

#define Log LogN(105)

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1 
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#define BUFF_SIZE 2048

//构造
CUdpM::CUdpM(void)
	: m_socket(INVALID_SOCKET)
	, m_bStop(false)
	, m_nLocalPort(0)
	, m_pfnDataRecv(nullptr)
	, m_addrSendTo({ 0 })

{
	m_pBuff = new char[BUFF_SIZE];
}

//析构
CUdpM::~CUdpM(void)
{
	CloseSocket(m_socket);
	delete[]m_pBuff;
	m_pBuff = nullptr;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	注册数据接收回调函数
* 输入参数：	pfn				-- 回调函数指针
*				pContext		-- 回调函数环境变量
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
void CUdpM::RegDataRecv(CT_DataRecv pfn, std::shared_ptr<void> pContex)
{
	m_pfnDataRecv = pfn;
	m_pRecvDataContext = pContex; 
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	创建Socket并绑定地址
* 输入参数：	strLocalIP		-- 本地IP
*				nLocalPort		-- 本地端口
*				bReuse			-- 是否允许端口复用
* 输出参数：	
* 返 回 值：	成功返回true，否则返回false。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
bool CUdpM::PreSocket( const std::string &strLocalIP, unsigned short nLocalPort, bool bReuse /* = true */ )
{

	//创建socket
	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//设置端口复用
	if (bReuse)
	{
		int bSockReuse = 1;
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &bSockReuse, sizeof(bSockReuse)))
		{
			Log("[CUdpM:PreSocket] setsockopt reuse failed --<%d - %s>", errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}
	}

	//设置为非阻塞的
	if (!SetNonblocking(m_socket))
	{
		Log("[CUdpM:PreSocket] SetNonblocking failed --<%d - %s>", errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//设置TTL
	unsigned int dwTTL = 32;
	if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &dwTTL, sizeof(dwTTL)))
	{
		Log("[CUdpM:PreSocket] setsockopt TTL failed --<%d - %s>", errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//邦定socket
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(nLocalPort);
	inet_pton(AF_INET,  strLocalIP.c_str(), (void*)&addr.sin_addr);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr *)&addr, sizeof(addr)))
	{
		Log("[CUdpM:PreSocket] bind failed -- LocalIP<%s:%d> error<%d - %s>",
			strLocalIP.c_str(), nLocalPort, errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//如果是自动分配的端口，则获取自动分配的端口号
	if (0 == nLocalPort)
	{
		sockaddr_in addr;
		socklen_t nAddrLen = sizeof(addr);
		int nRet = getsockname(m_socket, (sockaddr*)&addr, &nAddrLen);
		if (0 != nRet)
		{
			Log("[CUdpM::PreSocket]获取套接字的端口失败 Err<%d>", nRet);
			CloseSocket(m_socket);
			return false;
		}
		m_nLocalPort = htons(addr.sin_port);
		Log("[CUdpM::PreSocket]自动分配套接字端口<%u>", m_nLocalPort);
	}
	else
	{
		m_nLocalPort = nLocalPort;
	}
	m_strLocalIP = strLocalIP;
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	开始工作
* 输入参数：	bExpandBuff		-- 是否使用较大的接收缓冲区
*				nPort			-- 使用的本地端口
*				strLocalIP		-- 使用的本地IP
*				strMultiIP		-- 如果是组播，组播地址
*				bReuse			-- 是否允许端口复用
* 输出参数：	
* 返 回 值：	成功返回true，否则返回false。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
bool CUdpM::Start( int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */, unsigned short nPort /* = 0 */, 
	const std::string &strLocalIP /* = "" */, const std::string &strMultiIP /* = "" */, bool bReuse /* = true */ )
{
	Log("CUdpM::Start nRecvBuf<%d> nSendBuf<%d>", nRecvBuf, nSendBuf);
	if (nullptr == GetMonitor())
	{
		Log("[CUdpM::Start]还未设置网络驱动器！");
		return false;
	}

	if (INVALID_SOCKET == m_socket && !PreSocket(strMultiIP.empty() ? strLocalIP : strMultiIP, nPort, bReuse))
	{
		return false;
	}
	
	//设置Socket缓冲(一般默认是8192)
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CUdpM::Start]设置端口接收缓存大小失败<%s:%d> RecvBuff<%d>！", m_strLocalIP.c_str(), m_nLocalPort, nRecvBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CUdpM::Start]设置端口发送缓存大小失败<%s:%d> SendBuff<%d>！", m_strLocalIP.c_str(), m_nLocalPort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}

	//加入组播组
	if (!strMultiIP.empty())
	{
		struct ip_mreq mreq = { 0 };
		inet_pton(AF_INET, strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)))
		{
			Log("[CUdpM:Start] join multibroad failed -- MultiIP<%s> LocalIP<%s> Port<%d> error<%d - %s>",
				strMultiIP.c_str(), strLocalIP.c_str(), nPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}
	}
	m_strMultiIP = strMultiIP;
	
		//监听可写
	if (!GetMonitor()->Add(m_socket, EPOLLOUT | EPOLLIN, RecvDataCB, m_pThis.lock(), SendableCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CUdpM:Start] Add socket<%d> to CMonitor failed -- MultiIP<%s> LocalIP<%s> Port<%d> error<%d - %s>",
			m_socket, strMultiIP.c_str(), strLocalIP.c_str(), nPort, errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}
	return true;
}

//通知停止工作
bool CUdpM::QStop()
{
	Log("[CUdpM::QStop] fd<%d>", m_socket);
	if (!m_strMultiIP.empty())
	{
		//加入组播组
		struct ip_mreq mreq = { 0 };
		inet_pton(AF_INET, m_strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, m_strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)))
		{
			Log("drop menbership failed -- multiIP<%s> Local<%s:%d> error<%d - %s>",
				m_strMultiIP.c_str(), m_strLocalIP.c_str(), m_nLocalPort, errno, strerror(errno));
		}
		m_strMultiIP = "";
	}
	m_bStop = true;
	if (INVALID_SOCKET != m_socket)
	{
		//关闭读写，以触发EPOLL的错误
		shutdown(m_socket, SHUT_RDWR);
	}
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送数据
* 输入参数：	pData		-- 要发送的数据指针
*				nLen		-- 要发送的数据长度
*				strToIP		-- 目的IP
*				nToPort		-- 目的端口
* 输出参数：	
* 返 回 值：	成功返回true，否则返回false。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
bool CUdpM::Send(const char *pData, int nLen, const std::string &strToIP, unsigned short nToPort)
{
	if (!strToIP.empty())
	{
		m_addrSendTo.sin_family = AF_INET;
		inet_pton(AF_INET, strToIP.c_str(), (void*)&m_addrSendTo.sin_addr);
		m_addrSendTo.sin_port = htons(nToPort);
		if (0 == nLen)
		{
			return true;
		}
	}

	int nSend = sendto(m_socket, pData, nLen, 0, (sockaddr*)&m_addrSendTo, sizeof(m_addrSendTo));
	if (nSend == nLen)
	{
		//发送成功
		//Log("[CUdpM::Send] Send <%d> OK", nSend);
		return true;
	}
	//Log("[CUdpM::Send] Send failed: Sent<%d> != nLen<%d>", nSend, nLen);

	//发送失败
	switch (errno)
	{
	case  EAGAIN:	//缓冲区满了
		break;
	case EINTR:		//被中断打断了，应该重新发送一次
		Log("[CTcpClient::Send] EINTR occured, send once again");
		return Send(pData, nLen, strToIP, nToPort);
	default:
		Log("Send (%d < %d) failed -- errno<%d - %s>", nSend, nLen, errno, strerror(errno));
		break;
	}
	return false;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	处理接收到的数据
* 输入参数：	pData		-- 接收到的数据指针
*				nLen		-- 接收到的数据长度
*				strFromIP	-- 数据来自哪个IP
*				nFromPort	-- 数据来自哪个端口
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
void CUdpM::OnRecvData(char *pData, int nLen, const std::string &strFromIP, unsigned short nFromPort)
{
	//Log("[CUdpM::OnRecvData] Recv <%d> bytes From<%s:%d> ", nLen, strFromIP.c_str(), nFromPort);
	if (nullptr != m_pfnDataRecv)
	{
		m_pfnDataRecv(m_pRecvDataContext, pData, nLen, strFromIP, nFromPort);
	}
}

//可发送的处理函数
void CUdpM::OnSendable()
{
	//Log("[CUdpM::OnSendable]");
}

//即将退出时的处理
void CUdpM::OnError()
{
	Log("[CUdpM::OnError]");
	QStop();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	可以接收数据的通知回调函数
* 输入参数：	pContext	-- 环境变量
*				nfd			-- sokcet
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
void CUdpM::RecvDataCB(std::shared_ptr<void> pContext, int nfd)
{
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}

	//获取网络上的数据
	sockaddr_in addrFrom;
	socklen_t nAddrLen = sizeof(addrFrom);
	while (!pThis->m_bStop)
	{
		int nRecv = recvfrom(pThis->m_socket,
			pThis->m_pBuff,
			BUFF_SIZE,
			0,
			(sockaddr *)&addrFrom,
			&nAddrLen);
		if (-1 == nRecv)
		{
			if (EAGAIN == errno)//缓冲区已经被读空
			{
				break;
			}
			else if (EINTR == errno)//被中断打断了，应该重新试一次
			{
				continue;
			}
		}
		if (nRecv <= 0)
		{
			//连接可能关闭了，但是不处理，等待epoll的通知时处理
			Log("[CUdpM::RecvDataCB] errno <%d -- %s>", errno, strerror(errno));
			break;
		}
		char ip[16] = { 0 };
		inet_ntop(AF_INET, (void*)&(addrFrom.sin_addr), ip, 64);
		pThis->OnRecvData(pThis->m_pBuff, nRecv, ip, htons(addrFrom.sin_port));
	}
}

//可以发送数据的通知回调函数
void CUdpM::SendableCB(std::shared_ptr<void> pContext, int nfd)
{
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnSendable();

}
//发生错误的通知回调函数
void CUdpM::ErrCB( std::shared_ptr<void> pContext, int nFd, int nErrCode )
{
	Log("[CUdpM::ErrCB] fd<%d> Err0x<%x>", nFd, nErrCode);
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();	
	pThis->CloseSocket(pThis->m_socket);		
}

//关闭socket
void CUdpM::CloseSocket(int &s)
{
	if (INVALID_SOCKET != s)
	{
		close(s);
		s = INVALID_SOCKET;
	}
}

//将一个socket设置为非阻塞的
bool CUdpM::SetNonblocking(int nFd)
{
	int nFlags = fcntl(nFd, F_GETFL);
	if (-1 == nFlags)
	{
		//不应该发生的事件
		Log("[CTcpClient::SetNonblocking] getfl failed, must be care -- errno<%d - %s>",
			errno, strerror(errno));
		return false;
	}
	nFlags |= O_NONBLOCK;
	if (-1 == fcntl(nFd, F_SETFL, nFlags))
	{
		Log("[CTcpClient::SetNonblocking] setfl failed, must be care -- errno<%d - %s>",
			errno, strerror(errno));
		return false;
	}
	return true;
}


