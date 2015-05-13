// CrashServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../Common/CrashHandler.h"
#include "../Common/Log.h"


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


int _tmain(int argc, _TCHAR* argv[])
{
	DWORD dwThreadIdx;
	HANDLE hThread = CreateThread(NULL,0,Cmd_Thread,NULL,0,&dwThreadIdx);

	printf("start dump server !\n");
	CrashServerHandler *pServer = new CrashServerHandler();
	pServer->Start();

	WaitForSingleObject(hThread,INFINITE);
	SAFE_DELETE(pServer);

	sLog.Close();

	return 0;
}

