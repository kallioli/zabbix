/*
** Copyright (C) 2001-2026 Zabbix SIA
**
** This program is free software: you can redistribute it and/or modify it under the terms of
** the GNU Affero General Public License as published by the Free Software Foundation, version 3.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
** without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public License along with this program.
** If not, see <https://www.gnu.org/licenses/>.
**/

/* The handful of POSIX functions the proxy calls that MSVC does not provide. Declared in include/common/zbxsysinc.h */
/* next to the other Windows shims. */

#include "zbxcommon.h"

int	nanosleep(const struct timespec *req, struct timespec *rem)
{
	DWORD	ms;

	if (NULL != rem)
	{
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}

	if (NULL == req || 0 > req->tv_sec || 0 > req->tv_nsec || 999999999L < req->tv_nsec)
	{
		errno = EINVAL;
		return -1;
	}

	/* Sleep() has millisecond resolution; round up so that a sub-millisecond request still yields the processor */
	/* rather than spinning */
	ms = (DWORD)(req->tv_sec * 1000) + (DWORD)((req->tv_nsec + 999999L) / 1000000L);
	Sleep(ms);

	return 0;
}

unsigned int	sleep(unsigned int seconds)
{
	Sleep((DWORD)seconds * 1000);

	return 0;
}

int	gettimeofday(struct timeval *tv, void *tz)
{
	/* the FILETIME epoch is 1601-01-01, 11644473600 seconds before the Unix one */
#define ZBX_EPOCH_DIFF_SEC	11644473600ULL

	FILETIME	ft;
	ULARGE_INTEGER	t;

	ZBX_UNUSED(tz);

	if (NULL == tv)
	{
		errno = EINVAL;
		return -1;
	}

	GetSystemTimeAsFileTime(&ft);
	t.LowPart = ft.dwLowDateTime;
	t.HighPart = ft.dwHighDateTime;

	/* FILETIME counts 100-nanosecond intervals */
	tv->tv_sec = (long)(t.QuadPart / 10000000ULL - ZBX_EPOCH_DIFF_SEC);
	tv->tv_usec = (long)(t.QuadPart % 10000000ULL / 10);

	return 0;
#undef ZBX_EPOCH_DIFF_SEC
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses a time according to a format string                        *
 *                                                                            *
 * Comments: covers the conversions this tree actually uses - %Y %m %d %H %M  *
 *           %S %y %z and %c - and fails on anything else rather than         *
 *           silently misreading it. %c is taken as the C locale form,        *
 *           which is what produced the string in the first place.            *
 *                                                                            *
 * Return value: pointer past the last character consumed, or NULL if the     *
 *               input does not match the format                              *
 *                                                                            *
 ******************************************************************************/
static const char	*strptime_num(const char *s, int width, int min, int max, int *out)
{
	int	value = 0, digits = 0;

	while (' ' == *s)
		s++;

	while (digits < width && 0 != isdigit((unsigned char)*s))
	{
		value = value * 10 + (*s - '0');
		s++;
		digits++;
	}

	if (0 == digits || value < min || value > max)
		return NULL;

	*out = value;

	return s;
}

char	*strptime(const char *s, const char *format, struct tm *tm)
{
	const char	*f = format;
	int		value;

	while ('\0' != *f)
	{
		if ('%' != *f)
		{
			if (' ' == *f)
			{
				while (' ' == *s)
					s++;
			}
			else if (*s++ != *f)
			{
				return NULL;
			}

			f++;
			continue;
		}

		switch (*++f)
		{
			case 'Y':
				if (NULL == (s = strptime_num(s, 4, 0, 9999, &value)))
					return NULL;
				tm->tm_year = value - 1900;
				break;
			case 'y':
				if (NULL == (s = strptime_num(s, 2, 0, 99, &value)))
					return NULL;
				tm->tm_year = (69 <= value ? value : value + 100);
				break;
			case 'm':
				if (NULL == (s = strptime_num(s, 2, 1, 12, &value)))
					return NULL;
				tm->tm_mon = value - 1;
				break;
			case 'd':
				if (NULL == (s = strptime_num(s, 2, 1, 31, &value)))
					return NULL;
				tm->tm_mday = value;
				break;
			case 'H':
				if (NULL == (s = strptime_num(s, 2, 0, 23, &value)))
					return NULL;
				tm->tm_hour = value;
				break;
			case 'M':
				if (NULL == (s = strptime_num(s, 2, 0, 59, &value)))
					return NULL;
				tm->tm_min = value;
				break;
			case 'S':
				/* a leap second is representable */
				if (NULL == (s = strptime_num(s, 2, 0, 60, &value)))
					return NULL;
				tm->tm_sec = value;
				break;
			case 'z':
				/* +hhmm, -hhmm or Z; the offset is consumed but not applied, matching what the */
				/* callers expect from a struct tm */
				if ('Z' == *s)
				{
					s++;
				}
				else if ('+' == *s || '-' == *s)
				{
					s++;

					if (NULL == (s = strptime_num(s, 2, 0, 23, &value)))
						return NULL;

					if (':' == *s)
						s++;

					if (NULL == (s = strptime_num(s, 2, 0, 59, &value)))
						return NULL;
				}
				else
					return NULL;
				break;
			case 'c':
				if (NULL == (s = strptime(s, "%m/%d/%y %H:%M:%S", tm)))
					return NULL;
				break;
			case '%':
				if ('%' != *s++)
					return NULL;
				break;
			default:
				/* an unsupported conversion must not be read as a match */
				return NULL;
		}

		f++;
	}

	return (char *)s;
}
