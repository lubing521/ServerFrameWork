#ifndef LOGIN_GATE_H_
#define LOGIN_GATE_H_

//loginGate服务器

#include "../Common/GlobalDefine.h"
#include "../Common/CPool.h"
#include <list>

class CConnector;
class CSessionInfo;
class CLoginGateServer
{
public:
	CLoginGateServer();
	~CLoginGateServer();

	bool Start(char * szConnectorIP,xe_uint32 nConnectPort,char * szIp,xe_uint32 nPort);
	bool Stop();

	//投递AccecptEx
	bool PostAccept(OVERLAPPEDEX *pOl);
	bool DoAccecpt(CSessionInfo *pSession,OVERLAPPEDEX *pOl,xe_uint32 dwBytes);
	bool PostRecv(CSessionInfo *pSession);
	bool RecvPost(CSessionInfo *pSession, char* szBuf,xe_uint32 dwBytes);

	bool PostSend(CSessionInfo *pSession,char* szBuf,xe_uint32 dwBytesSend);

	// 将句柄绑定到完成端口中
	virtual bool _AssociateWithIOCP( CSessionInfo *pContext);

	static DWORD WINAPI Server_WorkThread(void *pParam);
	static DWORD WINAPI Send_WorkThread(void *pParam);		//发送线程

	inline HANDLE GetIocp() { return m_hIocp;}
	inline HANDLE GetShutDown() { return m_hShutDown;}
	inline bool   GetTerminate() { return m_bTerminated;}

	virtual void Update();

	void KillAllSession();

	inline bool IsRunning()		{ return !m_bTerminated;}

	void ProcessSendPacket();			//发包

	void SendPacket(CSessionInfo *pSession,char * szMsg,xe_uint32 nLen);

	void ReleaseOverlapped(OVERLAPPEDEX *pOl);
private:
	CSessionInfo*	m_pListenSession;
	//SOCKET			mListenSocket;
	SOCKADDR_IN		mAddr;
	HANDLE * m_pThread;
	xe_uint32 m_nCount;
	HANDLE   m_pBackendThread;
	
	HANDLE m_hIocp;

	CConnector* m_pConnector;

	xe_bool m_bTerminated;

	CLock m_cs;
	std::list<CSessionInfo*> m_xSessionList;

	CPool<TIOBUFF> *m_pIoPool;
	CPool<OVERLAPPEDEX> * m_pOverlappedPool;

	HANDLE m_hShutDown;
};
#endif