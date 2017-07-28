/*******************************************************************************
* 版权所有 (C) 2007
*     任何一个得到本文件副本的人，均可以使用和传播本文件，但请保留文件申明。
* 文件名称： TLog.h
* 文件标识： 
* 内容摘要： 本文件定义了一组用于输出日志的宏。
* 其它说明： 使用这些宏所输出的日志，将会被以UDP数据包的形式发送到指定的IP，届时
*			通过日志服务器软件可以在该IP上获取到这些日志信息。在Debug模式下，该宏
*			会形成TRACE。
*			本软件使用方法如下：
*			1、将本TLog.h和TLog.cpp加入需要使用它的工程。
*			2、在需要使用它进行日志输出的地方包含本头文件。
*			3、直接像使用"printf"或"TRACE"一样使用"LogMsg"或"LogN(n)"。
*			4、一种常见的做法是，在使用者的模块中转定义自己的"Log"，如"#define Log LogN(1000)"；
*			   然后，再直接像使用"printf"使用"Log"来写日志。
* 注意事项：
*			1、和printf以及TRACE不一样的是，本日志类在屏幕或打印窗口输出的日志末
*			尾会自动加入"\n"。
* 当前版本： V1.0
* 作    者： 周锋
* 完成日期： 2007-09-01
*
* 更新日期		作者		修改内容
--------------------------------------------------------------------------------
* 2008-04-10	周锋		更新日志软件使其在windows下支持宽字符日志输出。
* 2017-07-21	司文丽		不再支持网络输出日志，增加写入日志到文件。
*******************************************************************************/
#ifndef _TLOG_H_1234789329489217489271849372891
#define _TLOG_H_1234789329489217489271849372891

#include <string>
#include <thread>
#include <mutex>
#include <fstream>

namespace Tool
{

class TLog
{
public:

	//初始化
	static void Init();

	//设置是否屏蔽屏幕输出
	static void SetPrint(bool bOpen);

	//设置是否输出到文件
	static void SetWriteFile(bool bOpen);

	//构造函数
	TLog();

	//操作符"()"的重载
	void operator()(const char *pszFmt, ...) const;

#if defined _WIN32
	void operator()(const wchar_t * pszFmt, ...) const;
#endif

	//生成一个当前的日志文件名
	static std::string GetFileName(int nOffHour = 0);

protected:

	static bool m_bPrint;
	static bool m_bWriteFile;
	static std::mutex m_lock;
	static unsigned int m_nCount;
	static std::ofstream m_fLog;
};

//以下是用于输出日志的一组宏
#define LogN(n)		Tool::TLog()
#define LogMsg		LogN(0)

}

#endif

