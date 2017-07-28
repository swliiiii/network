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
	* �������ƣ�
	* ����������	��ȡ�ļ��ָ�����
	* ���������
	* ���������
	* �� �� ֵ��	����windows����'\\'��linux����'/'��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-03-01	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	��ȡ��ǰĿ¼��
	* ���������
	* ���������
	* �� �� ֵ��	��ǰĿ¼ȫ·����������ĩβ�ġ�\\����/����
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	��ȡ��ִ�г�������Ŀ¼��
	* ���������
	* ���������
	* �� �� ֵ��	���ؿ�ִ�г�������Ŀ¼������ֵ������ĩβ�ġ�\\����/����
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	���õ�ǰ����Ŀ¼��
	* ���������	lpszFolder	-- �����õĹ���Ŀ¼��
	* ���������
	* �� �� ֵ��	ִ�гɹ�����true��ִ��ʧ�ܷ���false��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-02-26	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	����ָ���Ķ༶�ļ�Ŀ¼��
	* ���������
	* ���������
	* �� �� ֵ��	��windows�����У����Ŀ¼�����ɹ���Ŀ¼�Ѵ��ڷ���true�����򷵻�false��
	*				linux���������Ƿ���true��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	Ϊ����ָ�����ļ�������Ҫ���ļ�Ŀ¼��
	* ���������
	* ���������
	* �� �� ֵ��	��windows�����У����Ŀ¼�����ɹ���Ŀ¼�Ѵ��ڷ���true�����򷵻�false��
	*				linux���������Ƿ���true��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
	*******************************************************************************/
	bool CFileEx::CreateFolderForFile(const char * lpszFile)
	{
		std::string strFolder = lpszFile;
		strFolder = strFolder.substr(0, strFolder.rfind(Separator()));
		return CreateFolder(strFolder.c_str());
	}

	/*******************************************************************************
	* �������ƣ�
	* ����������	��ȡָ��Ŀ¼�µ������ļ���������Ŀ¼����
	* ���������
	* ���������
	* �� �� ֵ��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	��ȡָ��Ŀ¼�µ�����Ŀ¼���������ļ�����
	* ���������
	* ���������
	* �� �� ֵ��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	����ȫ·����ȡ�ļ�����
	* ���������
	* ���������
	* �� �� ֵ��	���ػ�ȡ���ļ�����
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	��ȡ����ʣ��ռ䡣
	* ���������	lpszPath -- ����Ŀ¼��
	* ���������
	* �� �� ֵ��	���ش���ʣ��ռ�Ĵ�С����ȡʧ�ܷ���0����λMB��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2007-11-17	�ܷ�	      ����
	*******************************************************************************/
	unsigned int CFileEx::GetFreeDiskSpace(const char* lpszPath)
	{
#ifdef _WIN32
		//���Ӳ�̿ռ�
		ULARGE_INTEGER ulUserFree;
		ULARGE_INTEGER ulTotal;
		if (!GetDiskFreeSpaceEx(AdapterStr(std::string(lpszPath)), &ulUserFree, &ulTotal, NULL))
		{
			//���Ӳ��ʧ�ܣ�һ������Ϊ����·��������
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
	* �������ƣ�
	* ����������	ɾ��ָ�����ļ��С�
	* ���������
	* ���������
	* �� �� ֵ��	ִ�гɹ�����true��ִ��ʧ�ܷ���false��
	* ����˵����	ѭ��ɾ���������е��ļ����ݡ�
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-05	�ܷ�	      ����
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
	* �������ƣ�
	* ����������	ɾ��ָ�����ļ���
	* ���������
	* ���������
	* �� �� ֵ��	ִ�гɹ�����true��ִ��ʧ�ܷ���false��
	* ����˵����
	* �޸�����		�޸���	      �޸�����
	* ------------------------------------------------------------------------------
	* 2008-01-07	�ܷ�	      ����
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

