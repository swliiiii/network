#include "stdafx.h"
#include "TcpClient.h"
#include "Monitor.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <sstream>
#include "Tool/TLog.h"

#define Log LogN(102)

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1 
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#define RCV_BUF_SIZE 1000000

//构造
CTcpClient::CTcpClient()
	: m_socket(INVALID_SOCKET)
	, m_bStop(false)
	, m_nConnectFlag(0)
	, m_pfnData(nullptr)
	, m_pfnConnect(nullptr)
	, m_addrServer({0})
	, m_addrLocal({0})
	, m_nLocalPort(0)
{
}

//析构
CTcpClient::~CTcpClient()
{
	CloseSocket(m_socket);
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	注册回调函数
* 输入参数：	pfnData			-- 处理接收到数据的回调函数
*				pContextData	-- 处理接收到数据的环境变量
*				pfnConnect		-- 处理连接状态改变的回调函数
*				pContextConnect	-- 处理连接状态改变的环境变量
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::RegCallback( CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, 
	CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect )
{
	m_pfnData = pfnData;
	m_pContextData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pContextConnect = pContextConnect;
}

bool CTcpClient::PreSocket(const std::string &strLocalIP, unsigned short nLocalPort)
{
	//创建socket
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CTcpClient::Start] create socket failed! -- errno<%d - %s>", errno, strerror(errno));
		return false;
	}

	//设置socket为非阻塞的
	if (!SetNonblocking(m_socket))
	{
		Log("[CTcpClient::Start] fd<%d>set socket Noblocking failed!", m_socket);
		CloseSocket(m_socket);
		return false;
	}

	//绑定网卡		
	m_addrLocal.sin_family = AF_INET;
	inet_pton(AF_INET, strLocalIP.c_str(), (void*)&m_addrLocal.sin_addr);
	m_addrLocal.sin_port = htons(nLocalPort);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr*)&m_addrLocal, sizeof(m_addrLocal)))
	{
		Log("bind localIP<%s:%d> failed -- <%d - %s>", strLocalIP.c_str(), nLocalPort, errno, strerror(errno));
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
			Log("获取套接字的端口失败 Err<%d>", nRet);
			CloseSocket(m_socket);
			return false;
		}
		m_nLocalPort = htons(addr.sin_port);
		Log("自动分配套接字端口<%d>", m_nLocalPort);
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
* 功能描述：	开始连接服务器
* 输入参数：	strServerIP		-- 服务器IP
*				nServerPort		-- 服务器端口
*				strLocalIP		-- 使用的本地IP
*				nLocalPort		-- 使用的本地端口
* 输出参数：	
* 返 回 值：	成功返回true，否则返回false。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
bool CTcpClient::Start( const std::string &strServerIP, unsigned short nServerPort,	int nRecvBuf /* = 0 */, 
	int nSendBuf /* = 0 */, const std::string &strLocalIP /* = "" */, unsigned short nLocalPort /* = 0 */ )
{
	Log("[CTcpClient::Start] ServerIP<%s> ServerPort<%d> LocalIP<%s>",
		strServerIP.c_str(), nServerPort, strLocalIP.c_str());
	if (nullptr == GetMonitor())
	{
		Log("[CTcpClient::Start] Failed -- NO Monitor yet");
		return false;
	}
	if (m_nConnectFlag.load() > 0)
	{
		Log("[CTcpClient::Start] Failed -- the ConnectFlag != 0, can't repeat start");
		return false;
	}
	m_nConnectFlag.store(0);
	m_bStop = false;
	if (INVALID_SOCKET == m_socket && !PreSocket(strLocalIP, nLocalPort))
	{
		return false;
	}

	//设置Socket缓冲
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CTcpClient::Start]设置端口接收缓存大小失败<%s:%d> RecvBuff<%d>！", m_strLocalIP.c_str(), m_nLocalPort, nRecvBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClient::Start]设置端口发送缓存大小失败<%s:%d> SendBuff<%d>！", m_strLocalIP.c_str(), m_nLocalPort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}

	//开始连接
	m_addrServer.sin_family = AF_INET;
	m_addrServer.sin_port = htons(nServerPort);
	inet_pton(AF_INET, strServerIP.c_str(), (void*)&m_addrServer.sin_addr);
	int nRet = connect(m_socket, (sockaddr*)&m_addrServer, sizeof(m_addrServer));
	if (-1 == nRet && EINPROGRESS != errno)
	{
		//其他错误表示失败
		Log("[CTcpClient::Start] fd<%d> connect <%s:%d>failed! -- errno<%d - %s>", m_socket, strServerIP.c_str(), nServerPort, errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//加入epoll,以可读事件作为连接标识
	if (!GetMonitor()->Add(m_socket, EPOLLOUT | EPOLLIN, RecvDataCB, m_pThis.lock(), SendableCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CTcpClient::Start] socket <%d> Add to Epoll failed!", m_socket);
		CloseSocket(m_socket);
		return false;
	}

	std::ostringstream oss;
	oss << strServerIP << ":" << nServerPort << "/" << m_strLocalIP << ":" << m_nLocalPort;
	m_strFlag = oss.str();
	return true;
}

//关闭连接
bool CTcpClient::QStop()
{
	Log("[CTcpClient::QStop] fd <%d>", m_socket);
	m_bStop = true;
	if (INVALID_SOCKET != m_socket)
	{
		//关闭读写，以触发EPOLL的错误，shutdown一次对所有复制的sokcet全部有效。
		shutdown(m_socket, SHUT_RDWR);
	}
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送数据
* 输入参数：	pData		-- 要发送的数据指针
*				nLen		-- 要发送的数据长度，字节数
* 输出参数：	
* 返 回 值：	返回实际发送的数据长度。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
int CTcpClient::Send(const char *pData, int nLen)
{
	//Log("[CTcpClient::Send] socket<%d> nLen<%d>", m_socket, nLen);
	//Log(std::string(pData, nLen > 1000 ? 1000 : nLen).c_str());
	int nSend = send(m_socket, pData, nLen, 0);
	if (-1 != nSend)
	{
		//发送成功
		//Log("[CTcpClient::Send] socket<%d> Send <%d> OK", m_socket, nSend);
		return nSend;
	}
	Log("[CTcpClient::Send] socket<%d> Send failed: Sent<%d> != nLen<%d>", m_socket, nSend, nLen);
	
	//发送失败
	switch (errno)
	{
	case  EAGAIN:	//缓冲区满了
		break;
	case EINTR:		//被中断打断了，应该重新发送一次
		Log("[CTcpClient::Send] socket<%d> EINTR occured, send once again", m_socket);
		return Send(pData, nLen);
	default:
		//Log("fd<%d>Send (%d < %d) failed -- errno<%d - %s>", m_socket, nSend, nLen, errno, strerror(errno));
		break;
	}
	return 0;
}

//连接成功的处理函数
void CTcpClient::OnConnected()
{
	Log("[CTcpClient::OnConnected] socket<%d> connected!", m_socket);
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pContextConnect, true);
	}
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	收到数据的处理函数
* 输入参数：	Data		-- 接收到的数据
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::OnRecvData(Tool::TBuff<char> &Data)
{
	if (nullptr != m_pfnData)
	{
		Data.erase(0, m_pfnData(m_pContextData, Data));
	}
	else
	{
		Data.clear();
	}
}

//可发送的处理函数
void CTcpClient::OnSendable()
{	
	int DisconnectFlag = 0;
	if(m_nConnectFlag.compare_exchange_strong(DisconnectFlag, 1))
	{
		OnConnected();
	}	
}

//发生错误的处理函数
void CTcpClient::OnError()
{
	int nOrgFlag = m_nConnectFlag.fetch_sub(1);
	//1 == nOrgFlag 为连接断开
	//0 == nOrgFlag 为连接失败
	if (0 == nOrgFlag || 1 == nOrgFlag)
	{
		Log("[CTcpClient::OnError] socket<%d> Disconnected", m_socket);
		if (nullptr != m_pfnConnect)
		{
			m_pfnConnect(m_pContextConnect, false);
		}
	}
	QStop();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	接收到数据的处理回调函数
* 输入参数：	pContext	-- 环境变量
*				nfd			-- 套接字
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::RecvDataCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[CTcpClient::RecvDataCB] fd<%d>", nfd);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	const int nBuffSize = 1024;
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClient::RecvDataCB] the RCV BUF Size<%d> is too big, clear it!", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	while (!pThis->m_bStop)
	{		
		auto nPos = pThis->m_buffRecv.size();
		pThis->m_buffRecv.resize(pThis->m_buffRecv.size() + nBuffSize);
		ssize_t nRecv = recv(pThis->m_socket, &(pThis->m_buffRecv[nPos]), nBuffSize, 0);
		//Log("[CTcpClient::RecvDataCB]Recv Data Len<%d> errno<%d>", nRecv, errno);
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
			Log("[CTcpClient::RecvDataCB]connect maybe closed. errno <%d -- %s>", errno, strerror(errno));
			pThis->QStop();
			break;
		}	
	}
	//Log("[CTcpClient::RecvDataCB] End");
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	可发送通知的回调函数
* 输入参数：	pContext	-- 环境变量
*				nfd			-- 套接字
* 输出参数：	
* 返 回 值：	
* 其它说明：	本函数为边缘触发的。
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::SendableCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[%s]fd <%d> ", __FUNCTION__, nfd);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnSendable();	
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送错误的回调函数
* 输入参数：	pContext	-- 环境变量
*				nfd			-- 套接字
*				nError		-- 错误信息
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::ErrCB( std::shared_ptr<void> pContext, int nfd, int nError )
{
	Log("[CTcpClient::ErrCB] fd<%d> error<%x>", nfd, nError);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();	
	pThis->CloseSocket(pThis->m_socket);
}

//将一个socket设置为非阻塞的
bool CTcpClient::SetNonblocking(int nFd)
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

//关闭socket并置为无效值
void CTcpClient::CloseSocket(int &sock)
{
	if (INVALID_SOCKET != sock)
	{
		Log("[CTcpClient::CloseSocket]close socket <%d>", sock);
		close(sock);
		sock = INVALID_SOCKET;
	}
}
