#pragma once 

#include <string>
#include <sstream>

#ifndef ASSERT
#define ASSERT 
#endif

//�����Ҫ������־��ʹ��ǰ�붨���: #define HIDE_RING_BUFF_LOG

namespace Tool
{
	template<class T>
	class TRingBuff 
	{
		//����
		T *m_pData;

		//�����е�����֡��
		size_t m_nCount;

		//��ǰ��ʼλ��
		size_t m_nStart;

		//��󻺴������֡��Ŀ
		size_t m_nMaxSize;

		//ģ������
		std::string m_strName;

	public:

		//����(lpszNameΪ���������ƣ�������־���ʱ�������ĸ�����������־)
		TRingBuff(size_t nMaxSize, const char *lpszName = "default") 
			: m_nCount(0), m_nStart(0), m_nMaxSize(nMaxSize), m_strName(lpszName)
		{
			m_pData = new T[m_nMaxSize];
		}

		TRingBuff(size_t nMaxSize, int nName) 
			: m_nCount(0), m_nStart(0), m_nMaxSize(nMaxSize)
		{
			std::ostringstream os;
			os << nName;
			m_strName = os.str();
			m_pData = new T[m_nMaxSize];
		}

		//����
		~TRingBuff(){delete []m_pData;}

		//�������Ƿ�Ϊ��
		bool empty() const {return m_nCount == 0;}

		//�������Ƿ�����
		bool full() const {return m_nCount >= m_nMaxSize;}

		//������Ԫ�ظ���
		size_t size() const {return m_nCount;}

		//������������С
		size_t max_size() const {return m_nMaxSize;}

		//��ջ�����
		void clear(){m_nCount = 0;}

		//ɾ����������һ��Ԫ��
		bool pop_front()
		{
			if (empty())
			{
				return false;
			}
			m_nStart++; 
			if (m_nMaxSize == m_nStart)
			{
				m_nStart = 0;
			}
			m_nCount--;
			return true;
		}

		//ɾ�����������һ��Ԫ��
		bool pop_back()
		{
			if (empty())
			{
				return false;
			}
			m_nCount--;
			return true;
		}

		//�򻺳���ĩβ׷��һ������
		T & push_back()
		{
			if (m_nMaxSize == m_nCount)
			{
#ifndef HIDE_RING_BUFF_LOG
#ifdef LogN
				LogN(5001)("---<%s>��������֡����%d֡��������һ֡��", m_strName.c_str(), m_nMaxSize);
#elif defined TRACE
				TRACE("---<%s>��������֡����%d֡��������һ֡��\n", m_strName.c_str(), m_nMaxSize);
#else
				//printf("---<%s>��������֡����%d֡��������һ֡��\n", m_strName.c_str(), m_nMaxSize);
#endif
#endif				
				m_nStart++;
				if (m_nMaxSize == m_nStart)
				{
					m_nStart = 0;
				}
			}
			else
			{
				m_nCount++;
			}

			return back();
		}

		//ȡ����һ������
		T & front()
		{
			return (*this)[0];
		}

		//ȡ�����һ������
		T & back()
		{
			return (*this)[m_nCount - 1];
		}

		//ȡ��ָ��λ�õ�����
		T & operator[](size_t nPos)
		{
			ASSERT(m_nCount > nPos);
			size_t nRealPos = m_nStart + nPos;
			while (nRealPos >= m_nMaxSize)
			{
				nRealPos -= m_nMaxSize;
			}
			return m_pData[nRealPos];
		}

	};

}
