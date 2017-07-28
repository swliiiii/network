#include "stdafx.h"
#include "TcpServer.h"
#include "../Tool/TLog.h"

#define Log LogN(103)

#define RCV_BUF_SIZE 1000000

//
//���ÿ���ͻ��˵Ĵ������
//

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
CTcpClientBase::CTcpClientBase(SOCKET s, const std::string &strRemoteIP, unsigned short nRemotePort)
	: m_socket(s)
	, m_strRemoteIP(strRemoteIP)
	, m_nRemotePort(nRemotePort)
	, m_pContextSend(nullptr)
	, m_bShutdown(false)
	, m_pfnRecvData(nullptr)
	, m_pfnConnect(nullptr)
{
	Log("CTcpClientBase<%d>", s);
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//����
CTcpClientBase::~CTcpClientBase()
{
	Log("~CTcpClientBase<%d>", m_socket);
	QStop();
	CloseSocket(m_socket);
	WSACleanup();
}

//ע�����ݽ��ܻص�����
void CTcpClientBase::RegCB(CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect)
{
	m_pfnRecvData = pfnData;
	m_pRecvData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pConnect = pContextConnect;
}

//��ʼ�շ�
bool CTcpClientBase::Start( int nRecvBuf /* = 0 */, int nSendBuf /* = 0 */ )
{
	if (nullptr == GetMonitor())
	{
		Log("[%s]ʧ�ܣ���û������������������", __FUNCTION__);
		return false;
	}
	if (INVALID_SOCKET == m_socket)
	{
		Log("[%s] ʧ�ܣ� ��Ч��Socket��", __FUNCTION__);
		QStop();
		return false;
	}

	//����Socket����(һ��Ĭ����8192)
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
	if (!GetMonitor()->Attach(m_socket))
	{
		Log("[%s]��ӵ���ɶ˿�ʧ�ܣ� socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	if (!GetMonitor()->PostRecv(m_socket, RecvDataCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[%s]Ͷ��Recvʧ�ܣ� socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_mutextContext.lock();
	m_pContextSend = GetMonitor()->PostSend(m_socket, SentCB, m_pThis.lock(), ErrCB, m_pThis.lock());
	if (nullptr == m_pContextSend)
	{
		m_mutextContext.unlock();
		Log("[%s]��ȡ���ͻ�������ʧ�ܣ�socket<%d>", __FUNCTION__, m_socket);
		QStop();
		return false;
	}
	m_mutextContext.unlock();
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, true);
	}
	return true;
}

//ֹ֪ͨͣ����
bool CTcpClientBase::QStop()
{
	m_mutextContext.lock();
	m_bShutdown.store(true);
	if (INVALID_SOCKET != m_socket)
	{
		shutdown(m_socket, SD_BOTH);
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
			CloseSocket(m_socket);
			return false;
		}
	}
	if (0 != nSendBuf)
	{
		if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuf, sizeof(nSendBuf)))
		{
			Log("[CTcpClientBase::SetSocketBuff]���ö˿ڷ��ͻ����Сʧ�ܣ�Remote<%s:%d> SendBuff<%d>��", m_strRemoteIP.c_str(), m_nRemotePort, nSendBuf);
			CloseSocket(m_socket);
			return false;
		}
	}
	return true;
}

/*******************************************************************************
* �������ƣ�	
* ����������	Ͷ�ݷ�������
* ���������	pData		-- Ҫ���͵�����ָ��
*				nLen		-- Ҫ���͵����ݳ���
* ���������	
* �� �� ֵ��	Ͷ�ݳɹ�����Ͷ�ݵ����ݳ��ȣ����򷵻�0
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
int CTcpClientBase::Send(const char *pData, int nLen)
{
	std::lock_guard<std::mutex> lock(m_mutextContext);
	if (nullptr == m_pContextSend)
	{
		m_buffSend.append(pData, nLen);
		return nLen;
	}
	
	return ExeSend(pData, nLen);
}

int CTcpClientBase::ExeSend(const char *pData, int nLen)
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
		Log("[%s] Ͷ��sendʧ�ܣ�socket<%d>", __FUNCTION__, m_socket);
		return 0;
	}
	m_pContextSend = nullptr;
	return nLen;
}

//������յ�������
void CTcpClientBase::OnRecvData(Tool::TBuff<char> &Data)
{
	//Log("���յ�����<%d>�ֽ�", nLen);
	if (nullptr != m_pfnRecvData)
	{
		Data.erase(0, m_pfnRecvData(m_pRecvData, Data, m_pThis));
	}
	else
	{
		Data.clear();
	}
}

//�ɷ������ݵĴ�����
void CTcpClientBase::OnSendable()
{

}

//��������Ĵ�����
void CTcpClientBase::OnError()
{
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pConnect, false);
	}
	QStop();
}

//���յ����ݵĴ���ص�����
void CTcpClientBase::RecvDataCB(std::shared_ptr<void> pContext, SOCKET s, char *pData, int nLen)
{
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}	
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClientBase::RecvDataCB] ���ջ�����<%d>̫���ˣ���գ�", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	pThis->m_buffRecv.append(pData, nLen);
	pThis->OnRecvData(pThis->m_buffRecv);
	//Log("<%d> recv data ", pThis->m_socket);
}

/*******************************************************************************
* �������ƣ�	
* ����������	���ͽ���ص�����
* ���������	pContext	-- ��������
*				s			-- �׽���
*				nSent		-- ʵ�ʷ��͵��ֽ���
*				pContextSend	-- ����Ͷ�ݷ��͵Ļ�������
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClientBase::SentCB(std::shared_ptr<void> pContext, SOCKET s, int nSent, CMonitor::SendContext *pContextSend)
{
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
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
	if (pThis->m_bShutdown.load())
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
	pThis->OnSendable();
}

/*******************************************************************************
* �������ƣ�	
* ����������	��������Ĵ���ص�����
* ���������	pCotnext	-- ��������
*				s			-- ����������׽���
*				nErrCode	-- �������
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClientBase::ErrCB(std::shared_ptr<void> pContext, SOCKET s, int nErrCode)
{
	Log("[%s] socket<%d> Err<%d>", __FUNCTION__, s, nErrCode);
	CTcpClientBase *pThis = (CTcpClientBase*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();
}

//�ر�һ��socket����Ϊ��ЧֵINVALID_SOCKET
void CTcpClientBase::CloseSocket(SOCKET &s)
{
	if (INVALID_SOCKET != s)
	{
		closesocket(s);
		s = INVALID_SOCKET;
	}
}
