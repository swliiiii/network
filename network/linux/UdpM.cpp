#include "stdafx.h"
#include "UdpM.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "Tool/TLog.h"

#define Log LogN(105)

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1 
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#define BUFF_SIZE 2048

//����
CUdpM::CUdpM(void)
	: m_socket(INVALID_SOCKET)
	, m_bStop(false)
	, m_nLocalPort(0)
	, m_pfnDataRecv(nullptr)
	, m_addrSendTo({ 0 })

{
	m_pBuff = new char[BUFF_SIZE];
}

//����
CUdpM::~CUdpM(void)
{
	CloseSocket(m_socket);
	delete[]m_pBuff;
	m_pBuff = nullptr;
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

	//����socket
	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//���ö˿ڸ���
	if (bReuse)
	{
		int bSockReuse = 1;
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &bSockReuse, sizeof(bSockReuse)))
		{
			Log("[CUdpM:PreSocket] setsockopt reuse failed --<%d - %s>", errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}
	}

	//����Ϊ��������
	if (!SetNonblocking(m_socket))
	{
		Log("[CUdpM:PreSocket] SetNonblocking failed --<%d - %s>", errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//����TTL
	unsigned int dwTTL = 32;
	if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &dwTTL, sizeof(dwTTL)))
	{
		Log("[CUdpM:PreSocket] setsockopt TTL failed --<%d - %s>", errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//�socket
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(nLocalPort);
	inet_pton(AF_INET,  strLocalIP.c_str(), (void*)&addr.sin_addr);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr *)&addr, sizeof(addr)))
	{
		Log("[CUdpM:PreSocket] bind failed -- LocalIP<%s:%d> error<%d - %s>",
			strLocalIP.c_str(), nLocalPort, errno, strerror(errno));
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
			Log("[CUdpM::PreSocket]��ȡ�׽��ֵĶ˿�ʧ�� Err<%d>", nRet);
			CloseSocket(m_socket);
			return false;
		}
		m_nLocalPort = htons(addr.sin_port);
		Log("[CUdpM::PreSocket]�Զ������׽��ֶ˿�<%u>", m_nLocalPort);
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
	const std::string &strLocalIP /* = "" */, const std::string &strMultiIP /* = "" */, bool bReuse /* = true */ )
{
	Log("CUdpM::Start nRecvBuf<%d> nSendBuf<%d>", nRecvBuf, nSendBuf);
	if (nullptr == GetMonitor())
	{
		Log("[CUdpM::Start]��δ����������������");
		return false;
	}

	if (INVALID_SOCKET == m_socket && !PreSocket(strMultiIP.empty() ? strLocalIP : strMultiIP, nPort, bReuse))
	{
		return false;
	}
	
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

	//�����鲥��
	if (!strMultiIP.empty())
	{
		struct ip_mreq mreq = { 0 };
		inet_pton(AF_INET, strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)))
		{
			Log("[CUdpM:Start] join multibroad failed -- MultiIP<%s> LocalIP<%s> Port<%d> error<%d - %s>",
				strMultiIP.c_str(), strLocalIP.c_str(), nPort, errno, strerror(errno));
			CloseSocket(m_socket);
			return false;
		}
	}
	m_strMultiIP = strMultiIP;
	
		//������д
	if (!GetMonitor()->Add(m_socket, EPOLLOUT | EPOLLIN, RecvDataCB, m_pThis.lock(), SendableCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CUdpM:Start] Add socket<%d> to CMonitor failed -- MultiIP<%s> LocalIP<%s> Port<%d> error<%d - %s>",
			m_socket, strMultiIP.c_str(), strLocalIP.c_str(), nPort, errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}
	return true;
}

//ֹ֪ͨͣ����
bool CUdpM::QStop()
{
	Log("[CUdpM::QStop] fd<%d>", m_socket);
	if (!m_strMultiIP.empty())
	{
		//�����鲥��
		struct ip_mreq mreq = { 0 };
		inet_pton(AF_INET, m_strMultiIP.c_str(), (void*)&mreq.imr_multiaddr.s_addr);
		inet_pton(AF_INET, m_strLocalIP.c_str(), (void*)&mreq.imr_interface.s_addr);
		if (SOCKET_ERROR == setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)))
		{
			Log("drop menbership failed -- multiIP<%s> Local<%s:%d> error<%d - %s>",
				m_strMultiIP.c_str(), m_strLocalIP.c_str(), m_nLocalPort, errno, strerror(errno));
		}
		m_strMultiIP = "";
	}
	m_bStop = true;
	if (INVALID_SOCKET != m_socket)
	{
		//�رն�д���Դ���EPOLL�Ĵ���
		shutdown(m_socket, SHUT_RDWR);
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
bool CUdpM::Send(const char *pData, int nLen, const std::string &strToIP, unsigned short nToPort)
{
	if (!strToIP.empty())
	{
		m_addrSendTo.sin_family = AF_INET;
		inet_pton(AF_INET, strToIP.c_str(), (void*)&m_addrSendTo.sin_addr);
		m_addrSendTo.sin_port = htons(nToPort);
		if (0 == nLen)
		{
			return true;
		}
	}

	int nSend = sendto(m_socket, pData, nLen, 0, (sockaddr*)&m_addrSendTo, sizeof(m_addrSendTo));
	if (nSend == nLen)
	{
		//���ͳɹ�
		//Log("[CUdpM::Send] Send <%d> OK", nSend);
		return true;
	}
	//Log("[CUdpM::Send] Send failed: Sent<%d> != nLen<%d>", nSend, nLen);

	//����ʧ��
	switch (errno)
	{
	case  EAGAIN:	//����������
		break;
	case EINTR:		//���жϴ���ˣ�Ӧ�����·���һ��
		Log("[CTcpClient::Send] EINTR occured, send once again");
		return Send(pData, nLen, strToIP, nToPort);
	default:
		Log("Send (%d < %d) failed -- errno<%d - %s>", nSend, nLen, errno, strerror(errno));
		break;
	}
	return false;
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
	//Log("[CUdpM::OnRecvData] Recv <%d> bytes From<%s:%d> ", nLen, strFromIP.c_str(), nFromPort);
	if (nullptr != m_pfnDataRecv)
	{
		m_pfnDataRecv(m_pRecvDataContext, pData, nLen, strFromIP, nFromPort);
	}
}

//�ɷ��͵Ĵ�����
void CUdpM::OnSendable()
{
	//Log("[CUdpM::OnSendable]");
}

//�����˳�ʱ�Ĵ���
void CUdpM::OnError()
{
	Log("[CUdpM::OnError]");
	QStop();
}

/*******************************************************************************
* �������ƣ�	
* ����������	���Խ������ݵ�֪ͨ�ص�����
* ���������	pContext	-- ��������
*				nfd			-- sokcet
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/04/12	˾����	      ����
*******************************************************************************/
void CUdpM::RecvDataCB(std::shared_ptr<void> pContext, int nfd)
{
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}

	//��ȡ�����ϵ�����
	sockaddr_in addrFrom;
	socklen_t nAddrLen = sizeof(addrFrom);
	while (!pThis->m_bStop)
	{
		int nRecv = recvfrom(pThis->m_socket,
			pThis->m_pBuff,
			BUFF_SIZE,
			0,
			(sockaddr *)&addrFrom,
			&nAddrLen);
		if (-1 == nRecv)
		{
			if (EAGAIN == errno)//�������Ѿ�������
			{
				break;
			}
			else if (EINTR == errno)//���жϴ���ˣ�Ӧ��������һ��
			{
				continue;
			}
		}
		if (nRecv <= 0)
		{
			//���ӿ��ܹر��ˣ����ǲ������ȴ�epoll��֪ͨʱ����
			Log("[CUdpM::RecvDataCB] errno <%d -- %s>", errno, strerror(errno));
			break;
		}
		char ip[16] = { 0 };
		inet_ntop(AF_INET, (void*)&(addrFrom.sin_addr), ip, 64);
		pThis->OnRecvData(pThis->m_pBuff, nRecv, ip, htons(addrFrom.sin_port));
	}
}

//���Է������ݵ�֪ͨ�ص�����
void CUdpM::SendableCB(std::shared_ptr<void> pContext, int nfd)
{
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnSendable();

}
//���������֪ͨ�ص�����
void CUdpM::ErrCB( std::shared_ptr<void> pContext, int nFd, int nErrCode )
{
	Log("[CUdpM::ErrCB] fd<%d> Err0x<%x>", nFd, nErrCode);
	CUdpM *pThis = (CUdpM*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();	
	pThis->CloseSocket(pThis->m_socket);		
}

//�ر�socket
void CUdpM::CloseSocket(int &s)
{
	if (INVALID_SOCKET != s)
	{
		close(s);
		s = INVALID_SOCKET;
	}
}

//��һ��socket����Ϊ��������
bool CUdpM::SetNonblocking(int nFd)
{
	int nFlags = fcntl(nFd, F_GETFL);
	if (-1 == nFlags)
	{
		//��Ӧ�÷������¼�
		Log("[CTcpClient::SetNonblocking] getfl failed, must be care -- errno<%d - %s>",
			errno, strerror(errno));
		return false;
	}
	nFlags |= O_NONBLOCK;
	if (-1 == fcntl(nFd, F_SETFL, nFlags))
	{
		Log("[CTcpClient::SetNonblocking] setfl failed, must be care -- errno<%d - %s>",
			errno, strerror(errno));
		return false;
	}
	return true;
}


