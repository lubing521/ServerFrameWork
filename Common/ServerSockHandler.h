
#ifndef _DEFAULT_SOCKET_HANDLER
#define _DEFAULT_SOCKET_HANDLER

#include "GlobalDefine.h"
//io 操作

//io上下文
typedef struct tag_TCOMPLETIONPORT
{
	OVERLAPPED		Overlapped;
	WSABUF			DataBuf;
	CHAR			Buffer[DATA_BUFSIZE];
	INT				nOvFlag;
} _TCOMPLETIONPORT, * _LPTCOMPLETIONPORT;



typedef struct tag_TOVERLAPPEDEX
{
	CHAR			Buffer[DATA_BUFSIZE];
	WSABUF			DataBuf;
	SOCKET			Socket;
	WSAOVERLAPPED	Overlapped;
	DWORD			BytesSEND;
	DWORD			BytesRECV;
	INT				nOvFlag;
} _TOVERLAPPEDEX, * _LPTOVERLAPPEDEX;


BOOL InitServerSocket(SOCKET &s, SOCKADDR_IN* addr, UINT nMsgID, int nPort, long lEvent = 2);
BOOL ConnectToServer(SOCKET &s, SOCKADDR_IN* addr, UINT nMsgID, LPCTSTR lpServerIP, DWORD dwIP, int nPort, long lEvent);
BOOL ClearSocket(SOCKET &s);

BOOL CheckAvailableIOCP();

BOOL InitThread(LPTHREAD_START_ROUTINE lpRoutine);

BOOL CheckSocketError(LPARAM lParam);

#endif
