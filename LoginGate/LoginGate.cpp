#include "../Common/Connector.h"
#include "../Common/Log.h"
#include "../Common//MsWinsockUtil.h"

#include "LoginGate.h"

//tcp���ӳٷ��ͺͻ������0����������
extern void SetSocketProp( SOCKET socket );
//void _SetSocketProp( SOCKET socket )
//{
//	//tcp���ӳ�
//	int nodelay = 1;
//	setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
//
//	//���û������������
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
	// ����ǳ�ʱ�ˣ����ټ����Ȱ�  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// ȷ�Ͽͻ����Ƿ񻹻���...
		if( !_IsSocketAlive( cClient) )			//sendУ��
		{
			printf( "��⵽�ͻ����쳣�˳���\n");
			closesocket(cClient);
			cClient = INVALID_SOCKET;
			return true;
		}
		else
		{
			printf( "���������ʱ��������...\n");
			return true;
		}
	}  

	// �����ǿͻ����쳣�˳���
	else if( ERROR_NETNAME_DELETED==dwErr )		
	{
		printf( "��⵽�ͻ����쳣�˳���\n");			//�ͻ����ǹ�Xֱ���˳�
		closesocket(cClient);
		cClient = INVALID_SOCKET;
		return true;
	}
	else if (ERROR_CONNECTION_REFUSED == dwErr)
	{
		printf( "������δ����,�ܾ�����!\n");			//�ͻ����ǹ�Xֱ���˳�
		return true;
	}
	else
	{
		printf( "��ɶ˿ڲ������ִ����߳��˳���������룺%d\n",dwErr );
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
				//�˳�
				DWORD dwErr = GetLastError();

				if (dwErr == ERROR_CONNECTION_REFUSED)
				{
					//���Ӳ��Ϸ�����
					pConnector->ConnectFailed(dwErr);
				}
				else
				{
					if (pScoket != NULL)
					{
						// ��ʾһ����ʾ��Ϣ,�˳�����true
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
								//���ӳɹ�
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
				//�˳�
				DWORD dwErr = GetLastError();

				{
					if (pSession != NULL)
					{
						// ��ʾһ����ʾ��Ϣ,�˳�����true
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

	//����
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
	//��
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

	//����
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

	//�����߳̾��
	m_pThread = new HANDLE[m_nCount];


	//�����������߳�
	DWORD dwThreadId;
	for (int i = 0; i < m_nCount; ++i)
	{
		m_pThread[i] = CreateThread(NULL,0,Server_WorkThread,this,0,&dwThreadId);
	}

	m_pBackendThread = CreateThread(NULL,0,Send_WorkThread,this,0,&dwThreadId);
	//׼��
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
	//����
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

	//����ѷ������˵�iocontext���ܵ�����Ϣbuffer���ã�����m_lpfnGetAcceptExSockAddrsȥ��ȡ��һ����Ϣ
	//pIoContext->ResetBuffer();

	///////////////////////////////////////////////////////////////////////////
	// 1. ����ȡ������ͻ��˵ĵ�ַ��Ϣ
	// ��� m_lpfnGetAcceptExSockAddrs �����˰�~~~~~~
	// ��������ȡ�ÿͻ��˺ͱ��ض˵ĵ�ַ��Ϣ������˳��ȡ���ͻ��˷����ĵ�һ������
	MsWinsockUtil::m_lpfnGetAcceptExSockAddrs(pOl->pIOBuf.szData, DATA_BUFSIZE - ((sizeof(SOCKADDR_IN)+16)*2),  
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

#ifdef _DEBUG
	sLog.OutPutStr( "�ͻ��� %s:%d ����.", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port) );
	sLog.OutPutStr( "�ͻ��� %s:%d ��Ϣ��%s.",inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pOl->pIOBuf.szData);
#endif

	//�󶨵���ɶ˿����µĿͻ�������
	CSessionInfo *pNewSession = new CSessionInfo;
	pNewSession->sock = pOl->pIOBuf.sock;
	if (_AssociateWithIOCP(pNewSession)== false)
	{
		sLog.OutPutError("�½���socket ����ɶ˿�ʧ�ܣ�");
		SAFE_DELETE(pSession);
		return false;
	}

	m_cs.Lock();
	m_xSessionList.push_back(pNewSession);
	m_cs.UnLock();

	SetSocketProp(pNewSession->sock);

	//������Դ����һ������
	if (!RecvPost(pNewSession,pOl->pIOBuf.szData,dwBytes))
	{
		return false;
	}

	//��socket �ȴ���һ�����ݽ���
	if (!PostRecv(pNewSession))
	{
		return false;
	}

	//����Ͷ��
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
	// �����ںͿͻ���ͨ�ŵ�SOCKET�󶨵���ɶ˿���
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->sock, m_hIocp, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		sLog.OutPutError("ִ��CreateIoCompletionPort()���ִ���.������룺%d",GetLastError());
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


	//ɾ��
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

		SetEvent(m_hShutDown);		//�ź�

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

//����
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
