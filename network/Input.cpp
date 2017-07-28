#include "stdafx.h"
#include "Input.h"
#include <signal.h>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif // _WIN32
#include <string.h>
#include "Tool/TLog.h"


#define Log LogN(50)

namespace Input
{

	//程序退出标识
	std::atomic<bool> g_bQuit(false);

	//程序退出的通知条件变量
	std::mutex g_mtx;
	std::condition_variable g_cond;

#ifndef _WIN32

	// 信号处理函数
	void handle_signal(int signo)
	{
		switch (signo)
		{
		case SIGINT:
			Log("signal -- SIGINT");
			g_bQuit.store(true);
			g_cond.notify_one();
			break;
		case SIGTERM:
			Log("signal -- SIGTERM");
			g_bQuit.store(true);
			g_cond.notify_one();
			break;
		case SIGPIPE:
			Log("signal -- SIGPIPE");
			break;
		case SIGUSR1://值10
			break;
		case SIGUSR2://值12
			Log("signal -- SIGUSR2");
			break;
		default:
			Log("not handle signal <%d>", signo);
			break;
		}
	}
#endif // !_WIN32

	CInput::CInput()
	{
		m_pfn = nullptr;
		m_pContext = nullptr;
	}

	CInput::~CInput()
	{
	}

	void CInput::CaptureSignal()
	{
#ifdef _WIN32
		SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
		signal(SIGINT, handle_signal);
		signal(SIGPIPE, handle_signal);
		signal(SIGTERM, handle_signal);

		signal(SIGUSR1, handle_signal);
		signal(SIGUSR2, handle_signal);
#endif // _WIN32
	}

	//注册回调函数
	void CInput::RegCallback(CT_Timer pfn, void* pContext)
	{
		m_pfn = pfn;
		m_pContext = pContext;
	}


	//进入监听循环
	void CInput::Loop(int nMSec /* = 0 */)
	{
		if (nMSec <= 0)
		{
			nMSec = 60 * 60 * 1000;
		}
		g_bQuit.store(false);
		while (true)
		{
			std::unique_lock<std::mutex> ulock(g_mtx);
			g_cond.wait_for(ulock, std::chrono::milliseconds(nMSec));
			if (g_bQuit.load())
			{
				break;
			}
			if (nullptr != m_pfn)
			{
				m_pfn(m_pContext);
			}
		}
		Log("Input Thread End!");
	}

#ifdef WIN32

	//捕获控制台消息
	BOOL WINAPI CInput::ConsoleHandler(DWORD msgType)
	{
		if (msgType == CTRL_C_EVENT)
		{
			std::cout << "Ctl+C" << std::endl;
			g_bQuit = true;
			g_cond.notify_one();
			return TRUE;
		}
		else if (msgType == CTRL_CLOSE_EVENT)
		{
			printf("Close console window!\n");
			/* Note: The system gives you very limited time to exit in this condition */
			return TRUE;
		}
		/*
		Other messages:
		CTRL_BREAK_EVENT         Ctrl-Break pressed
		CTRL_LOGOFF_EVENT        User log off
		CTRL_SHUTDOWN_EVENT      System shutdown
		*/
		return FALSE;
	}
#endif // _WIN32

}
 
