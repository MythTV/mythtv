/*
 * hdhomerun_os_posix.c
 *
 * Copyright © 2006-2010 Silicondust USA Inc. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * As a special exception to the GNU Lesser General Public License,
 * you may link, statically or dynamically, an application with a
 * publicly distributed version of the Library to produce an
 * executable file containing portions of the Library, and
 * distribute that executable file under terms of your choice,
 * without any of the additional requirements listed in clause 4 of
 * the GNU Lesser General Public License.
 * 
 * By "a publicly distributed version of the Library", we mean
 * either the unmodified Library as distributed by Silicondust, or a
 * modified version of the Library that is distributed under the
 * conditions defined in the GNU Lesser General Public License.
 */

#include "hdhomerun_os.h"

uint32_t random_get32(void)
{
	FILE *fp = fopen("/dev/urandom", "rb");
	if (!fp) {
		return (uint32_t)getcurrenttime();
	}

	uint32_t Result;
	if (fread(&Result, 4, 1, fp) != 1) {
		Result = (uint32_t)getcurrenttime();
	}

	fclose(fp);
	return Result;
}

uint64_t getcurrenttime(void)
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint64_t result = 0;
	static uint64_t previous_time = 0;

	pthread_mutex_lock(&lock);

#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	uint64_t current_time = ((uint64_t)tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	uint64_t current_time = ((uint64_t)t.tv_sec * 1000) + (t.tv_usec / 1000);
#endif

	if (current_time > previous_time) {
		result += current_time - previous_time;
	}

	previous_time = current_time;

	pthread_mutex_unlock(&lock);
	return result;
}

void msleep_approx(uint64_t ms)
{
	unsigned int delay_s = ms / 1000;
	if (delay_s > 0) {
		sleep(delay_s);
		ms -= delay_s * 1000;
	}

	unsigned int delay_us = ms * 1000;
	if (delay_us > 0) {
		usleep(delay_us);
	}
}

void msleep_minimum(uint64_t ms)
{
	uint64_t stop_time = getcurrenttime() + ms;

	while (1) {
		uint64_t current_time = getcurrenttime();
		if (current_time >= stop_time) {
			return;
		}

		msleep_approx(stop_time - current_time);
	}
}

bool_t hdhomerun_vsprintf(char *buffer, char *end, const char *fmt, va_list ap)
{
	if (buffer >= end) {
		return FALSE;
	}

	int length = vsnprintf(buffer, end - buffer - 1, fmt, ap);
	if (length < 0) {
		*buffer = 0;
		return FALSE;
	}

	if (buffer + length + 1 > end) {
		*(end - 1) = 0;
		return FALSE;

	}

	return TRUE;
}

bool_t hdhomerun_sprintf(char *buffer, char *end, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	bool_t result = hdhomerun_vsprintf(buffer, end, fmt, ap);
	va_end(ap);
	return result;
}
