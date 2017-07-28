#include "stdafx.h"

#if defined _WIN32
#pragma warning(disable : 4996)
#pragma warning(disable : 4267)
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <iostream>
#include <stdarg.h>
#include "TLog.h"
#include <iomanip>
#include "FileEx.h"

namespace Tool
{

bool TLog::m_bPrint = true;
bool TLog::m_bWriteFile = false;
unsigned int TLog::m_nCount = 1;
std::ofstream TLog::m_fLog;
std::mutex TLog::m_lock;

TLog::TLog()
{
}

void TLog::Init()
{
}

void TLog::SetPrint(bool bOpen)
{
	m_bPrint = bOpen;
}

void TLog::SetWriteFile(bool bOpen)
{
	if (bOpen)
	{
		std::string strFile = GetFileName();
		Tool::CFileEx::CreateFolderForFile(strFile.c_str());
		m_lock.lock();
		if (!m_fLog.is_open())
		{
			m_fLog.open(strFile.c_str());
		}
		m_lock.unlock();
	}
	m_bWriteFile = bOpen;
}

void TLog::operator()(const char *pszFmt, ...) const
{	
	char pszMsg[1024] = {0};
	va_list ptr; 
	va_start(ptr, pszFmt);
	vsprintf(pszMsg, pszFmt, ptr); 
	va_end(ptr);

	std::lock_guard<std::mutex> lock(m_lock);

	if (m_bPrint)
	{
#if (defined TRACE) && (defined DEBUG)
	TRACE("TLog(%d) : %s\n", m_nLevel, pszMsg);		
#else 

#ifdef WIN32
		printf("%s\n", pszMsg);//windows下的这个操作不知为何特别耗时，但是比std::cout要快
#else
		std::cout << pszMsg << std::endl;
#endif // WIN32
#endif
	}
	if (m_bWriteFile)
	{
		if (m_fLog.is_open())
		{
			time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			tm t;
#ifdef _WIN32
			localtime_s(&t, &tt);
#else	
			localtime_r(&tt, &t);
#endif // _WIN32

			m_fLog << std::put_time(&t, "%d %H:%M:%S ") << pszMsg << std::endl;
			if ((++m_nCount>>15<<15) == m_nCount)
			{
				m_fLog.close();
				m_fLog.open(GetFileName());
			}
		}
	}
}

#if defined _WIN32
void TLog::operator()(const wchar_t * pszFmt, ...) const
{
	wchar_t pszMsg[1024];
	va_list ptr; 
	va_start(ptr, pszFmt);
	vswprintf(pszMsg, pszFmt, ptr); 
	va_end(ptr);

	char szMsg[1024];
	WideCharToMultiByte(CP_ACP, 0, pszMsg, -1, szMsg, 1024, NULL, NULL);
	(*this)(szMsg);
}
#endif

std::string TLog::GetFileName(int nOffHour /* = 0 */)
{
	time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(nOffHour));
	tm t;
#ifdef _WIN32
	localtime_s(&t, &tt);
#else	
	localtime_r(&tt, &t);
#endif // _WIN32

	char buff[32] = { 0 };
	strftime(buff, 32, "%y%m%d-%H%M%S-", &t);
	return CFileEx::GetExeDirectory() + CFileEx::Separator() + "log" + CFileEx::Separator() + buff + std::to_string(m_nCount) + ".log";
}

}

