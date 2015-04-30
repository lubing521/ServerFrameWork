#include "GlobalDefine.h"

char * xe_strstr( const char* src,const char* szFind )
{
	if (src == NULL || szFind == NULL)
		return NULL;

	char * cp = (char*)src;

	//没有字符
	if (*szFind == '\0')
		return NULL;

	char * s1, *s2;

	//循环查找
	while(*cp)
	{
		s1 = cp;
		s2 = (char *)szFind;

		//相同
		while(*s1 && *s2 && !(*s1 - *s2))
			s1++,s2++;

		//末尾
		if (!*s2)
			return s1;

		//否则
		if (*cp++ < 0)
			cp++;
	}

	return NULL;
}

char * xe_strchr(const char* src,int ch)
{
	if(src == NULL)
		return NULL;

	char * cp = (char*)src;

	//不等于
	while(*cp && (*cp != (char)ch))
	{
		if (*cp++ < 0)
			cp++;
	}

	if (*cp == (char)ch)
		return cp;
	
	return NULL;
}


xe_int32 AnsiStrToVal( const char *nptr )
{
	xe_int32 c	= (xe_int32)(unsigned char)*nptr++;
	xe_int32 total	= 0;

	while (c >= '0' && c <= '9') 
	{
		total = 10 * total + (c - '0');     /* accumulate digit */
		c = (xe_int32)(unsigned char)*nptr++;    /* get next char */
	}

	return total;
}

char* ValToAnsiStr( xe_uint32 val, char *buf )
{
	char *p;                /* pointer to traverse string */
    char *firstdig;         /* pointer to first digit */
    char temp;              /* temp char */
	char *next;
    unsigned digval;        /* value of digit */

    p = buf;

    firstdig = p;           /* save pointer to first digit */

    do {
        digval = (unsigned) (val % 10);
        val /= 10;	       /* get next digit */

        /* convert to ascii and store */
        if (digval > 9)
            *p++ = (char) (digval - 10 + 'a');  /* a letter */
        else
            *p++ = (char) (digval + '0');       /* a digit */
    } while (val > 0);

    /* We now have the digit of the number in the buffer, but in reverse
       order.  Thus we reverse them now. */

	next = p;
    *p-- = '\0';            /* terminate string; p points to last digit */

	//翻转
    do {
        temp = *p;
        *p = *firstdig;
        *firstdig = temp;   /* swap *p and *firstdig */
        --p;
        ++firstdig;         /* advance to next two digits */
    } while (firstdig < p); /* repeat until halfway */

	return next;
}


int memlen(const char *str)
{
	const char *eos = str;

	while(*eos++);

	return((int)(eos - str));
}
