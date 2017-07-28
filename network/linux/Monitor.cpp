#include "stdafx.h"
#include "Monitor.h"
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "Tool/TLog.h"

//��ʱ���ļ�����ʱ����������, (�ݶ���ֵΪ����24Сʱ��һ��ֵ)
#define MAX_TIMER_INTERVAL 86500000
//epoll�����������ֵ
#define MAX_EPOLL_EVENTS 1000
#define Log LogN(101)

bool TimerNode::aFlag[TIMER_FLAG_MAX] = {0};

//��ʱ����Ϣ�����������Ϊ����������Խ��鶨ʱ����ʱ�䲻Ҫ�ﵽMAX_TIMER_INTERVAL
bool operator <(const TimerPtr &L, const TimerPtr &R)
{	
	return (L.p->dwLast + L.p->dwInterval - (R.p->dwLast + R.p->dwInterval)) >= MAX_TIMER_INTERVAL;
}

unsigned int GetTickCount()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

//����
CMonitor::CMonitor()
	: m_bStop(false)
{
	m_nEpollFd = -1;
	m_pEvents = new epoll_event[MAX_EPOLL_EVENTS];
}

//����
CMonitor::~CMonitor(void)
{

}

//��ʼ��
bool CMonitor::Init()
{
	Log("[CMonitor::Init]");

#ifndef _WIN32
	FILE *pTemp = popen("sysctl -w net.core.rmem_max=8048576 net.core.wmem_max=8048576", "r");
	char buf[512] = { 0 };
	if (nullptr != pTemp)
	{
		while (fgets(buf, 512, pTemp))
		{
			Log("%s", buf);
		}
		fclose(pTemp);
		pTemp = nullptr;
	}
	pTemp = popen("ulimit -n 65535", "r");
	if (nullptr != pTemp)
	{
		while (fgets(buf, 512, pTemp))
		{
			Log("%s", buf);
		}
		fclose(pTemp);
		pTemp = nullptr;
	}
#endif

	//����epoll��ע�⣺��linux2.6.8�Ժ�epoll_create������size�����Ѳ��ٱ�ʹ��
	if (-1 == m_nEpollFd && -1 == (m_nEpollFd = epoll_create(MAX_EPOLL_EVENTS)))
	{
		Log("[CMonitor::Init] failed -- Err <%d - %s>", errno, strerror(errno));
		return false;
	}
	Log("EPoll = <%d>", m_nEpollFd);

	//���������߳�
	m_bStop = false;
	for (int i = 0; i < 1; ++i)
	{
		m_vecThread.push_back(std::thread(&CMonitor::TH_Work, this));
	}
	return true;
}

//����ʼ��
bool CMonitor::DeInit()
{
	Log("[CMonitor::DeInit]");

	//�ȴ������̹߳ر�
	m_bStop = true;
	for (size_t i = 0; i < m_vecThread.size(); ++i)
	{
		if (m_vecThread[i].joinable())
		{
			m_vecThread[i].join();
		}
	}
	m_vecThread.clear();
	
	//�ر�epoll
	if (-1 != m_nEpollFd)
	{
		close(m_nEpollFd);
		m_nEpollFd = -1;
	}	
	Log("[CMonitor::DeInit] OK");
	return true;
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
	TimerPtr pNode(std::make_shared<TimerNode>());
	int nFlag = pNode.p->Make(dwInterval, pContext, pfn);
	if (-1 == nFlag)
	{
		Log("[%s] ʧ�ܣ��޷���ȡ��ʱ����ʶ", __FUNCTION__);
		return -1;
	}
	pNode.p->Add(nFlag);
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
		if ((*it).p->nFlag == nFlag)
		{
			(*it).p->Del(nFlag);
			m_setTimer.erase(it);
			Log("[%s] ɾ����ʱ�� Flag<%d> �ܶ�ʱ����Ŀ<%d>��", __FUNCTION__, nFlag, m_setTimer.size());
			return true;
		}
	}
	return false;
}

/*******************************************************************************
* �������ƣ�	
* ����������	���һ���������ļ�������
* ���������	nFd				-- Ҫ��ӵ��ļ�������
*				nEvent			-- Ҫ�������¼�
*				pfnIn			-- ���Խ�������ʱ�Ĵ���ص�����
*				pContextIn		-- ���Խ������ݻص������Ļص�������������
*				pfnOut			-- ���Է�������ʱ�Ĵ���ص�����
*				pContextOut		-- ���Է�������ʱ�Ĵ���ص�������������
*				pfnErr			-- ��������Ĵ���ص�����
*				pContextErr		-- ��������Ĵ���ص�������������
* ���������	
* �� �� ֵ��	�ɹ�����true�����򷵻�false��
* ����˵����	ע�⣺EPOLLRDHUP|EPOLLET�����Զ����,���sokcet�����Ƿ������ġ�
*				ע�⣺��������EPOLLONESHOT����˱��ӿڲ�֧��ʹ��EPOLLONESHOT��
*				ע�⣺���ṩ��Ӧ��ɾ���ӿڣ����������ļ�����������EPOLLERR|EPOLLHUPʱ���Զ�ɾ�����ļ��������ļ�����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
bool CMonitor::Add( int nFd, unsigned int nEvent, CT_In pfnIn, std::shared_ptr<void> pContextIn,
	CT_Out pfnOut, std::shared_ptr<void> pContextOut, CT_Err pfnErr, std::shared_ptr<void> pContextErr )
{
//	Log("[CMonitor::Add](nFd=%u, nEvent=0x%x)", nFd, nEvent);
	if (-1 == m_nEpollFd)
	{
		Log("[CMonitor::Add] Failed -- Epoll not ready!");
		return false;
	}
	if ((nEvent & EPOLLONESHOT) == EPOLLONESHOT)
	{
		Log("[CMonitor::Add] Failed -- event (EPOLLONESHOT) are not Alowed!");
		return false;
	}
	Context *pContext = new Context;
	pContext->nFd = nFd;
	pContext->nEvents = nEvent|EPOLLRDHUP|EPOLLET;
	pContext->pfnIn = pfnIn;
	pContext->pContextIn = pContextIn;
	pContext->pfnOut = pfnOut;
	pContext->pContextOut = pContextOut;
	pContext->pfnErr = pfnErr;
	pContext->pContextErr = pContextErr;
	epoll_event stEvent;
	stEvent.events = pContext->nEvents;
	stEvent.data.ptr = pContext;
	if (-1 == epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, nFd, &stEvent))
	{
		Log("[CMonitor::CmdAdd::Exe] failed! -- epoll_ctl err<%d - %s>", errno, strerror(errno));
		delete pContext;
		return false;
	}
	return true;
}

//��ӡ��Ϣ
void CMonitor::Print()
{
}

//�����̺߳���
void CMonitor::TH_Work()
{
	unsigned int dwWait = GetNextTimer();
	unsigned short wCount = 0;
	while (!m_bStop)
	{
		//����Ƿ����¼��������ȴ�ʱ��ĵ�λ�Ǻ���
		int nfds = epoll_wait(m_nEpollFd, m_pEvents, MAX_EPOLL_EVENTS, dwWait);
		if (-1 == nfds && EINTR != errno)
		{
			Log("[CMonitor::Work] epoll_wait failed -- Err<%d - %s>", errno, strerror(errno));
			continue;;
		}
		if (0 == nfds)
		{
			//��ʱ��
			OnTimer();
			dwWait = GetNextTimer();
			continue;;
		}	
		dwWait = (0 == ++wCount) ? GetNextTimer() : 10;		
		for (int i = 0; i < nfds; ++i)
		{
			epoll_event &result = m_pEvents[i];
			Context *pContext = (Context*)result.data.ptr;
			//Log("nfd<%d>  -- Event<%x>", pContext->nFd, result.events);

			//EPOLLOUT = 0x4�����ӶϿ�ǰ�յ������ݣ�Ҳ��Ҫ����
			if (0 != (result.events & EPOLLIN) && nullptr != pContext->pfnIn)
			{
				pContext->pfnIn(pContext->pContextIn, pContext->nFd);
			}

			//EPOLLIN = 0x1
			if (0 != (result.events & EPOLLOUT) && nullptr != pContext->pfnOut)
			{
				pContext->pfnOut(pContext->pContextOut, pContext->nFd);
			}

			//������
			if (0 != (result.events & (/*EPOLLRDHUP|*/EPOLLERR | EPOLLHUP)) && nullptr != pContext->pfnErr)
			{
				//EPOLLRDHUP=0x2000  EPOLLERR = 0x8   EPOLLHUP=0x10
				//Log("fd<%d> Error", pContext->nFd);
				epoll_ctl(m_nEpollFd, EPOLL_CTL_DEL, pContext->nFd, nullptr);
				pContext->pfnErr(pContext->pContextErr, pContext->nFd, (result.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)));
				delete pContext;
				continue;
			}
		}
	}
	Log("[CMonitor::TH_Work]EpollFd<%d> Work Thread End!", m_nEpollFd);
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
		 nNext = pNode.p->dwLast + pNode.p->dwInterval - dwCur;
		 unsigned int nDif2 = dwCur - (pNode.p->dwLast + pNode.p->dwInterval);
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
	unsigned int nDif1 = (*it).p->dwLast + (*it).p->dwInterval - dwCur;
	if (nDif1 == 0 || nDif1 > MAX_TIMER_INTERVAL)
	{
		//��ʱʱ���ѵ�,������Ϊ������MAX_TIMER_INTERVAL�����ϵĶ�ʱ�������>MAX_TIMER_INTERVAL�룬����Ϊ��ʱʱ���Ѿ�����
		auto pNode = *it;
		m_setTimer.erase(it);
		pNode.p->dwLast = dwCur;
		m_setTimer.insert(pNode);
		m_mtxTimer.unlock();
		if (nullptr != pNode.p->pfn)
		{
			pNode.p->pfn(pNode.p->pContext, pNode.p->nFlag);
		}
		return OnTimer();
	}
	m_mtxTimer.unlock();
}


