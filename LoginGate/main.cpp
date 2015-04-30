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

CMTimeControl			g_TimeControl;			//定时器

CWHList<CSessionInfo*>			g_xSessionList;


bool _IsSocketAlive( SOCKET s )
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}

bool HandleError( SOCKET cClient,const DWORD& dwErr )
{
	// 如果是超时了，就再继续等吧  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// 确认客户端是否还活着...
		if( !_IsSocketAlive( cClient) )			//send校验
		{
			printf( "检测到客户端异常退出！");
			closesocket(cClient);
			return true;
		}
		else
		{
			printf( "网络操作超时！重试中...");
			return true;
		}
	}  

	// 可能是客户端异常退出了
	else if( ERROR_NETNAME_DELETED==dwErr )		
	{
		printf( "检测到客户端异常退出！");			//客户端是关X直接退出
		closesocket(cClient);
		return true;
	}
	else if (ERROR_CONNECTION_REFUSED == dwErr)
	{
		printf( "服务器未开启,拒绝访问!");			//客户端是关X直接退出
		return true;
	}
	else
	{
		printf( "完成端口操作出现错误，线程退出。错误代码：%d",dwErr );
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
								//连接成功
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
	//g_TimeControl.StartTime(timer_type_sec,1000);		//启动秒定时器


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