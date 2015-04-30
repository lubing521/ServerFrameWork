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

		//循环处理请求，直到收到shutdown消息
		while (WAIT_OBJECT_0 != WaitForSingleObject(pServer->m_hShutdownEvent,0))
		{
			BOOL bRet = GetQueuedCompletionStatus(pServer->m_hIOCompletionPort,&dwBytes,(PULONG_PTR)&pSession,(LPOVERLAPPED*)&pIoContext,INFINITE);
			
			if (pSession == EXIT_CODE)
				break;					//程序结束

			if (bRet == FALSE || pIoContext == NULL || pSession == NULL)
			{
				//退出
				DWORD dwErr = GetLastError();

				if (pSession != NULL)
				{
					// 显示一下提示信息,退出返回true
					if( !pServer->HandleError( pSession,dwErr ) )			
					{
						break;
					}

				}

				continue;  
			}
			else
			{
				// 判断是否有客户端断开了
				if ((dwBytes == 0) && (pIoContext->requestType == IOCP_REQUEST_READ || pIoContext->requestType == IOCP_REQUEST_WRITE))
				{
					//客户端掉线了
					pServer->_ShowMessage("客户端 %s:%d 断开连接.",inet_ntoa(pSession->addr.sin_addr), ntohs(pSession->addr.sin_port) );
					
					//删除所有内存中的pool
					//SOCKETPOOL->ReleasePerSocketSession(pSession);
					pServer->_CloseSocket(pSession->socket);		//关闭socket
					continue;
				}
				else
				{
					switch(pIoContext->requestType)
					{
					case IOCP_REQUEST_ACCEPT:
						//处理accept
						pServer->_DoAccpet(pSession,pIoContext,dwBytes);
						break;
					case IOCP_REQUEST_READ:
						//处理read
						pServer->_DoRecv(pSession,pIoContext,dwBytes);
						break;
					case IOCP_REQUEST_WRITE:
						//处理recv
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

	m_hIOCompletionPort = NULL;		//io端口
	m_hShutdownEvent = NULL;
	m_lpfnAcceptEx = NULL;			//acceptEx;
	m_pListenContext = NULL;		//服务器监听
	
	m_hBackendThread = NULL;

}

cIocpServer::~cIocpServer()
{
	//释放资源
	Stop();
}


//====================================================================================
//				    网络消息处理
//====================================================================================
void cIocpServer::_ShowMessage( char*szFormat,... )
{
	va_list ap;

	char szText[1024] = {0};
	va_start(ap,szFormat);
	vsprintf(szText,szFormat,ap);
	va_end(ap);

	//处理输出消息
	printf("%s",szText);		//暂时这样处理
}

//====================================================================================
//
//				    IOCP初始化和终止
//
//====================================================================================

bool cIocpServer::LoadSocketLib()
{
	WSADATA wd;
	int nResult;

	nResult = WSAStartup(MAKEWORD(2,2),&wd);

	if(nResult != NO_ERROR)
	{
		_ShowMessage("Init socket envirment error!");		//初始化socket环境
		return false;
	}

	return true;
}

//获取系统cpu
int cIocpServer::_GetNoOfProcessors()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

//初始化完成端口
bool cIocpServer::_InitializeIOCP()
{
	//建立完成端口
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,0);
	if (m_hIOCompletionPort == NULL)
	{
		_ShowMessage("call CreateIoCompletionPort failed");
	}

	//工作线程数量
	m_nThreads = 2 * _GetNoOfProcessors();	

	//工作线程句柄
	m_phWorkerThreads = new HANDLE[m_nThreads];


	//建立工作者线程
	DWORD dwThreadId;
	for (int i = 0; i < m_nThreads; ++i)
	{
		m_phWorkerThreads[i] = CreateThread(NULL,0,_WorkerThread,this,0,&dwThreadId);
	}

	_ShowMessage("iocp has success created!\n");

	return true;
}

//初始化服务器socket
bool cIocpServer::_InitializeListenSocket()
{
	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidAcceptEx = WSAID_ACCEPTEX;  
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS; 

	//服务器socket创建
	SOCKADDR_IN serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(m_nPort);
	serAddr.sin_addr.s_addr = m_strIP.empty() ? htonl(INADDR_ANY) : inet_addr(m_strIP.c_str());


	m_pListenContext = SOCKETPOOL->GetPerSocketSession();
	if (m_pListenContext)
	{
		// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
		m_pListenContext->socket = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);

		if (m_pListenContext->socket == INVALID_SOCKET)
		{
			_ShowMessage("init listen socket error : %d!\n",WSAGetLastError());
			return false;
		}

		//// 将Listen Socket绑定至完成端口中
		if (CreateIoCompletionPort((HANDLE)m_pListenContext->socket,m_hIOCompletionPort,(DWORD)m_pListenContext,0) == NULL)
		{
			_ShowMessage("绑定 Listen Socket至完成端口失败！错误代码: %d\n", WSAGetLastError());  
			RELEASE_SOCKET( m_pListenContext->socket );
			return false;
		}

		//bind
		int ret = bind(m_pListenContext->socket, (struct sockaddr *) &serAddr, sizeof(serAddr));
		if (ret == SOCKET_ERROR)
		{
			this->_ShowMessage("bind()函数执行错误.\n");  
			RELEASE_SOCKET( m_pListenContext->socket );
			return false;
		}


		// 开始进行监听
		if (SOCKET_ERROR == listen(m_pListenContext->socket,SOMAXCONN))
		{
			this->_ShowMessage("Listen()函数执行出现错误.\n");
			return false;
		}

		_SetSocketProp(m_pListenContext->socket);


		// 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
		// 所以需要额外获取一下函数的指针，
		// 获取AcceptEx函数指针
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
			_ShowMessage("WSAIoctl 未能获取AcceptEx函数指针。错误代码: %d\n", WSAGetLastError()); 
			_DeInitialize();
			return false;  
		}  

		// 获取GetAcceptExSockAddrs函数指针，也是同理
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
			_ShowMessage("WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针。错误代码: %d\n", WSAGetLastError());  
			_DeInitialize();
			return false; 
		}  


		// 为AcceptEx 准备参数，然后投递AcceptEx I/O请求，投递accept
		for( int i=0;i<MAX_POST_ACCEPT;i++ )
		{
			// 新建一个IO_CONTEXT
			PerIoContext* pAcceptIoContext  = IOPOOL->GetIoContext(m_pListenContext->cid);

			if (pAcceptIoContext)
			{
				m_pListenContext->AddIoContext(pAcceptIoContext);			//10个请求，重复利用

				if( false==this->_PostAccept( pAcceptIoContext ) )
				{
					m_pListenContext->RemoveIoContext(pAcceptIoContext);
					_CloseSocket(pAcceptIoContext->socket);		//关闭socket
					return false;
				}
			}
			
		}

		_ShowMessage( "投递 %d 个AcceptEx请求完毕",MAX_POST_ACCEPT );

		
		return true;
	}

	return false;
}

void cIocpServer::_DeInitialize()
{
	// 关闭系统退出事件句柄
	RELEASE_HANDLE(m_hShutdownEvent);

	// 释放工作者线程句柄指针
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}

	SAFE_DELETE(m_phWorkerThreads);

	RELEASE_HANDLE(m_hBackendThread);

	// 关闭IOCP句柄
	RELEASE_HANDLE(m_hIOCompletionPort);

	// 关闭监听Socket
	//SAFE_DELETE(m_pListenContext);
	SOCKETPOOL->ReleasePerSocketSession(m_pListenContext);

	m_pListenContext = NULL;

	_ShowMessage("释放资源完毕.\n");
}

/*******************
客户端获取端口绑定到完成端口上

*********************/
bool cIocpServer::_AssociateWithIOCP( PerSocketSession *pContext )
{
	// 将用于和客户端通信的SOCKET绑定到完成端口中
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->socket, m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		this->_ShowMessage("执行CreateIoCompletionPort()出现错误.错误代码：%d",GetLastError());
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

		// 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 ) 
		pAcceptIoContext->socket  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
		if( INVALID_SOCKET==pAcceptIoContext->socket )  
		{  
			_ShowMessage("创建用于Accept的Socket失败！错误代码: %d", WSAGetLastError()); 
			return false;  
		} 
		// 投递AcceptEx
		if(FALSE == m_lpfnAcceptEx( m_pListenContext->socket, pAcceptIoContext->socket, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
			sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  
		{  
			if(WSA_IO_PENDING != WSAGetLastError())  
			{  
				_ShowMessage("投递 AcceptEx 请求失败，错误代码: %d", WSAGetLastError());  
				return false;  
			}  
		} 


		return true;
	}
}


////////////////////////////////////////////////////////////
// 在有客户端连入的时候，进行处理

// 总之你要知道，传入的是ListenSocket的Context，我们需要复制一份出来给新连入的Socket用
// 原来的Context还是要在上面继续投递下一个Accept请求
// 当完成端口接受到accpet消息时，GetAcceptExSockaddrs同时获取到第一次消息
bool cIocpServer::_DoAccpet( PerSocketSession* pSocketContext, PerIoContext* pIoContext,DWORD dwBytes )
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
	this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),  
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

	this->_ShowMessage( "客户端 %s:%d 连入.", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port) );
	this->_ShowMessage( "客户额 %s:%d 信息：%s.",inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pIoContext->m_wsaBuf.buf );


	//绑定到完成端口上新的客户端连接
	PerSocketSession *pSession = SOCKETPOOL->GetPerSocketSession();
	pSession->socket = pIoContext->socket;
	pSession->addr = *ClientAddr;		

	//绑定到完成端上。
	if (_AssociateWithIOCP(pSession)== false)
	{
		_ShowMessage("新接入socket 绑定完成端口失败！");
	}

	_SetSocketProp(pSession->socket);

	//这里可以处理第一组数据
	_RecvPost(pSession,pIoContext,dwBytes);			//发送给后台线程

	//该socket 等待接受
	PerIoContext *pNewIoContext = IOPOOL->GetIoContext(pSession->socket,IOCP_REQUEST_READ,pSession->cid,*ClientAddr); 
	if (pNewIoContext)
	{
		//添加到pNewIoContext中
		pSession->AddIoContext(pIoContext);
		if (_PostRecv(pNewIoContext) == false)
		{
			pSession->RemoveIoContext(pNewIoContext);

			_CloseSocket(pNewIoContext->socket);		//关闭socket
			return false;
		}
	}


	//继续投递
	return _PostAccept(pIoContext);
}

//tcp无延迟发送和缓存池设0，立即发送
void cIocpServer::_SetSocketProp( SOCKET socket )
{
	//tcp无延迟
	int nodelay = 1;
	setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

	//设置缓冲池立即发送
	int zero = 0;
	setsockopt(socket,SOL_SOCKET,SO_SNDBUF,(char*)&zero,sizeof(zero));

	zero = 0;
	setsockopt(socket,SOL_SOCKET,SO_RCVBUF,(char*)&zero,sizeof(zero));
}

bool cIocpServer::_PostRecv(PerIoContext *pIoContext)
{
	//接受消息
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
			this->_ShowMessage("投递一个WSARecv失败！");
			return false;
		}
		return true;
	}
}

//处理接收数据
bool cIocpServer::_DoRecv( PerSocketSession* pSocketContext, PerIoContext* pIoContext ,DWORD dwBytes)
{
	if (pSocketContext == NULL || pIoContext == NULL)
		return false;
	else
	{
		//接受到消息
		//这里可以处理第一组数据
		_RecvPost(pSocketContext,pIoContext,dwBytes);			//发送给后台线程

		//处理继续投递
		return _PostRecv(pIoContext);		//继续等待接受
	}
}

//====================================================================================
//
//				       其他辅助函数定义
//
//====================================================================================


std::string cIocpServer::GetLocalIP()
{
	// 获得本机主机名
	char hostname[MAX_PATH] = {0};
	gethostname(hostname,MAX_PATH);                
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if(lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	// 取得IP地址列表中的第一个为返回的IP(因为一台主机可能会绑定多个IP)
	char* lpAddr = lpHostEnt->h_addr_list[0];      

	// 将IP地址转化成字符串形式
	struct in_addr inAddr;
	memmove(&inAddr,lpAddr,4);

	m_strIP = inet_ntoa(inAddr);        

	return m_strIP;
}


/////////////////////////////////////////////////////////////////////
// 判断客户端Socket是否已经断开，否则在一个无效的Socket上投递WSARecv操作会出现异常
// 使用的方法是尝试向这个socket发送数据，判断这个socket调用的返回值
// 因为如果客户端网络异常断开(例如客户端崩溃或者拔掉网线等)的时候，服务器端是无法收到客户端断开的通知的
bool cIocpServer::_IsSocketAlive( SOCKET s )
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}


///////////////////////////////////////////////////////////////////
// 显示并处理完成端口上的错误
bool cIocpServer::HandleError( PerSocketSession *pContext,const DWORD& dwErr )
{
	// 如果是超时了，就再继续等吧  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// 确认客户端是否还活着...
		if( !_IsSocketAlive( pContext->socket) )			//send校验
		{
			_ShowMessage( "检测到客户端异常退出！");
			SOCKETPOOL->ReleasePerSocketSession(pContext);
			return true;
		}
		else
		{
			this->_ShowMessage( "网络操作超时！重试中...");
			return true;
		}
	}  

	// 可能是客户端异常退出了
	else if( ERROR_NETNAME_DELETED==dwErr )		
	{
		this->_ShowMessage( "检测到客户端异常退出！");			//客户端是关X直接退出
		SOCKETPOOL->ReleasePerSocketSession(pContext);
		return true;
	}

	else
	{
		this->_ShowMessage( "完成端口操作出现错误，线程退出。错误代码：%d",dwErr );
		return false;
	}
}


//////////////////////////////////////////////////////////////////
//	启动服务器
bool cIocpServer::Start()
{
	//socket环境
	if (false == LoadSocketLib())
	{
		this->_ShowMessage("初始化socket 环境失败！\n");
		return false;
	}

	// 建立系统退出的事件通知,手动重置，刚开始无信号
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 初始化IOCP
	if (false == _InitializeIOCP())
	{
		this->_ShowMessage("初始化IOCP失败！\n");
		return false;
	}
	else
	{
		this->_ShowMessage("IOCP初始化完毕\n.");
	}

	// 初始化Socket
	if( false==_InitializeListenSocket() )
	{
		this->_ShowMessage("Listen Socket初始化失败！\n");
		this->_DeInitialize();
		return false;
	}
	else
	{
		this->_ShowMessage("Listen Socket初始化完毕.\n");
	}

	//初始化后台线程
	if (false == _InitializeBackendThread())
	{
		this->_ShowMessage("初始化后台线程失败\n");
	}
	else
	{
		this->_ShowMessage("初始化后台线程成功\n");
	}
	this->_ShowMessage("系统准备就绪，等候连接....\n");

	return true;
}

void cIocpServer::Stop()
{
	if (m_pListenContext != NULL && m_pListenContext->socket != INVALID_SOCKET)
	{
		//激活关闭线程
		SetEvent(m_hShutdownEvent); //

		//通知所有workthread关闭
		for (int i = 0; i < m_nThreads;++i)
		{
			PostQueuedCompletionStatus(m_hIOCompletionPort,0,(DWORD)EXIT_CODE,NULL);
		}

		//等待所有线程退出
		WaitForMultipleObjects(m_nThreads,m_phWorkerThreads,TRUE,INFINITE);

		//后台线程成功
		WaitForSingleObject(m_hBackendThread,INFINITE);

		//缓冲池shutdown
		m_IoSendBackList.clear();
		m_IoSendFontList.clear();
		IOPOOL->Shutdown();
		SOCKETPOOL->Shutdown();

		_DeInitialize();

		this->_ShowMessage("停止监听\n");
	}

	UnloadSocketLib();
}

//发送数据
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
			_ShowMessage("发送消息失败！");
			return false;
		}

		return true;
	}
	else
		return false;
}


bool cIocpServer::_DoSend( PerSocketSession * pSocketContext, PerIoContext * pIoContext )
{
	//发送都有IOPOOL派发
	IOPOOL->ReleaseIoContext(pIoContext);

	return true;
}

void cIocpServer::SendPacket(DWORD cid,BYTE bCmdGroup,BYTE bCmd,char * buffer, int buflength)
{
	if (cid == 0 || buffer == NULL)
		return;
	else
	{
		//int length = sizeof(Packet) + buflength - 1;		//总包长

		//if (length >=  MAX_PACKET -1)
		//{
		//	_ShowMessage("超出最大包！");
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

		//			memcpy(pIoContext->buffer+6,buffer,buflength);		//注意一点,包头+googlebuf

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
	//双缓冲
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

	//处理
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
		//pPacketEx->pPacket = (Packet *)malloc(dwBytes);		//消息
		//memcpy(pPacketEx->pPacket,pIoContext->buffer,dwBytes);

		////CAutoLock cs(&m_csRecvContext);
		////接收
		//pthread_mutex_lock(&m_csRecvContext);
		//m_IoRecvBackList.push_back(pPacketEx);
		//pthread_mutex_unlock(&m_csRecvContext);
	}
}

void cIocpServer::_IoRecv()
{
	////双缓冲
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

	////处理
	//while(!m_IoRecvFontList.empty())
	//{
	//	//

	//	//处理PacketEx,在自己相关处理packet中释放，
	//	PacketEx *pPacketEx = m_IoRecvFontList.front();
	//	m_IoRecvFontList.pop_front();

	//	if (pPacketEx)
	//	{
	//		if (BatchComplete(pPacketEx->cid,pPacketEx->pPacket) == false)
	//		{
	//			//恶意操作，返回工作者线程关闭该cid
	//			PerSocketSession * pSession = SOCKETPOOL->GetCID(pPacketEx->cid);
	//			_RequestCloseSocket(pSession);
	//		}
	//	}

	//	//释放，删除内存
	//	SAFE_DELETE(pPacketEx);
	//}

	//m_IoSendFontList.clear();
}

//恶意请求，关闭该socket
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
			strcpy(pIoContext->buffer,"close");		//关闭

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
