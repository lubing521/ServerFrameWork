#include "TimeControl.h"
#include <math.h>
//-------------------------
//class: CMTimeSpan
CMTimeSpan::CMTimeSpan()
{
	m_nDay	= 0;
	m_nHour = 0;
	m_nMin	= 0;
	m_nSec	= 0;
	m_lTick	= 0;
}

CMTimeSpan::CMTimeSpan( int sec,int min/*=0*/,int hour/*=0*/,int day/*=0*/ )
{
	SetTime(sec,min,hour,day);
}
CMTimeSpan::~CMTimeSpan()
{
	
}

void CMTimeSpan::SetTime( int sec,int min/*=0*/,int hour/*=0*/,int day/*=0*/ )
{
	if(sec !=0 || min != 0 || hour !=0 || day!=0)
	{
		m_nDay	= day;
		m_nHour = hour;
		m_nMin	= min;
		m_nSec	= sec;	
	}
	AdjustTime();
	SetTick();
}

CMTimeSpan CMTimeSpan::operator+( CMTimeSpan& mTime )
{
	CMTimeSpan mTimeReturn;
	mTimeReturn.m_nDay	= m_nDay	+ mTime.m_nDay;
	mTimeReturn.m_nHour = m_nHour	+ mTime.m_nHour;
	mTimeReturn.m_nMin	= m_nMin	+ mTime.m_nMin;
	mTimeReturn.m_nSec	= m_nSec	+ mTime.m_nSec;
	mTimeReturn.AdjustTime();
	mTimeReturn.SetTick();
	return mTimeReturn;
}


CMTimeSpan CMTimeSpan::operator-( CMTimeSpan& mTime )
{
	CMTimeSpan mTimeReturn;
	mTimeReturn.m_nDay	= m_nDay	- mTime.m_nDay;
	mTimeReturn.m_nHour = m_nHour	- mTime.m_nHour;
	mTimeReturn.m_nMin	= m_nMin	- mTime.m_nMin;
	mTimeReturn.m_nSec	= m_nSec	- mTime.m_nSec;
	mTimeReturn.AdjustTime();
	mTimeReturn.SetTick();
	return mTimeReturn;
}

void CMTimeSpan::SetTick( long lTick /*= 0*/ )
{
	m_lTick	= 0;
	if(lTick == 0)
	{
		//m_nDay*24*60*60
		long lTick	= (m_nDay<<4) + (m_nDay<<3);
		lTick		= (lTick<<6) - (lTick<<2);
		lTick		= (lTick<<6) - (lTick<<2);
		m_lTick		+= lTick;
		
		lTick		= (m_nHour<<6) - (m_nHour<<2);
		lTick		= (lTick<<6) - (lTick<<2);
		m_lTick		+= lTick;
		
		lTick		= (m_nMin<<6) - (m_nMin<<2);
		m_lTick		+= lTick;
		
		m_lTick		+= m_nSec;
	}
	else
		m_lTick		= lTick;
}

void CMTimeSpan::AdjustTime()
{
	int nTemp = m_nSec/60;
	m_nMin	= m_nMin + nTemp;
	m_nSec	= m_nSec - ((nTemp<<6)-(nTemp<<2));
	
	nTemp = m_nMin/60;
	m_nHour	= m_nHour + nTemp;
	m_nMin	= m_nMin - ((nTemp<<6)-(nTemp<<2));
	
	nTemp = m_nHour/24;
	m_nDay	= m_nDay + nTemp;
	m_nHour	= m_nHour - ((nTemp<<4)+(nTemp<<3));
}

//-----------------
//class CMTime
CMTime::CMTime()
{
	memset(&m_tmTime,0,sizeof(tm));
	m_tmTimeTick = 0;
}

CMTime::CMTime( time_t nTime )
{
	SetTime(nTime);
}

CMTime::CMTime( SYSTEMTIME tmSystem )
{
	SetTime(tmSystem);	
}

CMTime::CMTime( char* szTime )
{
	SetTime(szTime);	
}

CMTime::CMTime( tm tmTime )
{
	SetTime(tmTime);	
}
CMTime::~CMTime()
{
	
}
//static
time_t CMTime::GetCurTickCount()
{
	return time(NULL);
}

tm CMTime::GetCurTime()
{
	time_t tmtTime = time(NULL);
	return *(localtime(&tmtTime));	
}

char* CMTime::Format( char* szFormat/*=NULL*/ )
{
	memset(m_szTime,0,1024);
	int nSize = strftime(m_szTime,1024,szFormat,&m_tmTime);
	if(nSize == 0)
	{
		memset(m_szTime,0,1024);
	}
	return m_szTime;	
}

//method
tm CMTime::SetTime( time_t nTime )
{
	m_tmTimeTick	= nTime;
	m_tmTime		= *(localtime(&nTime));
	return m_tmTime;
}

tm CMTime::SetTime( char* szTime )
{
	SYSTEMTIME xSys;
	memset(&xSys,0,sizeof(SYSTEMTIME));
	sscanf(szTime,"%hu-%hu-%hu %hu:%hu:%hu",&xSys.wYear,&xSys.wMonth,&xSys.wDay,&xSys.wHour,&xSys.wMinute,&xSys.wSecond);
	this->SetTime(xSys);
	return m_tmTime;	
}

tm CMTime::SetTime( SYSTEMTIME xSys )
{
	_tzset();
	m_tmTime.tm_year	=  xSys.wYear - 1900;
	m_tmTime.tm_mon		= xSys.wMonth - 1;
	m_tmTime.tm_mday	= xSys.wDay;
	m_tmTime.tm_hour	= xSys.wHour;
	m_tmTime.tm_min		= xSys.wMinute;
	m_tmTime.tm_sec		= xSys.wSecond;
	m_tmTime.tm_wday	= xSys.wDayOfWeek;
	m_tmTime.tm_isdst	= _daylight;
	m_tmTimeTick		= mktime(&m_tmTime);
	return m_tmTime;	
}

tm CMTime::SetTime( tm tmTime )
{
	m_tmTime	= tmTime;
	m_tmTimeTick= mktime(&m_tmTime); 
	return m_tmTime;
}

time_t CMTime::GetTickCount()
{
	return m_tmTimeTick;
}

tm  CMTime::GetTime()
{
	return  m_tmTime;
}

SYSTEMTIME CMTime::GetSystime()
{
	SYSTEMTIME xSys;
	xSys.wYear = m_tmTime.tm_year + 1900;
	xSys.wMonth = m_tmTime.tm_mon + 1;
	xSys.wDay = m_tmTime.tm_mday;
	xSys.wHour = m_tmTime.tm_hour;
	xSys.wMinute = m_tmTime.tm_min;
	xSys.wSecond = m_tmTime.tm_sec;
	xSys.wDayOfWeek = m_tmTime.tm_wday;
	return xSys;
}

bool CMTime::NextWeek( int nDay/*=0*/ ,int nHour)
{
	CMTimeSpan mts;
	int n = nDay - m_tmTime.tm_wday + 7;
	mts.SetTime(0,0,0,n);
	*(this) = *(this) + mts;
	m_tmTime.tm_hour	= nHour;
	m_tmTime.tm_min		= 0;
	m_tmTime.tm_sec		= 0;
	m_tmTimeTick = mktime(&m_tmTime);
	return true;
}

bool CMTime::NextDay( int nDay/*=1*/,int nHour/*=0*/ )
{
	CMTimeSpan mts;
	mts.SetTime(0,0,0,nDay);
	*(this)				= *(this) + mts;
	m_tmTime.tm_hour	= nHour;
	m_tmTime.tm_min		= 0;
	m_tmTime.tm_sec		= 0;
	m_tmTimeTick = mktime(&m_tmTime);
	return true;
}

int CMTime::operator[]( BYTE bTimePartName )
{
	switch(bTimePartName)
	{
	case time_sec:
		return m_tmTime.tm_sec;
	case time_min:
		return m_tmTime.tm_min;
	case time_hour:
		return m_tmTime.tm_hour;
	case time_day:
		return m_tmTime.tm_mday;
	case time_month:
		return m_tmTime.tm_mon;
	case time_year:
		return m_tmTime.tm_year;
	case time_weekday:
		return m_tmTime.tm_wday;
	case time_yeardays:
		return m_tmTime.tm_yday;
	}
	return 0;
}

bool CMTime::operator>( CMTime& mTime)
{
	return m_tmTimeTick>mTime.m_tmTimeTick;
}

bool CMTime::operator<( CMTime& mTime)
{
	return m_tmTimeTick<mTime.m_tmTimeTick;	
}

bool CMTime::operator>=( CMTime& mTime)
{
	return m_tmTimeTick>=mTime.m_tmTimeTick;	
}

bool CMTime::operator<=( CMTime& mTime)
{
	return m_tmTimeTick<=mTime.m_tmTimeTick;	
}

bool CMTime::operator==( CMTime& mTime)
{
	return m_tmTimeTick==mTime.m_tmTimeTick;	
}

CMTime CMTime::operator-( CMTimeSpan& mTimeSpan )
{
	CMTime mTimeReturn;
	time_t tmTick = m_tmTimeTick - mTimeSpan.m_lTick;
	mTimeReturn.SetTime(tmTick);
	return mTimeReturn;
}

CMTimeSpan CMTime::operator-( CMTime& mTime )
{
	CMTimeSpan mTimeSpan;
	long lTick = abs(mTime.m_tmTimeTick - m_tmTimeTick);
	mTimeSpan.SetTick(lTick);
	mTimeSpan.SetTime(lTick);
	mTimeSpan.AdjustTime();
	return mTimeSpan;	
}

CMTime CMTime::operator+( CMTimeSpan& mTimeSpan )
{
	CMTime mTimeReturn;
	time_t tmTick = m_tmTimeTick + mTimeSpan.m_lTick;
	mTimeReturn.SetTime(tmTick);
	return mTimeReturn;	
}
//-----------------
//class CMTimeControl
CMTimeControl::CMTimeControl()
{
	m_dwTimerID		= 0;
	m_dwTimeCount	= 0;
	m_bCurType		= 0;
	m_pfunTime	    = NULL;

	m_dwTimePeriod	= 0;		//定时器
}

CMTimeControl::~CMTimeControl()
{
	if(m_dwTimerID > 0)
	{
		timeKillEvent(m_dwTimerID);
		m_dwTimerID = 0;
	}	
}

void CALLBACK CMTimeControl::_TimeCallBack(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	CMTimeControl* pTime = (CMTimeControl*)dwUser;
	if(pTime->m_bCurType != pTime->m_bType)//adjust timer
	{
		pTime->m_tmStartTime.SetTime(pTime->m_tmStartTime.GetCurTime());
// #ifdef _DEBUG
// 		printf("%s\n",pTime->m_tmStartTime.Format("%y-%m-%d %H:%M:%S"));
// #endif
		switch(pTime->m_bCurType)
		{
		case timer_type_sec:
			if(pTime->m_tmStartTime[time_sec] == 0)
				pTime->StartTime(pTime->m_bType,pTime->m_dwTimePeriod);
			break;
		case timer_type_min:
			if(pTime->m_tmStartTime[time_min] == 0)
				pTime->StartTime(pTime->m_bType,pTime->m_dwTimePeriod);
			break;
		}
	}
	else//timer begin
	{
		pTime->m_dwTimeCount ++;	
		if(pTime->m_pfunTime)
			pTime->m_pfunTime(pTime,pTime->m_pParam);
	}

}

int CMTimeControl::StartTime(BYTE bType,DWORD dwTimePeriod)
{
	AdjustTime(bType,dwTimePeriod);

	m_tmStartTime.SetTime(m_tmStartTime.GetCurTime());
#ifdef _DEBUG
	printf("%s\n",m_tmStartTime.Format("%y-%m-%d %H:%M:%S"));
#endif

	if(m_dwTimerID > 0)
	{
		timeKillEvent(m_dwTimerID);
		m_dwTimerID = 0;
	}
	if(timer_type_nul != bType)
		m_bType = bType;
	switch(m_bType)
	{
	case timer_type_sec:
		m_bCurType			= timer_type_sec;
		m_dwTimerID			= timeSetEvent(m_dwTimePeriod,1000,_TimeCallBack,(DWORD)this,TIME_PERIODIC);
		break;
	case timer_type_min:
		if(m_tmStartTime[time_sec]	!= 0)
		{
			m_bCurType		= timer_type_sec;
			m_dwTimerID		= timeSetEvent(1000,1000,_TimeCallBack,(DWORD)this,TIME_PERIODIC);
		}
		else
		{
			m_bCurType		= timer_type_min;
			m_dwTimerID		= timeSetEvent(m_dwTimePeriod,1000,_TimeCallBack,(DWORD)this,TIME_PERIODIC);
		}
		break;
	case timer_type_hour:
		if(m_tmStartTime[time_sec]	!= 0)
		{
			m_bCurType		= timer_type_sec;
			m_dwTimerID		= timeSetEvent(1000,1000,_TimeCallBack,(DWORD)this,TIME_PERIODIC);
		}
		else
		{
			if(m_tmStartTime[time_min]!= 0)
			{
				m_bCurType	= timer_type_min;
				m_dwTimerID	= timeSetEvent(60000,1000,_TimeCallBack,(DWORD)this,TIME_PERIODIC);
			}
			else
			{
				m_bCurType		= timer_type_hour;
				m_dwTimerID		= timeSetEvent(m_dwTimePeriod,1000,_TimeCallBack,(DWORD)this,TIME_PERIODIC);
			}
		}
		break;
	}
	return m_dwTimerID;
}

double CMTimeControl::UseTime( bool isEnd /*= false*/ )
{
	//retrieves the frequency of the high-resolution performance counter, 
	//if one exists. The frequency cannot change while the system is running.
	static	double		s_dfFreq = 0.0;
	static	LONGLONG	s_QPart;
	LARGE_INTEGER		litmp; 
	LONGLONG			QPart;
	double				dfMinus, dfTim;
	if(s_dfFreq>-0.00000001 && s_dfFreq<0.00000001)
	{
		QueryPerformanceFrequency(&litmp); 
		s_dfFreq = (double)litmp.QuadPart;	//获得计数器的时钟频率	
	}

	QueryPerformanceCounter(&litmp); 
	QPart = litmp.QuadPart;					// 获得初始值
	if(isEnd)
	{
		dfMinus = (double)(QPart-s_QPart); 
		dfTim = (dfMinus / s_dfFreq)*1000;			// 获得对应的时间值，单位为ms
/*		printf("Use Time:%gms\n",dfTim);*/
		return dfTim;
	}
	s_QPart = QPart;
	return 0;
}

void CMTimeControl::SetEvent( ONTIME pfunTime,void *pParam)
{
	m_pfunTime = pfunTime;
	m_pParam = pParam;
}

int CMTimeControl::GetTimeCount()
{
	return m_dwTimeCount;
}

void CMTimeControl::StopTime()
{
	if(m_dwTimerID > 0)
	{
		timeKillEvent(m_dwTimerID);
		m_dwTimerID = 0;
	}	
}

void CMTimeControl::AdjustTime( BYTE bType,DWORD dwTimePeriod )
{
	switch(bType)
	{
	case timer_type_nul:
		m_dwTimePeriod = 0;
		break;
	case timer_type_sec:
		{
			if (dwTimePeriod == 0 || dwTimePeriod < 1000)
				m_dwTimePeriod = 1000;
			m_dwTimePeriod = dwTimePeriod;
		}
		break;
	case timer_type_min:
		{
			if (dwTimePeriod == 0 || dwTimePeriod < 60000)
				m_dwTimePeriod = 60000;
			else
			{
				if (dwTimePeriod%60 == 0)
				{
					m_dwTimePeriod = dwTimePeriod;
				}
				else
				{
					m_dwTimePeriod = dwTimePeriod/60000*60000;
				}
			}
		}
		break;
	case timer_type_hour:
		{
			if (dwTimePeriod == 0 || dwTimePeriod < 3600000)
				m_dwTimePeriod = 3600000;
			else
			{
				if (dwTimePeriod%3600000 == 0)
				{
					m_dwTimePeriod = dwTimePeriod;
				}
				else
				{
					m_dwTimePeriod = dwTimePeriod/3600000*3600000;
				}
			}
		}
		break;
	}
}
