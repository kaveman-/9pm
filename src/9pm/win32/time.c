#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>

#define SEC2MIN 60L
#define SEC2HOUR (60L*SEC2MIN)
#define SEC2DAY (24L*SEC2HOUR)

/*
 *  days per month plus days/year
 */
static	int	dmsize[] =
{
	365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static	int	ldmsize[] =
{
	366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int	*yrsize(int yr);
long	tm2sec(SYSTEMTIME *tm);
long	tm2sec2(SYSTEMTIME *tm);

long
time(long *tp)
{
	SYSTEMTIME tm;
	long t;

	GetSystemTime(&tm);

	t = tm2sec(&tm);
	if(tp)
		*tp = t;

	return t;
}

long
tm2sec(SYSTEMTIME *tm)
{
	long secs;
	int i, *d2m;

	secs = 0;

	/*
	 *  seconds per year
	 */
	for(i = 1970; i < tm->wYear; i++){
		d2m = yrsize(i);
		secs += d2m[0] * SEC2DAY;
	}
	/*
	 *  seconds per month
	 */
	d2m = yrsize(tm->wYear);
	for(i = 1; i < tm->wMonth; i++)
		secs += d2m[i] * SEC2DAY;

	/*
	 * secs in last month
	 */
	secs += (tm->wDay-1) * SEC2DAY;

	/*
	 * hours, minutes, seconds
	 */
	secs += tm->wHour * SEC2HOUR;
	secs += tm->wMinute * SEC2MIN;
	secs += tm->wSecond;

	return secs;
}

long
tm2sec2(SYSTEMTIME *tm)
{
	long secs, days;
	int i, j, *d2m;

	secs = 0;
	days = 4;

	/*
	 *  seconds per year
	 */
	for(i = 1970; i < tm->wYear; i++){
		d2m = yrsize(i);
		secs += d2m[0] * SEC2DAY;
		days += d2m[0];
	}
	/*
	 *  seconds per month
	 */
	d2m = yrsize(tm->wYear);
	for(i = 1; i < tm->wMonth; i++) {
		secs += d2m[i] * SEC2DAY;
		days += d2m[i];
	}

	days %= 7;
	
	if(tm->wDay < 5) {
		for(i=0, j=0; i<d2m[tm->wMonth]; i++) {
			if(((days+i)%7) != tm->wDayOfWeek)
				continue;
			j++;
			if(j == tm->wDay)
				break;
		}
	} else {
		for(i=d2m[tm->wMonth]-1; i>=0; i--)
			if(((days+i)%7) == tm->wDayOfWeek)
				break;
	}

	/*
	 * secs in last month
	 */
	secs += i * SEC2DAY;

	/*
	 * hours, minutes, seconds
	 */
	secs += tm->wHour * SEC2HOUR;
	secs += tm->wMinute * SEC2MIN;
	secs += tm->wSecond;

	return secs;
}

/*
 *  return the days/month for the given year
 */
static int *
yrsize(int yr)
{
	if((yr % 4 == 0) && (yr % 100 != 0 || yr % 400 == 0))
		return ldmsize;
	else
		return dmsize;
}

void
tzname(Rune s[4], Rune *p)
{
	int i;
	Rune *q; 
	int c;

	s[3] = 0;

	/* try getting first three captials */
	for(i=0,q=p; i<3&&*q; q++) {
		c = *q;
		if(c >= L'A' && c <= L'Z')
			s[i++] = c;
	}
	
	if(i == 3)
		return;

	/* try first three letters */
	for(i=0,q=p; i<3&&*q; q++) {
		c = *q;
		if(c != L' ')
			s[i++] = c;
	}

	while(i < 3)
		s[i] = L'?';
}


void
timezoneinit(void)
{
	TIME_ZONE_INFORMATION tz;
	char buf[8000], *p;
	Rune s0[4], s1[4];
	int st, dt, i, sb, db;
	
	i = GetTimeZoneInformation(&tz);
	switch(i) {
	default:
		return;
	case TIME_ZONE_ID_UNKNOWN:
		sprint(buf, "??? %d ??? %d", -tz.Bias*60, -tz.Bias*60);
		putenv("timezone", buf);
		break;
	case TIME_ZONE_ID_STANDARD:
	case TIME_ZONE_ID_DAYLIGHT:
		sb = -60*tz.Bias;
		db = -60*(tz.Bias + tz.DaylightBias);
		break;
	}

 	tz.StandardDate.wYear = 1970;
	tz.DaylightDate.wYear = 1970;

	st = tm2sec2(&tz.StandardDate);
	dt = tm2sec2(&tz.DaylightDate);

	tzname(s0, tz.StandardName);
	tzname(s1, tz.DaylightName);

	p = buf;
	if(st < dt) {
		p += snprint(p, nelem(buf)-(p-buf), "%S %d %S %d\n", s1, db, s0, sb);
		for(i=0; i<68; i++) {
			tz.StandardDate.wYear = 1970+i;
			tz.DaylightDate.wYear = 1970+i;
			st = tm2sec2(&tz.StandardDate);
			dt = tm2sec2(&tz.DaylightDate);
			p += snprint(p, nelem(buf)-(p-buf), "%10d %10d",
				st+tz.DaylightBias*60, dt);
			if(i %3 < 2)
				*p++ = ' ';
			else
				*p++ = '\n';
		}
	} else {
		p += snprint(p, nelem(buf)-(p-buf), "%S %d %S %d\n", s0, sb, s1, db);
		for(i=0; i<68; i++) {
			tz.StandardDate.wYear = 1970+i;
			tz.DaylightDate.wYear = 1970+i;
			st = tm2sec2(&tz.StandardDate);
			dt = tm2sec2(&tz.DaylightDate);
			p += snprint(p, nelem(buf)-(p-buf), "%10d %10d", dt, st+tz.DaylightBias*60);
			if(i %3 < 2)
				*p++ = ' ';
			else
				*p++ = '\n';
		}
	}
	*p++ = '\n';
	*p++ = 0;

	putenv("timezone", buf);
}
