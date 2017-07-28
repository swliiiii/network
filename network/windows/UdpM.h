#pragma once
#include "Monitor.h"
#include <mutex>
#include <atomic>
#include <deque>
#include "../Tool/TRingBuff.h"
#include "../Tool/TBuff.h"

//UDPͨѶ��
class CUdpM : public CContextBase
{
public:

	//���յ����ݵĻص�����
	using CT_DataRecv = void(*) (
		std::shared_ptr<void> pContext,	//��������
		const char *pData,								//���յ�������ָ��
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

	/*
	ע�⣺����IP��ַ����������Ϊ�գ����Ǳ����һ�����ȱ�ݣ�����ʹ����IOCP������Ҫֹͣ����ʱ����û���ҵ�
	�ȽϺ�����Ч�ķ�ʽ֪ͨIOCP��disconnectEx��shutdown��UDP��������ģ�closesocket���������socket������
	���õ���IOCP�е�socket��Ϣ�����ǵ����⣬���Զ������ã����������Ϊ��ʹ�ñ�socket�򱾵�ַ�������ݣ�
	�����յ�����ַ�����Լ�������ʱ����Ϊ�û�Ҫֹͣ���������ټ���Ͷ�ݲ����������ж�����Դ�Ƿ����Ա�socket����
	��ַ����Ҫ���������ñ���IP��
	*/
	
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
		unsigned short nPort = 0,
		const std::string &strLocalIP = "", 
		const std::string &strMultiIP = "",
		bool bReuse = false
	);

	//ֹͣ����
	bool QStop();

	//��������
	bool Send(const char *pData, int nLen, const std::string &strToIP = "", unsigned short nToPort = 0);

	//��ȡ���ض˿�
	unsigned short GetLocalPort() { return m_nLocalPort; }

	//��ȡ����IP
	std::string GetLocalIP() { return m_strLocalIP; }

protected:

	//�׽���
	SOCKET m_socket;

	//�Ƿ�Ҫ�˳���
	std::atomic<bool> m_bStop;

	//���ղ����Ƿ�ֹͣ��
	std::atomic<bool> m_bStopRecv;

	//�󶨵ı���IP
	std::string m_strLocalIP;

	//�󶨵ı��ض˿�
	unsigned short m_nLocalPort;

	//�鲥��ַ
	std::string m_strMultiIP;

	//������صı��������ʱ���
	std::mutex m_mutextContext;					//������
	CMonitor::SendToContext *m_pContextSend;	//Ͷ��Send�ɹ���Ӧ����ΪNULL

	//���յ����ݵ�����ص������ͻ�������
	CT_DataRecv m_pfnDataRecv;
	std::shared_ptr<void> m_pRecvDataContext;

	//�������ݽڵ�
	struct BuffNode
	{
		std::string strDstIP;
		unsigned short nDstPort;
		Tool::TBuff<char> buff;

		BuffNode() : nDstPort(0){}

		void swap(BuffNode &node)
		{
			buff.swap(node.buff);
			strDstIP.swap(node.strDstIP);
			std::swap(nDstPort, node.nDstPort);
		}
	};

	Tool::TRingBuff<BuffNode> m_collBuff;

protected:

	//��������, ���ñ��ӿ�һ��Ҫ�ⲿ�Լ��������ͻ�����
	void ExeSend(const char *pData, int nLen, const std::string &strToIP = "", unsigned short nToPort = 0);

	//������յ�������
	virtual void OnRecvData(char *pData, int nLen, const std::string &strFromIP, unsigned short nFromPort);

	//�����ͳɹ�
	virtual void OnSent();

	//�������
	virtual void OnErr();

	//�������Ļص�����
	static void ErrCB(
		std::shared_ptr<void>  pContext,					//��������
		SOCKET s,								//����������׽���
		int nErrCode							//������룬ͬGetlastErr()�ķ���ֵ
		);

	//���յ����ݵĻص�����
	static bool RecvFromCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//���յ����ݵ��׽���
		char *pData,							//���յ�������ָ��
		int nLen,								//���յ��������ֽ�								
		const std::string strFromIP,			//����ԴIP
		unsigned short nFromPort							//����Դ�˿�
		);

	//�������ݽ���Ļص�����
	static void SentToCB(
		std::shared_ptr<void> pContext,					//��������
		SOCKET s,								//�������ݵ�socket
		int nSent,								//ʵ�ʷ��͵��ֽ���
		CMonitor::SendToContext *pContextSend	//���͵���Ϣ�ṹ�壬�ⲿ��ñ�������ֵ�󣬼�ȡ���˿���Ȩ���������ʹ�ã���ʹ��deleteɾ��������ͨ��PostSendTo��SendContext*���ӿڼ�����������,��������Ȩ��
		);

	//�˳�ǰ����״̬����Դ
	void Clear();

	//�ر�socket
	void CloseSocket(SOCKET &s);

};


