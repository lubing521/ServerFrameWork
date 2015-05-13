#include "../Common/Connector.h"
#include "../Common/Log.h"
#include "../Common//MsWinsockUtil.h"

#include "LoginGate.h"

//tcp无延迟发送和缓存池设0，立即发送
extern void SetSocketProp( SOCKET socket );
//void _SetSocketProp( SOCKET socket )
//{
//	//tcp无延迟
//	int nodelay = 1;
//	setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
//
//	//设置缓冲池立即发送
//	int zero = 0;
//	setsockopt(socket,SOL_SOCKET,SO_SNDBUF,(char*)&zero,sizeof(zero));
//
//	zero = 0;
//	setsockopt(socket,SOL_SOCKET,SO_RCVBUF,(char*)&zero,sizeof(zero));
//}


bool _IsSocketAlive( SOCKET s )
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}

bool HandleError( SOCKET & cClient,const DWORD& dwErr )
{
	// 如果是超时了，就再继续等吧  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// 确认客户端是否还活着...
		if( !_IsSocketAlive( cClient) )			//send校验
		{
			printf( "检测到客户端异常退出！\n");
			closesocket(cClient);
			cClient = INVALID_SOCKET;
			return true;
		}
		else
		{
			printf( "网络操作超时！重试中...\n");
			return true;
		}
	}  

	// 可能是客户端异常退出了
	else if( ERROR_NETNAME_DELETED==dwErr )		
	{
		printf( "检测到客户端异常退出！\n");			//客户端是关X直接退出
		closesocket(cClient);
		cClient = INVALID_SOCKET;
		return true;
	}
	else if (ERROR_CONNECTION_REFUSED == dwErr)
	{
		printf( "服务器未开启,拒绝访问!\n");			//客户端是关X直接退出
		return true;
	}
	else
	{
		printf( "完成端口操作出现错误，线程退出。错误代码：%d\n",dwErr );
		return false;
	}
}


DWORD WINAPI ConnectorWorkThread(void *pParam)
{
	CConnector *pConnector = (CConnector*)pParam;
	if (pConnector)
	{
		DWORD dwBytes = 0;
		OVERLAPPEDEX *pOl = NULL;
		CSSocket *pScoket = NULL;

		while(true)
		{
			BOOL bRet = GetQueuedCompletionStatus(pConnector->GetIocp(),&dwBytes,(LPDWORD)&pScoket,(LPOVERLAPPED*)&pOl,INFINITE);

			if (pScoket ==EXIT_CODE)
				break;

			if (bRet == FALSE)
			{
				//退出
				DWORD dwErr = GetLastError();

				if (dwErr == ERROR_CONNECTION_REFUSED)
				{
					//连接不上服务器
					pConnector->ConnectFailed(dwErr);
				}
				else
				{
					if (pScoket != NULL)
					{
						// 显示一下提示信息,退出返回true
						if(!HandleError( pScoket->socket,dwErr ) )			
						{
							pConnector->ReleaseIOConntext(pOl);
							break;
						}
					}
				}

				pConnector->ReleaseIOConntext(pOl);
				continue; 
			}
			else
			{
				if (dwBytes== 0 && (pOl &&(pOl->bOperator == IOCP_REQUEST_RECV || pOl->bOperator == IOCP_REQUEST_SEND)))
				{
					closesocket(pScoket->socket);
					continue;
				}
				else
				{
					if (pOl)
					{
						switch(pOl->bOperator)
						{
						case IOCP_REQUEST_RECV:
							{
								printf("socket:%d,recv:%s\n",pOl->pIOBuf.sock,pOl->pIOBuf.szData);
								//pConnector->ReleaseIOConntext(pOl);
								pScoket->PreRecv(pOl);
							}
							break;
						case IOCP_REQUEST_SEND:
							{
								//SAFE_DELETE(pOl->pIOBuf);
								pConnector->ReleaseIOConntext(pOl);
							}
							break;
						case  IOCP_REQUEST_CONNECT:
							{
								//连接成功
								pConnector->ReleaseIOConntext(pOl);
								pConnector->ConnectSuccess();
							}
							break;
						}
					}
				}
			}
		}
		return 1;
	}

	return 0;
}

DWORD WINAPI CLoginGateServer::Server_WorkThread( void *pParam )
{
	CLoginGateServer *pServer = (CLoginGateServer*)pParam;
	if (pServer)
	{
		OVERLAPPEDEX *pOl = NULL;
		CSessionInfo *pSession = NULL;

		DWORD dwBytes = 0;


		while (WAIT_OBJECT_0 != WaitForSingleObject(pServer->GetShutDown(),0))
		{
			BOOL bRet = GetQueuedCompletionStatus(pServer->GetIocp(),&dwBytes,(LPDWORD)&pSession,(LPOVERLAPPED*)&pOl,INFINITE);

			if (pSession ==EXIT_CODE)
				break;

			if(!pServer->IsRunning())
				break;

			if (bRet == FALSE)
			{
				//退出
				DWORD dwErr = GetLastError();

				{
					if (pSession != NULL)
					{
						// 显示一下提示信息,退出返回true
						if(!HandleError( pSession->sock,dwErr ) )			
						{
							pSession->Remove();
							break;
						}
					}
				}
				pSession->Remove();
				continue; 
			}
			else
			{
				if (dwBytes== 0 && (pOl &&(pOl->bOperator == IOCP_REQUEST_RECV || pOl->bOperator == IOCP_REQUEST_SEND)))
				{
					pSession->Remove();
					continue;
				}
				else
				{
					if (pOl)
					{
						switch(pOl->bOperator)
						{
						case IOCP_REQUEST_RECV:
							{
								printf("socket:%d,recv:%s\n",pOl->pIOBuf.sock,pOl->pIOBuf.szData);
								pServer->RecvPost(pSession,pOl->pIOBuf.szData,dwBytes);
								pServer->PostRecv(pSession);
							}
							break;
						case IOCP_REQUEST_SEND:
							{
								//SAFE_DELETE(pOl->pIOBuf);
								pServer->ReleaseOverlapped(pOl);
							}
							break;
						case IOCP_REQUEST_ACCEPT:
							{
								pServer->DoAccecpt(pSession,pOl,dwBytes);
							}
							break;
						}
					}
				}
			}
		}
		
	}
	return 0;
}


DWORD WINAPI CLoginGateServer::Send_WorkThread( void *pParam )
{
	CLoginGateServer *pServer = (CLoginGateServer*)pParam;
	if (pServer)
	{
		while (WAIT_OBJECT_0 != WaitForSingleObject(pServer->GetShutDown(),0))
		{
			if (!pServer->IsRunning())
				break;

			pServer->ProcessSendPacket();

			Sleep(10);
		}
	}
		
	return 0;
}

CLoginGateServer::CLoginGateServer()
{
	//mListenSocket = INVALID_SOCKET;
	m_pThread = NULL;
	m_nCount = 0;
	m_pBackendThread = INVALID_HANDLE_VALUE;
	m_hShutDown = INVALID_HANDLE_VALUE;

	m_pConnector = new CConnector();

	m_bTerminated = false;

	WSADATA wd;
	WSAStartup(MAKEWORD(2,2),&wd);

	m_pIoPool = new CPool<TIOBUFF>(1000);

	m_pListenSession = new CSessionInfo();
	m_pListenSession->sock = INVALID_SOCKET;
	m_pOverlappedPool = new CPool<OVERLAPPEDEX>(1000);
}

CLoginGateServer::~CLoginGateServer()
{
	Stop();
	SAFE_DELETE_ARRAY(m_pThread);
	SAFE_DELETE(m_pConnector);

	WSACleanup();

	m_bTerminated = true;

	SAFE_DELETE(m_pIoPool);
	SAFE_DELETE(m_pListenSession);
	SAFE_DELETE(m_pOverlappedPool);
}

bool CLoginGateServer::Start( char * szConnectorIP,xe_uint32 nConnectPort,char * szIp,xe_uint32 nPort )
{
	m_bTerminated = false;

	m_hShutDown = CreateEvent(NULL,TRUE,FALSE,NULL);

	//连接
	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,0);
	if (m_hIocp == INVALID_HANDLE_VALUE)
	{
		sLog.OutPutError("create iocompletionport failed!");
		return false;
	}

	//cpu*2
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	m_pListenSession->sock = WSASocket(AF_INET,SOCK_STREAM,0,0,0,WSA_FLAG_OVERLAPPED);

	if (m_pListenSession->sock == INVALID_SOCKET)
	{
		return false;
	}

	if (_AssociateWithIOCP(m_pListenSession) == false)
	{
		sLog.OutPutStr("listen socket bind iocp failed!");
		return false;
	}
	//绑定
	mAddr.sin_family		= AF_INET;
	mAddr.sin_addr.s_addr	= ( szIp == NULL || strlen( szIp ) == 0 ) ? htonl(INADDR_ANY) : inet_addr(szIp);
	mAddr.sin_port			= htons( nPort );

	xe_int32 err = bind(m_pListenSession->sock,(sockaddr*)&mAddr,sizeof(SOCKADDR_IN));
	if (err == SOCKET_ERROR)
	{
		sLog.OutPutError("[CAccept] bind port error!");
		SAFE_DELETE(m_pListenSession);
		return false;
	}

	//监听
	err = listen(m_pListenSession->sock,SOMAXCONN);
	if ( err == SOCKET_ERROR)
	{
		sLog.OutPutError("[CAccept] listen error!");
		//closesocket(m_pListenSession->sock);
		SAFE_DELETE(m_pListenSession);
		return false;
	}

	MsWinsockUtil::LoadExtensionFunction(m_pListenSession->sock);

	if (m_pConnector->Start(ConnectorWorkThread,szConnectorIP,nConnectPort) == false)
	{
		sLog.OutPutError("[connector] error!");
		//closesocket(m_pListenSession->sock);
		SAFE_DELETE(m_pListenSession);
		return false;
	}

	if (info.dwNumberOfProcessors > 4)
		m_nCount = 2*info.dwNumberOfProcessors;
	else
		m_nCount = info.dwNumberOfProcessors;

	//工作线程句柄
	m_pThread = new HANDLE[m_nCount];


	//建立工作者线程
	DWORD dwThreadId;
	for (int i = 0; i < m_nCount; ++i)
	{
		m_pThread[i] = CreateThread(NULL,0,Server_WorkThread,this,0,&dwThreadId);
	}

	m_pBackendThread = CreateThread(NULL,0,Send_WorkThread,this,0,&dwThreadId);
	//准备
	for (int i = 0; i < MAX_POST_ACCEPT; ++i)
	{
		OVERLAPPEDEX *pOl = m_pOverlappedPool->Alloc();//new OVERLAPPEDEX();

		PostAccept(pOl);
	}

	sLog.OutPutStr("Server Start!");

	return true;
}

bool CLoginGateServer::PostAccept( OVERLAPPEDEX *pOl)
{
	memset(pOl,0,sizeof(OVERLAPPEDEX));

	SOCKET newSocket = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);

	if (newSocket != INVALID_SOCKET)
	{
		SetSocketProp(newSocket);
	}

	pOl->sock = newSocket;

	
	pOl->bOperator = IOCP_REQUEST_ACCEPT;

	pOl->pIOBuf.sock = pOl->sock;

	WSABUF buf;
	buf.buf = pOl->pIOBuf.szData;
	buf.len = DATA_BUFSIZE;

	DWORD dwRecvBytes = 0;
	//接收
	//AcceptEx(listenSocket,m_socket,szBuffer,0,sizeof(SOCKADDR_IN) + 16,sizeof(SOCKADDR_IN)+16,&dwRecvBytes,&m_AccpetIo);
	int nRet = MsWinsockUtil::m_lpfnAccepteEx(m_pListenSession->sock,pOl->sock,buf.buf,buf.len-((sizeof(SOCKADDR_IN) + 16) * 2),sizeof(SOCKADDR_IN) + 16,sizeof(SOCKADDR_IN)+16,&dwRecvBytes,pOl);

	if (nRet ==	FALSE && WSAGetLastError() != WSA_IO_PENDING)
	{
		closesocket(newSocket);
		printf("error:%d\n",WSAGetLastError());
		ReleaseOverlapped(pOl);
		return false;
	}
	
	
	return true;
}

bool CLoginGateServer::DoAccecpt( CSessionInfo *pSession,OVERLAPPEDEX *pOl,xe_uint32 dwBytes )
{
	SOCKADDR_IN* ClientAddr = NULL;
	SOCKADDR_IN* LocalAddr = NULL;  
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);  

	//假设把服务器端的iocontext接受到的消息buffer重置，再用m_lpfnGetAcceptExSockAddrs去获取第一次消息
	//pIoContext->ResetBuffer();

	///////////////////////////////////////////////////////////////////////////
	// 1. 首先取得连入客户端的地址信息
	// 这个 m_lpfnGetAcceptExSockAddrs 不得了啊~~~~~~
	// 不但可以取得客户端和本地端的地址信息，还能顺便取出客户端发来的第一组数据
	MsWinsockUtil::m_lpfnGetAcceptExSockAddrs(pOl->pIOBuf.szData, DATA_BUFSIZE - ((sizeof(SOCKADDR_IN)+16)*2),  
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

#ifdef _DEBUG
	sLog.OutPutStr( "客户端 %s:%d 连入.", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port) );
	sLog.OutPutStr( "客户额 %s:%d 信息：%s.",inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pOl->pIOBuf.szData);
#endif

	//绑定到完成端口上新的客户端连接
	CSessionInfo *pNewSession = new CSessionInfo;
	pNewSession->sock = pOl->pIOBuf.sock;
	if (_AssociateWithIOCP(pNewSession)== false)
	{
		sLog.OutPutError("新接入socket 绑定完成端口失败！");
		SAFE_DELETE(pSession);
		return false;
	}

	m_cs.Lock();
	m_xSessionList.push_back(pNewSession);
	m_cs.UnLock();

	SetSocketProp(pNewSession->sock);

	//这里可以处理第一组数据
	if (!RecvPost(pNewSession,pOl->pIOBuf.szData,dwBytes))
	{
		return false;
	}

	//该socket 等待下一次数据接收
	if (!PostRecv(pNewSession))
	{
		return false;
	}

	//继续投递
	//OVERLAPPEDEX *pOl = new OVERLAPPEDEX();
	if (PostAccept(pOl))
	{
		return true;
	}
	else
	{
		closesocket(pNewSession->sock);
		SAFE_DELETE(pNewSession);
		ReleaseOverlapped(pOl);
		return false;
	}
	
	return true;
}

bool CLoginGateServer::_AssociateWithIOCP( CSessionInfo *pContext )
{
	// 将用于和客户端通信的SOCKET绑定到完成端口中
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->sock, m_hIocp, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		sLog.OutPutError("执行CreateIoCompletionPort()出现错误.错误代码：%d",GetLastError());
		return false;
	}

	
	return true;
}

bool CLoginGateServer::PostRecv( CSessionInfo *pSession )
{
	if (pSession)
	{
		OVERLAPPEDEX *ol =  m_pOverlappedPool->Alloc();
		memset(&ol,0,sizeof(OVERLAPPEDEX));
		ol->bOperator = IOCP_REQUEST_RECV;
		ol->pIOBuf.sock = pSession->sock;
		ol->sock = pSession->sock;
		WSABUF buf;
		buf.buf = ol->pIOBuf.szData;
		buf.len = DATA_BUFSIZE;

		DWORD dwBytes = 0, dwFlags = 0;
		int nRet = WSARecv(pSession->sock,&buf,1,&dwBytes,&dwFlags,ol,NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			pSession->Remove();
			ReleaseOverlapped(ol);
			return false;
		}

		return true;
	}

	return false;
}

bool CLoginGateServer::RecvPost( CSessionInfo *pSession, char* szBuf,xe_uint32 dwBytes )
{
	if (pSession)
	{
		TIOBUFF *pBuf = m_pIoPool->Alloc();
		strncpy(pBuf->szData,szBuf,dwBytes);
		pBuf->sock =pSession->sock;

		pSession->Push_RecvBack(pBuf);
	}

	return true;
}

bool CLoginGateServer::PostSend( CSessionInfo *pSession,char* szBuf,xe_uint32 dwBytesSend )
{
	if (pSession &&pSession->IsRemove() == false)
	{
		OVERLAPPEDEX *ol =  m_pOverlappedPool->Alloc();
		memset(&ol,0,sizeof(OVERLAPPEDEX));
		ol->bOperator = IOCP_REQUEST_SEND;
		ol->pIOBuf.sock = pSession->sock;
		ol->sock = pSession->sock;
		strncpy(ol->pIOBuf.szData,szBuf,dwBytesSend);

		WSABUF buf;
		buf.buf = ol->pIOBuf.szData;
		buf.len = dwBytesSend;

		DWORD dwBytes = 0, dwFlags = 0;
		int nRet = WSASend(pSession->sock,&buf,1,&dwBytes,dwFlags,ol,NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			pSession->Remove();
			ReleaseOverlapped(ol);
			return false;
		}

		return true;
	}

	return false;
}

void CLoginGateServer::Update()
{
	int nCount = 0;

	CSessionInfo*pSession = NULL;
	std::list<CSessionInfo*> tmplist;
	m_cs.Lock();
	for (std::list<CSessionInfo*>::iterator iter = m_xSessionList.begin(); iter != m_xSessionList.end(); ++iter)
	{
		pSession = *iter;
		if (pSession)
		{
			if (!pSession->IsRemove())
			{

			}
			else
			{
				m_xSessionList.erase(iter--);
				tmplist.push_back(pSession);
			}
		}
		else
		{
			m_xSessionList.erase(iter--);
		}
	}
	m_cs.UnLock();


	//删除
	for (std::list<CSessionInfo*>::iterator iter2 = tmplist.begin(); iter2!= tmplist.end(); ++iter2)
	{
		pSession = *iter2;
		if (pSession)
		{
			SAFE_DELETE(pSession);
		}
	}

	tmplist.clear();
}

bool CLoginGateServer::Stop()
{
	if(m_pListenSession->sock != INVALID_SOCKET)
	{
		//
		Update();
		KillAllSession();
		m_pConnector->Stop();

		SetEvent(m_hShutDown);		//信号

		for(int  i = 0; i < m_nCount; ++i)
		{
			PostQueuedCompletionStatus(m_hIocp,0,EXIT_CODE,NULL);
		}

		WaitForMultipleObjects(m_nCount,m_pThread,TRUE,INFINITE);

		WaitForSingleObject(m_pBackendThread,INFINITE);

		CloseHandle(m_pBackendThread);
		m_pBackendThread = INVALID_HANDLE_VALUE;

		SAFE_DELETE_ARRAY(m_pThread);
		CloseHandle(m_hIocp);
		m_hIocp = INVALID_HANDLE_VALUE;

		closesocket(m_pListenSession->sock);

		m_bTerminated = true;
	}

	return true;
}

void CLoginGateServer::KillAllSession()
{
	int nCount = 0;

	CSessionInfo*pSession = NULL;
	std::list<CSessionInfo*> tmplist;
	m_cs.Lock();
	for (std::list<CSessionInfo*>::iterator iter = m_xSessionList.begin(); iter != m_xSessionList.end(); ++iter)
	{
		pSession = *iter;
		if (pSession)
		{
			SAFE_DELETE(pSession);
		}
	}
	m_xSessionList.clear();
	m_cs.UnLock();
}

//发包
void CLoginGateServer::ProcessSendPacket()
{

}

void CLoginGateServer::ReleaseOverlapped( OVERLAPPEDEX *pOl )
{
	if (pOl)
	{
		memset(pOl,0,sizeof(OVERLAPPEDEX));
		m_pOverlappedPool->Free(pOl);
	}
}
