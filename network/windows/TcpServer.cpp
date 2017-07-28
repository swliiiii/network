#include "stdafx.h"
#include "TcpServer.h"
#include "../Tool/TLog.h"

#define Log LogN(103)

#define RCV_BUF_SIZE 1000000

//
//针对每个客户端的处理对象
//

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
CTcpClientBase::CTcpClientBase(SOCKET s, const std::string &strRemoteIP, unsigned short nRemotePort)
	: m_socket(s)
	, m_strRemoteIP(strRemoteIP)
	, m_nRemotePort(nRemotePort)
	, m_pContextSend(nullptr)
	, m_bShutdown(false)
	, m_pfnRecvData(nullptr)
	, m_pfnConnect(nullptr)
{
	Log("CTcpClientBase<%d>", s);
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//析构
CTcpClientBase::~CTcpClientBase()
{
	Log("~CTcpClientBase<%d>", m_socket);
	QStop();
	CloseSocket(m_socket);
	WSACleanup();
}

//注册数据接受回调函数
void CTcpClientBase::RegCB(CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect)
{
	m_pfnRecvData = pfnData;
	m_pRecvData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pConnect = pContextConnect;
}

//开始收发
bool CTcpClientBase::Start( int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */ )
{
	if (nullptr == GetMonitor())
	{
		Log("[%s]失败：还没有设置网络驱动器！", __FUNCTION__);
		return false;
	}
	if (INVALID_SOCKET == m_socket)
	{
		Log("[%s] 失败： 无效的Socket！", __FUNCTION__);
		QStop();
		return false;
	}

	//设置Socket缓冲(一般默认是8192)
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
	if (!GetMonitor()->Attach(m_socket))
	{
		Log("[%s]添加到完成端口失败： socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	if (!GetMonitor()->PostRecv(m_socket, RecvDataCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[%s]投递Recv失败： socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_mutextContext.lock();
	m_pContextSend = GetMonitor()->PostSend(m_socket, SentCB, m_pThis.lock(), ErrCB, m_pThis.lock());
	if (nullptr == m_pContextSend)
	{
		m_mutextContext.unlock();
		Log("[%s]获取发送环境变量失败！socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_mutextContext.unlock();
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, true);
	}
	return true;
}

//通知停止连接
bool CTcpClientBase::QStop()
{
	m_mutextContext.lock();
	m_bShutdown.store(true);
	if (INVALID_SOCKET != m_socket)
	{
		shutdown(m_socket, SD_BOTH);
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
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClientBase::SetSocketBuff]设置端口发送缓存大小失败，Remote<%s:%d> SendBuff<%d>！", m_strRemoteIP.c_str(), m_nRemotePort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	投递发送数据
* 输入参数：	pData		-- 要发送的数据指针
*				nLen		-- 要发送的数据长度
* 输出参数：	
* 返 回 值：	投递成功返回投递的数据长度，否则返回0
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
int CTcpClientBase::Send(const char *pData, int nLen)
{
	std::lock_guard<std::mutex> lock(m_mutextContext);
	if (nullptr == m_pContextSend)
	{
		m_buffSend.append(pData, nLen);
		return nLen;
	}
	
	return ExeSend(pData, nLen);
}

int CTcpClientBase::ExeSend(const char *pData, int nLen)
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
		Log("[%s] 投递send失败！socket<%d>", __FUNCTION__, m_socket);
		return 0;
	}
	m_pContextSend = nullptr;
	return nLen;
}

//处理接收到的数据
void CTcpClientBase::OnRecvData(Tool::TBuff<char> &Data)
{
	//Log("接收到数据<%d>字节", nLen);
	if (nullptr != m_pfnRecvData)
	{
		Data.erase(0, m_pfnRecvData(m_pRecvData, Data, m_pThis));
	}
	else
	{
		Data.clear();
	}
}

//可发送数据的处理函数
void CTcpClientBase::OnSendable()
{

}

//发生错误的处理函数
void CTcpClientBase::OnError()
{
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, false);
	}
	QStop();
}

//接收到数据的处理回调函数
void CTcpClientBase::RecvDataCB(std::shared_ptr<void> pContext, SOCKET s, char *pData, int nLen)
{
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}	
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClientBase::RecvDataCB] 接收缓冲区<%d>太大了，清空！", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	pThis->m_buffRecv.append(pData, nLen);
	pThis->OnRecvData(pThis->m_buffRecv);
	//Log("<%d> recv data ", pThis->m_socket);
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送结果回调函数
* 输入参数：	pContext	-- 环境变量
*				s			-- 套接字
*				nSent		-- 实际发送的字节数
*				pContextSend	-- 用于投递发送的环境变量
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClientBase::SentCB(std::shared_ptr<void> pContext, SOCKET s, int nSent, CMonitor::SendContext *pContextSend)
{
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
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
	if (pThis->m_bShutdown.load())
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
	pThis->OnSendable();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发生错误的处理回调函数
* 输入参数：	pCotnext	-- 环境变量
*				s			-- 发生错误的套接字
*				nErrCode	-- 错误代码
* 输出参数：	
* 返 回 值：	
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
*******************************************************************************/
void CTcpClientBase::ErrCB(std::shared_ptr<void> pContext, SOCKET s, int nErrCode)
{
	Log("[%s] socket<%d> Err<%d>", __FUNCTION__, s, nErrCode);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();
}

//关闭一个socket并置为无效值INVALID_SOCKET
void CTcpClientBase::CloseSocket(SOCKET &s)
{
	if (INVALID_SOCKET != s)
	{
		closesocket(s);
		s = INVALID_SOCKET;
	}
}
