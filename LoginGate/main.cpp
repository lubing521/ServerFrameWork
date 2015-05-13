#include "../Common/GlobalDefine.h"
#include "../Common/TimeControl.h"
#include "../Common/List.h"
#include "../Common/Session.h"
#include "../Common/Connector.h"
#include "LoginGate.h"


SOCKET			g_ssock = INVALID_SOCKET;
SOCKADDR_IN		g_saddr;

SOCKET			g_csock = INVALID_SOCKET;
SOCKADDR_IN		g_caddr;

bool			g_fTerminated = FALSE;

CMTimeControl			g_TimeControl;			//定时器

CWHList<CSessionInfo*>			g_xSessionList;




DWORD WINAPI Cmd_Thread(void *pParam)
{
	CLoginGateServer *pGate = (CLoginGateServer*)pParam;
	while(true)
	{
		char ch = getchar();
		if (ch =='q')
		{
			pGate->Stop();
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

	//CConnector *pConnector = new CConnector();
	//pConnector->Start(WorkThread,"10.10.10.82",12345);


	//CConnector *pConnector2 = new CConnector();
	//pConnector2->Start(WorkThread,"10.10.10.82",23456);

	CLoginGateServer *pGate = new CLoginGateServer();
	
	pGate->Start("10.10.10.82",5678,"",DEFAULT_PORT);
	

	DWORD dwThreadIdx;
	HANDLE hThread = CreateThread(NULL,0,Cmd_Thread,pGate,0,&dwThreadIdx);

	WaitForSingleObject(hThread,INFINITE);

	CloseHandle(hThread);

	SAFE_DELETE(pGate);

	return 0;
}