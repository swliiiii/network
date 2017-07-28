#pragma once
#include <set>
#include "Monitor.h"
#include <arpa/inet.h>
#include <atomic>
#include "Tool/TBuff.h"

#ifndef SOCKET
#define SOCKET int
#endif // !SOCKET


class CTcpClient : public CContextBase
{
public:

	//���캯��
	CTcpClient();

	//����
	virtual ~CTcpClient(void);

	//����״̬����ص�����
	using CT_Connect = void(*)(
		std::shared_ptr<CContextBase> pContext,		//��������
		bool bConnected									//true���� false�Ͽ�
		);

	//���յ����ݵĻص�����
	using CT_Data = int(*) (
		std::shared_ptr<CContextBase> pContext,		//��������
		Tool::TBuff<char> &Data								//���յ�������
		);

	//ע����յ����ݵĻص�����
	void RegCallback(
		CT_Data pfnData,								//���յ����ݵĻص�����ָ��
		std::shared_ptr<CContextBase> pContextData,		//���յ����ݵĻص�������������
		CT_Connect pfnConnect,							//���ӳɹ��Ļص�����ָ��
		std::shared_ptr<CContextBase> pContextConnect	//���ӳɹ��Ļص�������������
	);

	//����Socket���󶨵�ַ
	bool PreSocket(
		const std::string &strLocalIP,				//Ҫʹ�õı���IP
		unsigned short nLocalPort					//Ҫʹ�õı��ض˿�
	);

	//��ʼ���ӷ�����
	bool Start(
		const std::string &strServerIP,				//��������IP
		unsigned short nServerPort,					//�������Ķ˿�
		int nRecvBuf = 0,							//���ջ�������С��0��ʶʹ��Ĭ��ֵ
		int nSendBuf = 0,							//���ͻ�������С��0��ʶʹ��Ĭ��ֵ
		const std::string &strLocalIP = "",			//Ҫʹ�õı���IP
		unsigned short nLocalPort = 0				//Ҫʹ�õı��ض˿�
		);

	//ֹ֪ͨͣ����
	bool QStop();

	//��������
	int Send(const char *pData, int nLen);

	//��ȡ���ض˿�
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//��ȡ����IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//�ͻ����׽���
	int m_socket;

	//ֹͣ��ʶ
	bool m_bStop;

	//�Ƿ����ӵı�ʶ�� 0��ʶδ���ӣ�����0��ʶ������
	std::atomic<int> m_nConnectFlag;

	//������ı�ʶ
	std::string m_strFlag;

	//���յ����ݵ�����ص������ͻ�������
	CT_Data m_pfnData;
	std::shared_ptr<CContextBase> m_pContextData;

	//����״̬������ص������ͻ�������
	CT_Connect m_pfnConnect;
	std::shared_ptr<CContextBase> m_pContextConnect;

	//��������ַ
	sockaddr_in m_addrServer;

	//�����󶨵�ַ
	sockaddr_in m_addrLocal;

	//����IP��ַ�Ͷ˿�
	std::string m_strLocalIP;
	unsigned short m_nLocalPort;

	//���ջ�����
	Tool::TBuff<char> m_buffRecv;

protected:

	//���ӳɹ��Ĵ�����
	virtual void OnConnected();

	//������յ������ݵ�ʾ������
	virtual void OnRecvData(Tool::TBuff<char> &Data);

	//�ɷ��͵Ĵ�����
	virtual void OnSendable();

	//�����˳�ʱ�Ĵ���
	virtual void OnError();

	//���յ����ݵ�֪ͨ�ص�����
	static  void RecvDataCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd				//�����¼����ļ�������
		);

	//���Է������ݵ�֪ͨ�ص�����
	static void SendableCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd				//�����¼����ļ�������
		);

	//���������֪ͨ�ص�����
	static void ErrCB(
		std::shared_ptr<void> pContext,		//��������
		int nfd,					//�����¼����ļ�������
		int nError					//�����
		);


	//�������ݵĴ�����
	void RecvData();

	//��һ��socket����Ϊ��������
	bool SetNonblocking(int nFd);

	//�ر�socket����Ϊ��Чֵ
	void CloseSocket(int &s);
};

