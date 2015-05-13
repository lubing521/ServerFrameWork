#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_

//ûһ���������û���ռ��һ���߳�
#include "GlobalDefine.h"
#include "Session.h"
#include "CPool.h"

class CMTimeControl;

typedef DWORD (WINAPI *ThreadPrco)(void *pParam);			//�߳�ָ��

struct CSSocket
{
	SOCKET socket;

	int PreRecv(OVERLAPPEDEX *pOl);
	int SendPacket(OVERLAPPEDEX *pOl,char * szPacket);
};


class CConnector
{
public:
	CConnector();
	~CConnector();

	bool Start(ThreadPrco proc, char * szIp, xe_uint16 nPort);
	bool Stop();

	bool ConnectToServer();

	bool ConnectSuccess();						//���ӳɹ�
	bool ConnectFailed(DWORD dwError);			//����ʧ��

	DWORD SendHeatBeat();
	DWORD Reconnect();

	static DWORD SendHeatBeat(CMTimeControl* pTime,void *pParam);		//����������

	static DWORD Reconnect(CMTimeControl* pTime,void *pParam);		//��������

	inline HANDLE GetIocp() { return m_hIocp;}

	OVERLAPPEDEX * Alloc();
	void  ReleaseIOConntext(OVERLAPPEDEX *pOl);

private:
	HANDLE		m_hIocp;
	
	HANDLE		m_pThread;		//�����߳�
	char		m_szIp[20];		//ip�Ͷ˿�
	xe_uint16	m_nPort;		
	ThreadPrco	m_pProc;		//�̺߳���

	CSSocket		m_socket;		//socket

	//������ʱ����������ʱ��
	CMTimeControl * m_pReconnect;	//��������
	CMTimeControl * m_pHeatBeat;	//����

	CPool <OVERLAPPEDEX>* m_pIOPool;
}; 


#endif