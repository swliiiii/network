#pragma once
#include <thread>
#include <memory>
#include <vector>
#include <MSWSock.h>
#include <set>
#include <mutex>


//���������Ļ�����
class CMonitor;
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
	static bool WaitDestroy(std::weak_ptr<void> p, int nTimeout = INT_MAX)
	{
		while (!p.expired() && nTimeout-- > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return p.expired();
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

//Ͷ�ݵĲ�������
enum class OPIndex
{
	eNone,			//������
	eAcceptEx,		//���տͻ�������
	eConnectEx,		//���ӷ�����
	eSend,			//��������(Tcp)
	eRecv,			//��������(Tcp��
	eSendTo,		//��������(udp)
	eRecvFrom,		//��������(udp��
};
#define TIMER_FLAG_MAX 5000

//��ʱ����Ϣ
struct TimerNode
{
	//��ʱ���ص���������
	using CT_Timer = void(*)(
		std::shared_ptr<void> pContext,	//��������
		int nTimer				//��ʱ��ID
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

	int nFlag;					//��ʱ����ʶ
	unsigned int dwLast;		//�ϴδ�����ʱ��,��λ����
	unsigned int dwInterval;	//�����������λ����
	std::shared_ptr<void> pContext;		//��ʱ���ص�������������
	CT_Timer pfn;				//��ʱ���ص�����ָ��

	static bool aFlag[TIMER_FLAG_MAX];
};

using TimerPtr = std::shared_ptr<TimerNode>;

//��ʱ����Ϣ���������
bool operator <(const TimerPtr &L, const TimerPtr &R);


//��ɶ˿ڷ�װ��
class CMonitor
{
public:

	//tcp���յ����ݵĴ���ص���������
	using CT_RecvData = void(*)(
		std::shared_ptr<void>  pContext,					//��������
		SOCKET s,								//���յ����ݵ��׽���
		char *pData,							//���յ�������ָ��
		int nLen								//���յ��������ֽ�
		);

	//udp���յ����ݵĴ���ص���������
	using CT_RecvDataFrom = bool(*)(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//���յ����ݵ��׽���
		char *pData,							//���յ�������ָ��
		int nLen,								//���յ��������ֽ�								
		const std::string strFromIP,			//����ԴIP
		unsigned short nFromPort							//����Դ�˿�
		);

	//��������Ĵ���ص���������
	using CT_Err = void(*)(
		std::shared_ptr<void>  pContext,					//��������
		SOCKET s,								//����������׽���
		int nErrCode							//������룬ͬGetlastErr()�ķ���ֵ
		);

	//tcp�ͻ������ӵĴ���ص���������
	using CT_Client = void(*)(
		std::shared_ptr<void>  pContext,					//��������
		SOCKET s,								//�ͻ�������socket����������ʹ����ϸ�socket��Ӧ���رո�socket
		const std::string &strRemoteIP,			//�ͻ���IP
		int nRetmoteIP							//�ͻ��˶˿�
		);

	//tcp���ӷ������ɹ��Ĵ���ص���������
	using CT_Connected = void(*)(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s								//���ӷ��������׽���
		);

	//tcp�������ݽ���Ĵ���ص�����
	struct SendContext;
	using CT_Sent = void(*)(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//�������ݵ�socket
		int nSent,								//ʵ�ʷ��͵��ֽ���
		SendContext *pContextSend				//���͵���Ϣ�ṹ�壬�ⲿ��ñ�������ֵ�󣬼�ȡ���˿���Ȩ���������ʹ�ã���ʹ��deleteɾ��������ͨ��PostSend��SendContext*���ӿڼ����������ݡ�
		);

	//udp�������ݽ���Ĵ�����
	struct SendToContext;
	using CT_SentTo = void(*)(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//�������ݵ�socket
		int nSent,								//ʵ�ʷ��͵��ֽ���
		SendToContext *pContextSend				//���͵���Ϣ�ṹ�壬�ⲿ��ñ�������ֵ�󣬼�ȡ���˿���Ȩ���������ʹ�ã���ʹ��deleteɾ��������ͨ��PostSendTo��SendContext*���ӿڼ����������ݡ�
		);


	//������������
	struct Context
	{
		typedef bool (CMonitor::*ptrExe)(Context* pContext, DWORD dwDataLen);

		OVERLAPPED ovlp;					//Ͷ�ݵĲ���
		OPIndex op;							//Ͷ������
		ptrExe pfnExe;						//���ִ�к���
		SOCKET s;							//socket
		CT_Err pfnErr;						//socket����ʱ�Ĵ���ص�����
		std::shared_ptr<void>  pContextErr;			//socket����ʱ�Ĵ���ص�������������

		//����
		Context() : s(INVALID_SOCKET), op(OPIndex::eNone), pfnExe(nullptr), ovlp({ 0 }), pfnErr(nullptr)
		{}

		//����
		virtual ~Context()
		{}
	};

	//Tcp���տͻ������ӵĻ�������
	struct AcceptContext : public Context
	{
		WSABUF wsaBuff;										//�洢���ݵĻ�����
		SOCKET sClient;										//�ͻ������ӵ�socket
		LPFN_ACCEPTEX pfnAcceptEx;							//AcceptEx����ָ��
		LPFN_GETACCEPTEXSOCKADDRS pfnGetAcceptExSockAddrs;	//��ȡ�ͻ���IP��ַ�ĺ���ָ��
		CT_Client pfnClient;								//����ͻ�������ʱ�Ļص�����
		std::shared_ptr<void>  pContextClient;							//����ͻ������ӵĻص�������������

		//����
		AcceptContext()
			: sClient(INVALID_SOCKET)
			, pfnAcceptEx(nullptr)
			, pfnGetAcceptExSockAddrs(nullptr)
			, pfnClient(nullptr)
		{
			wsaBuff.len = (sizeof(sockaddr_in) + 16) * 2;	//������ʱֱ�ӻ��֪ͨ�����ȴ����յ�����
			wsaBuff.buf = new char[wsaBuff.len];
		}

		//����
		~AcceptContext()
		{
			if (INVALID_SOCKET != sClient)
			{
				closesocket(sClient);
				sClient = INVALID_SOCKET;
			}
			delete[]wsaBuff.buf;
		}
	};

	//Tcp���ӷ������Ļ�������
	struct ConnectExContext : public Context
	{
		CT_Connected pfnConnected;
		std::shared_ptr<void> pContextConnected;
		sockaddr_in addr;
		LPFN_CONNECTEX pfnConnectEx;	//ConnectEx����ָ��

		//����
		ConnectExContext() : pfnConnected(NULL), pfnConnectEx(nullptr) 
		{
			ZeroMemory(&addr, sizeof(addr));
		}

		//����
		~ConnectExContext(){}		
	};

	//Tcp�������ݵĻ�������
	struct RecvContext : public Context
	{
		WSABUF wsaBuff;				//�洢���ݵĻ�����
		CT_RecvData pfnRecv;		//���յ����ݵĴ���ص�����
		std::shared_ptr<void>  pContextRecv;	//���յ����ݵĴ���ص�������������

		//����
		RecvContext() : pfnRecv(nullptr)
		{
			wsaBuff.len = 8196;
			wsaBuff.buf = new char[wsaBuff.len];
		}

		//����
		~RecvContext()
		{
			delete[]wsaBuff.buf;
		}
	};

	//Udp�������ݵĻ�������
	struct  RecvFromContext : public Context
	{
		WSABUF wsaBuff;				//�洢���ݵĻ�����
		CT_RecvDataFrom pfnRecv;	//���յ����ݵĴ���ص�����
		std::shared_ptr<void>  pContextRecv;	//���յ����ݵĴ���ص�������������
		sockaddr_in addrFrom;		//����Դ��ַ
		int nAddrFromLen;			//����Դ��ַ����

		//����
		RecvFromContext() : pfnRecv(nullptr)
		{
			ZeroMemory(&addrFrom, sizeof(addrFrom));
			nAddrFromLen = sizeof(addrFrom);
			wsaBuff.len = 8196;
			wsaBuff.buf = new char[wsaBuff.len];
		}

		//����
		~RecvFromContext()
		{
			delete[] wsaBuff.buf;
		}
	};

	//Tcp�������ݵĻ�������
	struct SendContext : public Context
	{
		WSABUF wsaBuff;				//���ͻ�����
		DWORD  dwBuffLen;			//���ͻ������Ĵ�С
		CT_Sent pfnSent;			//���ͽ���Ĵ���ص�����
		std::shared_ptr<void> pContextSent;	//���ͽ���Ĵ���ص�������������

		//����
		SendContext() : pfnSent(nullptr)
		{
			wsaBuff.len = 0;
			wsaBuff.buf = nullptr;
			dwBuffLen = 0;
		}

		//����
		~SendContext()
		{
			if (nullptr != wsaBuff.buf)
			{
				delete[] wsaBuff.buf;
			}
		}
	};

	//UDP���ͻ�������
	struct SendToContext : public Context
	{
		WSABUF wsaBuff;				//���ͻ�����
		DWORD  dwBuffLen;			//���ͻ������Ĵ�С
		CT_SentTo pfnSent;			//���ͽ���Ĵ���ص�����
		std::shared_ptr<void> pContextSent;	//���ͽ���Ĵ���ص�������������
		sockaddr_in addSendTo;		//����Ŀ�ĵ�ַ

		//����
		SendToContext() : pfnSent(nullptr)
		{
			wsaBuff.len = 0;
			wsaBuff.buf = nullptr;
			dwBuffLen = 0;
			ZeroMemory(&addSendTo, sizeof(addSendTo));
		}

		//����
		~SendToContext()
		{
			if (nullptr != wsaBuff.buf)
			{
				delete[] wsaBuff.buf;
			}
		}
	};

public:

	//����
	CMonitor();

	//����
	~CMonitor();

	//��ʼ��
	bool Init();

	//����ʼ����Ӧ���ڹر����е�Ͷ�ݹ�������socket֮�󣬲ŵ��ñ��ӿ�
	bool DeInit();

	//��Ӷ�ʱ�������ض�ʱ���ı�ʶ���ñ�ʶ����ɾ����ʱ������ʶС��0��ʶʧ��
	//��ʱ����ʱ�������ܴ��ڻ����86500000����
	int AddTimer(
		unsigned int dwInterval,	//��ʱ���������λ����
		std::shared_ptr<void> pContext,		//��ʱ���ص�������������
		TimerNode::CT_Timer pfn		//��ʱ���ص�����ָ��
	);

	//ɾ����ʱ��
	bool DelTimer(
		int nFlag					//ɾ����ʱ��	
	);

	//��socket�󶨵���ɶ˿�
	bool Attach(SOCKET s);

	//Ͷ��Accept����
	bool PostAcceptEx(
		SOCKET s,						//���ܿͻ������ӵ��׽���
		CT_Client pfnClient,			//�пͻ������ӵĴ���ص�������������
		std::shared_ptr<void> pContextClient,		//�пͻ������ӵĴ���ص�������������		
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void>  pContextErr,		//��������Ĵ���ص�������������
		int nCount = 1					//Ͷ�ݵĴ���
	);

	//Ͷ��Connect����
	bool PostConnectEx(
		SOCKET s,						//���ӷ��������׽���
		sockaddr_in addrServer,			//������IP��ַ
		CT_Connected pfnConnected,		//���ӳɹ��Ļص�����
		std::shared_ptr<void> pContextConnected,	//���ӳɹ��Ļص�������������
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void>  pContextErr			//��������Ĵ���ص�������������
	);

	//Ͷ�ݽ������ݲ���(tcp)
	bool PostRecv(
		SOCKET s,						//�������ݵ��׽���
		CT_RecvData pfnRecv,			//���յ����ݵĴ���ص�������������
		std::shared_ptr<void> pContextRecv,		//���յ����ݵĴ���ص�������������
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void>  pContextErr			//��������Ĵ���ص�������������
	);

	//Ͷ�ݽ������ݲ�����udp��
	bool PostRecvFrom(
		SOCKET s,						//�������ݵ��׽���
		CT_RecvDataFrom pfnRecv,		//���յ����ݵĴ���ص�������������
		std::shared_ptr<void> pContextRecv,		//���յ����ݵĴ���ص�������������
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void>  pContextErr			//��������Ĵ���ص�������������
	);

	//Ͷ�ݷ������ݲ���(tcp)
	SendContext* PostSend(
		SOCKET s,						//�������ݵ��׽���
		CT_Sent pfnSent,				//���ͽ������ص�����
		std::shared_ptr<void> pContextSent,		//���ͽ������ص�������������
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void>  pContextErr			//��������Ĵ���ص�������������
	);
	
	//Ͷ�ݷ������ݲ���(udp)
	SendToContext* PostSendTo(
		SOCKET s,						//�������ݵ��׽���
		CT_SentTo pfnSent,				//���ͽ������ص�����
		std::shared_ptr<void> pContextSent,		//���ͽ������ص�������������
		CT_Err pfnErr,					//��������Ĵ���ص�����
		std::shared_ptr<void>  pContextErr			//��������Ĵ���ص�������������
	);

	//ʹ�û�������Ͷ�ݷ��Ͳ���
	bool PostSend(SendContext *pContext);

	//ʹ�û�������Ͷ�ݷ��Ͳ���
	bool PostSendTo(SendToContext *pContext);

protected:

	//��ɶ˿ھ��
	HANDLE m_hCP;

	//�����߳̾��
	std::vector<std::thread> m_vecThread;

	//�����߳��˳���ʶ
	bool m_bStop;

	//��ʱ���б����ʱ���
	std::multiset<TimerPtr> m_setTimer;	
	std::mutex m_mtxTimer;

protected:
	
	//�����߳�
	void ThreadFunc();

	//Ͷ�ݺ���
	bool PostAcceptEx(AcceptContext *pContext);
	bool PostConnectEx(ConnectExContext *pContext);
	bool PostRecv(RecvContext *pContext);
	bool PostRecvFrom(RecvFromContext *pContext);

	//�����¼��ĺ���
	bool ExeAcceptEx(Context *pContextBase, DWORD nDataLen);
	bool ExeConnectEx(Context *pContextBase, DWORD nDataLen);
	bool ExeRecv(Context *pContextBase, DWORD nDataLen);
	bool ExeRecvFrom(Context *pContextBase, DWORD nDataLen);
	bool ExeSend(Context *pContextBase, DWORD nDataLen);
	bool ExeSendTo(Context *pContextBase, DWORD nDataLen);

	//��������Ĵ���ɾ����������
	void OnErrDelete(Context *pContext);

	//��ʱ����غ���
	unsigned int GetNextTimer(); //��ȡ��һ�ζ�ʱ����ʱ��
	void OnTimer();				//��ʱ��������

	//��ȡAcceptEx������GetAcceptExSockAddr����ָ��
	bool GetAcceptExFunc(SOCKET s, LPFN_ACCEPTEX &pfnAcceptEx, LPFN_GETACCEPTEXSOCKADDRS &pfnGetAcceptExSockAddrs);

	//��ȡConnectEx����ָ��
	bool GetConnectExFunc(SOCKET s, LPFN_CONNECTEX &pfnConnectEx);

	//��ȡCPU����
	int GetProcessorCount();

};

