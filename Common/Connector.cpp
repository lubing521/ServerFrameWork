#include "Connector.h"
#include "TimeControl.h"
#include "MsWinsockUtil.h"

//重新连接心跳服务器

extern void SetSocketProp( SOCKET socket );

CConnector::CConnector()
{
	m_pProc = NULL;

	m_szIp[0] = '\0';

	m_pReconnect = new CMTimeControl();
	m_pHeatBeat = new CMTimeControl();

	m_socket.socket = INVALID_SOCKET;

	m_pThread = NULL;

	WSADATA wd;
	WSAStartup(MAKEWORD(2,2),&wd);
}


CConnector::~CConnector()
{
	Stop();

	SAFE_DELETE(m_pReconnect);
	SAFE_DELETE(m_pHeatBeat);
	WSACleanup();
}


bool CConnector::Start(ThreadPrco proc, char * szIp, xe_uint16 nPort)
{
	
	m_pProc = proc;

	if (szIp)
	{
		strncpy(m_szIp,szIp,20);
	}
	
	m_nPort = nPort;


	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,0);
	if (m_hIocp == NULL)
		return false;

	m_socket.socket = WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);

	if (m_socket.socket == INVALID_SOCKET)
	{
		printf("socket init fialed!");
		return false;
	}

	//// 将Listen Socket绑定至完成端口中
	if (CreateIoCompletionPort((HANDLE)m_socket.socket,m_hIocp,(DWORD)&m_socket,0) == NULL)
	{
		printf("绑定 Listen Socket至完成端口失败！错误代码: %d\n", WSAGetLastError());  
		closesocket(m_socket.socket);
		return false;
	}

	//加载客户端程序
	MsWinsockUtil::LoadClientFunction(m_socket.socket);
	
	//要绑定一个端口才有效
	// 以下的绑定很重要，也是容易漏掉的。（如果少了绑定，在ConnextEx 时将得到错误代码： 10022—提供了一个无效的参数 
	SOCKADDR_IN local_addr;  
	memset(&local_addr,0, sizeof(SOCKADDR_IN));   
	local_addr.sin_family = AF_INET;   
	int irt = bind(m_socket.socket, (sockaddr*)(&local_addr), sizeof(local_addr));
	

	//创建一条线程
	DWORD dwThreadIdx;
	m_pThread = CreateThread(NULL,0,m_pProc,this,0,&dwThreadIdx);

	//连接服务器
	ConnectToServer();

	//连接
	//if (ConnectToServer() == false)
	//{
	//	//连接失败
	//	//Reconnect(m_pReconnect);
	//	m_pReconnect->SetEvent(Reconnect,this);
	//	m_pReconnect->StartTime(timer_type_sec,5000);		//5miao1次
	//}
	//else
	//{
	//	m_pReconnect->StopTime();

	//	SetSocketProp(m_socket.socket); //设置参数

	//	//创建一条线程
	//	DWORD dwThreadIdx;
	//	m_pThread = CreateThread(NULL,0,m_pProc,this,0,&dwThreadIdx);

	//	//接收
	//	//SendHeatBeat();		//发送心跳包
	//	m_pHeatBeat->SetEvent(SendHeatBeat,this);
	//	m_pHeatBeat->StartTime(timer_type_sec,5000);		//1分钟1次

	//	//准备接收
	//	if (m_socket.PreRecv() == FALSE && WSAGetLastError() != WSA_IO_PENDING)
	//	{
	//		closesocket(m_socket.socket);
	//		return false;
	//	}
	//}

	return true;
}

bool CConnector::Stop()
{
	if (m_pThread)
	{
		PostQueuedCompletionStatus(m_hIocp,0,EXIT_CODE,NULL);
		WaitForSingleObject(m_pThread,INFINITE);

		RELEASE_HANDLE(m_pThread);
		RELEASE_HANDLE(m_hIocp);

		m_pHeatBeat->StopTime();
		m_pReconnect->StopTime();
		
		return true;
	}

	return false;
}

DWORD CConnector::Reconnect( CMTimeControl* pTime,void *pParam )
{
	CConnector *pConnector = (CConnector*)pParam;
	if (pTime &&pConnector)
	{
		return pConnector->Reconnect();
	}

	return 1;
}

DWORD CConnector::Reconnect()
{
	//连接
	ConnectToServer();

	return 1;
}

bool CConnector::ConnectToServer()
{
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_nPort);
	addr.sin_addr.s_addr = strlen(m_szIp) > 0 ? inet_addr(m_szIp) : htonl(INADDR_ANY);


	OVERLAPPEDEX ol;
	memset(&ol,0,sizeof(OVERLAPPEDEX));
	ol.bOperator = IOCP_REQUEST_CONNECT;
	
	// 重点
	DWORD dwSendBytes = 0;

	BOOL bResult =  MsWinsockUtil::m_lpnConnectEx (m_socket.socket ,
		(sockaddr *)&addr ,  // [in] 对方地址

		sizeof(addr) ,               // [in] 对方地址长度

		NULL ,				// [in] 连接后要发送的内容，这里不用

		0 ,				// [in] 发送内容的字节数 ，这里不用

		&dwSendBytes ,      // [out] 发送了多少个字节，这里不用

		&ol ); 


	if (!bResult )      // 返回值处理
	{
		//WSAEISCONN已存在
		if ( WSAGetLastError () != ERROR_IO_PENDING && WSAGetLastError() !=WSAEISCONN )   // 调用失败
		{
			printf("ConnextEx error: %d\n" ,WSAGetLastError ());
			return false ;
		}
		else // 操作未决（正在进行中 … ）
		{
			//printf("WSAGetLastError() == ERROR_IO_PENDING\n");// 操作正在进行中
		}

	}
	
	return true;
}

DWORD CConnector::SendHeatBeat( CMTimeControl* pTime,void *pParam)
{
	CConnector *pConnector = (CConnector*)pParam;
	if(pTime && pConnector)
	{
		return pConnector->SendHeatBeat();
	}

	return 1;
}

DWORD CConnector::SendHeatBeat()
{
	if (m_socket.SendPacket(PACKET_KEEPALIVE) == FALSE && WSAGetLastError() != WSA_IO_PENDING)
	{
		closesocket(m_socket.socket);
		return 0;
	}

	return 1;
}

//成功连接
bool CConnector::ConnectSuccess()
{
	printf("server connect success:%s,%d\n",strlen(m_szIp)>0?m_szIp:DEFAULT_IP,m_socket);
	
	//关闭当前定时器
	m_pReconnect->StopTime();

	if (!m_pHeatBeat->IsUse())
	{

		SetSocketProp(m_socket.socket); //设置参数

		//接收
		//SendHeatBeat();		//发送心跳包
		m_pHeatBeat->SetEvent(SendHeatBeat,this);
		m_pHeatBeat->StartTime(timer_type_sec,5000);		//1分钟1次

		//准备接收
		if (m_socket.PreRecv() == FALSE && WSAGetLastError() != WSA_IO_PENDING)
		{
			closesocket(m_socket.socket);
			return false;
		}
	}

	return true;
}

bool CConnector::ConnectFailed(DWORD dwError)
{
	printf("server connect failed:%s,%d,errno:%u\n",strlen(m_szIp)>0?m_szIp:DEFAULT_IP,m_nPort,dwError);
	//没有使用
	if (!m_pReconnect->IsUse())
	{
		m_pReconnect->SetEvent(Reconnect,this);
		m_pReconnect->StartTime(timer_type_sec,5000);		//5miao1次

		return true;
	}
	else
		return false;
}

int CSSocket::PreRecv()
{
	OVERLAPPEDEX ol;
	memset(&ol,0,sizeof(OVERLAPPEDEX));
	ol.bOperator = IOCP_REQUEST_RECV;
	ol.pIOBuf.sock = socket;
	WSABUF buf;
	buf.buf = ol.pIOBuf.szData;
	buf.len = DATA_BUFSIZE;

	DWORD dwBytes = 0, dwFlags = 0;
	return WSARecv(socket,&buf,1,&dwBytes,&dwFlags,&ol,NULL);
}

int CSSocket::SendPacket( char * szPacket )
{
	OVERLAPPEDEX ol;
	memset(&ol,0,sizeof(OVERLAPPEDEX));
	ol.bOperator = IOCP_REQUEST_SEND;
	ol.pIOBuf.sock = socket;

	int len = strlen(szPacket);

	strncpy(ol.pIOBuf.szData,szPacket,len);

	WSABUF buf;
	buf.buf = ol.pIOBuf.szData;
	buf.len = len+1;

	DWORD dwBytes = 0, dwFlags = 0;
	return WSASend(socket,&buf,1,&dwBytes,dwFlags,&ol,NULL);
}