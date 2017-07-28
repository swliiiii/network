#pragma once
#include <thread>
#include <memory>
#include <vector>
#include <MSWSock.h>
#include <set>
#include <mutex>


//环境变量的混入类
class CMonitor;
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
	static bool WaitDestroy(std::weak_ptr<void> p, int nTimeout = INT_MAX)
	{
		while (!p.expired() && nTimeout-- > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return p.expired();
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

//投递的操作类型
enum class OPIndex
{
	eNone,			//无意义
	eAcceptEx,		//接收客户端连接
	eConnectEx,		//连接服务器
	eSend,			//发送数据(Tcp)
	eRecv,			//接收数据(Tcp）
	eSendTo,		//发送数据(udp)
	eRecvFrom,		//接收数据(udp）
};
#define TIMER_FLAG_MAX 5000

//定时器信息
struct TimerNode
{
	//定时器回调函数定义
	using CT_Timer = void(*)(
		std::shared_ptr<void> pContext,	//环境变量
		int nTimer				//定时器ID
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

	int nFlag;					//定时器标识
	unsigned int dwLast;		//上次触发的时刻,单位毫秒
	unsigned int dwInterval;	//触发间隔，单位毫秒
	std::shared_ptr<void> pContext;		//定时器回调函数环境变量
	CT_Timer pfn;				//定时器回调函数指针

	static bool aFlag[TIMER_FLAG_MAX];
};

using TimerPtr = std::shared_ptr<TimerNode>;

//定时器信息的排序规则
bool operator <(const TimerPtr &L, const TimerPtr &R);


//完成端口封装类
class CMonitor
{
public:

	//tcp接收到数据的处理回调函数定义
	using CT_RecvData = void(*)(
		std::shared_ptr<void>  pContext,					//环境变量
		SOCKET s,								//接收到数据的套接字
		char *pData,							//接收到的数据指针
		int nLen								//接收到的数据字节
		);

	//udp接收到数据的处理回调函数定义
	using CT_RecvDataFrom = bool(*)(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//接收到数据的套接字
		char *pData,							//接收到的数据指针
		int nLen,								//接收到的数据字节								
		const std::string strFromIP,			//数据源IP
		unsigned short nFromPort							//数据源端口
		);

	//发生错误的处理回调函数定义
	using CT_Err = void(*)(
		std::shared_ptr<void>  pContext,					//环境变量
		SOCKET s,								//发生错误的套接字
		int nErrCode							//错误代码，同GetlastErr()的返回值
		);

	//tcp客户端连接的处理回调函数定义
	using CT_Client = void(*)(
		std::shared_ptr<void>  pContext,					//环境变量
		SOCKET s,								//客户端连接socket，调用者在使用完毕该socket后，应当关闭该socket
		const std::string &strRemoteIP,			//客户端IP
		int nRetmoteIP							//客户端端口
		);

	//tcp连接服务器成功的处理回调函数定义
	using CT_Connected = void(*)(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s								//连接服务器的套接字
		);

	//tcp发送数据结果的处理回调函数
	struct SendContext;
	using CT_Sent = void(*)(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//发送数据的socket
		int nSent,								//实际发送的字节数
		SendContext *pContextSend				//发送的信息结构体，外部获得本上下文值后，即取得了控制权，如果不再使用，请使用delete删除，或者通过PostSend（SendContext*）接口继续发送数据。
		);

	//udp发送数据结果的处理函数
	struct SendToContext;
	using CT_SentTo = void(*)(
		std::shared_ptr<void> pContext,					//环境变量
		SOCKET s,								//发送数据的socket
		int nSent,								//实际发送的字节数
		SendToContext *pContextSend				//发送的信息结构体，外部获得本上下文值后，即取得了控制权，如果不再使用，请使用delete删除，或者通过PostSendTo（SendContext*）接口继续发送数据。
		);


	//环境变量基类
	struct Context
	{
		typedef bool (CMonitor::*ptrExe)(Context* pContext, DWORD dwDataLen);

		OVERLAPPED ovlp;					//投递的参数
		OPIndex op;							//投递类型
		ptrExe pfnExe;						//结果执行函数
		SOCKET s;							//socket
		CT_Err pfnErr;						//socket报错时的处理回调函数
		std::shared_ptr<void>  pContextErr;			//socket报错时的处理回调函数环境变量

		//构造
		Context() : s(INVALID_SOCKET), op(OPIndex::eNone), pfnExe(nullptr), ovlp({ 0 }), pfnErr(nullptr)
		{}

		//析构
		virtual ~Context()
		{}
	};

	//Tcp接收客户端连接的环境变量
	struct AcceptContext : public Context
	{
		WSABUF wsaBuff;										//存储数据的缓冲区
		SOCKET sClient;										//客户端连接的socket
		LPFN_ACCEPTEX pfnAcceptEx;							//AcceptEx函数指针
		LPFN_GETACCEPTEXSOCKADDRS pfnGetAcceptExSockAddrs;	//获取客户端IP地址的函数指针
		CT_Client pfnClient;								//处理客户端连接时的回调函数
		std::shared_ptr<void>  pContextClient;							//处理客户端连接的回调函数环境变量

		//构造
		AcceptContext()
			: sClient(INVALID_SOCKET)
			, pfnAcceptEx(nullptr)
			, pfnGetAcceptExSockAddrs(nullptr)
			, pfnClient(nullptr)
		{
			wsaBuff.len = (sizeof(sockaddr_in) + 16) * 2;	//有连接时直接获得通知，不等待接收到数据
			wsaBuff.buf = new char[wsaBuff.len];
		}

		//析构
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

	//Tcp连接服务器的环境变量
	struct ConnectExContext : public Context
	{
		CT_Connected pfnConnected;
		std::shared_ptr<void> pContextConnected;
		sockaddr_in addr;
		LPFN_CONNECTEX pfnConnectEx;	//ConnectEx函数指针

		//构造
		ConnectExContext() : pfnConnected(NULL), pfnConnectEx(nullptr) 
		{
			ZeroMemory(&addr, sizeof(addr));
		}

		//析构
		~ConnectExContext(){}		
	};

	//Tcp接收数据的环境变量
	struct RecvContext : public Context
	{
		WSABUF wsaBuff;				//存储数据的缓冲区
		CT_RecvData pfnRecv;		//接收到数据的处理回调函数
		std::shared_ptr<void>  pContextRecv;	//接收到数据的处理回调函数环境变量

		//构造
		RecvContext() : pfnRecv(nullptr)
		{
			wsaBuff.len = 8196;
			wsaBuff.buf = new char[wsaBuff.len];
		}

		//析构
		~RecvContext()
		{
			delete[]wsaBuff.buf;
		}
	};

	//Udp接收数据的环境变量
	struct  RecvFromContext : public Context
	{
		WSABUF wsaBuff;				//存储数据的缓冲区
		CT_RecvDataFrom pfnRecv;	//接收到数据的处理回调函数
		std::shared_ptr<void>  pContextRecv;	//接收到数据的处理回调函数环境变量
		sockaddr_in addrFrom;		//数据源地址
		int nAddrFromLen;			//数据源地址长度

		//构造
		RecvFromContext() : pfnRecv(nullptr)
		{
			ZeroMemory(&addrFrom, sizeof(addrFrom));
			nAddrFromLen = sizeof(addrFrom);
			wsaBuff.len = 8196;
			wsaBuff.buf = new char[wsaBuff.len];
		}

		//析构
		~RecvFromContext()
		{
			delete[] wsaBuff.buf;
		}
	};

	//Tcp发送数据的环境变量
	struct SendContext : public Context
	{
		WSABUF wsaBuff;				//发送缓冲区
		DWORD  dwBuffLen;			//发送缓冲区的大小
		CT_Sent pfnSent;			//发送结果的处理回调函数
		std::shared_ptr<void> pContextSent;	//发送结果的处理回调函数环境变量

		//构造
		SendContext() : pfnSent(nullptr)
		{
			wsaBuff.len = 0;
			wsaBuff.buf = nullptr;
			dwBuffLen = 0;
		}

		//析构
		~SendContext()
		{
			if (nullptr != wsaBuff.buf)
			{
				delete[] wsaBuff.buf;
			}
		}
	};

	//UDP发送环境变量
	struct SendToContext : public Context
	{
		WSABUF wsaBuff;				//发送缓冲区
		DWORD  dwBuffLen;			//发送缓冲区的大小
		CT_SentTo pfnSent;			//发送结果的处理回调函数
		std::shared_ptr<void> pContextSent;	//发送结果的处理回调函数环境变量
		sockaddr_in addSendTo;		//发送目的地址

		//构造
		SendToContext() : pfnSent(nullptr)
		{
			wsaBuff.len = 0;
			wsaBuff.buf = nullptr;
			dwBuffLen = 0;
			ZeroMemory(&addSendTo, sizeof(addSendTo));
		}

		//析构
		~SendToContext()
		{
			if (nullptr != wsaBuff.buf)
			{
				delete[] wsaBuff.buf;
			}
		}
	};

public:

	//构造
	CMonitor();

	//析构
	~CMonitor();

	//初始化
	bool Init();

	//反初始化，应该在关闭所有的投递过操作的socket之后，才调用本接口
	bool DeInit();

	//添加定时器，返回定时器的标识，该标识用于删除定时器，标识小于0标识失败
	//定时器的时间间隔不能大于或等于86500000毫秒
	int AddTimer(
		unsigned int dwInterval,	//定时器间隔，单位毫秒
		std::shared_ptr<void> pContext,		//定时器回调函数环境变量
		TimerNode::CT_Timer pfn		//定时器回调函数指针
	);

	//删除定时器
	bool DelTimer(
		int nFlag					//删除定时器	
	);

	//将socket绑定到完成端口
	bool Attach(SOCKET s);

	//投递Accept操作
	bool PostAcceptEx(
		SOCKET s,						//接受客户端连接的套接字
		CT_Client pfnClient,			//有客户端连接的处理回调函数环境变量
		std::shared_ptr<void> pContextClient,		//有客户端连接的处理回调函数环境变量		
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void>  pContextErr,		//发生错误的处理回调函数环境变量
		int nCount = 1					//投递的次数
	);

	//投递Connect操作
	bool PostConnectEx(
		SOCKET s,						//连接服务器的套接字
		sockaddr_in addrServer,			//服务器IP地址
		CT_Connected pfnConnected,		//连接成功的回调函数
		std::shared_ptr<void> pContextConnected,	//连接成功的回调函数环境变量
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void>  pContextErr			//发生错误的处理回调函数环境变量
	);

	//投递接收数据操作(tcp)
	bool PostRecv(
		SOCKET s,						//接收数据的套接字
		CT_RecvData pfnRecv,			//接收到数据的处理回调函数环境变量
		std::shared_ptr<void> pContextRecv,		//接收到数据的处理回调函数环境变量
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void>  pContextErr			//发生错误的处理回调函数环境变量
	);

	//投递接收数据操作（udp）
	bool PostRecvFrom(
		SOCKET s,						//接收数据的套接字
		CT_RecvDataFrom pfnRecv,		//接收到数据的处理回调函数环境变量
		std::shared_ptr<void> pContextRecv,		//接收到数据的处理回调函数环境变量
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void>  pContextErr			//发生错误的处理回调函数环境变量
	);

	//投递发送数据操作(tcp)
	SendContext* PostSend(
		SOCKET s,						//发送数据的套接字
		CT_Sent pfnSent,				//发送结果处理回调函数
		std::shared_ptr<void> pContextSent,		//发送结果处理回调函数环境变量
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void>  pContextErr			//发生错误的处理回调函数环境变量
	);
	
	//投递发送数据操作(udp)
	SendToContext* PostSendTo(
		SOCKET s,						//发送数据的套接字
		CT_SentTo pfnSent,				//发送结果处理回调函数
		std::shared_ptr<void> pContextSent,		//发送结果处理回调函数环境变量
		CT_Err pfnErr,					//发生错误的处理回调函数
		std::shared_ptr<void>  pContextErr			//发生错误的处理回调函数环境变量
	);

	//使用环境变量投递发送操作
	bool PostSend(SendContext *pContext);

	//使用环境变量投递发送操作
	bool PostSendTo(SendToContext *pContext);

protected:

	//完成端口句柄
	HANDLE m_hCP;

	//工作线程句柄
	std::vector<std::thread> m_vecThread;

	//工作线程退出标识
	bool m_bStop;

	//定时器列表及访问保护
	std::multiset<TimerPtr> m_setTimer;	
	std::mutex m_mtxTimer;

protected:
	
	//工作线程
	void ThreadFunc();

	//投递函数
	bool PostAcceptEx(AcceptContext *pContext);
	bool PostConnectEx(ConnectExContext *pContext);
	bool PostRecv(RecvContext *pContext);
	bool PostRecvFrom(RecvFromContext *pContext);

	//处理事件的函数
	bool ExeAcceptEx(Context *pContextBase, DWORD nDataLen);
	bool ExeConnectEx(Context *pContextBase, DWORD nDataLen);
	bool ExeRecv(Context *pContextBase, DWORD nDataLen);
	bool ExeRecvFrom(Context *pContextBase, DWORD nDataLen);
	bool ExeSend(Context *pContextBase, DWORD nDataLen);
	bool ExeSendTo(Context *pContextBase, DWORD nDataLen);

	//发生错误的处理，删除环境变量
	void OnErrDelete(Context *pContext);

	//定时器相关函数
	unsigned int GetNextTimer(); //获取下一次定时器的时间
	void OnTimer();				//定时器处理函数

	//获取AcceptEx函数和GetAcceptExSockAddr函数指针
	bool GetAcceptExFunc(SOCKET s, LPFN_ACCEPTEX &pfnAcceptEx, LPFN_GETACCEPTEXSOCKADDRS &pfnGetAcceptExSockAddrs);

	//获取ConnectEx函数指针
	bool GetConnectExFunc(SOCKET s, LPFN_CONNECTEX &pfnConnectEx);

	//获取CPU核数
	int GetProcessorCount();

};

