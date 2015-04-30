#include "TcpServer.h"

DWORD WINAPI cIocpServer::_WorkerThread( LPVOID lpParam )
{
	cIocpServer *pServer = (cIocpServer*)lpParam;
	if (pServer)
	{
		//
		PerIoContext *pIoContext = NULL;
		PerSocketSession *pSession = NULL;

		DWORD dwBytes = 0;

		//ѭ����������ֱ���յ�shutdown��Ϣ
		while (WAIT_OBJECT_0 != WaitForSingleObject(pServer->m_hShutdownEvent,0))
		{
			BOOL bRet = GetQueuedCompletionStatus(pServer->m_hIOCompletionPort,&dwBytes,(PULONG_PTR)&pSession,(LPOVERLAPPED*)&pIoContext,INFINITE);
			
			if (pSession == EXIT_CODE)
				break;					//�������

			if (bRet == FALSE || pIoContext == NULL || pSession == NULL)
			{
				//�˳�
				DWORD dwErr = GetLastError();

				if (pSession != NULL)
				{
					// ��ʾһ����ʾ��Ϣ,�˳�����true
					if( !pServer->HandleError( pSession,dwErr ) )			
					{
						break;
					}

				}

				continue;  
			}
			else
			{
				// �ж��Ƿ��пͻ��˶Ͽ���
				if ((dwBytes == 0) && (pIoContext->requestType == IOCP_REQUEST_READ || pIoContext->requestType == IOCP_REQUEST_WRITE))
				{
					//�ͻ��˵�����
					pServer->_ShowMessage("�ͻ��� %s:%d �Ͽ�����.",inet_ntoa(pSession->addr.sin_addr), ntohs(pSession->addr.sin_port) );
					
					//ɾ�������ڴ��е�pool
					//SOCKETPOOL->ReleasePerSocketSession(pSession);
					pServer->_CloseSocket(pSession->socket);		//�ر�socket
					continue;
				}
				else
				{
					switch(pIoContext->requestType)
					{
					case IOCP_REQUEST_ACCEPT:
						//����accept
						pServer->_DoAccpet(pSession,pIoContext,dwBytes);
						break;
					case IOCP_REQUEST_READ:
						//����read
						pServer->_DoRecv(pSession,pIoContext,dwBytes);
						break;
					case IOCP_REQUEST_WRITE:
						//����recv
						pServer->_DoSend(pSession,pIoContext);
						break;
					case IOCP_REQUEST_CLOSE:
						pServer->_DoClose(pSession,pIoContext);
						break;
					}
				}
			}
		}
	}

	return 0;
}

DWORD WINAPI cIocpServer::_BackEndThread( LPVOID lpParam )
{
	cIocpServer *pServer = (cIocpServer*)lpParam;
	if (pServer)
	{

		while(WAIT_OBJECT_0 != WaitForSingleObject(pServer->m_hShutdownEvent,0))
		{
			//timeout
			pServer->_IoSend();

			//recv
			pServer->_IoRecv();

			Sleep(10);			
		}
	}

	return 0;
}




cIocpServer::cIocpServer()
{
	m_nThreads = 0;
	m_phWorkerThreads = NULL;

	m_strIP = DEFAULT_IP;
	m_nPort = DEFAULT_PORT;

	m_hIOCompletionPort = NULL;		//io�˿�
	m_hShutdownEvent = NULL;
	m_lpfnAcceptEx = NULL;			//acceptEx;
	m_pListenContext = NULL;		//����������
	
	m_hBackendThread = NULL;

}

cIocpServer::~cIocpServer()
{
	//�ͷ���Դ
	Stop();
}


//====================================================================================
//				    ������Ϣ����
//====================================================================================
void cIocpServer::_ShowMessage( char*szFormat,... )
{
	va_list ap;

	char szText[1024] = {0};
	va_start(ap,szFormat);
	vsprintf(szText,szFormat,ap);
	va_end(ap);

	//���������Ϣ
	printf("%s",szText);		//��ʱ��������
}

//====================================================================================
//
//				    IOCP��ʼ������ֹ
//
//====================================================================================

bool cIocpServer::LoadSocketLib()
{
	WSADATA wd;
	int nResult;

	nResult = WSAStartup(MAKEWORD(2,2),&wd);

	if(nResult != NO_ERROR)
	{
		_ShowMessage("Init socket envirment error!");		//��ʼ��socket����
		return false;
	}

	return true;
}

//��ȡϵͳcpu
int cIocpServer::_GetNoOfProcessors()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

//��ʼ����ɶ˿�
bool cIocpServer::_InitializeIOCP()
{
	//������ɶ˿�
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,0);
	if (m_hIOCompletionPort == NULL)
	{
		_ShowMessage("call CreateIoCompletionPort failed");
	}

	//�����߳�����
	m_nThreads = 2 * _GetNoOfProcessors();	

	//�����߳̾��
	m_phWorkerThreads = new HANDLE[m_nThreads];


	//�����������߳�
	DWORD dwThreadId;
	for (int i = 0; i < m_nThreads; ++i)
	{
		m_phWorkerThreads[i] = CreateThread(NULL,0,_WorkerThread,this,0,&dwThreadId);
	}

	_ShowMessage("iocp has success created!\n");

	return true;
}

//��ʼ��������socket
bool cIocpServer::_InitializeListenSocket()
{
	// AcceptEx �� GetAcceptExSockaddrs ��GUID�����ڵ�������ָ��
	GUID GuidAcceptEx = WSAID_ACCEPTEX;  
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS; 

	//������socket����
	SOCKADDR_IN serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(m_nPort);
	serAddr.sin_addr.s_addr = m_strIP.empty() ? htonl(INADDR_ANY) : inet_addr(m_strIP.c_str());


	m_pListenContext = SOCKETPOOL->GetPerSocketSession();
	if (m_pListenContext)
	{
		// ��Ҫʹ���ص�IO�������ʹ��WSASocket������Socket���ſ���֧���ص�IO����
		m_pListenContext->socket = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);

		if (m_pListenContext->socket == INVALID_SOCKET)
		{
			_ShowMessage("init listen socket error : %d!\n",WSAGetLastError());
			return false;
		}

		//// ��Listen Socket������ɶ˿���
		if (CreateIoCompletionPort((HANDLE)m_pListenContext->socket,m_hIOCompletionPort,(DWORD)m_pListenContext,0) == NULL)
		{
			_ShowMessage("�� Listen Socket����ɶ˿�ʧ�ܣ��������: %d\n", WSAGetLastError());  
			RELEASE_SOCKET( m_pListenContext->socket );
			return false;
		}

		//bind
		int ret = bind(m_pListenContext->socket, (struct sockaddr *) &serAddr, sizeof(serAddr));
		if (ret == SOCKET_ERROR)
		{
			this->_ShowMessage("bind()����ִ�д���.\n");  
			RELEASE_SOCKET( m_pListenContext->socket );
			return false;
		}


		// ��ʼ���м���
		if (SOCKET_ERROR == listen(m_pListenContext->socket,SOMAXCONN))
		{
			this->_ShowMessage("Listen()����ִ�г��ִ���.\n");
			return false;
		}

		_SetSocketProp(m_pListenContext->socket);


		// ʹ��AcceptEx��������Ϊ���������WinSock2�淶֮���΢�������ṩ����չ����
		// ������Ҫ�����ȡһ�º�����ָ�룬
		// ��ȡAcceptEx����ָ��
		DWORD dwBytes = 0;  
		if(SOCKET_ERROR == WSAIoctl(
			m_pListenContext->socket, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&GuidAcceptEx, 
			sizeof(GuidAcceptEx), 
			&m_lpfnAcceptEx, 
			sizeof(m_lpfnAcceptEx), 
			&dwBytes, 
			NULL, 
			NULL))  
		{  
			_ShowMessage("WSAIoctl δ�ܻ�ȡAcceptEx����ָ�롣�������: %d\n", WSAGetLastError()); 
			_DeInitialize();
			return false;  
		}  

		// ��ȡGetAcceptExSockAddrs����ָ�룬Ҳ��ͬ��
		if(SOCKET_ERROR == WSAIoctl(
			m_pListenContext->socket, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&GuidGetAcceptExSockAddrs,
			sizeof(GuidGetAcceptExSockAddrs), 
			&m_lpfnGetAcceptExSockAddrs, 
			sizeof(m_lpfnGetAcceptExSockAddrs),   
			&dwBytes, 
			NULL, 
			NULL))  
		{  
			_ShowMessage("WSAIoctl δ�ܻ�ȡGuidGetAcceptExSockAddrs����ָ�롣�������: %d\n", WSAGetLastError());  
			_DeInitialize();
			return false; 
		}  


		// ΪAcceptEx ׼��������Ȼ��Ͷ��AcceptEx I/O����Ͷ��accept
		for( int i=0;i<MAX_POST_ACCEPT;i++ )
		{
			// �½�һ��IO_CONTEXT
			PerIoContext* pAcceptIoContext  = IOPOOL->GetIoContext(m_pListenContext->cid);

			if (pAcceptIoContext)
			{
				m_pListenContext->AddIoContext(pAcceptIoContext);			//10�������ظ�����

				if( false==this->_PostAccept( pAcceptIoContext ) )
				{
					m_pListenContext->RemoveIoContext(pAcceptIoContext);
					_CloseSocket(pAcceptIoContext->socket);		//�ر�socket
					return false;
				}
			}
			
		}

		_ShowMessage( "Ͷ�� %d ��AcceptEx�������",MAX_POST_ACCEPT );

		
		return true;
	}

	return false;
}

void cIocpServer::_DeInitialize()
{
	// �ر�ϵͳ�˳��¼����
	RELEASE_HANDLE(m_hShutdownEvent);

	// �ͷŹ������߳̾��ָ��
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}

	SAFE_DELETE(m_phWorkerThreads);

	RELEASE_HANDLE(m_hBackendThread);

	// �ر�IOCP���
	RELEASE_HANDLE(m_hIOCompletionPort);

	// �رռ���Socket
	//SAFE_DELETE(m_pListenContext);
	SOCKETPOOL->ReleasePerSocketSession(m_pListenContext);

	m_pListenContext = NULL;

	_ShowMessage("�ͷ���Դ���.\n");
}

/*******************
�ͻ��˻�ȡ�˿ڰ󶨵���ɶ˿���

*********************/
bool cIocpServer::_AssociateWithIOCP( PerSocketSession *pContext )
{
	// �����ںͿͻ���ͨ�ŵ�SOCKET�󶨵���ɶ˿���
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->socket, m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		this->_ShowMessage("ִ��CreateIoCompletionPort()���ִ���.������룺%d",GetLastError());
		return false;
	}

	return true;
}


bool cIocpServer::_PostAccept( PerIoContext* pAcceptIoContext )
{
	if (pAcceptIoContext == NULL)
		return false;
	else
	{
		pAcceptIoContext->requestType = IOCP_REQUEST_ACCEPT;
		pAcceptIoContext->socket = INVALID_SOCKET;
		pAcceptIoContext->ResetBuffer();

		WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
		OVERLAPPED *p_ol = &pAcceptIoContext->wsaOverlapped;

		DWORD dwBytes = 0;

		// Ϊ�Ժ�������Ŀͻ�����׼����Socket( ������봫ͳaccept�������� ) 
		pAcceptIoContext->socket  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
		if( INVALID_SOCKET==pAcceptIoContext->socket )  
		{  
			_ShowMessage("��������Accept��Socketʧ�ܣ��������: %d", WSAGetLastError()); 
			return false;  
		} 
		// Ͷ��AcceptEx
		if(FALSE == m_lpfnAcceptEx( m_pListenContext->socket, pAcceptIoContext->socket, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
			sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  
		{  
			if(WSA_IO_PENDING != WSAGetLastError())  
			{  
				_ShowMessage("Ͷ�� AcceptEx ����ʧ�ܣ��������: %d", WSAGetLastError());  
				return false;  
			}  
		} 


		return true;
	}
}


////////////////////////////////////////////////////////////
// ���пͻ��������ʱ�򣬽��д���

// ��֮��Ҫ֪�����������ListenSocket��Context��������Ҫ����һ�ݳ������������Socket��
// ԭ����Context����Ҫ���������Ͷ����һ��Accept����
// ����ɶ˿ڽ��ܵ�accpet��Ϣʱ��GetAcceptExSockaddrsͬʱ��ȡ����һ����Ϣ
bool cIocpServer::_DoAccpet( PerSocketSession* pSocketContext, PerIoContext* pIoContext,DWORD dwBytes )
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
	this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),  
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

	this->_ShowMessage( "�ͻ��� %s:%d ����.", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port) );
	this->_ShowMessage( "�ͻ��� %s:%d ��Ϣ��%s.",inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pIoContext->m_wsaBuf.buf );


	//�󶨵���ɶ˿����µĿͻ�������
	PerSocketSession *pSession = SOCKETPOOL->GetPerSocketSession();
	pSession->socket = pIoContext->socket;
	pSession->addr = *ClientAddr;		

	//�󶨵���ɶ��ϡ�
	if (_AssociateWithIOCP(pSession)== false)
	{
		_ShowMessage("�½���socket ����ɶ˿�ʧ�ܣ�");
	}

	_SetSocketProp(pSession->socket);

	//������Դ����һ������
	_RecvPost(pSession,pIoContext,dwBytes);			//���͸���̨�߳�

	//��socket �ȴ�����
	PerIoContext *pNewIoContext = IOPOOL->GetIoContext(pSession->socket,IOCP_REQUEST_READ,pSession->cid,*ClientAddr); 
	if (pNewIoContext)
	{
		//��ӵ�pNewIoContext��
		pSession->AddIoContext(pIoContext);
		if (_PostRecv(pNewIoContext) == false)
		{
			pSession->RemoveIoContext(pNewIoContext);

			_CloseSocket(pNewIoContext->socket);		//�ر�socket
			return false;
		}
	}


	//����Ͷ��
	return _PostAccept(pIoContext);
}

//tcp���ӳٷ��ͺͻ������0����������
void cIocpServer::_SetSocketProp( SOCKET socket )
{
	//tcp���ӳ�
	int nodelay = 1;
	setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

	//���û������������
	int zero = 0;
	setsockopt(socket,SOL_SOCKET,SO_SNDBUF,(char*)&zero,sizeof(zero));

	zero = 0;
	setsockopt(socket,SOL_SOCKET,SO_RCVBUF,(char*)&zero,sizeof(zero));
}

bool cIocpServer::_PostRecv(PerIoContext *pIoContext)
{
	//������Ϣ
	if (pIoContext == NULL)
		return false;
	else
	{
		pIoContext->ResetBuffer();

		pIoContext->requestType = IOCP_REQUEST_READ;

		WSABUF *pBuf = &pIoContext->m_wsaBuf;
		OVERLAPPED *pOl = &pIoContext->wsaOverlapped;
		DWORD dwBytes = 0;
		DWORD dwFlags = 0;

		int bRet =WSARecv(pIoContext->socket,pBuf,1,&dwBytes,&dwFlags,pOl,NULL);
		if (bRet == SOCKET_ERROR  && WSAGetLastError() != WSA_IO_PENDING)
		{
			this->_ShowMessage("Ͷ��һ��WSARecvʧ�ܣ�");
			return false;
		}
		return true;
	}
}

//�����������
bool cIocpServer::_DoRecv( PerSocketSession* pSocketContext, PerIoContext* pIoContext ,DWORD dwBytes)
{
	if (pSocketContext == NULL || pIoContext == NULL)
		return false;
	else
	{
		//���ܵ���Ϣ
		//������Դ����һ������
		_RecvPost(pSocketContext,pIoContext,dwBytes);			//���͸���̨�߳�

		//�������Ͷ��
		return _PostRecv(pIoContext);		//�����ȴ�����
	}
}

//====================================================================================
//
//				       ����������������
//
//====================================================================================


std::string cIocpServer::GetLocalIP()
{
	// ��ñ���������
	char hostname[MAX_PATH] = {0};
	gethostname(hostname,MAX_PATH);                
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if(lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	// ȡ��IP��ַ�б��еĵ�һ��Ϊ���ص�IP(��Ϊһ̨�������ܻ�󶨶��IP)
	char* lpAddr = lpHostEnt->h_addr_list[0];      

	// ��IP��ַת�����ַ�����ʽ
	struct in_addr inAddr;
	memmove(&inAddr,lpAddr,4);

	m_strIP = inet_ntoa(inAddr);        

	return m_strIP;
}


/////////////////////////////////////////////////////////////////////
// �жϿͻ���Socket�Ƿ��Ѿ��Ͽ���������һ����Ч��Socket��Ͷ��WSARecv����������쳣
// ʹ�õķ����ǳ��������socket�������ݣ��ж����socket���õķ���ֵ
// ��Ϊ����ͻ��������쳣�Ͽ�(����ͻ��˱������߰ε����ߵ�)��ʱ�򣬷����������޷��յ��ͻ��˶Ͽ���֪ͨ��
bool cIocpServer::_IsSocketAlive( SOCKET s )
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}


///////////////////////////////////////////////////////////////////
// ��ʾ��������ɶ˿��ϵĴ���
bool cIocpServer::HandleError( PerSocketSession *pContext,const DWORD& dwErr )
{
	// ����ǳ�ʱ�ˣ����ټ����Ȱ�  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// ȷ�Ͽͻ����Ƿ񻹻���...
		if( !_IsSocketAlive( pContext->socket) )			//sendУ��
		{
			_ShowMessage( "��⵽�ͻ����쳣�˳���");
			SOCKETPOOL->ReleasePerSocketSession(pContext);
			return true;
		}
		else
		{
			this->_ShowMessage( "���������ʱ��������...");
			return true;
		}
	}  

	// �����ǿͻ����쳣�˳���
	else if( ERROR_NETNAME_DELETED==dwErr )		
	{
		this->_ShowMessage( "��⵽�ͻ����쳣�˳���");			//�ͻ����ǹ�Xֱ���˳�
		SOCKETPOOL->ReleasePerSocketSession(pContext);
		return true;
	}

	else
	{
		this->_ShowMessage( "��ɶ˿ڲ������ִ����߳��˳���������룺%d",dwErr );
		return false;
	}
}


//////////////////////////////////////////////////////////////////
//	����������
bool cIocpServer::Start()
{
	//socket����
	if (false == LoadSocketLib())
	{
		this->_ShowMessage("��ʼ��socket ����ʧ�ܣ�\n");
		return false;
	}

	// ����ϵͳ�˳����¼�֪ͨ,�ֶ����ã��տ�ʼ���ź�
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// ��ʼ��IOCP
	if (false == _InitializeIOCP())
	{
		this->_ShowMessage("��ʼ��IOCPʧ�ܣ�\n");
		return false;
	}
	else
	{
		this->_ShowMessage("IOCP��ʼ�����\n.");
	}

	// ��ʼ��Socket
	if( false==_InitializeListenSocket() )
	{
		this->_ShowMessage("Listen Socket��ʼ��ʧ�ܣ�\n");
		this->_DeInitialize();
		return false;
	}
	else
	{
		this->_ShowMessage("Listen Socket��ʼ�����.\n");
	}

	//��ʼ����̨�߳�
	if (false == _InitializeBackendThread())
	{
		this->_ShowMessage("��ʼ����̨�߳�ʧ��\n");
	}
	else
	{
		this->_ShowMessage("��ʼ����̨�̳߳ɹ�\n");
	}
	this->_ShowMessage("ϵͳ׼���������Ⱥ�����....\n");

	return true;
}

void cIocpServer::Stop()
{
	if (m_pListenContext != NULL && m_pListenContext->socket != INVALID_SOCKET)
	{
		//����ر��߳�
		SetEvent(m_hShutdownEvent); //

		//֪ͨ����workthread�ر�
		for (int i = 0; i < m_nThreads;++i)
		{
			PostQueuedCompletionStatus(m_hIOCompletionPort,0,(DWORD)EXIT_CODE,NULL);
		}

		//�ȴ������߳��˳�
		WaitForMultipleObjects(m_nThreads,m_phWorkerThreads,TRUE,INFINITE);

		//��̨�̳߳ɹ�
		WaitForSingleObject(m_hBackendThread,INFINITE);

		//�����shutdown
		m_IoSendBackList.clear();
		m_IoSendFontList.clear();
		IOPOOL->Shutdown();
		SOCKETPOOL->Shutdown();

		_DeInitialize();

		this->_ShowMessage("ֹͣ����\n");
	}

	UnloadSocketLib();
}

//��������
bool cIocpServer::_PostSend( PerIoContext *pIoContext )
{
	if (pIoContext != NULL && pIoContext->socket != INVALID_SOCKET)
	{
		DWORD dwBytes = 0;
		DWORD dwFlags = 0;
		WSABUF *pWsaBuf = &pIoContext->m_wsaBuf;
		OVERLAPPED *ol = &pIoContext->wsaOverlapped;

		int ret = WSASend(pIoContext->socket,pWsaBuf,1,&dwBytes,dwFlags,ol,NULL);
		if (ret == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError() )
		{
			_ShowMessage("������Ϣʧ�ܣ�");
			return false;
		}

		return true;
	}
	else
		return false;
}


bool cIocpServer::_DoSend( PerSocketSession * pSocketContext, PerIoContext * pIoContext )
{
	//���Ͷ���IOPOOL�ɷ�
	IOPOOL->ReleaseIoContext(pIoContext);

	return true;
}

void cIocpServer::SendPacket(DWORD cid,BYTE bCmdGroup,BYTE bCmd,char * buffer, int buflength)
{
	if (cid == 0 || buffer == NULL)
		return;
	else
	{
		//int length = sizeof(Packet) + buflength - 1;		//�ܰ���

		//if (length >=  MAX_PACKET -1)
		//{
		//	_ShowMessage("����������");
		//}
		//else
		//{
		//	PerSocketSession * pSession = SOCKETPOOL->GetCID(cid);
		//	if(pSession)
		//	{
		//		PerIoContext *pIoContext = IOPOOL->GetIoContext(pSession->socket,IOCP_REQUEST_WRITE,cid);
		//		if (pIoContext)
		//		{
	
		//			BuildPacketSize(bCmdGroup,bCmd,pIoContext->buffer,buflength);

		//			memcpy(pIoContext->buffer+6,buffer,buflength);		//ע��һ��,��ͷ+googlebuf

		//			_SendPost(pIoContext);
		//		}
		//	}
		//}

		
	}

}

void cIocpServer::_SendPost( PerIoContext *pIoContext )
{
	CAutoLock cs(&m_csSendContext);
	m_IoSendBackList.push_back(pIoContext);
}

void cIocpServer::_IoSend()
{
	//˫����
	list<PerIoContext*> tmp;
	//EnterCriticalSection(&m_csSendContext);
	if (true)
	{
		CAutoLock cs(&m_csSendContext);
		tmp = m_IoSendFontList;
		m_IoSendFontList = m_IoSendBackList;
		m_IoSendBackList = tmp;
	}
	//LeaveCriticalSection(&m_csSendContext);

	//����
	while(!m_IoSendFontList.empty())
	{
		//
		PerIoContext *pTmpContext = m_IoSendFontList.front();
		m_IoSendFontList.pop_front();

		if (false == _PostSend(pTmpContext))
		{
			IOPOOL->ReleaseIoContext(pTmpContext);
			_CloseSocket(pTmpContext->socket);
		}
	}

	m_IoSendFontList.clear();
}

bool cIocpServer::_InitializeBackendThread()
{
	DWORD dwThread;
	m_hBackendThread = CreateThread(NULL,0,_BackEndThread,this,0,&dwThread);

	if (m_hBackendThread)
	{
		return true;
	}
	else
		return false;
}

void cIocpServer::_RecvPost(PerSocketSession* pSocketContext, PerIoContext *pIoContext ,DWORD dwBytes)
{
	if (pIoContext == NULL || pSocketContext == NULL)
		return;
	else
	{
		//PacketEx *pPacketEx = new PacketEx();
		//pPacketEx->cid = pSocketContext->cid;
		//pPacketEx->pPacket = (Packet *)malloc(dwBytes);		//��Ϣ
		//memcpy(pPacketEx->pPacket,pIoContext->buffer,dwBytes);

		////CAutoLock cs(&m_csRecvContext);
		////����
		//pthread_mutex_lock(&m_csRecvContext);
		//m_IoRecvBackList.push_back(pPacketEx);
		//pthread_mutex_unlock(&m_csRecvContext);
	}
}

void cIocpServer::_IoRecv()
{
	////˫����
	//list<PacketEx*> tmp;
	////EnterCriticalSection(&m_csRecvContext);
	//pthread_mutex_lock(&m_csRecvContext);
	//{
	//	tmp = m_IoRecvFontList;
	//	m_IoRecvFontList = m_IoRecvBackList;
	//	m_IoRecvBackList = tmp;
	//}
	//pthread_mutex_unlock(&m_csRecvContext);
	////LeaveCriticalSection(&m_csRecvContext);

	////����
	//while(!m_IoRecvFontList.empty())
	//{
	//	//

	//	//����PacketEx,���Լ���ش���packet���ͷţ�
	//	PacketEx *pPacketEx = m_IoRecvFontList.front();
	//	m_IoRecvFontList.pop_front();

	//	if (pPacketEx)
	//	{
	//		if (BatchComplete(pPacketEx->cid,pPacketEx->pPacket) == false)
	//		{
	//			//������������ع������̹߳رո�cid
	//			PerSocketSession * pSession = SOCKETPOOL->GetCID(pPacketEx->cid);
	//			_RequestCloseSocket(pSession);
	//		}
	//	}

	//	//�ͷţ�ɾ���ڴ�
	//	SAFE_DELETE(pPacketEx);
	//}

	//m_IoSendFontList.clear();
}

//�������󣬹رո�socket
void cIocpServer::_RequestCloseSocket( PerSocketSession *pSession )
{
	if (pSession == NULL)
	{
		return;
	}
	else
	{
		PerIoContext *pIoContext = IOPOOL->GetIoContext(pSession->socket,IOCP_REQUEST_CLOSE,pSession->cid);
		if(pIoContext)
		{
			//
			strcpy(pIoContext->buffer,"close");		//�ر�

			PostQueuedCompletionStatus(m_hIOCompletionPort,strlen(pIoContext->buffer),(DWORD)pSession,&pIoContext->wsaOverlapped);
		}
	}
}

bool cIocpServer::_DoClose( PerSocketSession *pSocketContext,PerIoContext *pIoContext )
{
	if (pSocketContext == NULL || pIoContext == NULL)
		return false;
	else
	{
		//
		IOPOOL->ReleaseIoContext(pIoContext);

		//
		_CloseSocket(pSocketContext->socket);
		//SOCKETPOOL->ReleasePerSocketSession(pSocketContext);

		return true;
	}
}


bool cIocpServer::BatchComplete(DWORD cid,char *pPacket)
{
	return false;
}

void cIocpServer::_CloseSocket( SOCKET s )
{
	if (s != INVALID_SOCKET)
	{
		closesocket(s);
	}
}
