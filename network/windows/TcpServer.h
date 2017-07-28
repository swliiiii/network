#pragma once
#include "Monitor.h"
#include <thread>
#include <sstream>
#include <mutex>
#include <WS2tcpip.h>
#include <atomic>
#include "../Tool/TBuff.h"
#include "../Tool/TLog.h"

//����һ��Tcp���ӵĻ���
class CTcpClientBase : public CContextBase
{
public:

	//����״̬֪ͨ�ص�����
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//��������
		bool bConnect								//���ӱ�ʶ
		);

	//���յ����ݵĻص������������Ѿ���������ݳ��ȣ�˵�����Ѿ�����������ݣ����ڱ������ڲ�ɾ����
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//��������
		Tool::TBuff<char> &Data,					//���յ������ݻ���
		std::weak_ptr<CContextBase> pTcp			//�����߱����ָ��
		);


	//����
	CTcpClientBase(
		SOCKET s,									//�������ӵ��׽���
		const std::string &strRemoteIP,				//Զ��IP 
		unsigned short nRemotePort					//Զ�˶˿�
	);

	//����
	virtual ~CTcpClientBase();

	//ע����յ����ݵĻص�����
	void RegCB(
		CT_Data pfnData,
		std::shared_ptr<CContextBase> pContextData,
		CT_Connect pfnConnect, 
		std::shared_ptr<CContextBase> pContextConnect
	);

	//��ʼ�շ�����
	virtual bool Start(
		int nRecvBuf = 0,							//���ջ�������С��0��ʶʹ��Ĭ��ֵ
		int nSendBuf = 0							//���ͻ�������С��0��ʶʹ��Ĭ��ֵ
	);

	//ֹ֪ͨͣ����
	virtual bool QStop();

	//���û�������С
	bool SetSocketBuff(
		int nRecvBuf = 0,							//���ջ�������С��0��ʶʹ��Ĭ��ֵ
		int nSendBuf = 0							//���ͻ�������С��0��ʶʹ��Ĭ��ֵ
	);

	//Ͷ�ݷ�������
	int Send(
		const char *pData,			//�����͵�����ָ��
		int nLen					//�����͵����ݳ���
	);

	//��ȡԶ��IP�Ͷ˿�
	std::string & GetRemoteIP() { return m_strRemoteIP; }
	unsigned short GetRemotePort() { return m_nRemotePort; }

protected:

	//�׽���
	SOCKET m_socket;

	//socket�Ƿ�shutdown
	std::atomic<bool> m_bShutdown;

	//Զ�̵�IP�Ͷ˿�
	std::string m_strRemoteIP;
	unsigned short m_nRemotePort;

	//������ر��������ʱ���
	std::mutex m_mutextContext;				//������
	CMonitor::SendContext *m_pContextSend;	//Ͷ�ݵĻ�������

	//���ջ�����
	Tool::TBuff<char> m_buffRecv;

	//���յ���������ص�����
	CT_Data m_pfnRecvData;
	std::shared_ptr<CContextBase> m_pRecvData;

	//����ͨ������ص�����
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pConnect;

	//���ͻ�����
	Tool::TBuff<char> m_buffSend;

protected:

	//ʵ��Ͷ�ݷ��ͣ� ��Ҫ���ñ��������������ͱ�����
	int ExeSend(const char *pData, int nLen);

	//������յ�������
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//�����ͳɹ�
	virtual void OnSendable();

	//��������Ĵ���
	virtual void OnError();

	//���ͽ���ص�����
	static void SentCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//�������ݵ�socket
		int nSent,								//ʵ�ʷ��͵��ֽ���
		CMonitor::SendContext *pContextSend		//���͵���Ϣ�ṹ�壬�ⲿͨ����ʱ����ñ����󣬸���������ֵȻ�����Send�ӿڷ���
	);

	//���յ����ݵĴ���ص�����
	static void RecvDataCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//���յ����ݵ��׽���
		char *pData,							//���յ�������ָ��
		int nLen								//���յ��������ֽ�
	);

	//��������Ĵ���ص�����
	static void ErrCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//����������׽���
		int nErrCode							//������룬ͬGetlastErr()�ķ���ֵ
	);

	//�ر�һ��socket
	void CloseSocket(SOCKET &s);
};

#define LogTS LogN(104)

//Tcp��������
template<class T>
class CTcpServer : public CContextBase
{
public:

	//����
	CTcpServer()
		: m_sock(INVALID_SOCKET)
		, m_pfnClient(nullptr)

	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	//����
	virtual ~CTcpServer()
	{
		LogTS("~CTcpServer");
		CloseSocket(m_sock);
		WSACleanup();
	}

	//�����ͻ��˶���֪ͨ�ص�����������true��ʾ�Զ����������ӣ�����false��ʾ��Ҫ�Զ�����������
	using CT_Client = bool(*)(
		std::shared_ptr<CContextBase> pContext,
		std::shared_ptr<T> pClient,
		const std::string &strRemoteIP,
		int nRemotePort
		);

	//ע�ᴴ���ͻ��˵�֪ͨ�ص�����
	void RegClientCB(CT_Client pfn, std::shared_ptr<CContextBase> pContext)
	{
		m_pfnClient = pfn;
		m_pContext = pContext;
	}

	//��ȡ���õı��ص�ַ
	std::string LocalIP() { return m_strLocalIP; }
	unsigned short LocalPort() { return m_nLocalPort; }

	//���԰󶨵�ַ
	bool PreSocket(const std::string &strIP, unsigned short nPort)
	{
		//��������ʼ����socket
		m_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == m_sock)
		{
			LogTS("[%s]����socketʧ��!", __FUNCTION__);
			return false;
		}
		sockaddr_in addr{ 0 };
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, strIP.c_str(), (void*)&addr.sin_addr);
		addr.sin_port = htons(nPort);
		if (SOCKET_ERROR == bind(m_sock, (sockaddr*)&addr, sizeof(addr)))
		{
			LogTS("[%s]�󶨵�ַʧ��<%s:%d> -- <%d>", __FUNCTION__, strIP.c_str(), nPort, GetLastError());
			QStop();
			return false;
		}
		m_strLocalIP = strIP;
		m_nLocalPort = nPort;
		return true;
	}

	//��ʼ����
	bool Start(
		const std::string &strIP = "",			//������IP��ַ
		unsigned short nPort = 0				//�����Ķ˿ں�
	)
	{
		if (nullptr == GetMonitor())
		{
			LogTS("[%s]��û������������������", __FUNCTION__);
			return false;
		}

		if (INVALID_SOCKET == m_sock && !PreSocket(strIP, nPort))
		{
			return false;
		}
	
		if (SOCKET_ERROR == listen(m_sock, SOMAXCONN))
		{
			LogTS("[%s]��������ʧ��<%s:%d>", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
			QStop();
			return false;
		}
		if (!GetMonitor()->Attach(m_sock))
		{
			LogTS("[%s]��ӵ���ɶ˿�ʧ��<%s:%d>", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
			QStop();
			return false;
		}

		//������socket����ɶ˿ڰ�
		if (!GetMonitor()->PostAcceptEx(m_sock, ClientCB, m_pThis.lock(), ErrCB, m_pThis.lock(), 5))
		{
			LogTS("[%s]Ͷ��AcceptExʧ��<%s:%d>", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
			QStop();
			return false;
		}
		LogTS("[%s]��ʼ�� <%s:%d> ������", __FUNCTION__, m_strLocalIP.c_str(), m_nLocalPort);
		return true;
	}

	//ֹͣ�����������еĿͻ���
	bool QStop()
	{
		if (INVALID_SOCKET != m_sock)
		{
			//listen��socket�ƺ��ǲ��ܱ�shutdown��ֻ��Closesocket��
			//������������close��socket�󣬸�sokcet�������ã�������ɶ˿�
			//�ϼ�¼�Ļص���Ϣ���ҵĿ����ԣ���Ȼһ���ڳ����˳�ʱ���ر�lisentsocket�������������
			//���ǵ�������lisent�ķ�����ʱ������Ҫע���������⣬���Խ�����������ע�ᵽ������ɶ˿ڡ�
			int nRet = shutdown(m_sock, SD_RECEIVE);//�˴�ִ��ʧ��
			CloseSocket(m_sock);
		}
		return true;
	}

protected:


	//�ͻ������ӵĴ���ص�����
	static void ClientCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//�ͻ�������socket����������ʹ����ϸ�socket��Ӧ���رո�socket
		const std::string &strRemoteIP,			//�ͻ���IP
		int nRetmotePort						//�ͻ��˶˿�
	)
	{
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}
		setsockopt(s,
			SOL_SOCKET,
			SO_UPDATE_ACCEPT_CONTEXT,
			(char*)&pThis->m_sock,
			sizeof(pThis->m_sock)
			);
		auto p = std::make_shared<T>(s, strRemoteIP, nRetmotePort);
		p->SetMonitor(pThis->GetMonitor());
		p->SetThisPtr(p);
		bool bStart = true;
		if (nullptr != pThis->m_pfnClient)
		{
			bStart = pThis->m_pfnClient(pThis->m_pContext, p, strRemoteIP, nRetmotePort);
		}
		if (bStart)
		{
			p->Start();
		}
	}

	//��������Ĵ���ص�����
	static void ErrCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//����������׽���
		int nErrCode							//������룬ͬGetlastErr()�ķ���ֵ
	)
	{
		CTcpServer<T> *pThis = (CTcpServer<T>*)pContext.get();
		if (pThis == nullptr)
		{
			return;
		}
		if (INVALID_SOCKET != pThis->m_sock)
		{
			shutdown(pThis->m_sock, SD_BOTH);
		}
	}

	//�ر�һ���׽���
	void CloseSocket(SOCKET &s)
	{
		if (INVALID_SOCKET != s)
		{
			closesocket(s);
			s = INVALID_SOCKET;
		}
	}

protected:

	//�������׽���
	SOCKET m_sock;

	//�����ͻ��˶���Ļص������ͻ�������
	CT_Client m_pfnClient;
	std::shared_ptr<CContextBase> m_pContext;

	//����IP�Ͷ˿�
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;
};
