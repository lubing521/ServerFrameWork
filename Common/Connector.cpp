#include "Connector.h"
#include "TimeControl.h"
#include "MsWinsockUtil.h"

//������������������

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

	//// ��Listen Socket������ɶ˿���
	if (CreateIoCompletionPort((HANDLE)m_socket.socket,m_hIocp,(DWORD)&m_socket,0) == NULL)
	{
		printf("�� Listen Socket����ɶ˿�ʧ�ܣ��������: %d\n", WSAGetLastError());  
		closesocket(m_socket.socket);
		return false;
	}

	//���ؿͻ��˳���
	MsWinsockUtil::LoadClientFunction(m_socket.socket);
	
	//Ҫ��һ���˿ڲ���Ч
	// ���µİ󶨺���Ҫ��Ҳ������©���ġ���������˰󶨣���ConnextEx ʱ���õ�������룺 10022���ṩ��һ����Ч�Ĳ��� 
	SOCKADDR_IN local_addr;  
	memset(&local_addr,0, sizeof(SOCKADDR_IN));   
	local_addr.sin_family = AF_INET;   
	int irt = bind(m_socket.socket, (sockaddr*)(&local_addr), sizeof(local_addr));
	

	//����һ���߳�
	DWORD dwThreadIdx;
	m_pThread = CreateThread(NULL,0,m_pProc,this,0,&dwThreadIdx);

	//���ӷ�����
	ConnectToServer();

	//����
	//if (ConnectToServer() == false)
	//{
	//	//����ʧ��
	//	//Reconnect(m_pReconnect);
	//	m_pReconnect->SetEvent(Reconnect,this);
	//	m_pReconnect->StartTime(timer_type_sec,5000);		//5miao1��
	//}
	//else
	//{
	//	m_pReconnect->StopTime();

	//	SetSocketProp(m_socket.socket); //���ò���

	//	//����һ���߳�
	//	DWORD dwThreadIdx;
	//	m_pThread = CreateThread(NULL,0,m_pProc,this,0,&dwThreadIdx);

	//	//����
	//	//SendHeatBeat();		//����������
	//	m_pHeatBeat->SetEvent(SendHeatBeat,this);
	//	m_pHeatBeat->StartTime(timer_type_sec,5000);		//1����1��

	//	//׼������
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
	//����
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
	
	// �ص�
	DWORD dwSendBytes = 0;

	BOOL bResult =  MsWinsockUtil::m_lpnConnectEx (m_socket.socket ,
		(sockaddr *)&addr ,  // [in] �Է���ַ

		sizeof(addr) ,               // [in] �Է���ַ����

		NULL ,				// [in] ���Ӻ�Ҫ���͵����ݣ����ﲻ��

		0 ,				// [in] �������ݵ��ֽ��� �����ﲻ��

		&dwSendBytes ,      // [out] �����˶��ٸ��ֽڣ����ﲻ��

		&ol ); 


	if (!bResult )      // ����ֵ����
	{
		//WSAEISCONN�Ѵ���
		if ( WSAGetLastError () != ERROR_IO_PENDING && WSAGetLastError() !=WSAEISCONN )   // ����ʧ��
		{
			printf("ConnextEx error: %d\n" ,WSAGetLastError ());
			return false ;
		}
		else // ����δ�������ڽ����� �� ��
		{
			//printf("WSAGetLastError() == ERROR_IO_PENDING\n");// �������ڽ�����
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

//�ɹ�����
bool CConnector::ConnectSuccess()
{
	printf("server connect success:%s,%d\n",strlen(m_szIp)>0?m_szIp:DEFAULT_IP,m_socket);
	
	//�رյ�ǰ��ʱ��
	m_pReconnect->StopTime();

	if (!m_pHeatBeat->IsUse())
	{

		SetSocketProp(m_socket.socket); //���ò���

		//����
		//SendHeatBeat();		//����������
		m_pHeatBeat->SetEvent(SendHeatBeat,this);
		m_pHeatBeat->StartTime(timer_type_sec,5000);		//1����1��

		//׼������
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
	//û��ʹ��
	if (!m_pReconnect->IsUse())
	{
		m_pReconnect->SetEvent(Reconnect,this);
		m_pReconnect->StartTime(timer_type_sec,5000);		//5miao1��

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