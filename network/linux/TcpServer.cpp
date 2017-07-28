#include "stdafx.h"
#include "TcpServer.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define Log LogN(103)
#define RCV_BUF_SIZE 1000000

/*******************************************************************************
* 函数名称：	
* 功能描述：	构造
* 输入参数：	s				-- 连接的套接字
*				strRemoteIP		-- 远端的IP
*				nRemotePort		-- 远端的端口
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
CTcpClientBase::CTcpClientBase(int s, const std::string &strRemoteIP, unsigned short nRemotePort)
	: m_socket(s)
	, m_strRemoteIP(strRemoteIP)
	, m_nRemotePort(nRemotePort)
	, m_bStop(false)
	, m_pfnRecvData(nullptr)
	, m_pfnConnect(nullptr)
{
}

//析构
CTcpClientBase::~CTcpClientBase()
{
	CloseSocket(m_socket);
}

//注册数据接受回调函数
void CTcpClientBase::RegCB(CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect)
{
	m_pfnRecvData = pfnData;
	m_pRecvData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pConnect = pContextConnect;
}

//开始收发数据
bool CTcpClientBase::Start( int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */ )
{
	Log("CTcpClientBase::Start");
	if (nullptr == GetMonitor())
	{
		Log("[CTcpClientBase::Start] failed -- NO Monitor yet");
		return false;
	}
	m_bStop = false;
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CTcpClientBase::Start] failed：INVALID_SOCKET!");
		return false;
	}
	//设置Socket缓冲
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CCTcpClientBase::Start]设置端口接收缓存大小失败，Remote<%s:%d> RecvBuff<%d>！", m_strRemoteIP.c_str(), m_nRemotePort, nRecvBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClientBase::Start]设置端口发送缓存大小失败，Remote<%s:%d> SendBuff<%d>！", m_strRemoteIP.c_str(), m_nRemotePort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (!GetMonitor()->Add(m_socket, EPOLLIN | EPOLLOUT, RecvDataCB, m_pThis.lock(), SendableCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CTcpClientBase::Start]Add socket<%d> EPOLLOUT to CMoniteor failed", m_socket);
		CloseSocket(m_socket);
		return false;
	}
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, true);
	}
	return true;
}

//通知关闭连接
bool CTcpClientBase::QStop()
{
	Log("[CTcpClientBase::QStop] socket<%d>", m_socket);
	if (INVALID_SOCKET != m_socket)
	{
		//关闭读写，以触发EPOLL的错误，不能马上调用close,否则epoll有可能无法触发了
		shutdown(m_socket, SHUT_RDWR);
	}
	return true;
}

/*******************************************************************************
* 函数名称：
* 功能描述：	设置soket的读写缓冲区
* 输入参数：	nRecvBuf		-- 接收缓冲区大小，0标识使用默认值
*				nSendBuf		-- 发送缓冲区大小，0标识使用默认值
* 输出参数：
* 返 回 值：	成功返回true，否则返回false
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/06/16	司文丽	      创建
*******************************************************************************/
bool CTcpClientBase::SetSocketBuff(int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */)
{
	Log("[CTcpClientBase::SetSocketBuff] nRecvBuf<%d> nSendBuf<%d>", nRecvBuf, nSendBuf);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CTcpClientBase::SetSocketBuff] Soket还未创建"); 
		return false;
	}

	//设置Socket缓冲(一般默认是8192)
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CTcpCCTcpClientBase::SetSocketBuff]设置端口接收缓存大小失败，Remote<%s:%d> RecvBuff<%d>！", m_strRemoteIP.c_str(), m_nRemotePort, nRecvBuf);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClientBase::SetSocketBuff]设置端口发送缓存大小失败，Remote<%s:%d> SendBuff<%d>！", m_strRemoteIP.c_str(), m_nRemotePort, nSendBuf);
			return false;
		}
	}
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送数据	
* 输入参数：	pData		-- 要发送的数据指针
*				nLen		-- 要发送的数据长度
* 输出参数：	
* 返 回 值：	实际发送的数据长度。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
int CTcpClientBase::Send(const char *pData, int nLen)
{
	//Log("[CTcpClientBase::Send] nLen<%d>", nLen);
	int nSend = send(m_socket, pData, nLen, 0);
	if (-1 != nSend)
	{
		//发送成功
		//Log("[CTcpClientBase::Send] Send <%d> OK", nSend);
		return nSend;
	}
//	Log("[CTcpClientBase::Send] Send failed: Sent<%d> != nLen<%d>", nSend, nLen);

	//发送失败
	switch (errno)
	{
	case  EAGAIN:	//缓冲区满了,通知外部
		break;
	case EINTR:		//被中断打断了，应该重新发送一次
		Log("[CTcpClientBase::Send] EINTR occured, send once again");
		return Send(pData, nLen);
	default:
		Log("Send (%d < %d) failed -- errno<%d - %s>", nSend, nLen, errno, strerror(errno));
		break;
	}
	return 0;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	接收到数据的处理函数
* 输入参数：	Data		-- 接收到的数据
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClientBase::OnRecvData(Tool::TBuff<char> &Data)
{
	//Log("[CTcpClientBase::OnRecvData] Recv <%d> bytes", Data.size());
	if (nullptr != m_pfnRecvData)
	{
		Data.erase(0, m_pfnRecvData(m_pRecvData, Data, m_pThis));
	}
	else
	{
		Data.clear();
	}
}

//可以发送数据通知的处理回调函数
void CTcpClientBase::OnSendable()
{
}

//发生错误时的处理
void CTcpClientBase::OnError()
{
	Log("[CTcpClientBase::OnError] socket <%d>", m_socket);
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, false);
	}
	QStop();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	有数据可接收的处理回调函数
* 输入参数：	pCotnext	-- 环境变量
*				nfd			-- 套接字
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClientBase::RecvDataCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[CTcpClientBase::RecvDataCB] fd<%d>", nfd);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClientBase::RecvDataCB] the RCV BUF Size<%d> is too big, clear it!", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	const int nBuffSize = 1024;
	while (!pThis->m_bStop)
	{
		auto nPos = pThis->m_buffRecv.size();
		pThis->m_buffRecv.resize(pThis->m_buffRecv.size() + nBuffSize);
		ssize_t nRecv = recv(pThis->m_socket, &(pThis->m_buffRecv[nPos]), nBuffSize, 0);
		//Log("[CTcpClientBase::RecvDataCB]Recv Data Len<%d> errno<%d>", nRecv, errno);
		if (nRecv > 0)
		{
			pThis->m_buffRecv.resize(pThis->m_buffRecv.size() + nRecv - nBuffSize);
			pThis->OnRecvData(pThis->m_buffRecv);
			if (nRecv < nBuffSize)
			{
				//对于Tcp，由此就可以判断数据已经被读空（udp则不行）
				break;
			}
			continue;
		}
		pThis->m_buffRecv.resize(pThis->m_buffRecv.size() - nBuffSize);
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
			Log("[CTcpClientBase::RecvDataCB]connect maybe closed. errno <%d -- %s>", errno, strerror(errno));
			pThis->QStop();
			break;
		}
	}
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	可以发送数据的处理回调函数
* 输入参数：	pCotnext	-- 环境变量
*				nfd			-- 套接字
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClientBase::SendableCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[CTcpClientBase::SendableCB] fd<%d>", nfd);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnSendable();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发生错误的处理回调函数
* 输入参数：	pCotnext	-- 环境变量
*				nfd			-- 套接字
*				nError		-- 错误信息
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClientBase::ErrCB( std::shared_ptr<void> pContext, int nfd, int nError )
{
	Log("[CTcpClientBase::ErrCB] fd<%d> Error<%x>", nfd, nError);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();
	pThis->CloseSocket(pThis->m_socket);	
}

//关闭socket并置为无效值
void CTcpClientBase::CloseSocket(int &s)
{
	if (INVALID_SOCKET != s)
	{
		Log("close socket <%d>", s);
		close(s);
		s = INVALID_SOCKET;
	}
}
