#pragma once
#include <string>
#include <thread>
#include "Monitor.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#ifndef SOCKET
#define SOCKET int
#endif // !SOCKET

//UDP��װ��
class CUdpM : public CContextBase
{
public:

	//���յ����ݵĻص���������
	using CT_DataRecv = void(*) (
		std::shared_ptr<void> pContext,	//��������
		const char *pData,							//���յ�������ָ��
		int nLen,									//���յ��������ֽ���
		const std::string &strFromIP,				//����ԴIP
		short nFromPort								//����Դ�˿�
		);

	//���캯��
	CUdpM();

	//����
	virtual ~CUdpM();

	//ע����յ����ݵĻص�����
	void RegDataRecv(CT_DataRecv pfn, std::shared_ptr<void> pContex);

	//����Socket���󶨵�ַ
	bool PreSocket(
		const std::string &strLocalIP,			//Ҫʹ�õı���IP
		unsigned short nLocalPort,				//Ҫʹ�õı��ض˿�
		bool bReuse = true						//�Ƿ�����˿ڸ���
	);

	//��ʼ����
	bool Start(
		int nRecvBuf = 0,
		int nSendBuf = 0,
		unsigned short nPort = 0,				//���ض˿�
		const std::string &strLocalIP = "",		//����IP
		const std::string &strMultiIP = "",		//�鲥IP
		bool bReuse = true						//�Ƿ�����˿ڸ���
	);

	//ֹ֪ͨͣ����
	bool QStop();

	//��������
	virtual bool Send(const char *pData, int nLen, const std::string &strToIP = "", unsigned short nToPort = 0);

	//��ȡ���ض˿�
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//��ȡ����IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//������յ������ݵ�ʾ������
	virtual void OnRecvData(char *pData, int nLen, const std::string &strFromIP, unsigned short nFromPort);

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
		int nError					//������Ϣ
	);

	//�ر�socket
	void CloseSocket(int &s);

	//����Ϊ��������
	bool SetNonblocking(int nFd);

protected:

	//�׽���
	int m_socket;

	//�󶨵ı���IP
	std::string m_strLocalIP;

	//�󶨵ı��ض˿�
	unsigned short m_nLocalPort;

	//�鲥��ַ
	std::string m_strMultiIP;

	//ֹͣ��ʶ
	bool m_bStop;

	//�������ݵĻ�����
	char *m_pBuff;

	//���յ����ݵ�����ص������ͻ�������
	CT_DataRecv m_pfnDataRecv;
	std::shared_ptr<void> m_pRecvDataContext;

	//�ϴη��͵�Ŀ�ĵ�ַ
	sockaddr_in m_addrSendTo;

};
