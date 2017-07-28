#pragma once
/*******************************************************************************
* ��Ȩ���� (C) 2017
*     �κ�һ���õ����ļ��������ˣ�������ʹ�úʹ������ļ������뱣���ļ�������
* �ļ����ƣ� Input.h
* ˵    ���� ������һ�����Լ����û��˳��������֪࣬ͨ���ṩ��ʱ�����ܡ�
* ע�����
* ��ǰ�汾�� V1.0
* ��    �ߣ� ˾����
* ������ڣ� 2017-07-01

* ��������		����		�޸�����
--------------------------------------------------------------------------------
* 2017-07-1	˾����			������
*******************************************************************************/

#include <iostream>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace Input
{

//�û����������ص���������
using CT_Timer = void(*)(
	void* pContext				//��������
	);

extern std::atomic<bool> g_bQuit;
extern std::condition_variable g_cond;

//���ڼ����û��������
class CInput
{
public:
	CInput();

	~CInput(void);

	//��ʼ�����ź�
	void CaptureSignal();

	//ע��ص�����
	void RegCallback(CT_Timer pfn, void* pContext);

	//�������ѭ��
	void Loop(int nMSec = 0);

	static void Exit()
	{
		g_bQuit.store(true);
		g_cond.notify_one();
	}

protected:

	//��ʱ�ص�����
	CT_Timer m_pfn;
	void* m_pContext;

public:

#ifdef _WIN32

	//�������̨��Ϣ
	static BOOL WINAPI ConsoleHandler(DWORD msgType);
#endif //_WIN32

};

}

