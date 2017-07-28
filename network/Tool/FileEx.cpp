#include "stdafx.h"
#include "FileEx.h"
#include <algorithm>

#ifndef _WIN32
#include <string.h>
#else
#include <shlobj.h>
#endif

namespace Tool
{

#ifndef _WIN32
	int CFileEx::m_snDepth = -1;
	bool CFileEx::m_sbFile = true;	//true for file and false for folder
	std::vector<std::string> CFileEx::m_svecFile;
#endif

		
#ifdef UNICODE
#define AdapterStr(s) (TCHAR*)(s2ws((s).c_str()).c_str())
#else
#define AdapterStr(s) s.c_str())
#endif


	/*******************************************************************************
	* 函数名称：
	* 功能描述：	获取文件分隔符。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	对于windows返回'\\'，linux返回'/'。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-03-01	周锋	      创建
	*******************************************************************************/
	char CFileEx::Separator()
	{
#ifdef _WIN32
		return '\\';
#else
		return '/';
#endif
	}


	/*******************************************************************************
	* 函数名称：
	* 功能描述：	获取当前目录。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	当前目录全路径，不包括末尾的“\\”或“/”。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	std::string CFileEx::GetCurDirectory()
	{
		std::string strRet;
#ifdef _WIN32
		TCHAR buff[256];
		GetCurrentDirectory(256, buff);
		strRet = AdapterTChar(buff);
#else
		strRet = get_current_dir_name();
#endif
		return strRet;
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	获取可执行程序所在目录。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	返回可执行程序所在目录，返回值不包括末尾的“\\”或“/”。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	std::string CFileEx::GetExeDirectory()
	{
		std::string strRet;

#ifdef _WIN32
		TCHAR buff[256];
		GetModuleFileName(NULL, buff, 256);	
		strRet = AdapterTChar(buff);
#else
		char buff[256] = { 0 };
		readlink("/proc/self/exe", buff, 256);
		strRet = buff;
#endif

		strRet = strRet.substr(0, strRet.rfind(Separator()));
		return strRet;
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	设置当前工作目录。
	* 输入参数：	lpszFolder	-- 待设置的工作目录。
	* 输出参数：
	* 返 回 值：	执行成功返回true，执行失败返回false。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-02-26	周锋	      创建
	*******************************************************************************/
	bool CFileEx::SetCurDirectory(const char * lpszFolder)
	{
#ifdef _WIN32
		return !!SetCurrentDirectory(AdapterStr(std::string(lpszFolder)));
#else
		return (chdir(lpszFolder) == 0);
#endif
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	创建指定的多级文件目录。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	在windows环境中，如果目录创建成功或目录已存在返回true，否则返回false。
	*				linux环境中总是返回true。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	bool CFileEx::CreateFolder(const char * lpszFolder)
	{
		if (0 == lpszFolder)
		{
			return false;
		}
		std::string strFolder = lpszFolder;
		if (strFolder.empty())
		{
			return false;
		}
		if (Separator() != strFolder[0] && std::string::npos == strFolder.find(':'))
		{
			std::string strCurDir = GetCurDirectory();
			strCurDir += Separator();
			strFolder.insert(strFolder.begin(), strCurDir.begin(), strCurDir.end());
		}
#ifdef _WIN32
		TCHAR *p = AdapterStr(strFolder);
		int nRet = SHCreateDirectoryEx(
			NULL,
			AdapterStr(strFolder),
			NULL
		);
		return ERROR_SUCCESS == nRet || ERROR_ALREADY_EXISTS == nRet;
#else
		std::string strCmd = "mkdir -p \"" + strFolder + std::string("\"");
		system(strCmd.c_str());
		return true;
#endif
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	为创建指定的文件创建必要的文件目录。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	在windows环境中，如果目录创建成功或目录已存在返回true，否则返回false。
	*				linux环境中总是返回true。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	bool CFileEx::CreateFolderForFile(const char * lpszFile)
	{
		std::string strFolder = lpszFile;
		strFolder = strFolder.substr(0, strFolder.rfind(Separator()));
		return CreateFolder(strFolder.c_str());
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	获取指定目录下的所有文件（不包括目录）。
	* 输入参数：
	* 输出参数：
	* 返 回 值：
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	void CFileEx::GetSubFiles(const char * lpszFolder, std::vector<std::string> &vecFile)
	{
		std::string strFolder = lpszFolder;
		if (strFolder.empty())
		{
			return;
		}
		if (*strFolder.rbegin() == Separator())
		{
			strFolder.erase(strFolder.length(), 1);
		}
		vecFile.clear();
#ifdef _WIN32
		WIN32_FIND_DATA p;
		HANDLE h = FindFirstFile(AdapterStr(strFolder+"\\*.*"), &p);
		if (INVALID_HANDLE_VALUE != h)
		{
			while (true)
			{
				if (0 == (p.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					vecFile.push_back(strFolder + Separator() + AdapterTChar(p.cFileName));
				}
				if (0 == FindNextFile(h, &p))
				{
					break;
				}
			}
			FindClose(h);
		}
#else
		m_svecFile.clear();
		m_snDepth = -1;
		m_sbFile = true;
		ftw(strFolder.c_str(), FileFunc, 500);
		vecFile = m_svecFile;
#endif
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	获取指定目录下的所有目录（不包括文件）。
	* 输入参数：
	* 输出参数：
	* 返 回 值：
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	void CFileEx::GetSubFoders(const char * lpszFolder, std::vector<std::string> &vecFolder)
	{
		std::string strFolder = lpszFolder;
		if (strFolder.empty())
		{
			return;
		}
		if (*strFolder.rbegin() == Separator())
		{
			strFolder.erase(strFolder.length(), 1);
		}
		vecFolder.clear();
#ifdef _WIN32
		WIN32_FIND_DATA p;
		HANDLE h = FindFirstFile(AdapterStr(strFolder + "\\*"), &p);
		if (INVALID_HANDLE_VALUE != h)
		{
			while (true)
			{
				if (0 != (p.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					vecFolder.push_back(strFolder + Separator() + AdapterTChar(p.cFileName));
				}
				if (0 == FindNextFile(h, &p))
				{
					break;
				}
			}
			FindClose(h);
		}
#else
		m_svecFile.clear();
		m_snDepth = -1;
		m_sbFile = false;
		ftw(strFolder.c_str(), FileFunc, 500);
		vecFolder = m_svecFile;
#endif
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	根据全路径获取文件名。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	返回获取的文件名。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	std::string CFileEx::Path2FileName(const char *lpszPath)
	{
		std::string strRet = lpszPath;
		if (strRet.empty())
		{
			return strRet;
		}
		if (*strRet.rbegin() == Separator())
		{
			strRet.erase(strRet.length() - 1);
		}
		std::string::size_type pos = strRet.rfind(Separator());
		if (std::string::npos == pos)
		{
			return strRet;
		}
		return strRet.substr(pos + 1);
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	获取磁盘剩余空间。
	* 输入参数：	lpszPath -- 磁盘目录。
	* 输出参数：
	* 返 回 值：	返回磁盘剩余空间的大小，获取失败返回0。单位MB。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2007-11-17	周锋	      创建
	*******************************************************************************/
	unsigned int CFileEx::GetFreeDiskSpace(const char* lpszPath)
	{
#ifdef _WIN32
		//检测硬盘空间
		ULARGE_INTEGER ulUserFree;
		ULARGE_INTEGER ulTotal;
		if (!GetDiskFreeSpaceEx(AdapterStr(std::string(lpszPath)), &ulUserFree, &ulTotal, NULL))
		{
			//监测硬盘失败，一般是因为存盘路径不存在
			return 0;
		}
		unsigned int nMb = 1024 * 1024;
		unsigned int nUserFree = ulUserFree.HighPart * 4096 + ulUserFree.LowPart / nMb;
		//UINT nTotal = ulTotal.HighPart * 4096 + ulTotal.LowPart / nMb;
		return nUserFree;

#else
		struct statfs stVfs;
		memset(&stVfs, 0, sizeof(stVfs));
		if (-1 == statfs(lpszPath, &stVfs))
		{
			return 0;
		}
		return (unsigned int)(stVfs.f_bfree / 1024 / 1024 * stVfs.f_bsize);
#endif
	}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	删除指定的文件夹。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	执行成功返回true，执行失败返回false。
	* 其它说明：	循环删除里面所有的文件内容。
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-05	周锋	      创建
	*******************************************************************************/
	bool CFileEx::DelFolder(const char * lpszFolder)
	{
#ifdef _WIN32
		CString strPath = CString(lpszFolder) + '\0';
		SHFILEOPSTRUCT fs;
		fs.hwnd = NULL;
		fs.wFunc = FO_DELETE;
		fs.pFrom = strPath;
		fs.pTo = NULL;
		fs.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		fs.fAnyOperationsAborted = TRUE;
		return (0 == SHFileOperation(&fs));
#else
		std::string strCmd = std::string("rm -rfd \"") + lpszFolder + std::string("\"");
		system(strCmd.c_str());
		return true;
#endif
}

	/*******************************************************************************
	* 函数名称：
	* 功能描述：	删除指定的文件。
	* 输入参数：
	* 输出参数：
	* 返 回 值：	执行成功返回true，执行失败返回false。
	* 其它说明：
	* 修改日期		修改人	      修改内容
	* ------------------------------------------------------------------------------
	* 2008-01-07	周锋	      创建
	*******************************************************************************/
	bool CFileEx::DelFile(const char* lpszPath)
	{
#ifdef _WIN32
		return 0 != DeleteFile(AdapterStr(std::string(lpszPath)));
		
#else
		return 0 == remove(lpszPath);
#endif
}

#ifndef _WIN32
	int CFileEx::FileFunc(const char *file, const struct stat *sb, int flag)
	{
		int nDepth = std::count(file, file + strlen(file), '/');
		if (-1 == m_snDepth)
		{
			m_snDepth = nDepth;
		}
		if (nDepth - m_snDepth == 1)
		{
			if (m_sbFile && FTW_F == flag || !m_sbFile && FTW_D == flag)
			{
				m_svecFile.push_back(std::string(file));
			}
		}
		return 0;
	}
#endif

#ifdef _WIN32
	std::string CFileEx::ws2s(const std::wstring& ws)
	{
		std::string strCurLocale = setlocale(LC_ALL, NULL); // curLocale = "C";  
		setlocale(LC_ALL, "chs");
		size_t nSize = 2 * ws.size()+1;
		std::string strRet(nSize, '\0');
		wcstombs_s(&nSize, const_cast<char*>(strRet.data()), nSize, ws.c_str(), nSize);
		setlocale(LC_ALL, strCurLocale.c_str());
		strRet.resize(nSize - 1);
		return strRet;
	}
	
	std::wstring CFileEx::s2ws(const std::string &s)
	{
		setlocale(LC_ALL, "chs");
		size_t nSize = s.size()+1;
		std::wstring strRet(nSize, _T('\0'));
		mbstowcs_s(&nSize, const_cast<wchar_t*>(strRet.data()), nSize, s.c_str(), nSize * sizeof(wchar_t));
		setlocale(LC_ALL, "C");
		strRet.resize(nSize-1);
		return strRet;
	}

	std::string CFileEx::AdapterTChar(TCHAR *s)
	{
#ifdef UNICODE
		return ws2s(s);
#else
		return s;
#endif
	}

#endif // _WIN32


}

