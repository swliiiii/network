#include "stdafx.h"
#include "TcpClient.h"
#include <sstream>
#include <WS2tcpip.h>
#include "../Tool/TLog.h"

#define Log LogN(102)

#define RCV_BUF_SIZE 1000000

//构造
CTcpClient::CTcpClient()
	: m_socket(INVALID_SOCKET)
	, m_pContextSend(nullptr)
	, m_pfnData(nullptr)
	, m_pfnConnect(nullptr)
	, m_addrServer({0})
	, m_bShutdown(false)
	, m_nLocalPort(0)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//析构
CTcpClient::~CTcpClient()
{
	CloseSocket(m_socket);
	WSACleanup();
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
void CTcpClient::RegCallback( CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, CT_Connect pfnConnect,
	std::shared_ptr<CContextBase> pContextConnect )
{
	m_pfnData = pfnData;
	m_pContextData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pContextConnect = pContextConnect;
}

//创建Socket并绑定地址
bool CTcpClient::PreSocket(const std::string &strLocalIP, unsigned short nLocalPort)
{
	m_socket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	Log("创建socket的值是<%d>", m_socket);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[%s]创建socket失败!", __FUNCTION__);
		return false;
	}
	sockaddr_in addr{ 0 };
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, strLocalIP.c_str(), (void*)&addr.sin_addr);
	addr.sin_port = htons(nLocalPort);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr*)&addr, sizeof(addr)))
	{
		Log("[%s]绑定网卡失败<%s:%d> -- <%d>", __FUNCTION__, strLocalIP.c_str(), nLocalPort, GetLastError());
		CloseSocket(m_socket);
		return false;
	}

	//如果是自动分配的端口，则获取自动分配的端口号
	if (0 == nLocalPort)
	{
		sockaddr_in addr;
		int nAddrLen = sizeof(addr);
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
bool CTcpClient::Start( const std::string &strServerIP, unsigned short nServerPort, int nRecvBuf /* = 0 */, 
	int nSendBuf /* = 0 */, const std::string &strLocalIP /* = "" */, unsigned short nLocalPort /* = 0 */ )
{
	if (nullptr == GetMonitor())
	{
		Log("[%s] 还没有设置网络驱动器！", __FUNCTION__);
		return false;
	}
	if (INVALID_SOCKET == m_socket && !PreSocket(strLocalIP, nLocalPort))
	{
		return false;
	}

	//设置Socket缓冲(一般默认是8192)
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
	if (!GetMonitor()->Attach(m_socket))
	{
		Log("%s] 添加到完成端口失败：socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_addrServer.sin_family = AF_INET;
	inet_pton(AF_INET, strServerIP.c_str(), (void*)&m_addrServer.sin_addr);
	m_addrServer.sin_port = htons(nServerPort);
	if (!GetMonitor()->PostConnectEx(m_socket, m_addrServer, ConnectedCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("%s] 投递Connect请求失败：socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}

	m_mutextContext.lock();
	m_pContextSend = GetMonitor()->PostSend(m_socket, SentCB, m_pThis.lock(), ErrCB, m_pThis.lock());
	if (nullptr == m_pContextSend)
	{
		m_mutextContext.unlock();
		Log("%s] 获取发送环境变量失败：socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_mutextContext.unlock();
	Log("[%s] socket<%d> 正在尝试连接<%s:%d>", __FUNCTION__, m_socket, strServerIP.c_str(), nServerPort);

	std::ostringstream oss;
	oss << strServerIP << ":" << nServerPort << "/" << m_strLocalIP << ":" << m_nLocalPort;
	m_strFlag = oss.str();
	return true;
}

//通知停止连接
bool CTcpClient::QStop()
{
	//Log("CTcpClient::QStop");
	m_mutextContext.lock();
	m_bShutdown.store(true);
	if (INVALID_SOCKET != m_socket)
	{	
		//windows 下 如果对方死活不关闭，则该socket可能要过2分钟才能被完成端口发现
		shutdown(m_socket, SD_SEND);
	}
	if (nullptr != m_pContextSend)
	{
		delete m_pContextSend;
		m_pContextSend = nullptr;
	}
	m_mutextContext.unlock();
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	投递发送数据
* 输入参数：	pData		-- 要发送的数据指针
*				nLen		-- 要发送的数据长度，字节数
* 输出参数：	
* 返 回 值：	成功返回投递的数据长度，失败返回0。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
int CTcpClient::Send(const char *pData, int nLen)
{
	std::lock_guard<std::mutex> lock(m_mutextContext);
	if (nullptr == m_pContextSend)
	{
		m_buffSend.append(pData, nLen);
		return nLen;
	}
	return ExeSend(pData, nLen);
}

//实际投递发送， 需要调用本函数者锁定发送保护锁
int CTcpClient::ExeSend(const char *pData, int nLen)
{
	if ((int)m_pContextSend->dwBuffLen < nLen)
	{
		if (nullptr != m_pContextSend->wsaBuff.buf)
		{
			delete[]m_pContextSend->wsaBuff.buf;
		}
		m_pContextSend->dwBuffLen = nLen * 2;
		m_pContextSend->wsaBuff.buf = new char[m_pContextSend->dwBuffLen];
	}
	memcpy(m_pContextSend->wsaBuff.buf, pData, nLen);
	m_pContextSend->wsaBuff.len = nLen;
	bool bRet = GetMonitor()->PostSend(m_pContextSend);
	if (!bRet)
	{
		Log("[%s] 投递Send失败 : socket<%d>", __FUNCTION__, m_socket);
		return 0;
	}
	m_pContextSend = nullptr;
	return nLen;
}

//连接成功的处理函数
void CTcpClient::OnConnected()
{
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pContextConnect, true);
	}
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	接收到数据的处理函数
* 输入参数：	pData		-- 接收到的数据指针
*				nLen		-- 接收到的数据长度
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::OnRecvData(Tool::TBuff<char> &Data)
{
	//Log("接收到数据<%d>字节", nLen);
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
}

//发生错误退出
void CTcpClient::OnError()
{
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pContextConnect, false);
	}	
	QStop();	
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	接收到数据的处理回调函数
* 输入参数：	pContext	-- 环境变量
*				s			-- 接收到数据的套接字
*				pData		-- 接收到的数据指针
*				nLen		-- 接收到的数据长度
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::RecvDataCB(std::shared_ptr<void> pContext, SOCKET s, char *pData, int nLen)
{
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClient::RecvDataCB] 接收缓冲区<%d>太大了，清空！", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	pThis->m_buffRecv.append(pData, nLen);
	pThis->OnRecvData(pThis->m_buffRecv);	
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	连接成功的回调函数
* 输入参数：	pContext		-- 环境变量
*				s				-- 连接成功事件的套接字
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::ConnectedCB( std::shared_ptr<void> pContext, SOCKET s )
{
	Log("[%s] socket <%d> 连接服务器成功！", __FUNCTION__, s);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}

	//注意：此处非常重要，更新socket的状态，否则shutdown等函数不可用于该sokcet
	setsockopt(s,
		SOL_SOCKET,
		SO_UPDATE_CONNECT_CONTEXT,
		NULL,
		0);

	//开始接收数据
	if (!pThis->GetMonitor()->PostRecv(s, RecvDataCB, pContext, ErrCB, pContext))
	{
		Log("[%s] 投递 Recv失败：socket<%d>", __FUNCTION__, pThis->m_socket);
		pThis->QStop();
		return;
	}
	pThis->OnConnected();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送成功的回调函数
* 输入参数：	pContext		-- 环境变量
*				s				-- 发送事件的套接字
*				nSent			-- 发送成功的字节数
*				pContextSend	-- 投递发送操作需要使用的环境变量
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::SentCB(std::shared_ptr<void> pContext, SOCKET s, int nSent, CMonitor::SendContext *pContextSend)
{
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pContextSend->wsaBuff.len != nSent)
	{
		Log("发送失败：希望发送<%d>字节，实际发送<%d>字节", pContextSend->wsaBuff.len, nSent);
	}
	bool bSent = true;
	pThis->m_mutextContext.lock();
	if (pThis->m_bShutdown.load ())
	{
		delete pContextSend;
		bSent = false;
	}
	else
	{
		pThis->m_pContextSend = pContextSend;
		if (pThis->m_buffSend.size() > 0)
		{
			pThis->ExeSend(&pThis->m_buffSend[0], (int)pThis->m_buffSend.size());
			pThis->m_buffSend.clear();
			bSent = false;
		}
	}
	pThis->m_mutextContext.unlock();
	if (bSent)
	{
		pThis->OnSendable();
	}
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发生错误的处理回调函数
* 输入参数：	pContext	-- 环境变量
*				s			-- 发生错误的套接字
*				nErrCode	-- 错误码
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::ErrCB(std::shared_ptr<void> pContext, SOCKET s, int nErrCode)
{
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	关闭套接字并将其置为无效值
* 输入参数：	s				-- 套接字
* 输出参数：	s				-- 套接字
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClient::CloseSocket(SOCKET &s)
{
	if (INVALID_SOCKET != s)
	{
		shutdown(m_socket, SD_BOTH);
		closesocket(s);
		s = INVALID_SOCKET;
	}
}

