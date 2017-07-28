#pragma once
#include "Monitor.h"
#include <mutex>
#include <atomic>
#include "../Tool/TBuff.h"

//Tcp�ͻ�����
class CTcpClient :	public CContextBase
{
public:	

	//���캯��
	CTcpClient();
	
	//����
	virtual ~CTcpClient();

	//����״̬����ص�����
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//��������
		bool bConnected								//true���� false�Ͽ�
		);

	//���յ����ݵĻص������������Ѿ���������ݳ��ȣ�˵�����Ѿ�����������ݣ����ڱ������ڲ�ɾ����
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//��������
		 Tool::TBuff<char> &Data							//���յ������ݻ���
		);

	//ע����յ����ݵĻص�����
	void RegCallback(
		CT_Data pfnData,						//���յ����ݵĻص�����ָ��
		std::shared_ptr<CContextBase> pContextData,		//���յ����ݵĻص�������������
		CT_Connect pfnConnect,					//���ӳɹ��Ļص�����ָ��
		std::shared_ptr<CContextBase> pContextConnect	//���ӳɹ��Ļص�������������
	);

	//����Socket���󶨵�ַ
	bool PreSocket(
		const std::string &strLocalIP,			//Ҫʹ�õı���IP
		unsigned short nLocalPort				//Ҫʹ�õı��ض˿�
	);

	//��ʼ����
	bool Start(
		const std::string &strServerIP,			//������IP
		unsigned short nServerPort,				//�������˿�
		int nRecvBuf = 0,						//���ջ�������С��0��ʶʹ��Ĭ��ֵ
		int nSendBuf = 0,						//���ͻ�������С��0��ʶʹ��Ĭ��ֵ
		const std::string &strLocalIP = "",		//����IP
		unsigned short nLocalPort = 0			//���ض˿�
	);

	//ֹ֪ͨͣ����
	bool QStop();

	//Ͷ�ݷ�������
	int Send(const char *pData, int nLen);

	//��ȡ���ض˿�
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//��ȡ����IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//�׽���
	SOCKET m_socket;

	//�׽����Ƿ�shutdown��
	std::atomic<bool> m_bShutdown;

	//������صı�����������
	std::mutex m_mutextContext;				//������
	CMonitor::SendContext *m_pContextSend;	//Ͷ��Send�ɹ���Ӧ����ΪNULL

	//���ӱ�ʶ
	std::string m_strFlag;

	//���յ����ݵ�����ص������ͻ�������
	CT_Data m_pfnData;
	std::shared_ptr<CContextBase> m_pContextData;

	//����״̬������ص������ͻ�������
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pContextConnect;

	//��������ַ
	sockaddr_in m_addrServer;

	//����IP��ַ�Ͷ˿�
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;

	//���ջ�����
	Tool::TBuff<char> m_buffRecv;

	//���ͻ�����
	Tool::TBuff<char> m_buffSend;

protected:
	
	//ʵ��Ͷ�ݷ��ͣ� ��Ҫ���ñ��������������ͱ�����
	int ExeSend(const char *pData, int nLen);

	//�������ӳɹ�
	virtual void OnConnected();

	//������յ�����
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//�ɷ��͵Ĵ�����
	virtual void OnSendable();

	//���������˳�
	virtual void OnError();	

	//���ӷ������ɹ��Ļص�����
	static void ConnectedCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s								//���ӷ��������׽���
		);

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
		std::shared_ptr<void> pContext,						//��������
		SOCKET s,								//����������׽���
		int nErrCode							//������룬ͬGetlastErr()�ķ���ֵ
	);

	//�ر�һ���׽���
	void CloseSocket(SOCKET &s);

	
};

