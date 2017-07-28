#include "stdafx.h"
#include "Monitor.h"
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "Tool/TLog.h"

//定时器的间隔最大时长，毫秒数, (暂定该值为大于24小时的一个值)
#define MAX_TIMER_INTERVAL 86500000
//epoll监视数组最大值
#define MAX_EPOLL_EVENTS 1000
#define Log LogN(101)

bool TimerNode::aFlag[TIMER_FLAG_MAX] = {0};

//定时器信息的排序规则，因为这个规则，所以建议定时器的时间不要达到MAX_TIMER_INTERVAL
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

//构造
CMonitor::CMonitor()
	: m_bStop(false)
{
	m_nEpollFd = -1;
	m_pEvents = new epoll_event[MAX_EPOLL_EVENTS];
}

//析构
CMonitor::~CMonitor(void)
{

}

//初始化
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

	//创建epoll，注意：在linux2.6.8以后，epoll_create函数的size参数已不再被使用
	if (-1 == m_nEpollFd && -1 == (m_nEpollFd = epoll_create(MAX_EPOLL_EVENTS)))
	{
		Log("[CMonitor::Init] failed -- Err <%d - %s>", errno, strerror(errno));
		return false;
	}
	Log("EPoll = <%d>", m_nEpollFd);

	//创建工作线程
	m_bStop = false;
	for (int i = 0; i < 1; ++i)
	{
		m_vecThread.push_back(std::thread(&CMonitor::TH_Work, this));
	}
	return true;
}

//反初始化
bool CMonitor::DeInit()
{
	Log("[CMonitor::DeInit]");

	//等待处理线程关闭
	m_bStop = true;
	for (size_t i = 0; i < m_vecThread.size(); ++i)
	{
		if (m_vecThread[i].joinable())
		{
			m_vecThread[i].join();
		}
	}
	m_vecThread.clear();
	
	//关闭epoll
	if (-1 != m_nEpollFd)
	{
		close(m_nEpollFd);
		m_nEpollFd = -1;
	}	
	Log("[CMonitor::DeInit] OK");
	return true;
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
	TimerPtr pNode(std::make_shared<TimerNode>());
	int nFlag = pNode.p->Make(dwInterval, pContext, pfn);
	if (-1 == nFlag)
	{
		Log("[%s] 失败！无法获取定时器标识", __FUNCTION__);
		return -1;
	}
	pNode.p->Add(nFlag);
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
		if ((*it).p->nFlag == nFlag)
		{
			(*it).p->Del(nFlag);
			m_setTimer.erase(it);
			Log("[%s] 删除定时器 Flag<%d> 总定时器数目<%d>个", __FUNCTION__, nFlag, m_setTimer.size());
			return true;
		}
	}
	return false;
}

/*******************************************************************************
* 函数名称：	
* 功能描述：	添加一个监听的文件描述符
* 输入参数：	nFd				-- 要添加的文件描述符
*				nEvent			-- 要侦听的事件
*				pfnIn			-- 可以接收数据时的处理回调函数
*				pContextIn		-- 可以接收数据回调函数的回调函数环境变量
*				pfnOut			-- 可以发送数据时的处理回调函数
*				pContextOut		-- 可以发送数据时的处理回调函数环境变量
*				pfnErr			-- 发生错误的处理回调函数
*				pContextErr		-- 发生错误的处理回调函数环境变量
* 输出参数：	
* 返 回 值：	成功返回true，否则返回false。
* 其它说明：	注意：EPOLLRDHUP|EPOLLET将被自动添加,因此sokcet必须是非阻塞的。
*				注意：不适用于EPOLLONESHOT，因此本接口不支持使用EPOLLONESHOT。
*				注意：不提供对应的删除接口，当监听的文件描述符出现EPOLLERR|EPOLLHUP时，自动删除对文件描述符的监听。
* 修改日期		修改人	      修改内容
* ------------------------------------------------------------------------------
* 2017/03/07	司文丽	      创建
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

//打印信息
void CMonitor::Print()
{
}

//工作线程函数
void CMonitor::TH_Work()
{
	unsigned int dwWait = GetNextTimer();
	unsigned short wCount = 0;
	while (!m_bStop)
	{
		//检查是否有事件发生，等待时间的单位是毫秒
		int nfds = epoll_wait(m_nEpollFd, m_pEvents, MAX_EPOLL_EVENTS, dwWait);
		if (-1 == nfds && EINTR != errno)
		{
			Log("[CMonitor::Work] epoll_wait failed -- Err<%d - %s>", errno, strerror(errno));
			continue;;
		}
		if (0 == nfds)
		{
			//定时器
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

			//EPOLLOUT = 0x4，连接断开前收到的数据，也需要处理
			if (0 != (result.events & EPOLLIN) && nullptr != pContext->pfnIn)
			{
				pContext->pfnIn(pContext->pContextIn, pContext->nFd);
			}

			//EPOLLIN = 0x1
			if (0 != (result.events & EPOLLOUT) && nullptr != pContext->pfnOut)
			{
				pContext->pfnOut(pContext->pContextOut, pContext->nFd);
			}

			//错误处理
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
	unsigned int nDif1 = (*it).p->dwLast + (*it).p->dwInterval - dwCur;
	if (nDif1 == 0 || nDif1 > MAX_TIMER_INTERVAL)
	{
		//定时时间已到,我们认为不存在MAX_TIMER_INTERVAL秒以上的定时器，如果>MAX_TIMER_INTERVAL秒，则认为定时时间已经过了
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


