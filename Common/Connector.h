#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_

//没一个连接器用户都占用一条线程
#include "GlobalDefine.h"
#include "Session.h"
#include "CPool.h"

class CMTimeControl;

typedef DWORD (WINAPI *ThreadPrco)(void *pParam);			//线程指针

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

	bool ConnectSuccess();						//连接成功
	bool ConnectFailed(DWORD dwError);			//连接失败

	DWORD SendHeatBeat();
	DWORD Reconnect();

	static DWORD SendHeatBeat(CMTimeControl* pTime,void *pParam);		//发送心跳包

	static DWORD Reconnect(CMTimeControl* pTime,void *pParam);		//重新连接

	inline HANDLE GetIocp() { return m_hIocp;}

	OVERLAPPEDEX * Alloc();
	void  ReleaseIOConntext(OVERLAPPEDEX *pOl);

private:
	HANDLE		m_hIocp;
	
	HANDLE		m_pThread;		//处理线程
	char		m_szIp[20];		//ip和端口
	xe_uint16	m_nPort;		
	ThreadPrco	m_pProc;		//线程函数

	CSSocket		m_socket;		//socket

	//心跳定时器和重连定时器
	CMTimeControl * m_pReconnect;	//重新连接
	CMTimeControl * m_pHeatBeat;	//心跳

	CPool <OVERLAPPEDEX>* m_pIOPool;
}; 


#endif