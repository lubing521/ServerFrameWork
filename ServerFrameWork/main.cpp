#include <stdio.h>
#include "../Common/GlobalDefine.h"
#include "../Common/TimeControl.h"

SOCKET sock[10];

DWORD Work(CMTimeControl *pTm,void *pParam)
{
	SOCKET s = (SOCKET)pParam;

	int nSendBytes = send(s,PACKET_KEEPALIVE,strlen(PACKET_KEEPALIVE),0);

	return nSendBytes;
}

int main()
{
	WSADATA wd;
	WSAStartup(MAKEWORD(2,2),&wd);
	CMTimeControl *pTmp[10];// = NULL;

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(DEFAULT_PORT);
	addr.sin_addr.s_addr = inet_addr("10.10.10.82");

	for (int i = 0; i < 1; ++i)
	{
		sock[i] = socket(AF_INET,SOCK_STREAM,0);
		if (sock[i] != INVALID_SOCKET)
		{
			int nErrro = connect(sock[i],(sockaddr*)&addr,sizeof(SOCKADDR_IN));
			if(nErrro != SOCKET_ERROR)
			{
				pTmp[i] = new CMTimeControl();
				pTmp[i]->SetEvent(Work,(void*)sock[i]);
				pTmp[i]->StartTime(timer_type_sec,5000);
			}
			else
			{
				printf("errno:%d\n",GetLastError());
			}
		}

	}


	char ch;// = getchar();
	while( true)
	{
		ch = getchar();

		if (ch =='q')
		{
			break;
		}
	}

	WSACleanup();

	return 0;
}