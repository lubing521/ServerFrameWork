#include <stdio.h>
#include "../Common/GlobalDefine.h"


int main()
{
	WSADATA wd;

	WSAStartup(MAKEWORD(2,2),&wd);

	SOCKET s = socket(AF_INET,SOCK_STREAM,0);

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5678);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(s,(SOCKADDR*)&addr,sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("error!\n");

		return 1;
	}

	listen(s,SOMAXCONN);

	while(true)
	{
		SOCKADDR_IN clientaddr;
		int len = sizeof(clientaddr);
		SOCKET sClient = accept(s,(sockaddr*)&clientaddr,&len);
		if (sClient != INVALID_SOCKET)
		{
			printf("connect success:%s,socket:%d\n",inet_ntoa(clientaddr.sin_addr),sClient);
			//≤ªœ‡Õ¨
			char szBuffer[1024] = {0};

			int nRecv = 0;
			while((nRecv = recv(sClient,szBuffer,1024,0))!=SOCKET_ERROR)
			{
				printf("socket:%d,recv:%s\n",sClient,szBuffer);
				
				char szTmp[1024] = {0};
				sprintf(szTmp,"send:%s",szBuffer);

				int nSend = send(sClient,szTmp,strlen(szTmp)+1,0);
				if (nSend == 0)
				{
					break;
				}
			}
		}
	}

	WSACleanup();

	return 0;
}