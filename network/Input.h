#pragma once
/*******************************************************************************
* 版权所有 (C) 2017
*     任何一个得到本文件副本的人，均可以使用和传播本文件，但请保留文件申明。
* 文件名称： Input.h
* 说    明： 本类是一个可以监视用户退出操作的类，通知还提供定时器功能。
* 注意事项：
* 当前版本： V1.0
* 作    者： 司文丽
* 完成日期： 2017-07-01

* 更新日期		作者		修改内容
--------------------------------------------------------------------------------
* 2017-07-1	司文丽			创建。
*******************************************************************************/

#include <iostream>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace Input
{

//用户输入命令处理回调函数定义
using CT_Timer = void(*)(
	void* pContext				//环境变量
	);

extern std::atomic<bool> g_bQuit;
extern std::condition_variable g_cond;

//用于监听用户输入的类
class CInput
{
public:
	CInput();

	~CInput(void);

	//开始捕获信号
	void CaptureSignal();

	//注册回调函数
	void RegCallback(CT_Timer pfn, void* pContext);

	//进入监听循环
	void Loop(int nMSec = 0);

	static void Exit()
	{
		g_bQuit.store(true);
		g_cond.notify_one();
	}

protected:

	//定时回调函数
	CT_Timer m_pfn;
	void* m_pContext;

public:

#ifdef _WIN32

	//捕获控制台消息
	static BOOL WINAPI ConsoleHandler(DWORD msgType);
#endif //_WIN32

};

}

