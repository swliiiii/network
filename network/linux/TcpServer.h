#pragma once
#include "Monitor.h"
#include <string>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "Tool/TBuff.h"
#include "Tool/TLog.h"

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1 
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef SOCKET
#define SOCKET int
#endif // !SOCKET

//����һ��Tcp���ӵĻ���
class CTcpClientBase : public CContextBase
{
public:

	//����״̬֪ͨ�ص�����
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//��������
		bool bConnect						//���ӱ�ʶ
		);

	//���յ����ݵĻص������������Ѿ���������ݳ��ȣ�˵�����Ѿ�����������ݣ����ڱ������ڲ�ɾ����
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//��������
		Tool::TBuff<char> &Data,					//���յ������ݻ���
		std::weak_ptr<CContextBase> pTcp	//�����߱����ָ��
		);


	//����
	CTcpClientBase(int s, const std::string &strRemoteIP, unsigned short nRemotePort);

	//����
	virtual ~CTcpClientBase();

	//ע����յ����ݵĻص�����
	void RegCB(CT_Data pfn, std::shared_ptr<CContextBase> pContext, CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect);

	//��ʼ�շ�����
	virtual bool Start(
		int nRecvBuf = 0,							//���ջ�������С��0��ʶʹ��Ĭ��ֵ
		int nSendBuf = 0							//���ͻ�������С��0��ʶʹ��Ĭ��ֵ
		);

	//֪ͨ�ر�����
	virtual bool QStop();

	//���û�������С
	bool SetSocketBuff(
		int nRecvBuf = 0,							//���ջ�������С��0��ʶʹ��Ĭ��ֵ
		int nSendBuf = 0							//���ͻ�������С��0��ʶʹ��Ĭ��ֵ
	);

	//��������
	int Send(const char *pData, int nLen);

	//��ȡԶ��IP�Ͷ˿�
	std::string & GetRemoteIP() { return m_strRemoteIP; }
	unsigned short GetRemotePort() { return m_nRemotePort; }

protected:

	//�׽���
	int m_socket;

	//Զ�̵�IP�Ͷ˿�
	std::string m_strRemoteIP;
	unsigned short m_nRemotePort;

	//ֹͣ��ʶ
	bool m_bStop;

	//���ջ�����
	Tool::TBuff<char> m_buffRecv;

	//���յ���������ص�����
	CT_Data m_pfnRecvData;
	std::shared_ptr<CContextBase> m_pRecvData;

	//����ͨ������ص�����
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pConnect;

protected:

	//���յ����ݵ�ʵ�ʴ�����
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//�ɷ��͵Ĵ�����
	virtual void OnSendable();

	//�����˳�ʱ�Ĵ���
	virtual void OnError();

	//���յ����ݵ�֪ͨ�ص�����
	static void RecvDataCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd						//�����¼����ļ�������
	);

	//���Է������ݵ�֪ͨ�ص�����
	static void SendableCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd						//�����¼����ļ�������
	);

	//���������֪ͨ�ص�����
	static void ErrCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd,					//�����¼����ļ�������
		int nError					//������Ϣ
	);

	//�ر�socket
	void CloseSocket(int &s);

};


#define LogTS LogN(104)

//Tcp��������
template<class T>
class CTcpServer : public CContextBase
{
public:

	//���죬��ֹ�ⲿֱ�Ӵ�������Ӧ�õ���Create�ӿ�
	CTcpServer()
		: m_socket(INVALID_SOCKET)
		, m_pfnClient(nullptr)

	{
		LogTS("CTcpServer");
	}

	//����
	virtual ~CTcpServer()
	{
		LogTS("~CTcpServer");
		CloseSocket(m_socket);
	}


	//�����ͻ��˶���֪ͨ�ص�����������true��ʾ�Զ����������ӣ�����false��ʾ��Ҫ�Զ�����������
	using CT_Client = bool(*)(
		std::shared_ptr<CContextBase> pContext,
		std::shared_ptr<T> pClient,
		const std::string &strRemoteIP,
		int nRemotePort
		);

	//ע�ᴴ���ͻ��˵�֪ͨ�ص�����
	void RegClientCB(CT_Client pfn, std::shared_ptr<CContextBase>  pContext)
	{
		m_pfnClient = pfn;
		m_pContext = pContext;
	}

	//��ȡ���õı��ص�ַ
	std::string LocalIP() { return m_strLocalIP; }
	unsigned short LocalPort() { return m_nLocalPort; }
	
	bool PreSocket(const std::string &strIP, unsigned short nPort)
	{
		//����socket
		m_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (INVALID_SOCKET == m_socket)
		{
			LogTS("[CTcpServer::Start] create socket failed! -- error <%d - %s>",
				errno, strerror(errno));
			return false;
		}

		//�󶨱�����ַ
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(nPort);
		inet_pton(AF_INET, strIP.c_str(), (void*)&addr.sin_addr);
		if (-1 == bind(m_socket, (sockaddr*)&addr, sizeof addr))
		{
			LogTS("[CTcpServer::Start] <%s:%d> bind failed! -- error<%d - %s>",
				strIP.c_str(), nPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}

		//����Ϊ��������
		if (!SetNonblocking(m_socket))
		{
			CloseSocket(m_socket);
			return false;
		}
		m_strLocalIP = strIP;
		m_nLocalPort = nPort;
		return true;
	}
			
	//����������
	bool Start(
		const std::string &strIP = "",						//������IP��ַ
		unsigned short nPort = 0							//�����Ķ˿ں�
	)
	{
		if (nullptr == GetMonitor())
		{
			LogTS("[CTcpServr::Start] failed -- NO Monitor yet!");
			return false;
		}
		if (INVALID_SOCKET == m_socket && !PreSocket(strIP, nPort))
		{
			return false;
		}

		//��������
		if (-1 == listen(m_socket, 20))
		{
			LogTS("[CTcpServer::Start] listen at <%s:%d>  failed! -- error<%d - %s>",
				m_strLocalIP.c_str(), m_nLocalPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}

		//��ӵ�CMonitor
		if (!GetMonitor()->Add(m_socket, EPOLLIN, ClientCB, m_pThis.lock(), nullptr, m_pThis.lock(), ErrCB, m_pThis.lock()))
		{
			LogTS("[CTcpServer::Start] <%s:%d> Add to CMonitor failed! -- error<%d - %s>",
				m_strLocalIP.c_str(), m_nLocalPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}
		LogTS("[CTcpServr::Start]  Listen at <%s:%d>", m_strLocalIP.c_str(), m_nLocalPort);
		return true;
	}

	//ֹͣ������
	bool QStop()
	{
		LogTS("[CTcpServer::QStop] socket<%d>", m_socket);
		if (INVALID_SOCKET != m_socket)
		{
			shutdown(m_socket, SHUT_RDWR);		
		}
	}

	//��ӡ�ڲ���Ϣ
	void Print()
	{
	}

protected:
	
	//�ͻ������ӵĴ���ص�����
	static void ClientCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd						//�����¼����ļ�������
	)
	{
		LogTS("[CTcpServer::ClientCB]");
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}

		while (true)
		{
			sockaddr_in addrPeer;
			socklen_t nAddrLen = sizeof(addrPeer);
			int s = accept(pThis->m_socket, (sockaddr*)&addrPeer, &nAddrLen);
			if (-1 == s)
			{
				if (EAGAIN == errno)
				{
					return;
				}
				LogTS("[CTcpServer::ClientCB] accept fiale -- Errno<%d - %s>", errno, strerror(errno));
				continue;				 
			}
			if (!pThis->SetNonblocking(s))
			{
				LogTS("[CTcpServer::ClientCB] setNonblocking failed, close the socket!");
				pThis->CloseSocket(s);
				continue;;
			}
			char buff[32]{ 0 };
			inet_ntop(AF_INET, (void*)&(addrPeer.sin_addr), buff, 64);	
			auto p = std::make_shared<T>(s, buff, htons(addrPeer.sin_port));
			p->SetMonitor(pThis->GetMonitor());
			p->SetThisPtr(p);
			bool bStart = true;
			if (nullptr != pThis->m_pfnClient)
			{
				bStart = pThis->m_pfnClient(pThis->m_pContext, p, buff, htons(addrPeer.sin_port));
			}	
			if (bStart)
			{
				p->Start();
			}
		}
	}

	//��������Ĵ���ص�����
	static void ErrCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd,					//�����¼����ļ�������
		int nError					//������Ϣ
	)
	{
		LogTS("[CTcpServer::ErrCB]");
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}
		pThis->CloseSocket(pThis->m_socket);
	}
	
	//��һ��socket����Ϊ��������
	bool SetNonblocking(int nFd)
	{
		int nFlags = fcntl(nFd, F_GETFL);
		if (-1 == nFlags)
		{
			//��Ӧ�÷������¼�
			LogTS("[CTcpServer::SetNonblocking] getfl failed, must be care -- errno<%d - %s>",
				errno, strerror(errno));
			return false;
		}
		nFlags |= O_NONBLOCK;
		if (-1 == fcntl(nFd, F_SETFL, nFlags))
		{
			LogTS("[CTcpServer::SetNonblocking] setfl failed, must be care -- errno<%d - %s>",
				errno, strerror(errno));
			return false;
		}
		return true;
	}

	//�ر�socket
	void CloseSocket(int &s)
	{
		if (INVALID_SOCKET != s)
		{
			LogTS("close socket <%d>", s);
			close(s);
			s = INVALID_SOCKET;
		}	
	}
	
protected:

	//�������׽���
	int m_socket;

	//�����ͻ��˶���Ļص������ͻ�������
	CT_Client m_pfnClient;
	std::shared_ptr<CContextBase> m_pContext;

	//����IP�Ͷ˿�
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;
};
