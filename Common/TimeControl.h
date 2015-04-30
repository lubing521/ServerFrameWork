#ifndef _MMTIME_CONTROL_H_ 
#define _MMTIME_CONTROL_H_
/********************************************************************

	purpose:	1.CMTimeSpan 时间间隔
					CMTimeSpan=CMTimeSpan+CMTimeSpan;
					SetTime(xx,xx,xx,xx);
					SetTick();
					AdjustTime();
				2.CMTime
				static:
					GetCurTime();
					GetCurTickCount();
				normal:
					SetTime(xx);
					GetTime();GetTickCount();GetSystime();
					Format("char format");
					>,<,==,<=,>=
					CMTime=CMTime-CMTimeSpan;
					CMTimeSpan=CMTime-CMTime;
					CMTime=CMTime+CMTimeSpan;
				3.CMTimeControl
					StartTime();
//  struct tm {
// 	int tm_sec;		/* [0,59] */
// 	int tm_min;		/* [0,59] */
// 	int tm_hour;	/* [0,23] */
// 	int tm_mday;	/* [1,31] */
// 	int tm_mon;		/* [0,11] */
// 	int tm_year;	/* 实际年份减去1900 */
// 	int tm_wday;	/* [0,6],0代表星期天，1代表星期一 */
// 	int tm_yday;	/* [0,365],0代表1月1日，1代表1月2日 */
// 	int tm_isdst;	/* 夏令时标识符，实行夏令时的时候，tm_isdst为正。不实行夏令时的时候，tm_isdst为0；不了解情况时，tm_isdst()为负。*/
// };

//UTC(世界标准时间):协调世界时，又称为世界标准时间，也就是大家所熟知的格林威治标准时间(Greenwich Mean Time，GMT)。
//Calendar Time(日历时间):日历时间，是用“从一个标准时间点到此时的时间经过的秒数”来表示的时间。
//epoch(时间点):时间点。时间点在标准C/C++中是一个整数，它用此时的时间和标准时间点相差的秒数(即日历时间)来表示。
//clock tick(时钟计时单元):时钟计时单元，一个时钟计时单元的时间长短是由CPU控制的。一个clock tick不是CPU的一个时钟周期，而是C/C++的一个基本计时单位
/*********************************************************************/
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include <Windows.h>
#include <sys/timeb.h>

#pragma comment(lib,"Winmm.lib")
enum TimeType//秒,分定时,小时定时
{
	timer_type_nul,  //adjust
	timer_type_sec,
	timer_type_min,
	timer_type_hour,
};

enum TimePartName
{
	time_sec,
	time_min,
	time_hour,
	time_day,
	time_month,	  //[0,11]
	time_year,	  //实际年份减去1900
	time_weekday, //[0,6],0代表星期天,1代表星期一
	time_yeardays,//[0,365],0代表1月1日,1代表1月2日	  
};
class CMTime;
class CMTimeControl;
class CMTimeSpan
{
	friend CMTime;
public:
	CMTimeSpan();
	CMTimeSpan(int sec,int min=0,int hour=0,int day=0);
	~CMTimeSpan();

	void				SetTime(int sec,int min=0,int hour=0,int day=0);
	void				SetTick(long lTick = 0);            //transition timespan in tick
	void				AdjustTime();                       //adjust time in normal

	DWORD				GetSenconds()	{ return m_lTick;}
	int					GetDays()		{ return m_nDay;}
	CMTimeSpan			operator+(CMTimeSpan& mTime);
	CMTimeSpan			operator-(CMTimeSpan& mTime);
private:
	int		m_nDay;
	int		m_nHour;
	int		m_nMin;
	int		m_nSec;

	long	m_lTick;
};

class CMTime
{
public:
	CMTime();
	CMTime(time_t nTime);
	CMTime(SYSTEMTIME tmSystem);
	CMTime(char* szTime);
	CMTime(tm tmTime);
	~CMTime();

	static tm		GetCurTime();
	static time_t	GetCurTickCount();		//获取1970年开始的当前秒数

	tm				SetTime(time_t nTime);
	tm				SetTime(SYSTEMTIME tmSystem);
	tm				SetTime(char* szTime);
	tm				SetTime(tm tmTime);

	tm				GetTime();
	time_t			GetTickCount();
	SYSTEMTIME		GetSystime();

	char*			Format(char* szFormat = "%y-%m-%d %H:%M:%S");
	//----Get Part of Time----
	bool			NextWeek(int nDay=0,int nHour=0);//next week day nHour:00:00 [0,6],0代表星期天,1代表星期一
	bool			NextDay(int nDay=1,int nHour=0); //next nDay     00:00:00
	//************************************
	// Method:    operator[]
	// FullName:  CMTime::operator[]
	// Access:    public 
	// Returns:   int
	// Qualifier:
	// Parameter: BYTE bTimePartName
	// 				time_sec,
	// 				time_min,
	// 				time_hour,
	// 				time_day,
	// 				time_month,
	// 				time_year,
	// 				time_weekday, //[0,6],0代表星期天，1代表星期一
	// 				time_yeardays,//[0,365],0代表1月1日，1代表1月2日
	//************************************
	int				operator[](BYTE bTimePartName);
	bool			operator>(CMTime& mTime);
	bool			operator<(CMTime& mTime);
	bool			operator>=(CMTime& mTime);
 	bool			operator<=(CMTime& mTime);
	bool			operator==(CMTime& mTime);
	CMTime			operator-(CMTimeSpan& mTimeSpan);
	CMTimeSpan		operator-(CMTime& mTime);
	CMTime			operator+(CMTimeSpan& mTimeSpan);
private:
	tm				m_tmTime;
	time_t			m_tmTimeTick;
	char			m_szTime[1024];
};

//定时器
typedef  DWORD (*ONTIME)(CMTimeControl* pTime,void *pParam);
class CMTimeControl
{
public:
	CMTimeControl();
	~CMTimeControl();

	static double UseTime(bool isEnd = false); 
	//-------------------------part of timer----------------
	void	SetEvent(ONTIME  pfunTime,void *pParam);

	int		StartTime(BYTE bType,DWORD dwTimePeriod );
	void	StopTime();
	int		GetTimeCount();
	void	AdjustTime(BYTE bType,DWORD dwTimePeriod );		//调节时间

	inline	bool IsUse()		{ return m_dwTimerID != 0;}			//定时器是否已经运行起来
private:
	static void CALLBACK _TimeCallBack(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2);	
private:
	ONTIME  m_pfunTime;
	//data for timer
	DWORD	m_dwTimerID;
	DWORD	m_dwTimeCount;	
	bool	m_bAdjustTimer;//false adjust  true timer
	CMTime	m_tmStartTime;
	BYTE	m_bType;//Timer Time Type
	BYTE	m_bCurType;//Current Time Type

	DWORD	m_dwTimePeriod;		//定时间,默认时间

	void*	m_pParam;		//
};
#endif