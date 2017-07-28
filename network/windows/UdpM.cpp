#include "stdafx.h"
#include "UdpM.h"
#include <WS2tcpip.h>
#include "../Tool/TLog.h"

#define Log LogN(105)

//构造
CUdpM::CUdpM()
	: m_pContextSend(nullptr)
	, m_socket(INVALID_SOCKET)
	, m_bStop(false)
	, m_nLocalPort(0)
	, m_pfnDataRecv(nullptr)
	, m_collBuff(1024)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//析构
CUdpM::~CUdpM()
{
	CloseSocket(m_socket);
	WSACleanup();
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
	if (strLocalIP.empty())
	{
		Log("[CUdpM::PreSocket] 本地IP不允许为空！");
		return false;
	}

	//创建并开始侦听socket
	m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CUdpM::PreSocket]创建socket失败!");
		return false;
	}

	if (bReuse)
	{
		//设置端口复用
		int bSockReuse = 1;
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&bSockReuse, sizeof(bSockReuse)))
		{
			Log("[CUdpM::PreSocket]端口复用失败! -- %d", WSAGetLastError());
			CloseSocket(m_socket);
			return false;
		}
	}

	//绑定地址
	sockaddr_in addr{ 0 };
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, strLocalIP.c_str(), (void*)&addr.sin_addr);
	addr.sin_port = htons(nLocalPort);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr*)&addr, sizeof(addr)))
	{
		Log("[CUdpM::PreSocket]绑定地址失败<%s:%d> -- <%d>", strLocalIP.c_str(), nLocalPort, GetLastError());
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
		Log("自动分配套接字端口<%u>", m_nLocalPort);
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
	const std::string &strLocalIP /* = "" */, const std::string &strMultiIP /* = "" */, bool bReuse /* = false */ )
{
	if (nullptr == GetMonitor())
	{
		Log("[CUdpM::Start]还未设置网络驱动器！");
		return false;
	}
	if (INVALID_SOCKET == m_socket && !PreSocket(strLocalIP, nPort, bReuse))
	{
		return false;
	}
	m_strMultiIP = strMultiIP;
	
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

	if (!strMultiIP.empty())
	{
		//加入组播组
		struct ip_mreq mreq = {0};
		inet_pton(AF_INET, strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,	(char*)&mreq, sizeof(mreq)))
		{
			Log("[CUdpM::Start]加入组播组<%s>失败<%s:%d>！", strMultiIP.c_str(), strLocalIP.c_str(), nPort);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (!GetMonitor()->Attach(m_socket))
	{
		Log("[CUdpM::Start] 添加到完成端口失败：socket<%d>", m_socket);
		Clear();
		CloseSocket(m_socket);
		return false;
	}

	m_mutextContext.lock();
	m_pContextSend = GetMonitor()->PostSendTo(m_socket, SentToCB, m_pThis.lock(), ErrCB, m_pThis.lock());
	if (NULL == m_pContextSend)
	{
		m_mutextContext.unlock();
		Log("[CUdpM::Start]获取发送环境变量失败: socket<%d>", m_socket);
		Clear();
		CloseSocket(m_socket);
		return false;
	}
	m_mutextContext.unlock();

	if (!GetMonitor()->PostRecvFrom(m_socket, RecvFromCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CUdpM::Start] 失败：socket<%d>", m_socket);
		Clear();
		CloseSocket(m_socket);
		return false;
	}

	Log("[CUdpM::Start]socket<%d>开始在<%s:%d(组播<%s>)> 接收数据", m_socket, m_strLocalIP.c_str(), nPort, m_strMultiIP.c_str());
	return true;
}

//通知停止工作
bool CUdpM::QStop()
{
	if (INVALID_SOCKET != m_socket)
	{
		//int nRet = shutdown(m_socket, SD_BOTH);
		//Log("%d", nRet);
		//CloseSocket(m_socket);
		char a = 'a';
		Send(&a, 1, m_strLocalIP, m_nLocalPort);
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
bool CUdpM::Send(const char *pData, int nLen, const std::string &strToIP /* = "" */, unsigned short nToPort /* = 0 */)
{
	std::lock_guard<std::mutex> lock(m_mutextContext);
	if (nullptr != m_pContextSend)
	{
		ExeSend(pData, nLen, strToIP, nToPort);
		return true;
	}
	m_collBuff.push_back();
	auto &node = m_collBuff.back();
	node.buff.clear();
	node.buff.append(pData, nLen);
	node.strDstIP = strToIP;
	node.nDstPort = nToPort;
	return true;
}

void CUdpM::ExeSend(const char *pData, int nLen, const std::string &strToIP /* = "" */, unsigned short nToPort /* = 0 */)
{
	if (!strToIP.empty())
	{
		m_pContextSend->addSendTo.sin_family = AF_INET;
		inet_pton(AF_INET, strToIP.c_str(), (void*)&m_pContextSend->addSendTo.sin_addr);
		m_pContextSend->addSendTo.sin_port = htons(nToPort);
		if (0 == nLen)
		{
			return;
		}
	}
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

	bool bRet = GetMonitor()->PostSendTo(m_pContextSend);
	if (!bRet)
	{
		Log("[%s] 投递Send失败 : socket<%d>", __FUNCTION__, m_socket);
	}
	else
	{
		m_pContextSend = nullptr;
	}
	return;
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
	if (nullptr != m_pfnDataRecv)
	{
		m_pfnDataRecv(m_pRecvDataContext, pData, nLen, strFromIP, nFromPort);
	}
}

//发送完毕的处理函数
void CUdpM::OnSent()
{
}

//发生错误的处理函数
void CUdpM::OnErr()
{

}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发生错误的回调函数
* 输入参数：	pContext		-- 环境变量
*				s				-- socket
*				nErrCode		-- 错误代码
* 输出参数：	
* 返 回 值：	
* 其它说明：	本函数被调用时，socket很可能已经丧失正常的收发功能了
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/11	司文丽	      创建
*******************************************************************************/
void CUdpM::ErrCB(std::shared_ptr<void> pContext, SOCKET s, int nErrCode)
{
	Log("[CUdpM::ErrCB] socket<%d> ErrCode<%d>", s, nErrCode);
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		Log("[[CUdpM::ErrCB]] this指针为空!");
		return;
	}
	pThis->OnErr();
	pThis->Clear();
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	接收到数据的通知回调函数
* 输入参数：	pContext	-- 环境变量
*				s			-- socket
*				pData		-- 接收到的数据指针
*				nLen		-- 接收到的数据长度
*				strFromIP	-- 数据来自哪个IP
*				nFromPort	-- 数据来自哪个端口
* 输出参数：	
* 返 回 值：	继续投递接收操作返回true，不再继续接收返回false。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
bool CUdpM::RecvFromCB(std::shared_ptr<void> pContext, SOCKET s, char *pData, int nLen, const std::string strFromIP, unsigned short nFromPort)
{
	//Log("socket<%d>接收到%d字节数据", s, nLen);
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		Log("[CUdpM::RecvFromCB] this 指针为空！一定是代码有错误！");
		return false;
	}
	if (strFromIP == pThis->m_strLocalIP && nFromPort == pThis->m_nLocalPort)
	{
		//尝试清理一次
		Log("[CUdpM::RecvFromCB]sokcet<%d> Port<%d> 收到停止命令", pThis->m_socket, pThis->m_nLocalPort);
		pThis->Clear();
		return false;
	}
	pThis->OnRecvData(pData, nLen, strFromIP, nFromPort);
	return true;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	发送结果回调函数
* 输入参数：	pContext		-- 环境变量
*				s				-- socket
*				nSent			-- 成功发送的数据字节数
*				pContextSend	-- 发送数据使用的环境变量
* 输出参数：	
* 返 回 值：	无。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/04/12	司文丽	      创建
*******************************************************************************/
void CUdpM::SentToCB(std::shared_ptr<void> pContext, SOCKET s, int nSent, CMonitor::SendToContext *pContextSend)
{
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pContextSend->wsaBuff.len != nSent)
	{
		Log("[CUdpM::SentToCB]发送结果：希望发送<%d>字节 ！= 实际发送<%d>字节", pContextSend->wsaBuff.len, nSent);
	}

	bool bSent = true;
	pThis->m_mutextContext.lock();
	
	if (pThis->m_bStop.load() && pThis->m_collBuff.empty())
	{
		delete pContextSend;
		bSent = false;
	}
	else
	{
		pThis->m_pContextSend = pContextSend;
		if (!pThis->m_collBuff.empty())
		{
			auto &node = pThis->m_collBuff.front();
			pThis->ExeSend(&node.buff[0], (int)node.buff.size(), node.strDstIP, node.nDstPort);
			pThis->m_collBuff.pop_front();
			bSent = false;
		}
	}
	pThis->m_mutextContext.unlock();
	if (bSent)
	{
		pThis->OnSent();
	}
}

//退出组播组、删除发送环境变量
void CUdpM::Clear()
{
	Log("[CUdpM::Clear]");
	if (!m_strMultiIP.empty())
	{
		//退出组播组
		struct ip_mreq mreq = { 0 };
		inet_pton(AF_INET, m_strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, m_strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq)))
		{
			Log("退出组播组<%s>失败<%s:%d>！", m_strMultiIP.c_str(), m_strLocalIP.c_str(), m_nLocalPort);
		}
		m_strMultiIP = "";
	}
	std::lock_guard<std::mutex> lock(m_mutextContext);
	m_bStop.store(true);
	if (nullptr != m_pContextSend)
	{
		delete m_pContextSend;
		m_pContextSend = nullptr;
	}
}

//关闭socket
void CUdpM::CloseSocket(SOCKET &s)
{
	if (INVALID_SOCKET != s)
	{
		closesocket(s);
		s = INVALID_SOCKET;
	}
}
