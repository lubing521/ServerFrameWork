#include "../Common/GlobalDefine.h"
#include "../Common/TimeControl.h"
#include "../Common/List.h"
#include "../Common/Session.h"
#include "../Common/Connector.h"


SOCKET			g_ssock = INVALID_SOCKET;
SOCKADDR_IN		g_saddr;

SOCKET			g_csock = INVALID_SOCKET;
SOCKADDR_IN		g_caddr;

bool			g_fTerminated = FALSE;

CMTimeControl			g_TimeControl;			//��ʱ��

CWHList<CSessionInfo*>			g_xSessionList;


bool _IsSocketAlive( SOCKET s )
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}

bool HandleError( SOCKET cClient,const DWORD& dwErr )
{
	// ����ǳ�ʱ�ˣ����ټ����Ȱ�  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// ȷ�Ͽͻ����Ƿ񻹻���...
		if( !_IsSocketAlive( cClient) )			//sendУ��
		{
			printf( "��⵽�ͻ����쳣�˳���");
			closesocket(cClient);
			return true;
		}
		else
		{
			printf( "���������ʱ��������...");
			return true;
		}
	}  

	// �����ǿͻ����쳣�˳���
	else if( ERROR_NETNAME_DELETED==dwErr )		
	{
		printf( "��⵽�ͻ����쳣�˳���");			//�ͻ����ǹ�Xֱ���˳�
		closesocket(cClient);
		return true;
	}
	else if (ERROR_CONNECTION_REFUSED == dwErr)
	{
		printf( "������δ����,�ܾ�����!");			//�ͻ����ǹ�Xֱ���˳�
		return true;
	}
	else
	{
		printf( "��ɶ˿ڲ������ִ����߳��˳���������룺%d",dwErr );
		return false;
	}
}


DWORD WINAPI WorkThread(void *pParam)
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
							break;
						}
					}
				}


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
								pScoket->PreRecv();
							}
							break;
						case IOCP_REQUEST_SEND:
							{
								//SAFE_DELETE(pOl->pIOBuf);
							}
							break;
						case  IOCP_REQUEST_CONNECT:
							{
								//���ӳɹ�
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

DWORD WINAPI Cmd_Thread(void *pParam)
{
	while(true)
	{
		char ch = getchar();
		if (ch =='q')
		{
			break;
		}
	}

	return 1;
}

void CALLBACK Test_TimeCallBack(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	static int i = 0;

	printf("test:%d",++i);
}
int main()
{
	//g_TimeControl.StartTime(timer_type_sec,1000);		//�����붨ʱ��


	printf("start server!\n");

	CConnector *pConnector = new CConnector();
	pConnector->Start(WorkThread,"10.10.10.82",12345);


	CConnector *pConnector2 = new CConnector();
	pConnector2->Start(WorkThread,"10.10.10.82",23456);
	

	DWORD dwThreadIdx;
	HANDLE hThread = CreateThread(NULL,0,Cmd_Thread,NULL,0,&dwThreadIdx);

	WaitForSingleObject(hThread,INFINITE);


	return 0;
}