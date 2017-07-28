#include "stdafx.h"
#include "Monitor.h"
#include <sstream>
#include <WS2tcpip.h>
#include "../Tool/TLog.h"

//��ʱ���ļ�����ʱ����������
#define MAX_TIMER_INTERVAL 86500000
#define Log LogN(101)

bool TimerNode::aFlag[TIMER_FLAG_MAX] = {0};

//��ʱ����Ϣ�����������Ϊ����������Խ��鶨ʱ����ʱ�䲻Ҫ�ﵽMAX_TIMER_INTERVAL
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

//��ʼ��
bool CMonitor::Init()
{
	Log("[%s]", __FUNCTION__);

	//������ɶ˿�
	int nThreadCount = 1;// GetProcessorCount();
	m_hCP = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL,
		0,
		nThreadCount			//����Ӧ�ó���ͬʱִ�е��߳�������0��ʾ���ݴ���������������
	);
	if (NULL == m_hCP)
	{
		Log("[%s]������ɶ˿�ʧ��: err<%d>", __FUNCTION__, GetLastError());
		return false;
	}
	  
	//���������߳�,Ŀǰ���ڶ�ʱ����Ƶ�Ӱ�죬�������ö���߳�
	m_bStop = false;
	for (int i = 0; i < nThreadCount; ++i)
	{
		m_vecThread.push_back(std::thread(&CMonitor::ThreadFunc, this));
	}
	return true;
}

//����ʼ��
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
* �������ƣ�
* ����������	��Ӷ�ʱ��,���ض�ʱ���ı�ʶ
* ���������	dwInterval		-- ��ʱ���������λ����
*				pContext		-- ��ʱ���ص�������������
*				pfn				-- ��ʱ���ص�����ָ��
* ���������
* �� �� ֵ��	�ɹ����ض�ʱ����ʶ�����򷵻�-1��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/09	˾����	      ����
*******************************************************************************/
int CMonitor::AddTimer(unsigned int dwInterval, std::shared_ptr<void> pContext, TimerNode::CT_Timer pfn)
{
	std::lock_guard<std::mutex> lock(m_mtxTimer);
	if (dwInterval > MAX_TIMER_INTERVAL)
	{
		Log("[%s] ʧ�ܣ�Interval(%d)  > %d��", __FUNCTION__, dwInterval, MAX_TIMER_INTERVAL);
		return -1;
	}
	TimerPtr pNode = std::make_shared<TimerNode>();
	int nFlag = pNode->Make(dwInterval, pContext, pfn);
	if (-1 == nFlag)
	{
		Log("[%s] ʧ�ܣ��޷���ȡ��ʱ����ʶ", __FUNCTION__);
		return -1;
	}
	pNode->Add(nFlag);
	m_setTimer.insert(pNode);	
	Log("[%s] �¶�ʱ�� Flag<%d> Interval<%d>���룬�ܶ�ʱ����Ŀ<%d>", __FUNCTION__, nFlag, dwInterval, m_setTimer.size());
	return nFlag;
}

/*******************************************************************************
* �������ƣ�
* ����������	ɾ����ʱ��
* ���������	nFlag		-- Ҫɾ���Ķ�ʱ����ʶ
* ���������
* �� �� ֵ��	�ɹ�����true����ʱ����ʶ�����ڷ���fasle��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/09	˾����	      ����
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
			Log("[%s] ɾ����ʱ�� Flag<%d> �ܶ�ʱ����Ŀ<%d>��", __FUNCTION__, nFlag, m_setTimer.size());
			return true;
		}
	}
	return false;
}

//��socket�󶨵���ɶ˿�
bool CMonitor::Attach(SOCKET s)
{
	if (NULL == m_hCP)
	{
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
		return false;
	}
	if (m_hCP != CreateIoCompletionPort((HANDLE)s, m_hCP, s, 0))
	{
		Log("[%s]��ӵ���ɶ˿�ʧ��: Socket<%d>, err<%d> ", __FUNCTION__, s, GetLastError());
		return false;
	}
	return true;
}

bool CMonitor::PostAcceptEx( SOCKET s, CT_Client pfnClient, std::shared_ptr<void> pContextClient,
	CT_Err pfnErr, std::shared_ptr<void> pContextErr,int nCount /* = 1*/)
{
	if (NULL == m_hCP)
	{
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
		return false;
	}
	LPFN_ACCEPTEX pfnAcceptEx = nullptr;
	LPFN_GETACCEPTEXSOCKADDRS pfnGetAddr = nullptr;
	if (!GetAcceptExFunc(s, pfnAcceptEx, pfnGetAddr))
	{
		Log("[%s]��ȡAcceptEx����ָ��ʧ��: socket<%d>", __FUNCTION__, s);
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
			Log("[%s]��ӵ�<%d>�� Accept ʧ��: socket<%d>",__FUNCTION__, i, s);
			delete pContext;
			Log("delete <%x>", pContext);
			return false;
		}
	}
	//Log("[%s] ���Accept ��<%d>��: socket<%d>", __FUNCTION__, nCount, s);
	return true;
}

bool CMonitor::PostConnectEx( SOCKET s, sockaddr_in addrServer, CT_Connected pfnConnected,
	std::shared_ptr<void> pContextConnected, CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
	if (NULL == m_hCP)
	{
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
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
		Log("[%s]��� Connected ʧ��: socket<%d> ", __FUNCTION__, s);
		delete pContext;
		Log("delete <%x>", pContext);
		return false;
	}
	//Log("[%s]��� Connected �ɹ�: socket <%d>", __FUNCTION__, s);
	return true;
}

bool CMonitor::PostRecv( SOCKET s, CT_RecvData pfnRecv, std::shared_ptr<void> pContextRecv,
	CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
	if (NULL == m_hCP)
	{
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
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
		Log("[%s]��� Recv ʧ�� : socket<%d>", __FUNCTION__, s);
		delete pContext;
		Log("delete <%x>", pContext);
		return false;
	}
	//Log("[%s]��� Recv : socket<%d>", __FUNCTION__, s);
	return true;
}

bool CMonitor::PostRecvFrom(SOCKET s, CT_RecvDataFrom pfnRecv, std::shared_ptr<void> pContextRecv,
	CT_Err pfnErr, std::shared_ptr<void> pContextErr)
{

	if (NULL == m_hCP)
	{
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
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
		Log("[%s]��� Recvfrom ʧ��: socket<%d>", __FUNCTION__, s);
		delete pContext;
		Log("delete <%x>", pContext);
		return false;
	}
	//Log("[%s]��� Recvfrom : socket<%d>", __FUNCTION__, s);
	return true;
}


CMonitor::SendContext* CMonitor::PostSend( SOCKET s, CT_Sent pfnSent, std::shared_ptr<void> pContextSent, CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
	if (NULL == m_hCP)
	{
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
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
		Log("[%s] ��δ��ʼ��: socket<%d>!", __FUNCTION__, s);
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
			Log("[%s]��ɶ˿�<%d>���յ��˳���Ϣ���˳���", __FUNCTION__, m_hCP);
			break;
		}
		//Log("[%s]�������¼�: socket <%d>", __FUNCTION__, pCompletionKey);
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
				//ֱ�Ӵ������˳����������ڴ�й¶����
				Log("��ɶ˿ڳ������ش�����ˣ���Ҫ�˳���");
				break;
			}
			if (nullptr != lpOverlapped)
			{
				Log("[%s]��ɶ˿ڷ���socket<%d>ʧ�ܣ����ܶϿ���<%d>��", __FUNCTION__, pCompletionKey, GetLastError());
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
			Log("[%s] ���󣺲�Ӧ�ó��ֵ������ִ�к���ָ��Ϊ��, һ���Ǵ�������! socket <%d>", __FUNCTION__, pContext->s);
		}		
		if (!bExeRet)
		{
			OnErrDelete(pContext);
		}
	}
	Log("[%s] ��ɶ˿�<%d>, �߳��˳���", __FUNCTION__, m_hCP);
}

bool CMonitor::PostAcceptEx(AcceptContext *pContext)
{
	pContext->sClient = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == pContext->sClient)
	{
		Log("[%s]�������ܿͻ��˵�Socketʧ�ܣ��������<%d>", __FUNCTION__, WSAGetLastError());
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
		Log("[%s]Ͷ��AcceptExʧ�ܣ��������<%d>", __FUNCTION__, WSAGetLastError());
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
		Log("[%s]Ͷ��ConnectExʧ�ܣ��������<%d>", __FUNCTION__, WSAGetLastError());
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
		Log("[%s]Ͷ��Recvʧ��: socket<%d>, Err<%d>", __FUNCTION__, pContext->s, WSAGetLastError());
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
		Log("[%s]Ͷ��RecvFromʧ��: socket<%d>, Err<%d>", __FUNCTION__, pContext->s, WSAGetLastError());
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
		Log("[%s]Ͷ��Sendʧ�ܣ� socket<%d>, Err<%d>", __FUNCTION__,  pContext->s, WSAGetLastError());
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
		Log("[%s]Ͷ��SendToʧ�ܣ� socket<%d>, Err<%d>", __FUNCTION__, pContext->s, WSAGetLastError());
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

	//������ⲿ
	char buff[32]{ 0 };
	inet_ntop(AF_INET, (void*)&(addrRemote->sin_addr), buff, 64);
	Log("[%s]�ͻ���<%s:%d>����: socket<%d> ���յ�����<%d>�ֽ�", __FUNCTION__,
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
		Log("[%s]�ر�δ�����յĿͻ���socket <%d> ", __FUNCTION__,  pContext->sClient);
		closesocket(pContext->sClient);
	}
	pContext->sClient = INVALID_SOCKET;
	return PostAcceptEx(pContext);	
}

bool CMonitor::ExeConnectEx(Context *pContextBase, DWORD nDataLen)
{
	//Log("%s", __FUNCTION__);
	ConnectExContext *pContext = (ConnectExContext *)pContextBase;
	//Log("[%s]���ӷ������ɹ��� socket<%d>", __FUNCTION__, pContext->s);
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
		Log("[%s] �����ֽ���Ϊ0��socket<%d>�Ͽ��ˣ�", __FUNCTION__, pContext->s);
		return false;
	}
	if (nullptr != pContext->pfnRecv)
	{
		//Log("[%s]Socket<%d>���յ���%d �ֽ�", __FUNCTION__, pContext->s, nDataLen);
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
		//������ⲿ
		char buff[32]{ 0 };
		inet_ntop(AF_INET, (void*)&pContext->addrFrom.sin_addr, buff, 64);
		//Log("[%s]Socket<%d>���յ�����<%s:%d>�ģ�%d �ֽ�", __FUNCTION__, pContext->s,
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

//��������Ĵ���ɾ����������
void CMonitor::OnErrDelete(Context *pContext)
{
	if (nullptr != pContext->pfnErr)
	{
		pContext->pfnErr(pContext->pContextErr, pContext->s, WSAGetLastError());
	}
	delete pContext;
	//Log("delete<%x>", pContext);
}

//��ȡ��һ�ζ�ʱ����ʱ��
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

//��ʱ��������
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
		//��ʱʱ���ѵ�,������Ϊ������MAX_TIMER_INTERVAL�����ϵĶ�ʱ�������>MAX_TIMER_INTERVAL�룬����Ϊ��ʱʱ���Ѿ�����
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
	//������GUID
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
		Log("��ȡAcceptEx����ָ��ʧ�ܣ�������룺%d", WSAGetLastError());
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
		Log("��ȡGetAcceptExSockAddr����ָ��ʧ�ܣ�������룺%d", WSAGetLastError());
		return false;
	}
	return true;
}

//��ȡConnectEx����ָ��
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
		Log("��ȡConnectEx����ָ��ʧ�ܣ�������룺%d", WSAGetLastError());
		return false;
	}
	return true;
}


