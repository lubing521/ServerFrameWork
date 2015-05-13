
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include "CrashHandler.h"
#include "Log.h"

const size_t kMaximumLineLength = 256;
const int kMaxLoadString = 100;


//wstring���ֽڲ�Ϊ0������FALSE
bool WStringToString(const std::wstring &wstr,std::string &str)
{    
	int nLen = (int)wstr.length();    
	str.resize(nLen,' ');

	int nResult = WideCharToMultiByte(CP_ACP,0,(LPCWSTR)wstr.c_str(),nLen,(LPSTR)str.c_str(),nLen,NULL,NULL);
	
	if (nResult == 0)
	{
		return false;
	}
	return true;
}
bool StringToWString(const std::string &str,std::wstring &wstr)
{    
	int nLen = (int)str.length();    
	wstr.resize(nLen,L' ');

	int nResult = MultiByteToWideChar(CP_ACP,0,(LPCSTR)str.c_str(),nLen,(LPWSTR)wstr.c_str(),nLen);
	if (nResult == 0)
	{
		return false;
	}

	return true;
}

bool ShowDumpResults(const wchar_t* dump_path,
	const wchar_t* minidump_id,
	void* context,
	EXCEPTION_POINTERS* exinfo,
	MDRawAssertionInfo* assertion,
	bool succeeded) 
{
	if (succeeded)		
		printf("dump success,guid:%ws!\n",minidump_id);
	else
		printf("dump failed!\n");
	return succeeded;
}

CrashClientHandler::CrashClientHandler()
{
	handler = NULL;
	kCustomInfoEntries = NULL;
}

CrashClientHandler::~CrashClientHandler()
{
	SAFE_DELETE(handler);
	SAFE_DELETE(kCustomInfoEntries);
}

void CrashClientHandler::Start( wchar_t * pAppName )
{
	kCustomInfoEntries = new google_breakpad::CustomInfoEntry[2];
	kCustomInfoEntries[0] = google_breakpad::CustomInfoEntry(L"prod", pAppName);
	kCustomInfoEntries[1] = google_breakpad::CustomInfoEntry(L"ver", L"1.0");

	google_breakpad::CustomClientInfo custom_info = {kCustomInfoEntries, 2};

	if (access(".\dumps",0) == -1)
	{
		mkdir("dumps");
	}

	handler = new google_breakpad::ExceptionHandler(L".\\dumps\\",
		NULL,
		ShowDumpResults,
		NULL,
		google_breakpad::ExceptionHandler::HANDLER_ALL,
		MiniDumpNormal,
		kPipeName,
		&custom_info);

	if (handler == NULL)
	{
		printf("%s","fuck!!!!");
		return;
	}

}



CrashServerHandler::CrashServerHandler()
{
	crash_server = NULL;
}

CrashServerHandler::~CrashServerHandler()
{
	SAFE_DELETE(crash_server);
}


//��ʾ����
static void ShowClientConnected(void* context,
	const google_breakpad::ClientInfo* client_info) 
{
		char szTmp[1024] = {0};
		sprintf(szTmp,"Client connected:\t\t%d",client_info->pid());
		
		sLog.OutPutDumpStr(szTmp);
}
//��ʾ�ͻ��˱�����Ϣ
static void ShowClientCrashed(void* context,
	const google_breakpad::ClientInfo* client_info,
	const wstring* dump_path)
{

	char szTmp[1024] = {0};
	sprintf(szTmp,"Client requested dump:\t\t%d",client_info->pid());

	sLog.OutPutDumpStr(szTmp);


	google_breakpad::CustomClientInfo custom_info = client_info->GetCustomInfo();
	if (custom_info.count <= 0) {
		return;
	}

	wstring str_line;
	for (size_t i = 0; i < custom_info.count; ++i) {
		if (i > 0) {
			str_line += L", ";
		}
		str_line += custom_info.entries[i].name;
		str_line += L": ";
		str_line += custom_info.entries[i].value;
	}

	std::string str;

	WStringToString(str_line,str);

	sLog.OutPutDumpStr((char*)str.c_str());
}

//�˳�
static void ShowClientExited(void* context,
	const google_breakpad::ClientInfo* client_info) 
{
	char szTmp[1024] = {0};
	sprintf(szTmp,"Client exited:\t\t%d",client_info->pid());

	sLog.OutPutDumpStr(szTmp);
}

void CrashServerHandler::Start()
{
	if (crash_server)
		return;

	if (access(".\dumps",0) == -1)
	{
		mkdir("dumps");
	}

	 std::wstring dump_path = L".\\Dumps\\";

	crash_server = new google_breakpad::CrashGenerationServer(kPipeName,
		NULL,
		ShowClientConnected,
		NULL,
		ShowClientCrashed,
		NULL,
		ShowClientExited,
		NULL,
		NULL,
		NULL,
		true,
		&dump_path);

	if (!crash_server->Start()) {
		sLog.OutPutDumpStr("Unable to start dump server");
		SAFE_DELETE(crash_server);
	}
}
