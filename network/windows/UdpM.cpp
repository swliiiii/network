#include "stdafx.h"
#include "UdpM.h"
#include <WS2tcpip.h>
#include "../Tool/TLog.h"

#define Log LogN(105)

//����
CUdpM::CUdpM()
	: m_pContextSend(nullptr)
	, m_socket(INVALID_SOCKET)
	, m_bStop(false)
	, m_nLocalPort(0)
	, m_pfnDataRecv(nullptr)
	, m_collBuff(1024)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//����
CUdpM::~CUdpM()
{
	CloseSocket(m_socket);
	WSACleanup();
}

/*******************************************************************************
* �������ƣ�
* ����������	ע�����ݽ��ջص�����
* ���������	pfn				-- �ص�����ָ��
*				pContext		-- �ص�������������
* ���������
* �� �� ֵ��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
void CUdpM::RegDataRecv(CT_DataRecv pfn, std::shared_ptr<void> pContex)
{
	m_pfnDataRecv = pfn;
	m_pRecvDataContext = pContex;
}

/*******************************************************************************
* �������ƣ�
* ����������	����Socket���󶨵�ַ
* ���������	strLocalIP		-- ����IP
*				nLocalPort		-- ���ض˿�
*				bReuse			-- �Ƿ�����˿ڸ���
* ���������
* �� �� ֵ��	�ɹ�����true�����򷵻�false��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
bool CUdpM::PreSocket( const std::string &strLocalIP, unsigned short nLocalPort, bool bReuse /* = true */ )
{
	if (strLocalIP.empty())
	{
		Log("[CUdpM::PreSocket] ����IP������Ϊ�գ�");
		return false;
	}

	//��������ʼ����socket
	m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CUdpM::PreSocket]����socketʧ��!");
		return false;
	}

	if (bReuse)
	{
		//���ö˿ڸ���
		int bSockReuse = 1;
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&bSockReuse, sizeof(bSockReuse)))
		{
			Log("[CUdpM::PreSocket]�˿ڸ���ʧ��! -- %d", WSAGetLastError());
			CloseSocket(m_socket);
			return false;
		}
	}

	//�󶨵�ַ
	sockaddr_in addr{ 0 };
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, strLocalIP.c_str(), (void*)&addr.sin_addr);
	addr.sin_port = htons(nLocalPort);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr*)&addr, sizeof(addr)))
	{
		Log("[CUdpM::PreSocket]�󶨵�ַʧ��<%s:%d> -- <%d>", strLocalIP.c_str(), nLocalPort, GetLastError());
		CloseSocket(m_socket);
		return false;
	}

	//������Զ�����Ķ˿ڣ����ȡ�Զ�����Ķ˿ں�
	if (0 == nLocalPort)
	{
		sockaddr_in addr;
		socklen_t nAddrLen = sizeof(addr);
		int nRet = getsockname(m_socket, (sockaddr*)&addr, &nAddrLen);
		if (0 != nRet)
		{
			Log("��ȡ�׽��ֵĶ˿�ʧ�� Err<%d>", nRet);
			CloseSocket(m_socket);
			return false;
		}
		m_nLocalPort = htons(addr.sin_port);
		Log("�Զ������׽��ֶ˿�<%u>", m_nLocalPort);
	}
	else
	{
		m_nLocalPort = nLocalPort;
	}
	m_strLocalIP = strLocalIP;

	return true;
}

/*******************************************************************************
* �������ƣ�
* ����������	��ʼ����
* ���������	bExpandBuff		-- �Ƿ�ʹ�ýϴ�Ľ��ջ�����
*				nPort			-- ʹ�õı��ض˿�
*				strLocalIP		-- ʹ�õı���IP
*				strMultiIP		-- ������鲥���鲥��ַ
*				bReuse			-- �Ƿ�����˿ڸ���
* ���������
* �� �� ֵ��	�ɹ�����true�����򷵻�false��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
bool CUdpM::Start( int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */, unsigned short nPort /* = 0 */, 
	const std::string &strLocalIP /* = "" */, const std::string &strMultiIP /* = "" */, bool bReuse /* = false */ )
{
	if (nullptr == GetMonitor())
	{
		Log("[CUdpM::Start]��δ����������������");
		return false;
	}
	if (INVALID_SOCKET == m_socket && !PreSocket(strLocalIP, nPort, bReuse))
	{
		return false;
	}
	m_strMultiIP = strMultiIP;
	
	//����Socket����(һ��Ĭ����8192)
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CUdpM::Start]���ö˿ڽ��ջ����Сʧ��<%s:%d> RecvBuff<%d>��", m_strLocalIP.c_str(), m_nLocalPort, nRecvBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CUdpM::Start]���ö˿ڷ��ͻ����Сʧ��<%s:%d> SendBuff<%d>��", m_strLocalIP.c_str(), m_nLocalPort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}

	if (!strMultiIP.empty())
	{
		//�����鲥��
		struct ip_mreq mreq = {0};
		inet_pton(AF_INET, strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,	(char*)&mreq, sizeof(mreq)))
		{
			Log("[CUdpM::Start]�����鲥��<%s>ʧ��<%s:%d>��", strMultiIP.c_str(), strLocalIP.c_str(), nPort);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (!GetMonitor()->Attach(m_socket))
	{
		Log("[CUdpM::Start] ��ӵ���ɶ˿�ʧ�ܣ�socket<%d>", m_socket);
		Clear();
		CloseSocket(m_socket);
		return false;
	}

	m_mutextContext.lock();
	m_pContextSend = GetMonitor()->PostSendTo(m_socket, SentToCB, m_pThis.lock(), ErrCB, m_pThis.lock());
	if (NULL == m_pContextSend)
	{
		m_mutextContext.unlock();
		Log("[CUdpM::Start]��ȡ���ͻ�������ʧ��: socket<%d>", m_socket);
		Clear();
		CloseSocket(m_socket);
		return false;
	}
	m_mutextContext.unlock();

	if (!GetMonitor()->PostRecvFrom(m_socket, RecvFromCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CUdpM::Start] ʧ�ܣ�socket<%d>", m_socket);
		Clear();
		CloseSocket(m_socket);
		return false;
	}

	Log("[CUdpM::Start]socket<%d>��ʼ��<%s:%d(�鲥<%s>)> ��������", m_socket, m_strLocalIP.c_str(), nPort, m_strMultiIP.c_str());
	return true;
}

//ֹ֪ͨͣ����
bool CUdpM::QStop()
{
	if (INVALID_SOCKET != m_socket)
	{
		//int nRet = shutdown(m_socket, SD_BOTH);
		//Log("%d", nRet);
		//CloseSocket(m_socket);
		char a = 'a';
		Send(&a, 1, m_strLocalIP, m_nLocalPort);
	}
	return true;
}

/*******************************************************************************
* �������ƣ�
* ����������	��������
* ���������	pData		-- Ҫ���͵�����ָ��
*				nLen		-- Ҫ���͵����ݳ���
*				strToIP		-- Ŀ��IP
*				nToPort		-- Ŀ�Ķ˿�
* ���������
* �� �� ֵ��	�ɹ�����true�����򷵻�false��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
bool CUdpM::Send(const char *pData, int nLen, const std::string &strToIP /* = "" */, unsigned short nToPort /* = 0 */)
{
	std::lock_guard<std::mutex> lock(m_mutextContext);
	if (nullptr != m_pContextSend)
	{
		ExeSend(pData, nLen, strToIP, nToPort);
		return true;
	}
	m_collBuff.push_back();
	auto &node = m_collBuff.back();
	node.buff.clear();
	node.buff.append(pData, nLen);
	node.strDstIP = strToIP;
	node.nDstPort = nToPort;
	return true;
}

void CUdpM::ExeSend(const char *pData, int nLen, const std::string &strToIP /* = "" */, unsigned short nToPort /* = 0 */)
{
	if (!strToIP.empty())
	{
		m_pContextSend->addSendTo.sin_family = AF_INET;
		inet_pton(AF_INET, strToIP.c_str(), (void*)&m_pContextSend->addSendTo.sin_addr);
		m_pContextSend->addSendTo.sin_port = htons(nToPort);
		if (0 == nLen)
		{
			return;
		}
	}
	if ((int)m_pContextSend->dwBuffLen < nLen)
	{
		if (nullptr != m_pContextSend->wsaBuff.buf)
		{
			delete[]m_pContextSend->wsaBuff.buf;
		}
		m_pContextSend->dwBuffLen = nLen * 2;
		m_pContextSend->wsaBuff.buf = new char[m_pContextSend->dwBuffLen];
	}
	memcpy(m_pContextSend->wsaBuff.buf, pData, nLen);
	m_pContextSend->wsaBuff.len = nLen;

	bool bRet = GetMonitor()->PostSendTo(m_pContextSend);
	if (!bRet)
	{
		Log("[%s] Ͷ��Sendʧ�� : socket<%d>", __FUNCTION__, m_socket);
	}
	else
	{
		m_pContextSend = nullptr;
	}
	return;
}

/*******************************************************************************
* �������ƣ�
* ����������	������յ�������
* ���������	pData		-- ���յ�������ָ��
*				nLen		-- ���յ������ݳ���
*				strFromIP	-- ���������ĸ�IP
*				nFromPort	-- ���������ĸ��˿�
* ���������
* �� �� ֵ��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
void CUdpM::OnRecvData(char *pData, int nLen, const std::string &strFromIP, unsigned short nFromPort)
{
	if (nullptr != m_pfnDataRecv)
	{
		m_pfnDataRecv(m_pRecvDataContext, pData, nLen, strFromIP, nFromPort);
	}
}

//������ϵĴ�����
void CUdpM::OnSent()
{
}

//��������Ĵ�����
void CUdpM::OnErr()
{

}

/*******************************************************************************
* �������ƣ�	
* ����������	��������Ļص�����
* ���������	pContext		-- ��������
*				s				-- socket
*				nErrCode		-- �������
* ���������	
* �� �� ֵ��	
* ����˵����	������������ʱ��socket�ܿ����Ѿ�ɥʧ�������շ�������
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/11	˾����	      ����
*******************************************************************************/
void CUdpM::ErrCB(std::shared_ptr<void> pContext, SOCKET s, int nErrCode)
{
	Log("[CUdpM::ErrCB] socket<%d> ErrCode<%d>", s, nErrCode);
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		Log("[[CUdpM::ErrCB]] thisָ��Ϊ��!");
		return;
	}
	pThis->OnErr();
	pThis->Clear();
}

/*******************************************************************************
* �������ƣ�	
* ����������	���յ����ݵ�֪ͨ�ص�����
* ���������	pContext	-- ��������
*				s			-- socket
*				pData		-- ���յ�������ָ��
*				nLen		-- ���յ������ݳ���
*				strFromIP	-- ���������ĸ�IP
*				nFromPort	-- ���������ĸ��˿�
* ���������	
* �� �� ֵ��	����Ͷ�ݽ��ղ�������true�����ټ������շ���false��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
bool CUdpM::RecvFromCB(std::shared_ptr<void> pContext, SOCKET s, char *pData, int nLen, const std::string strFromIP, unsigned short nFromPort)
{
	//Log("socket<%d>���յ�%d�ֽ�����", s, nLen);
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		Log("[CUdpM::RecvFromCB] this ָ��Ϊ�գ�һ���Ǵ����д���");
		return false;
	}
	if (strFromIP == pThis->m_strLocalIP && nFromPort == pThis->m_nLocalPort)
	{
		//��������һ��
		Log("[CUdpM::RecvFromCB]sokcet<%d> Port<%d> �յ�ֹͣ����", pThis->m_socket, pThis->m_nLocalPort);
		pThis->Clear();
		return false;
	}
	pThis->OnRecvData(pData, nLen, strFromIP, nFromPort);
	return true;
}

/*******************************************************************************
* �������ƣ�	
* ����������	���ͽ���ص�����
* ���������	pContext		-- ��������
*				s				-- socket
*				nSent			-- �ɹ����͵������ֽ���
*				pContextSend	-- ��������ʹ�õĻ�������
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
void CUdpM::SentToCB(std::shared_ptr<void> pContext, SOCKET s, int nSent, CMonitor::SendToContext *pContextSend)
{
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pContextSend->wsaBuff.len != nSent)
	{
		Log("[CUdpM::SentToCB]���ͽ����ϣ������<%d>�ֽ� ��= ʵ�ʷ���<%d>�ֽ�", pContextSend->wsaBuff.len, nSent);
	}

	bool bSent = true;
	pThis->m_mutextContext.lock();
	
	if (pThis->m_bStop.load() && pThis->m_collBuff.empty())
	{
		delete pContextSend;
		bSent = false;
	}
	else
	{
		pThis->m_pContextSend = pContextSend;
		if (!pThis->m_collBuff.empty())
		{
			auto &node = pThis->m_collBuff.front();
			pThis->ExeSend(&node.buff[0], (int)node.buff.size(), node.strDstIP, node.nDstPort);
			pThis->m_collBuff.pop_front();
			bSent = false;
		}
	}
	pThis->m_mutextContext.unlock();
	if (bSent)
	{
		pThis->OnSent();
	}
}

//�˳��鲥�顢ɾ�����ͻ�������
void CUdpM::Clear()
{
	Log("[CUdpM::Clear]");
	if (!m_strMultiIP.empty())
	{
		//�˳��鲥��
		struct ip_mreq mreq = { 0 };
		inet_pton(AF_INET, m_strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, m_strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq)))
		{
			Log("�˳��鲥��<%s>ʧ��<%s:%d>��", m_strMultiIP.c_str(), m_strLocalIP.c_str(), m_nLocalPort);
		}
		m_strMultiIP = "";
	}
	std::lock_guard<std::mutex> lock(m_mutextContext);
	m_bStop.store(true);
	if (nullptr != m_pContextSend)
	{
		delete m_pContextSend;
		m_pContextSend = nullptr;
	}
}

//�ر�socket
void CUdpM::CloseSocket(SOCKET &s)
{
	if (INVALID_SOCKET != s)
	{
		closesocket(s);
		s = INVALID_SOCKET;
	}
}
