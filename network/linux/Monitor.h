#pragma once
#include <vector>
#include <map>
#include <sys/epoll.h>
#include <memory>
#include <mutex>
#include <thread>
#include <set>
#include <limits.h>

//��ȡ����ʱ�������
extern unsigned int GetTickCount();

class CMonitor;

//���������Ļ�����
class CContextBase
{
public:
	CContextBase() {}
	virtual ~CContextBase() {}

	//������·������,Ҫʹ���������������ú�������������ʹ������������֮ǰ���á�
	void SetMonitor(std::shared_ptr<CMonitor> p)
	{
		m_pMonitor = p;
	}

	//��ȡ����������
	std::shared_ptr<CMonitor> GetMonitor()
	{
		return m_pMonitor;
	}
	
	//�������������ָ��
	void SetThisPtr(std::weak_ptr<CContextBase> p)
	{
		m_pThis = p;
	}

	//�ȴ�ָ���Ķ�������
	static void WaitDestroy(std::weak_ptr<void> p, int nTimeout = INT_MAX)
	{
		while (!p.expired() && nTimeout-- > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	//����ָ��������
	template <typename T>
	static std::shared_ptr<T> Create()
	{
		std::shared_ptr<T> p(new T());
		p->SetThisPtr(p);
		return p;
	}
	
protected:

	//���������ָ��
	std::weak_ptr<CContextBase> m_pThis;

	//����������ָ��
	std::shared_ptr<CMonitor> m_pMonitor;

};


//�������¼������Ļص���������
using CT_In=  void (*)(
	std::shared_ptr<void> pContext,		//��������
	int nfd						//�����¼����ļ�������
	);

//������õĻص���������
using CT_Out = void(*)(
	std::shared_ptr<void> pContext,		//��������
	int nfd						//�����¼����ļ�������
	);

//����Ļص���������
using CT_Err = void(*)(
	std::shared_ptr<void> pContext,		//��������
	int nfd,					//�����¼����ļ�������
	int nError					//�����
	);

#define TIMER_FLAG_MAX 5000

//��ʱ����Ϣ
struct TimerNode
{
	//��ʱ���ص���������
	using CT_Timer = void(*)(
		std::shared_ptr<void> pContext,	//��������
		int nTimer						//��ʱ��ID
		);

	TimerNode() : nFlag(-1), dwLast(0), dwInterval(0), pfn(nullptr) {}

	//���ɶ�ʱ����Ϣ
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

	//��Ӷ�ʱ��
	static void Add(int nFlag)
	{
		aFlag[nFlag] = true; 
	}

	//ɾ����ʱ����ʶ
	static void Del(int nFlag)
	{
		aFlag[nFlag] = false;
	}

	int nFlag;							//��ʱ����ʶ
	unsigned int dwLast;				//�ϴδ�����ʱ��,��λ����
	unsigned int dwInterval;			//�����������λ����
	std::shared_ptr<void> pContext;		//��ʱ���ص�������������
	CT_Timer pfn;						//��ʱ���ص�����ָ��

	static bool aFlag[TIMER_FLAG_MAX];
};

//TimerNode������ָ���װ�ֱࣨ����������ָ���<������������ã��������·�װ�£�
struct TimerPtr
{
	std::shared_ptr<TimerNode> p;
	TimerPtr(std::shared_ptr<TimerNode> _p) : p(_p) {}
};

//��ʱ����Ϣ���������
bool operator <(const TimerPtr &L, const TimerPtr &R);

class CMonitor
{
public:
	CMonitor();

	~CMonitor(void);

	//��ʼ��
	bool Init();

	//����ʼ��
	bool DeInit();

	//��Ӷ�ʱ��,���ض�ʱ���ı�ʶ���ñ�ʶ����ɾ����ʱ��,ʧ�ܷ���-1
	//��ʱ����ʱ�������ܴ��ڻ����86500000����
	int AddTimer(unsigned int dwInterval, std::shared_ptr<void> pContext, TimerNode::CT_Timer pfn);

	//ɾ����ʱ��
	bool DelTimer(int nFlag);

	//���һ���������ļ�������������ļ��������Ѿ����ڣ���᷵��ʧ�ܡ�
	//ע�⣺EPOLLRDHUP|EPOLLET�����Զ����,���sokcet�����Ƿ������ġ�
	//ע�⣺��������EPOLLONESHOT����˱��ӿڲ�֧��ʹ��EPOLLONESHOT��
	//ע�⣺���ṩ��Ӧ��ɾ���ӿڣ����������ļ�����������EPOLLERR|EPOLLHUPʱ���Զ�ɾ�����ļ��������ļ�����
	bool Add(
		int nFd,						//Ҫ��ӵ��ļ�������
		unsigned int nEvent,			//Ҫ�������¼�
		CT_In pfnIn,					//���Խ�������ʱ�Ĵ���ص�����
		std::shared_ptr<void> pContextIn,			//���Խ������ݻص������Ļص�������������		
		CT_Out pfnOut,					//���Է�������ʱ�Ĵ���ص�����
		std::shared_ptr<void> pContextOut,			//���Է�������ʱ�Ĵ���ص�������������
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void> pContextErr			//��������Ĵ���ص�������������
		);

	//��ӡ�ڲ���Ϣ
	void Print();

protected:

	//�߳�
	std::vector<std::thread> m_vecThread;

	//�߳�ֹͣ��ʶ
	volatile bool m_bStop;

	//�¼�
	epoll_event *m_pEvents;

	//Epoll���ļ�������
	int m_nEpollFd;

	//һ��Socket���õĻ�������
	struct Context
	{	
		int nFd;						//��Ӧ��socket
		unsigned int nEvents;			//���ӵ��¼�
		CT_In pfnIn;					//���������¼���֪ͨ�ص�����
		std::shared_ptr<void> pContextIn;			//���������¼���֪ͨ�ص�������������
		CT_Out pfnOut;					//���������֪ͨ�ص�����
		std::shared_ptr<void> pContextOut;			//���������֪ͨ�ص�������������
		CT_Err pfnErr;					//�����¼���֪ͨ�ص��������������¼��󣬸�socket����CMonitor�ڲ���ɾ����
		std::shared_ptr<void> pContextErr;			//�����¼���֪ͨ�ص�������������

		Context(): pfnIn(nullptr), pfnOut(nullptr) ,pfnErr(nullptr)
		{}
	};

	//��ʱ���б����ʱ���
	std::multiset<TimerPtr> m_setTimer;	
	std::mutex m_mtxTimer;

protected:

	//�����̺߳���
	void TH_Work();

	//��ʱ����غ���
	unsigned int GetNextTimer(); //��ȡ��һ�ζ�ʱ����ʱ��
	void OnTimer();				 //��ʱ��������

};

