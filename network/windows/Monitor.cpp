#include "stdafx.h"
#include "Monitor.h"
#include <sstream>
#include <WS2tcpip.h>
#include "../Tool/TLog.h"

//定时器的间隔最大时长，毫秒数
#define MAX_TIMER_INTERVAL 86500000
#define Log LogN(101)

bool TimerNode::aFlag[TIMER_FLAG_MAX] = {0};

//定时器信息的排序规则，因为这个规则，所以建议定时器的时间不要达到MAX_TIMER_INTERVAL
bool operator <(const TimerPtr &L, const TimerPtr &R)
{	
	return (L->dwLast + L->dwInterval - (R->dwLast + R->dwInterval)) >= MAX_TIMER_INTERVAL;
}

CMonitor::CMonitor()
	: m_hCP(NULL)
	, m_bStop(false)
{
}

CMonitor::~CMonitor()
{
	DeInit();
	if (NULL != m_hCP)
	{
		CloseHandle(m_hCP);
		m_hCP = NULL;
	}
}

//初始化
bool CMonitor::Init()
{
	Log("[%s]", __FUNCTION__);

	//创建完成端口
	int nThreadCount = 1;// GetProcessorCount();
	m_hCP = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL,
		0,
		nThreadCount			//允许应用程序同时执行的线程数量，0表示根据处理器个数创建。
	);
	if (NULL == m_hCP)
	{
		Log("[%s]创建完成端口失败: err<%d>", __FUNCTION__, GetLastError());
		return false;
	}
	  
	//创建工作线程,目前由于定时器设计的影响，还不能用多个线程
	m_bStop = false;
	for (int i = 0; i < nThreadCount; ++i)
	{
		m_vecThread.push_back(std::thread(&CMonitor::ThreadFunc, this));
	}
	return true;
}

//反初始化
bool CMonitor::DeInit()
{
	Log("[%s]", __FUNCTION__);
	m_bStop = true;
	for (size_t i = 0; i < m_vecThread.size(); ++i)
	{
		PostQueuedCompletionStatus(m_hCP, 0, INVALID_SOCKET, 0);
	}
	for (auto &t : m_vecThread)
	{
		t.join();
	}
	m_vecThread.clear();
	if (NULL != m_hCP)
	{
		CloseHandle(m_hCP);
		m_hCP = NULL;
	}
	return false;
}

/*******************************************************************************
* 函数名称：
* 功能描述：	添加定时器,返回定时器的标识
* 输入参数：	dwInterval		-- 定时器间隔，单位毫秒
*				pContext		-- 定时器回调函数环境变量
*				pfn				-- 定时器回调函数指针
* 输出参数：
* 返 回 值：	成功返回定时器标识，否则返回-1。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/09	司文丽	      创建
*******************************************************************************/
int CMonitor::AddTimer(unsigned int dwInterval, std::shared_ptr<void> pContext, TimerNode::CT_Timer pfn)
{
	std::lock_guard<std::mutex> lock(m_mtxTimer);
	if (dwInterval > MAX_TIMER_INTERVAL)
	{
		Log("[%s] 失败！Interval(%d)  > %d秒", __FUNCTION__, dwInterval, MAX_TIMER_INTERVAL);
		return -1;
	}
	TimerPtr pNode = std::make_shared<TimerNode>();
	int nFlag = pNode->Make(dwInterval, pContext, pfn);
	if (-1 == nFlag)
	{
		Log("[%s] 失败！无法获取定时器标识", __FUNCTION__);
		return -1;
	}
	pNode->Add(nFlag);
	m_setTimer.insert(pNode);	
	Log("[%s] 新定时器 Flag<%d> Interval<%d>毫秒，总定时器数目<%d>", __FUNCTION__, nFlag, dwInterval, m_setTimer.size());
	return nFlag;
}

/*******************************************************************************
* 函数名称：
* 功能描述：	删除定时器
* 输入参数：	nFlag		-- 要删除的定时器标识
* 输出参数：
* 返 回 值：	成功返回true，定时器标识不存在返回fasle。
* 其它说明：
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/09	司文丽	      创建
*******************************************************************************/
bool CMonitor::DelTimer(int nFlag)
{
	std::lock_guard<std::mutex> lock(m_mtxTimer);
	for (auto it = m_setTimer.begin(); it != m_setTimer.end(); ++it)
	{
		if ((*it)->nFlag == nFlag)
		{
			(*it)->Del(nFlag);
			m_setTimer.erase(it);
			Log("[%s] 删除定时器 Flag<%d> 总定时器数目<%d>个", __FUNCTION__, nFlag, m_setTimer.size());
			return true;
		}
	}
	return false;
}

//将socket绑定到完成端口
bool CMonitor::Attach(SOCKET s)
{
	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return false;
	}
	if (m_hCP != CreateIoCompletionPort((HANDLE)s, m_hCP, s, 0))
	{
		Log("[%s]添加到完成端口失败: Socket<%d>, err<%d> ", __FUNCTION__, s, GetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostAcceptEx( SOCKET s, CT_Client pfnClient, std::shared_ptr<void> pContextClient,
	CT_Err pfnErr, std::shared_ptr<void> pContextErr,int nCount /* = 1*/)
{
	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return false;
	}
	LPFN_ACCEPTEX pfnAcceptEx = nullptr;
	LPFN_GETACCEPTEXSOCKADDRS pfnGetAddr = nullptr;
	if (!GetAcceptExFunc(s, pfnAcceptEx, pfnGetAddr))
	{
		Log("[%s]获取AcceptEx函数指针失败: socket<%d>", __FUNCTION__, s);
		return false;
	}
	for (int i = 0; i < nCount; ++i)
	{
		AcceptContext *pContext = new AcceptContext;
		//Log("new <%x>", pContext);
		pContext->s = s;
		pContext->op = OPIndex::eAcceptEx;
		pContext->pfnExe = &CMonitor::ExeAcceptEx;
		pContext->pfnAcceptEx = pfnAcceptEx;
		pContext->pfnGetAcceptExSockAddrs = pfnGetAddr;
		pContext->pfnClient = pfnClient;
		pContext->pContextClient = pContextClient;
		pContext->pfnErr = pfnErr;
		pContext->pContextErr = pContextErr;
		if (!PostAcceptEx(pContext))
		{
			Log("[%s]添加第<%d>次 Accept 失败: socket<%d>",__FUNCTION__, i, s);
			delete pContext;
			Log("delete <%x>", pContext);
			return false;
		}
	}
	//Log("[%s] 添加Accept 共<%d>次: socket<%d>", __FUNCTION__, nCount, s);
	return true;
}

bool CMonitor::PostConnectEx( SOCKET s, sockaddr_in addrServer, CT_Connected pfnConnected,
	std::shared_ptr<void> pContextConnected, CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return false;
	}

	LPFN_CONNECTEX pfnConnectEx(nullptr);
	if (!GetConnectExFunc(s, pfnConnectEx))
	{
		return false;
	}
	ConnectExContext *pContext = new ConnectExContext;
	pContext->s = s;
	pContext->op = OPIndex::eConnectEx;
	pContext->pfnExe = &CMonitor::ExeConnectEx;
	pContext->addr = addrServer;
	pContext->pfnErr = pfnErr;
	pContext->pContextErr = pContextErr;
	pContext->pfnConnected = pfnConnected;
	pContext->pContextConnected = pContextConnected;
	pContext->pfnConnectEx = pfnConnectEx;
	if (!PostConnectEx(pContext))
	{
		Log("[%s]添加 Connected 失败: socket<%d> ", __FUNCTION__, s);
		delete pContext;
		Log("delete <%x>", pContext);
		return false;
	}
	//Log("[%s]添加 Connected 成功: socket <%d>", __FUNCTION__, s);
	return true;
}

bool CMonitor::PostRecv( SOCKET s, CT_RecvData pfnRecv, std::shared_ptr<void> pContextRecv,
	CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return false;
	}

	RecvContext *pContext = new RecvContext;
	//Log("new <%x>", pContext);
	pContext->s = s;
	pContext->op = OPIndex::eRecv;
	pContext->pfnExe = &CMonitor::ExeRecv;
	pContext->pfnRecv = pfnRecv;
	pContext->pContextRecv = pContextRecv;
	pContext->pfnErr = pfnErr;
	pContext->pContextErr = pContextErr;
	if (!PostRecv(pContext))
	{
		Log("[%s]添加 Recv 失败 : socket<%d>", __FUNCTION__, s);
		delete pContext;
		Log("delete <%x>", pContext);
		return false;
	}
	//Log("[%s]添加 Recv : socket<%d>", __FUNCTION__, s);
	return true;
}

bool CMonitor::PostRecvFrom(SOCKET s, CT_RecvDataFrom pfnRecv, std::shared_ptr<void> pContextRecv,
	CT_Err pfnErr, std::shared_ptr<void> pContextErr)
{

	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return false;
	}

	RecvFromContext *pContext = new RecvFromContext;
	//Log("new <%x>", pContext);
	pContext->s = s;
	pContext->op = OPIndex::eRecvFrom;
	pContext->pfnExe = &CMonitor::ExeRecvFrom;
	pContext->pfnRecv = pfnRecv;
	pContext->pContextRecv = pContextRecv;
	pContext->pfnErr = pfnErr;
	pContext->pContextErr = pContextErr;
	if (!PostRecvFrom(pContext))
	{
		Log("[%s]添加 Recvfrom 失败: socket<%d>", __FUNCTION__, s);
		delete pContext;
		Log("delete <%x>", pContext);
		return false;
	}
	//Log("[%s]添加 Recvfrom : socket<%d>", __FUNCTION__, s);
	return true;
}


CMonitor::SendContext* CMonitor::PostSend( SOCKET s, CT_Sent pfnSent, std::shared_ptr<void> pContextSent, CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return NULL;
	}
	SendContext *pContext = new SendContext;
	//Log("new <%x>", pContext);
	pContext->s = s;
	pContext->op = OPIndex::eSend;
	pContext->pfnExe = &CMonitor::ExeSend;
	pContext->pfnSent = pfnSent;
	pContext->pContextSent = pContextSent;
	pContext->pfnErr = pfnErr;
	pContext->pContextErr = pContextErr;
	return pContext;
}

CMonitor::SendToContext* CMonitor::PostSendTo(SOCKET s, CT_SentTo pfnSent, std::shared_ptr<void> pContextSent, CT_Err pfnErr, std::shared_ptr<void> pContextErr)
{
	if (NULL == m_hCP)
	{
		Log("[%s] 还未初始化: socket<%d>!", __FUNCTION__, s);
		return NULL;
	}
	SendToContext *pContext = new SendToContext;
	//Log("new <%x>", pContext);
	pContext->s = s;
	pContext->op = OPIndex::eSendTo;
	pContext->pfnExe = &CMonitor::ExeSendTo;
	pContext->pfnSent = pfnSent;
	pContext->pContextSent = pContextSent;
	pContext->pfnErr = pfnErr;
	pContext->pContextErr = pContextErr;
	return pContext;
}

int CMonitor::GetProcessorCount()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}

void CMonitor::ThreadFunc()
 {
	DWORD dwNumberOfBytes(0);
	ULONG_PTR pCompletionKey(0);
	LPOVERLAPPED lpOverlapped;
	unsigned int dwWait = GetNextTimer();
	unsigned short wCount = 0;
	while (!m_bStop)
	{
		BOOL bRet = GetQueuedCompletionStatus(
			m_hCP,
			&dwNumberOfBytes,
			&pCompletionKey,
			&lpOverlapped,
			dwWait
		);

		if (INVALID_SOCKET == pCompletionKey)
		{
			Log("[%s]完成端口<%d>接收到退出消息！退出！", __FUNCTION__, m_hCP);
			break;
		}
		//Log("[%s]发生了事件: socket <%d>", __FUNCTION__, pCompletionKey);
		if (!bRet)
		{
			DWORD dwErr = GetLastError();
			if (WAIT_TIMEOUT == dwErr)
			{
				dwWait = GetNextTimer();
				OnTimer();
			}
			else if (nullptr == lpOverlapped)
			{
				//直接从这里退出，存在着内存泄露风险
				Log("完成端口出现了重大故障了！将要退出！");
				break;
			}
			if (nullptr != lpOverlapped)
			{
				Log("[%s]完成端口访问socket<%d>失败，可能断开了<%d>！", __FUNCTION__, pCompletionKey, GetLastError());
				Context *pContext = CONTAINING_RECORD(lpOverlapped, Context, ovlp);
				OnErrDelete(pContext);
			}
			continue;
		}
		dwWait = (0 == ++wCount) ? GetNextTimer() : 10;
		Context *pContext = CONTAINING_RECORD(lpOverlapped, Context, ovlp);
		bool bExeRet = false;
		if (nullptr != pContext->pfnExe)
		{
			bExeRet = (this->*(pContext->pfnExe))(pContext, dwNumberOfBytes);
		}
		else
		{
			Log("[%s] 错误：不应该出现的情况，执行函数指针为空, 一定是代码有误! socket <%d>", __FUNCTION__, pContext->s);
		}		
		if (!bExeRet)
		{
			OnErrDelete(pContext);
		}
	}
	Log("[%s] 完成端口<%d>, 线程退出！", __FUNCTION__, m_hCP);
}

bool CMonitor::PostAcceptEx(AcceptContext *pContext)
{
	pContext->sClient = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == pContext->sClient)
	{
		Log("[%s]创建接受客户端的Socket失败！错误代码<%d>", __FUNCTION__, WSAGetLastError());
		return false;
	}

	int nSockAddrLenAdd16 = sizeof(sockaddr_in) + 16;
	if (!pContext->pfnAcceptEx(
		pContext->s,
		pContext->sClient,
		pContext->wsaBuff.buf,
		pContext->wsaBuff.len - nSockAddrLenAdd16 * 2,
		nSockAddrLenAdd16,
		nSockAddrLenAdd16,
		nullptr,
		&pContext->ovlp
	) && WSA_IO_PENDING != WSAGetLastError())
	{
		Log("[%s]投递AcceptEx失败，错误代码<%d>", __FUNCTION__, WSAGetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostConnectEx(ConnectExContext *pContext)
{
	if (!pContext->pfnConnectEx(
		pContext->s,
		(sockaddr*)&pContext->addr,
		sizeof(pContext->addr),
		nullptr,
		0,
		nullptr,
		&pContext->ovlp
	) && WSA_IO_PENDING != WSAGetLastError())
	{
		Log("[%s]投递ConnectEx失败，错误代码<%d>", __FUNCTION__, WSAGetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostRecv(RecvContext *pContext)
{
	DWORD dwBytes(0), dwFlag(0);
	int nRet = WSARecv(pContext->s, &pContext->wsaBuff, 1, &dwBytes, &dwFlag, &pContext->ovlp, NULL);
	
	if (SOCKET_ERROR == nRet && WSA_IO_PENDING != WSAGetLastError())
	{
		Log("[%s]投递Recv失败: socket<%d>, Err<%d>", __FUNCTION__, pContext->s, WSAGetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostRecvFrom(RecvFromContext *pContext)
{
	DWORD dwFlag(0);
	int nRet = WSARecvFrom(
		pContext->s,
		&pContext->wsaBuff,
		1,
		nullptr,
		&dwFlag,
		(sockaddr*)&pContext->addrFrom, 
		&pContext->nAddrFromLen,
		&pContext->ovlp,
		nullptr
	);
	if (SOCKET_ERROR == nRet && WSA_IO_PENDING != WSAGetLastError())
	{
		Log("[%s]投递RecvFrom失败: socket<%d>, Err<%d>", __FUNCTION__, pContext->s, WSAGetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostSend(SendContext *pContext)
{
	int nRet = WSASend(
		pContext->s,
		&pContext->wsaBuff,
		1,
		nullptr,
		0,
		&pContext->ovlp,
		nullptr
		);
	if (SOCKET_ERROR == nRet && WSA_IO_PENDING != WSAGetLastError())
	{
		Log("[%s]投递Send失败： socket<%d>, Err<%d>", __FUNCTION__,  pContext->s, WSAGetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostSendTo(SendToContext *pContext)
{
	int nRet = WSASendTo(
		pContext->s,
		&pContext->wsaBuff,
		1,
		nullptr,
		0,
		(sockaddr*)&pContext->addSendTo,
		sizeof(pContext->addSendTo),
		&pContext->ovlp,
		nullptr
	);
	if (SOCKET_ERROR == nRet && WSA_IO_PENDING != WSAGetLastError())
	{
		Log("[%s]投递SendTo失败： socket<%d>, Err<%d>", __FUNCTION__, pContext->s, WSAGetLastError());
		return false;
	}
	return true;
}

bool CMonitor::ExeAcceptEx(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	AcceptContext *pContext = (AcceptContext *)pContextBase;
	sockaddr_in *addrRemote(nullptr), *addrLocal(nullptr);
	int nRemoteLen(sizeof(sockaddr_in)), nLocalLen(sizeof(sockaddr_in));
	int nAddrLenAdd16 = sizeof(sockaddr_in) + 16;
	pContext->pfnGetAcceptExSockAddrs(
		pContext->wsaBuff.buf,
		pContext->wsaBuff.len - nAddrLenAdd16 * 2,
		nAddrLenAdd16,
		nAddrLenAdd16,
		(sockaddr**)&addrLocal,
		&nLocalLen,
		(sockaddr**)&addrRemote,
		&nRemoteLen
	);

	//输出至外部
	char buff[32]{ 0 };
	inet_ntop(AF_INET, (void*)&(addrRemote->sin_addr), buff, 64);
	Log("[%s]客户端<%s:%d>连接: socket<%d> 接收到数据<%d>字节", __FUNCTION__,
		buff, htons(addrRemote->sin_port), pContext->sClient, nDataLen);
	if (nullptr != pContext->pfnClient)
	{
		pContext->pfnClient(pContext->pContextClient,
			pContext->sClient,
			std::string(buff),
			htons(addrRemote->sin_port)
		);
	}
	else
	{
		Log("[%s]关闭未被接收的客户端socket <%d> ", __FUNCTION__,  pContext->sClient);
		closesocket(pContext->sClient);
	}
	pContext->sClient = INVALID_SOCKET;
	return PostAcceptEx(pContext);	
}

bool CMonitor::ExeConnectEx(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	ConnectExContext *pContext = (ConnectExContext *)pContextBase;
	//Log("[%s]连接服务器成功： socket<%d>", __FUNCTION__, pContext->s);
	if (nullptr != pContext->pfnConnected)
	{
		pContext->pfnConnected(pContext->pContextConnected, pContext->s);
	}
	delete pContext;
	Log("delete <%x>", pContext);
	return true;
}

bool CMonitor::ExeRecv(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	RecvContext *pContext = (RecvContext *)pContextBase;
	if (0 == nDataLen)
	{
		Log("[%s] 传输字节数为0，socket<%d>断开了！", __FUNCTION__, pContext->s);
		return false;
	}
	if (nullptr != pContext->pfnRecv)
	{
		//Log("[%s]Socket<%d>接收到：%d 字节", __FUNCTION__, pContext->s, nDataLen);
		pContext->pfnRecv(pContext->pContextRecv, pContext->s, pContext->wsaBuff.buf, nDataLen);
	}
	return PostRecv(pContext);
}

bool CMonitor::ExeRecvFrom(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	RecvFromContext *pContext = (RecvFromContext *)pContextBase;
	if (nullptr != pContext->pfnRecv)
	{
		//输出至外部
		char buff[32]{ 0 };
		inet_ntop(AF_INET, (void*)&pContext->addrFrom.sin_addr, buff, 64);
		//Log("[%s]Socket<%d>接收到来自<%s:%d>的：%d 字节", __FUNCTION__, pContext->s,
		//	buff, htons(pContext->addrFrom.sin_port), nDataLen);
		if (!pContext->pfnRecv(
			pContext->pContextRecv,
			pContext->s,
			pContext->wsaBuff.buf,
			nDataLen,
			buff,
			htons(pContext->addrFrom.sin_port)
		))
		{
			return false;
		}
	}
	return PostRecvFrom(pContext);
}

bool CMonitor::ExeSend(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	SendContext *pContext = (SendContext *)pContextBase;
	if (nullptr !=  pContext->pfnSent)
	{
		pContext->pfnSent(pContext->pContextSent, pContext->s, nDataLen, pContext);
	}
	return true;
}

bool CMonitor::ExeSendTo(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	SendToContext *pContext = (SendToContext *)pContextBase;
	if (nullptr != pContext->pfnSent)
	{
		pContext->pfnSent(pContext->pContextSent, pContext->s, nDataLen, pContext);
	}
	return true;
}

//发生错误的处理，删除环境变量
void CMonitor::OnErrDelete(Context *pContext)
{
	if (nullptr != pContext->pfnErr)
	{
		pContext->pfnErr(pContext->pContextErr, pContext->s, WSAGetLastError());
	}
	delete pContext;
	//Log("delete<%x>", pContext);
}

//获取下一次定时器的时间
unsigned int CMonitor::GetNextTimer()
{
	unsigned int dwCur = GetTickCount();
	if (m_setTimer.empty())
	{
		return 500;
	}
	unsigned int nNext(500);
	m_mtxTimer.lock();
	for (auto &pNode : m_setTimer)
	{
		 nNext = pNode->dwLast + pNode->dwInterval - dwCur;
		 unsigned int nDif2 = dwCur - (pNode->dwLast + pNode->dwInterval);
		 if (nDif2 < nNext)
		 {
			 m_mtxTimer.unlock();
			 OnTimer();
			 return GetNextTimer();
		 }
		 break;
	}
	m_mtxTimer.unlock();
	if (nNext > 500)
	{
		nNext = 500;
	}
	return nNext;
}

//定时器处理函数
void CMonitor::OnTimer()
{
	if (m_setTimer.empty())
	{
		return;
	}
	m_mtxTimer.lock();
	if (m_setTimer.empty())
	{
		m_mtxTimer.unlock();
		return;
	}
	unsigned int dwCur = GetTickCount();
	auto it = m_setTimer.begin();
	unsigned int nDif1 = (*it)->dwLast + (*it)->dwInterval - dwCur;
	if (nDif1 == 0 || nDif1 > MAX_TIMER_INTERVAL)
	{
		//定时时间已到,我们认为不存在MAX_TIMER_INTERVAL秒以上的定时器，如果>MAX_TIMER_INTERVAL秒，则认为定时时间已经过了
		auto pNode = *it;
		m_setTimer.erase(it);
		pNode->dwLast = dwCur;
		m_setTimer.insert(pNode);
		m_mtxTimer.unlock();
		if (nullptr != pNode->pfn)
		{
			pNode->pfn(pNode->pContext, pNode->nFlag);
		}
		return OnTimer();
	}
	m_mtxTimer.unlock();
}

bool CMonitor::GetAcceptExFunc(SOCKET s, LPFN_ACCEPTEX  &pfnAcceptEx, LPFN_GETACCEPTEXSOCKADDRS &pfnGetAcceptExSockAddrs)
{
	//函数的GUID
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	GUID guidGetAcceptExSockAddr = WSAID_GETACCEPTEXSOCKADDRS;

	DWORD dwBytes = 0;
	if (SOCKET_ERROR == WSAIoctl(s,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx,
		sizeof(guidAcceptEx),
		&pfnAcceptEx,
		sizeof(pfnAcceptEx),
		&dwBytes,
		nullptr,
		nullptr))
	{
		Log("获取AcceptEx函数指针失败！错误代码：%d", WSAGetLastError());
		return false;
	}
	if (SOCKET_ERROR == WSAIoctl(s,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidGetAcceptExSockAddr,
		sizeof(guidGetAcceptExSockAddr),
		&pfnGetAcceptExSockAddrs,
		sizeof(pfnGetAcceptExSockAddrs),
		&dwBytes,
		nullptr,
		nullptr
	))
	{
		Log("获取GetAcceptExSockAddr函数指针失败！错误代码：%d", WSAGetLastError());
		return false;
	}
	return true;
}

//获取ConnectEx函数指针
bool CMonitor::GetConnectExFunc(SOCKET s, LPFN_CONNECTEX &pfnConnectEx)
{
	DWORD dwBytes = 0;
	GUID guid = WSAID_CONNECTEX;
	if (SOCKET_ERROR == WSAIoctl(s,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid,
		sizeof(guid),
		&pfnConnectEx,
		sizeof(pfnConnectEx),
		&dwBytes,
		nullptr,
		nullptr
		))
	{
		Log("获取ConnectEx函数指针失败！错误代码：%d", WSAGetLastError());
		return false;
	}
	return true;
}


