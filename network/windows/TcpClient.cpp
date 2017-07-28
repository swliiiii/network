#include "stdafx.h"
#include "TcpClient.h"
#include <sstream>
#include <WS2tcpip.h>
#include "../Tool/TLog.h"

#define Log LogN(102)

#define RCV_BUF_SIZE 1000000

//����
CTcpClient::CTcpClient()
	: m_socket(INVALID_SOCKET)
	, m_pContextSend(nullptr)
	, m_pfnData(nullptr)
	, m_pfnConnect(nullptr)
	, m_addrServer({0})
	, m_bShutdown(false)
	, m_nLocalPort(0)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//����
CTcpClient::~CTcpClient()
{
	CloseSocket(m_socket);
	WSACleanup();
}

/*******************************************************************************
* �������ƣ�	
* ����������	ע��ص�����
* ���������	pfnData			-- ������յ����ݵĻص�����
*				pContextData	-- ������յ����ݵĻ�������
*				pfnConnect		-- ��������״̬�ı�Ļص�����
*				pContextConnect	-- ��������״̬�ı�Ļ�������
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::RegCallback( CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, CT_Connect pfnConnect,
	std::shared_ptr<CContextBase> pContextConnect )
{
	m_pfnData = pfnData;
	m_pContextData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pContextConnect = pContextConnect;
}

//����Socket���󶨵�ַ
bool CTcpClient::PreSocket(const std::string &strLocalIP, unsigned short nLocalPort)
{
	m_socket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	Log("����socket��ֵ��<%d>", m_socket);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[%s]����socketʧ��!", __FUNCTION__);
		return false;
	}
	sockaddr_in addr{ 0 };
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, strLocalIP.c_str(), (void*)&addr.sin_addr);
	addr.sin_port = htons(nLocalPort);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr*)&addr, sizeof(addr)))
	{
		Log("[%s]������ʧ��<%s:%d> -- <%d>", __FUNCTION__, strLocalIP.c_str(), nLocalPort, GetLastError());
		CloseSocket(m_socket);
		return false;
	}

	//������Զ�����Ķ˿ڣ����ȡ�Զ�����Ķ˿ں�
	if (0 == nLocalPort)
	{
		sockaddr_in addr;
		int nAddrLen = sizeof(addr);
		int nRet = getsockname(m_socket, (sockaddr*)&addr, &nAddrLen);
		if (0 != nRet)
		{
			Log("��ȡ�׽��ֵĶ˿�ʧ�� Err<%d>", nRet);
			CloseSocket(m_socket);
			return false;
		}
		m_nLocalPort = htons(addr.sin_port);
		Log("�Զ������׽��ֶ˿�<%d>", m_nLocalPort);
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
* ����������	��ʼ���ӷ�����
* ���������	strServerIP		-- ������IP
*				nServerPort		-- �������˿�
*				strLocalIP		-- ʹ�õı���IP
*				nLocalPort		-- ʹ�õı��ض˿�
* ���������	
* �� �� ֵ��	�ɹ�����true�����򷵻�false��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
bool CTcpClient::Start( const std::string &strServerIP, unsigned short nServerPort, int nRecvBuf /* = 0 */, 
	int nSendBuf /* = 0 */, const std::string &strLocalIP /* = "" */, unsigned short nLocalPort /* = 0 */ )
{
	if (nullptr == GetMonitor())
	{
		Log("[%s] ��û������������������", __FUNCTION__);
		return false;
	}
	if (INVALID_SOCKET == m_socket && !PreSocket(strLocalIP, nLocalPort))
	{
		return false;
	}

	//����Socket����(һ��Ĭ����8192)
	if (0 != nRecvBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(nRecvBuf)))
		{
			Log("[CTcpClient::Start]���ö˿ڽ��ջ����Сʧ��<%s:%d> RecvBuff<%d>��", m_strLocalIP.c_str(), m_nLocalPort, nRecvBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClient::Start]���ö˿ڷ��ͻ����Сʧ��<%s:%d> SendBuff<%d>��", m_strLocalIP.c_str(), m_nLocalPort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	if (!GetMonitor()->Attach(m_socket))
	{
		Log("%s] ��ӵ���ɶ˿�ʧ�ܣ�socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_addrServer.sin_family = AF_INET;
	inet_pton(AF_INET, strServerIP.c_str(), (void*)&m_addrServer.sin_addr);
	m_addrServer.sin_port = htons(nServerPort);
	if (!GetMonitor()->PostConnectEx(m_socket, m_addrServer, ConnectedCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("%s] Ͷ��Connect����ʧ�ܣ�socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}

	m_mutextContext.lock();
	m_pContextSend = GetMonitor()->PostSend(m_socket, SentCB, m_pThis.lock(), ErrCB, m_pThis.lock());
	if (nullptr == m_pContextSend)
	{
		m_mutextContext.unlock();
		Log("%s] ��ȡ���ͻ�������ʧ�ܣ�socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_mutextContext.unlock();
	Log("[%s] socket<%d> ���ڳ�������<%s:%d>", __FUNCTION__, m_socket, strServerIP.c_str(), nServerPort);

	std::ostringstream oss;
	oss << strServerIP << ":" << nServerPort << "/" << m_strLocalIP << ":" << m_nLocalPort;
	m_strFlag = oss.str();
	return true;
}

//ֹ֪ͨͣ����
bool CTcpClient::QStop()
{
	//Log("CTcpClient::QStop");
	m_mutextContext.lock();
	m_bShutdown.store(true);
	if (INVALID_SOCKET != m_socket)
	{	
		//windows �� ����Է�����رգ����socket����Ҫ��2���Ӳ��ܱ���ɶ˿ڷ���
		shutdown(m_socket, SD_SEND);
	}
	if (nullptr != m_pContextSend)
	{
		delete m_pContextSend;
		m_pContextSend = nullptr;
	}
	m_mutextContext.unlock();
	return true;
}

/*******************************************************************************
* �������ƣ�	
* ����������	Ͷ�ݷ�������
* ���������	pData		-- Ҫ���͵�����ָ��
*				nLen		-- Ҫ���͵����ݳ��ȣ��ֽ���
* ���������	
* �� �� ֵ��	�ɹ�����Ͷ�ݵ����ݳ��ȣ�ʧ�ܷ���0��
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
int CTcpClient::Send(const char *pData, int nLen)
{
	std::lock_guard<std::mutex> lock(m_mutextContext);
	if (nullptr == m_pContextSend)
	{
		m_buffSend.append(pData, nLen);
		return nLen;
	}
	return ExeSend(pData, nLen);
}

//ʵ��Ͷ�ݷ��ͣ� ��Ҫ���ñ��������������ͱ�����
int CTcpClient::ExeSend(const char *pData, int nLen)
{
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
	bool bRet = GetMonitor()->PostSend(m_pContextSend);
	if (!bRet)
	{
		Log("[%s] Ͷ��Sendʧ�� : socket<%d>", __FUNCTION__, m_socket);
		return 0;
	}
	m_pContextSend = nullptr;
	return nLen;
}

//���ӳɹ��Ĵ�����
void CTcpClient::OnConnected()
{
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pContextConnect, true);
	}
}

/*******************************************************************************
* �������ƣ�	
* ����������	���յ����ݵĴ�����
* ���������	pData		-- ���յ�������ָ��
*				nLen		-- ���յ������ݳ���
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::OnRecvData(Tool::TBuff<char> &Data)
{
	//Log("���յ�����<%d>�ֽ�", nLen);
	if (nullptr != m_pfnData)
	{
		Data.erase(0, m_pfnData(m_pContextData, Data));
	}
	else
	{
		Data.clear();
	}
}

//�ɷ��͵Ĵ�����
void CTcpClient::OnSendable()
{
}

//���������˳�
void CTcpClient::OnError()
{
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pContextConnect, false);
	}	
	QStop();	
}

/*******************************************************************************
* �������ƣ�	
* ����������	���յ����ݵĴ���ص�����
* ���������	pContext	-- ��������
*				s			-- ���յ����ݵ��׽���
*				pData		-- ���յ�������ָ��
*				nLen		-- ���յ������ݳ���
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::RecvDataCB(std::shared_ptr<void> pContext, SOCKET s, char *pData, int nLen)
{
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClient::RecvDataCB] ���ջ�����<%d>̫���ˣ���գ�", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	pThis->m_buffRecv.append(pData, nLen);
	pThis->OnRecvData(pThis->m_buffRecv);	
}

/*******************************************************************************
* �������ƣ�	
* ����������	���ӳɹ��Ļص�����
* ���������	pContext		-- ��������
*				s				-- ���ӳɹ��¼����׽���
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::ConnectedCB( std::shared_ptr<void> pContext, SOCKET s )
{
	Log("[%s] socket <%d> ���ӷ������ɹ���", __FUNCTION__, s);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}

	//ע�⣺�˴��ǳ���Ҫ������socket��״̬������shutdown�Ⱥ����������ڸ�sokcet
	setsockopt(s,
		SOL_SOCKET,
		SO_UPDATE_CONNECT_CONTEXT,
		NULL,
		0);

	//��ʼ��������
	if (!pThis->GetMonitor()->PostRecv(s, RecvDataCB, pContext, ErrCB, pContext))
	{
		Log("[%s] Ͷ�� Recvʧ�ܣ�socket<%d>", __FUNCTION__, pThis->m_socket);
		pThis->QStop();
		return;
	}
	pThis->OnConnected();
}

/*******************************************************************************
* �������ƣ�	
* ����������	���ͳɹ��Ļص�����
* ���������	pContext		-- ��������
*				s				-- �����¼����׽���
*				nSent			-- ���ͳɹ����ֽ���
*				pContextSend	-- Ͷ�ݷ��Ͳ�����Ҫʹ�õĻ�������
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::SentCB(std::shared_ptr<void> pContext, SOCKET s, int nSent, CMonitor::SendContext *pContextSend)
{
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	if (pContextSend->wsaBuff.len != nSent)
	{
		Log("����ʧ�ܣ�ϣ������<%d>�ֽڣ�ʵ�ʷ���<%d>�ֽ�", pContextSend->wsaBuff.len, nSent);
	}
	bool bSent = true;
	pThis->m_mutextContext.lock();
	if (pThis->m_bShutdown.load ())
	{
		delete pContextSend;
		bSent = false;
	}
	else
	{
		pThis->m_pContextSend = pContextSend;
		if (pThis->m_buffSend.size() > 0)
		{
			pThis->ExeSend(&pThis->m_buffSend[0], (int)pThis->m_buffSend.size());
			pThis->m_buffSend.clear();
			bSent = false;
		}
	}
	pThis->m_mutextContext.unlock();
	if (bSent)
	{
		pThis->OnSendable();
	}
}

/*******************************************************************************
* �������ƣ�	
* ����������	��������Ĵ���ص�����
* ���������	pContext	-- ��������
*				s			-- ����������׽���
*				nErrCode	-- ������
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::ErrCB(std::shared_ptr<void> pContext, SOCKET s, int nErrCode)
{
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();
}

/*******************************************************************************
* �������ƣ�	
* ����������	�ر��׽��ֲ�������Ϊ��Чֵ
* ���������	s				-- �׽���
* ���������	s				-- �׽���
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::CloseSocket(SOCKET &s)
{
	if (INVALID_SOCKET != s)
	{
		shutdown(m_socket, SD_BOTH);
		closesocket(s);
		s = INVALID_SOCKET;
	}
}

