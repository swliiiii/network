#include "stdafx.h"
#include "TcpClient.h"
#include "Monitor.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <sstream>
#include "Tool/TLog.h"

#define Log LogN(102)

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1 
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#define RCV_BUF_SIZE 1000000

//����
CTcpClient::CTcpClient()
	: m_socket(INVALID_SOCKET)
	, m_bStop(false)
	, m_nConnectFlag(0)
	, m_pfnData(nullptr)
	, m_pfnConnect(nullptr)
	, m_addrServer({0})
	, m_addrLocal({0})
	, m_nLocalPort(0)
{
}

//����
CTcpClient::~CTcpClient()
{
	CloseSocket(m_socket);
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
void CTcpClient::RegCallback( CT_Data pfnData, std::shared_ptr<CContextBase> pContextData, 
	CT_Connect pfnConnect, std::shared_ptr<CContextBase> pContextConnect )
{
	m_pfnData = pfnData;
	m_pContextData = pContextData;
	m_pfnConnect = pfnConnect;
	m_pContextConnect = pContextConnect;
}

bool CTcpClient::PreSocket(const std::string &strLocalIP, unsigned short nLocalPort)
{
	//����socket
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == m_socket)
	{
		Log("[CTcpClient::Start] create socket failed! -- errno<%d - %s>", errno, strerror(errno));
		return false;
	}

	//����socketΪ��������
	if (!SetNonblocking(m_socket))
	{
		Log("[CTcpClient::Start] fd<%d>set socket Noblocking failed!", m_socket);
		CloseSocket(m_socket);
		return false;
	}

	//������		
	m_addrLocal.sin_family = AF_INET;
	inet_pton(AF_INET, strLocalIP.c_str(), (void*)&m_addrLocal.sin_addr);
	m_addrLocal.sin_port = htons(nLocalPort);
	if (SOCKET_ERROR == bind(m_socket, (sockaddr*)&m_addrLocal, sizeof(m_addrLocal)))
	{
		Log("bind localIP<%s:%d> failed -- <%d - %s>", strLocalIP.c_str(), nLocalPort, errno, strerror(errno));
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
bool CTcpClient::Start( const std::string &strServerIP, unsigned short nServerPort,	int nRecvBuf /* = 0 */, 
	int nSendBuf /* = 0 */, const std::string &strLocalIP /* = "" */, unsigned short nLocalPort /* = 0 */ )
{
	Log("[CTcpClient::Start] ServerIP<%s> ServerPort<%d> LocalIP<%s>",
		strServerIP.c_str(), nServerPort, strLocalIP.c_str());
	if (nullptr == GetMonitor())
	{
		Log("[CTcpClient::Start] Failed -- NO Monitor yet");
		return false;
	}
	if (m_nConnectFlag.load() > 0)
	{
		Log("[CTcpClient::Start] Failed -- the ConnectFlag != 0, can't repeat start");
		return false;
	}
	m_nConnectFlag.store(0);
	m_bStop = false;
	if (INVALID_SOCKET == m_socket && !PreSocket(strLocalIP, nLocalPort))
	{
		return false;
	}

	//����Socket����
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

	//��ʼ����
	m_addrServer.sin_family = AF_INET;
	m_addrServer.sin_port = htons(nServerPort);
	inet_pton(AF_INET, strServerIP.c_str(), (void*)&m_addrServer.sin_addr);
	int nRet = connect(m_socket, (sockaddr*)&m_addrServer, sizeof(m_addrServer));
	if (-1 == nRet && EINPROGRESS != errno)
	{
		//���������ʾʧ��
		Log("[CTcpClient::Start] fd<%d> connect <%s:%d>failed! -- errno<%d - %s>", m_socket, strServerIP.c_str(), nServerPort, errno, strerror(errno));
		CloseSocket(m_socket);
		return false;
	}

	//����epoll,�Կɶ��¼���Ϊ���ӱ�ʶ
	if (!GetMonitor()->Add(m_socket, EPOLLOUT | EPOLLIN, RecvDataCB, m_pThis.lock(), SendableCB, m_pThis.lock(), ErrCB, m_pThis.lock()))
	{
		Log("[CTcpClient::Start] socket <%d> Add to Epoll failed!", m_socket);
		CloseSocket(m_socket);
		return false;
	}

	std::ostringstream oss;
	oss << strServerIP << ":" << nServerPort << "/" << m_strLocalIP << ":" << m_nLocalPort;
	m_strFlag = oss.str();
	return true;
}

//�ر�����
bool CTcpClient::QStop()
{
	Log("[CTcpClient::QStop] fd <%d>", m_socket);
	m_bStop = true;
	if (INVALID_SOCKET != m_socket)
	{
		//�رն�д���Դ���EPOLL�Ĵ���shutdownһ�ζ����и��Ƶ�sokcetȫ����Ч��
		shutdown(m_socket, SHUT_RDWR);
	}
	return true;
}

/*******************************************************************************
* �������ƣ�	
* ����������	��������
* ���������	pData		-- Ҫ���͵�����ָ��
*				nLen		-- Ҫ���͵����ݳ��ȣ��ֽ���
* ���������	
* �� �� ֵ��	����ʵ�ʷ��͵����ݳ��ȡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
int CTcpClient::Send(const char *pData, int nLen)
{
	//Log("[CTcpClient::Send] socket<%d> nLen<%d>", m_socket, nLen);
	//Log(std::string(pData, nLen > 1000 ? 1000 : nLen).c_str());
	int nSend = send(m_socket, pData, nLen, 0);
	if (-1 != nSend)
	{
		//���ͳɹ�
		//Log("[CTcpClient::Send] socket<%d> Send <%d> OK", m_socket, nSend);
		return nSend;
	}
	Log("[CTcpClient::Send] socket<%d> Send failed: Sent<%d> != nLen<%d>", m_socket, nSend, nLen);
	
	//����ʧ��
	switch (errno)
	{
	case  EAGAIN:	//����������
		break;
	case EINTR:		//���жϴ���ˣ�Ӧ�����·���һ��
		Log("[CTcpClient::Send] socket<%d> EINTR occured, send once again", m_socket);
		return Send(pData, nLen);
	default:
		//Log("fd<%d>Send (%d < %d) failed -- errno<%d - %s>", m_socket, nSend, nLen, errno, strerror(errno));
		break;
	}
	return 0;
}

//���ӳɹ��Ĵ�����
void CTcpClient::OnConnected()
{
	Log("[CTcpClient::OnConnected] socket<%d> connected!", m_socket);
	if (nullptr != m_pfnConnect)
	{
		m_pfnConnect(m_pContextConnect, true);
	}
}

/*******************************************************************************
* �������ƣ�	
* ����������	�յ����ݵĴ�����
* ���������	Data		-- ���յ�������
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::OnRecvData(Tool::TBuff<char> &Data)
{
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
	int DisconnectFlag = 0;
	if(m_nConnectFlag.compare_exchange_strong(DisconnectFlag, 1))
	{
		OnConnected();
	}	
}

//��������Ĵ�����
void CTcpClient::OnError()
{
	int nOrgFlag = m_nConnectFlag.fetch_sub(1);
	//1 == nOrgFlag Ϊ���ӶϿ�
	//0 == nOrgFlag Ϊ����ʧ��
	if (0 == nOrgFlag || 1 == nOrgFlag)
	{
		Log("[CTcpClient::OnError] socket<%d> Disconnected", m_socket);
		if (nullptr != m_pfnConnect)
		{
			m_pfnConnect(m_pContextConnect, false);
		}
	}
	QStop();
}

/*******************************************************************************
* �������ƣ�	
* ����������	���յ����ݵĴ���ص�����
* ���������	pContext	-- ��������
*				nfd			-- �׽���
* ���������	
* �� �� ֵ��	�ޡ�
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::RecvDataCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[CTcpClient::RecvDataCB] fd<%d>", nfd);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	const int nBuffSize = 1024;
	if (pThis->m_buffRecv.size() > RCV_BUF_SIZE)
	{
		Log("[CTcpClient::RecvDataCB] the RCV BUF Size<%d> is too big, clear it!", pThis->m_buffRecv.size());
		pThis->m_buffRecv.clear();
	}
	while (!pThis->m_bStop)
	{		
		auto nPos = pThis->m_buffRecv.size();
		pThis->m_buffRecv.resize(pThis->m_buffRecv.size() + nBuffSize);
		ssize_t nRecv = recv(pThis->m_socket, &(pThis->m_buffRecv[nPos]), nBuffSize, 0);
		//Log("[CTcpClient::RecvDataCB]Recv Data Len<%d> errno<%d>", nRecv, errno);
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
			Log("[CTcpClient::RecvDataCB]connect maybe closed. errno <%d -- %s>", errno, strerror(errno));
			pThis->QStop();
			break;
		}	
	}
	//Log("[CTcpClient::RecvDataCB] End");
}

/*******************************************************************************
* �������ƣ�	
* ����������	�ɷ���֪ͨ�Ļص�����
* ���������	pContext	-- ��������
*				nfd			-- �׽���
* ���������	
* �� �� ֵ��	
* ����˵����	������Ϊ��Ե�����ġ�
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::SendableCB(std::shared_ptr<void> pContext, int nfd)
{
	//Log("[%s]fd <%d> ", __FUNCTION__, nfd);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnSendable();	
}

/*******************************************************************************
* �������ƣ�	
* ����������	���ʹ���Ļص�����
* ���������	pContext	-- ��������
*				nfd			-- �׽���
*				nError		-- ������Ϣ
* ���������	
* �� �� ֵ��	
* ����˵����
* �޸�����		�޸���	      �޸�����
* ------------------------------------------------------------------------------
* 2017/03/07	˾����	      ����
*******************************************************************************/
void CTcpClient::ErrCB( std::shared_ptr<void> pContext, int nfd, int nError )
{
	Log("[CTcpClient::ErrCB] fd<%d> error<%x>", nfd, nError);
	CTcpClient *pThis = (CTcpClient*)pContext.get();
	if (pThis == nullptr)
	{
		return;
	}
	pThis->OnError();	
	pThis->CloseSocket(pThis->m_socket);
}

//��һ��socket����Ϊ��������
bool CTcpClient::SetNonblocking(int nFd)
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

//�ر�socket����Ϊ��Чֵ
void CTcpClient::CloseSocket(int &sock)
{
	if (INVALID_SOCKET != sock)
	{
		Log("[CTcpClient::CloseSocket]close socket <%d>", sock);
		close(sock);
		sock = INVALID_SOCKET;
	}
}
