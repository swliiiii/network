#pragma once
#include <vector>
#include <map>
#include <sys/epoll.h>
#include <memory>
#include <mutex>
#include <thread>
#include <set>
#include <limits.h>

//获取启动时间毫秒数
extern unsigned int GetTickCount();

class CMonitor;

//环境变量的混入类
class CContextBase
{
public:
	CContextBase() {}
	virtual ~CContextBase() {}

	//设置网路驱动器,要使用网络驱动器，该函数必须在类内使用网络驱动器之前调用。
	void SetMonitor(std::shared_ptr<CMonitor> p)
	{
		m_pMonitor = p;
	}

	//获取网络驱动器
	std::shared_ptr<CMonitor> GetMonitor()
	{
		return m_pMonitor;
	}
	
	//保存自身的智能指针
	void SetThisPtr(std::weak_ptr<CContextBase> p)
	{
		m_pThis = p;
	}

	//等待指定的对象析构
	static void WaitDestroy(std::weak_ptr<void> p, int nTimeout = INT_MAX)
	{
		while (!p.expired() && nTimeout-- > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	//创建指定的子类
	template <typename T>
	static std::shared_ptr<T> Create()
	{
		std::shared_ptr<T> p(new T());
		p->SetThisPtr(p);
		return p;
	}
	
protected:

	//自身的智能指针
	std::weak_ptr<CContextBase> m_pThis;

	//网络驱动器指针
	std::shared_ptr<CMonitor> m_pMonitor;

};


//有输入事件发生的回调函数定义
using CT_In=  void (*)(
	std::shared_ptr<void> pContext,		//环境变量
	int nfd						//发生事件的文件描述符
	);

//输出可用的回调函数定义
using CT_Out = void(*)(
	std::shared_ptr<void> pContext,		//环境变量
	int nfd						//发生事件的文件描述符
	);

//错误的回调函数定义
using CT_Err = void(*)(
	std::shared_ptr<void> pContext,		//环境变量
	int nfd,					//发生事件的文件描述符
	int nError					//错误号
	);

#define TIMER_FLAG_MAX 5000

//定时器信息
struct TimerNode
{
	//定时器回调函数定义
	using CT_Timer = void(*)(
		std::shared_ptr<void> pContext,	//环境变量
		int nTimer						//定时器ID
		);

	TimerNode() : nFlag(-1), dwLast(0), dwInterval(0), pfn(nullptr) {}

	//生成定时器信息
	int Make(unsigned int _dwInterval, std::shared_ptr<void> _pContext, CT_Timer _pfn)
	{
		for (int i = 0; i < TIMER_FLAG_MAX; ++i)
		{
			if (aFlag[i] == false)
			{
				nFlag = i;
				dwLast = GetTickCount();
				dwInterval = _dwInterval;
				pContext = _pContext;
				pfn = _pfn;
				return nFlag;
			}
		}
		return -1;
	}

	//添加定时器
	static void Add(int nFlag)
	{
		aFlag[nFlag] = true; 
	}

	//删除定时器标识
	static void Del(int nFlag)
	{
		aFlag[nFlag] = false;
	}

	int nFlag;							//定时器标识
	unsigned int dwLast;				//上次触发的时刻,单位毫秒
	unsigned int dwInterval;			//触发间隔，单位毫秒
	std::shared_ptr<void> pContext;		//定时器回调函数环境变量
	CT_Timer pfn;						//定时器回调函数指针

	static bool aFlag[TIMER_FLAG_MAX];
};

//TimerNode的智能指针封装类（直接重载智能指针的<运算符不起作用，所以重新封装下）
struct TimerPtr
{
	std::shared_ptr<TimerNode> p;
	TimerPtr(std::shared_ptr<TimerNode> _p) : p(_p) {}
};

//定时器信息的排序规则
bool operator <(const TimerPtr &L, const TimerPtr &R);

class CMonitor
{
public:
	CMonitor();

	~CMonitor(void);

	//初始化
	bool Init();

	//反初始化
	bool DeInit();

	//添加定时器,返回定时器的标识，该标识用于删除定时器,失败返回-1
	//定时器的时间间隔不能大于或等于86500000毫秒
	int AddTimer(unsigned int dwInterval, std::shared_ptr<void> pContext, TimerNode::CT_Timer pfn);

	//删除定时器
	bool DelTimer(int nFlag);

	//添加一个监听的文件描述符，如果文件描述符已经存在，则会返回失败。
	//注意：EPOLLRDHUP|EPOLLET将被自动添加,因此sokcet必须是非阻塞的。
	//注意：不适用于EPOLLONESHOT，因此本接口不支持使用EPOLLONESHOT。
	//注意：不提供对应的删除接口，当监听的文件描述符出现EPOLLERR|EPOLLHUP时，自动删除对文件描述符的监听。
	bool Add(
		int nFd,						//要添加的文件描述符
		unsigned int nEvent,			//要侦听的事件
		CT_In pfnIn,					//可以接收数据时的处理回调函数
		std::shared_ptr<void> pContextIn,			//可以接收数据回调函数的回调函数环境变量		
		CT_Out pfnOut,					//可以发送数据时的处理回调函数
		std::shared_ptr<void> pContextOut,			//可以发送数据时的处理回调函数环境变量
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void> pContextErr			//发生错误的处理回调函数环境变量
		);

	//打印内部信息
	void Print();

protected:

	//线程
	std::vector<std::thread> m_vecThread;

	//线程停止标识
	volatile bool m_bStop;

	//事件
	epoll_event *m_pEvents;

	//Epoll的文件描述符
	int m_nEpollFd;

	//一个Socket对用的环境变量
	struct Context
	{	
		int nFd;						//对应的socket
		unsigned int nEvents;			//监视的事件
		CT_In pfnIn;					//发生输入事件的通知回调函数
		std::shared_ptr<void> pContextIn;			//发生输入事件的通知回调函数环境变量
		CT_Out pfnOut;					//可以输出的通知回调函数
		std::shared_ptr<void> pContextOut;			//可以输出的通知回调函数环境变量
		CT_Err pfnErr;					//错误事件的通知回调函数，发生此事件后，该socket会在CMonitor内部被删除。
		std::shared_ptr<void> pContextErr;			//错误事件的通知回调函数环境变量

		Context(): pfnIn(nullptr), pfnOut(nullptr) ,pfnErr(nullptr)
		{}
	};

	//定时器列表及访问保护
	std::multiset<TimerPtr> m_setTimer;	
	std::mutex m_mtxTimer;

protected:

	//工作线程函数
	void TH_Work();

	//定时器相关函数
	unsigned int GetNextTimer(); //获取下一次定时器的时间
	void OnTimer();				 //定时器处理函数

};

