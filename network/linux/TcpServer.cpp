#include "stdafx.h"
#include "TcpServer.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define Log LogN(103)
#define RCV_BUF_SIZE 1000000

/*******************************************************************************
* �������ƣ�	
* ����������	����
* ���������	s				-- ���ӵ��׽���
*				strRemoteIP		-- Զ�˵�IP
*				nRemotePort		-- Զ�˵Ķ˿�
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
CTcpClientBase::CTcpClientBase(int s, const std::string &strRemoteIP, unsigned short nRemotePort)
	: m_socket(s)
	, m_strRemoteIP(strRemoteIP)
	, m_nRemotePort(nRemotePort)
	, m_bStop(false)
	, m_pfnRecvData(nullptr)
	, m_pfnConnect(nullptr)
{
}

//����
CTcpClientBase::~CTcpClientBase()
{
	CloseSocket(m_socket);
}

//ע�����ݽ��ܻص�����
void CTcpClientBase::RegCB(CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect)
{
	m_pfnRecvData = pfnData;
	m_pRecvData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pConnect = pContextConnect;
}

//��ʼ�շ�����
bool CTcpClientBase::Start( int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */ )
{
	Log("CTcpClientBase::Start");
	if (nullptr == GetMonitor())
	{
		Log("[CTcpClientBase::Start] failed -- NO Monitor yet");
		return false;
	}
	m_bStop = false;
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CTcpClientBase::Start] failed��INVALID_SOCKET!");
		return false;
	}
	//����Socket����
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CCTcpClientBase::Start]���ö˿ڽ��ջ����Сʧ�ܣ�Remote<%s:%d> RecvBuff<%d>��", m_strRemoteIP.c_str(), m_nRemotePort, nRecvBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClientBase::Start]���ö˿ڷ��ͻ����Сʧ�ܣ�Remote<%s:%d> SendBuff<%d>��", m_strRemoteIP.c_str(), m_nRemotePort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (!GetMonitor()->Add(m_socket, EPOLLIN | EPOLLOUT, RecvDataCB, m_pThis.lock(), SendableCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CTcpClientBase::Start]Add socket<%d> EPOLLOUT to CMoniteor failed", m_socket);
		CloseSocket(m_socket);
		return false;
	}
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, true);
	}
	return true;
}

//֪ͨ�ر�����
bool CTcpClientBase::QStop()
{
	Log("[CTcpClientBase::QStop] socket<%d>", m_socket);
	if (INVALID_SOCKET != m_socket)
	{
		//�رն�д���Դ���EPOLL�Ĵ��󣬲������ϵ���close,����epoll�п����޷�������
		shutdown(m_socket, SHUT_RDWR);
	}
	return true;
}

/*******************************************************************************
* �������ƣ�
* ����������	����soket�Ķ�д������
* ���������	nRecvBuf		-- ���ջ�������С��0��ʶʹ��Ĭ��ֵ
*				nSendBuf		-- ���ͻ�������С��0��ʶʹ��Ĭ��ֵ
* ���������
* �� �� ֵ��	�ɹ�����true�����򷵻�false
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/06/16	˾����	      ����
*******************************************************************************/
bool CTcpClientBase::SetSocketBuff(int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */)
{
	Log("[CTcpClientBase::SetSocketBuff] nRecvBuf<%d> nSendBuf<%d>", nRecvBuf, nSendBuf);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CTcpClientBase::SetSocketBuff] Soket��δ����"); 
		return false;
	}

	//����Socket����(һ��Ĭ����8192)
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CTcpCCTcpClientBase::SetSocketBuff]���ö˿ڽ��ջ����Сʧ�ܣ�Remote<%s:%d> RecvBuff<%d>��", m_strRemoteIP.c_str(), m_nRemotePort, nRecvBuf);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClientBase::SetSocketBuff]���ö˿ڷ��ͻ����Сʧ�ܣ�Remote<%s:%d> SendBuff<%d>��", m_strRemoteIP.c_str(), m_nRemotePort, nSendBuf);
			return false;
		}
	}
	return true;
}

/*******************************************************************************
* �������ƣ�	
* ����������	��������	
* ���������	pData		-- Ҫ���͵�����ָ��
*				nLen		-- Ҫ���͵����ݳ���
* ���������	
* �� �� ֵ��	ʵ�ʷ��͵����ݳ��ȡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
int CTcpClientBase::Send(const char *pData, int nLen)
{
	//Log("[CTcpClientBase::Send] nLen<%d>", nLen);
	int nSend = send(m_socket, pData, nLen, 0);
	if (-1 != nSend)
	{
		//���ͳɹ�
		//Log("[CTcpClientBase::Send] Send <%d> OK", nSend);
		return nSend;
	}
//	Log("[CTcpClientBase::Send] Send failed: Sent<%d> != nLen<%d>", nSend, nLen);

	//����ʧ��
	switch (errno)
	{
	case  EAGAIN:	//����������,֪ͨ�ⲿ
		break;
	case EINTR:		//���жϴ���ˣ�Ӧ�����·���һ��
		Log("[CTcpClientBase::Send] EINTR occured, send once again");
		return Send(pData, nLen);
	default:
		Log("Send (%d < %d) failed -- errno<%d - %s>", nSend, nLen, errno, strerror(errno));
		break;
	}
	return 0;
}

/*******************************************************************************
* �������ƣ�	
* ����������	���յ����ݵĴ�����
* ���������	Data		-- ���յ�������
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClientBase::OnRecvData(Tool::TBuff<char> &Data)
{
	//Log("[CTcpClientBase::OnRecvData] Recv <%d> bytes", Data.size());
	if (nullptr != m_pfnRecvData)
	{
		Data.erase(0, m_pfnRecvData(m_pRecvData, Data, m_pThis));
	}
	else
	{
		Data.clear();
	}
}

//���Է�������֪ͨ�Ĵ���ص�����
void CTcpClientBase::OnSendable()
{
}

//��������ʱ�Ĵ���
void CTcpClientBase::OnError()
{
	Log("[CTcpClientBase::OnError] socket <%d>", m_socket);
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, false);
	}
	QStop();
}

/*******************************************************************************
* �������ƣ�	
* ����������	�����ݿɽ��յĴ���ص�����
* ���������	pCotnext	-- ��������
*				nfd			-- �׽���
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClientBase::RecvDataCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[CTcpClientBase::RecvDataCB] fd<%d>", nfd);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClientBase::RecvDataCB] the RCV BUF Size<%d> is too big, clear it!", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	const int nBuffSize = 1024;
	while (!pThis->m_bStop)
	{
		auto nPos = pThis->m_buffRecv.size();
		pThis->m_buffRecv.resize(pThis->m_buffRecv.size() + nBuffSize);
		ssize_t nRecv = recv(pThis->m_socket, &(pThis->m_buffRecv[nPos]), nBuffSize, 0);
		//Log("[CTcpClientBase::RecvDataCB]Recv Data Len<%d> errno<%d>", nRecv, errno);
		if (nRecv > 0)
		{
			pThis->m_buffRecv.resize(pThis->m_buffRecv.size() + nRecv - nBuffSize);
			pThis->OnRecvData(pThis->m_buffRecv);
			if (nRecv < nBuffSize)
			{
				//����Tcp���ɴ˾Ϳ����ж������Ѿ������գ�udp���У�
				break;
			}
			continue;
		}
		pThis->m_buffRecv.resize(pThis->m_buffRecv.size() - nBuffSize);
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
			Log("[CTcpClientBase::RecvDataCB]connect maybe closed. errno <%d -- %s>", errno, strerror(errno));
			pThis->QStop();
			break;
		}
	}
}

/*******************************************************************************
* �������ƣ�	
* ����������	���Է������ݵĴ���ص�����
* ���������	pCotnext	-- ��������
*				nfd			-- �׽���
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClientBase::SendableCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[CTcpClientBase::SendableCB] fd<%d>", nfd);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnSendable();
}

/*******************************************************************************
* �������ƣ�	
* ����������	��������Ĵ���ص�����
* ���������	pCotnext	-- ��������
*				nfd			-- �׽���
*				nError		-- ������Ϣ
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClientBase::ErrCB( std::shared_ptr<void> pContext, int nfd, int nError )
{
	Log("[CTcpClientBase::ErrCB] fd<%d> Error<%x>", nfd, nError);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();
	pThis->CloseSocket(pThis->m_socket);	
}

//�ر�socket����Ϊ��Чֵ
void CTcpClientBase::CloseSocket(int &s)
{
	if (INVALID_SOCKET != s)
	{
		Log("close socket <%d>", s);
		close(s);
		s = INVALID_SOCKET;
	}
}
