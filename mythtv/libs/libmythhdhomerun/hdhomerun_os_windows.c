/*
 * hdhomerun_os_windows.c
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
	HCRYPTPROV hProv;
	if (!CryptAcquireContext(&hProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		return (uint32_t)rand();
	}

	uint32_t Result;
	CryptGenRandom(hProv, sizeof(Result), (BYTE*)&Result);

	CryptReleaseContext(hProv, 0);
	return Result;
}

uint64_t getcurrenttime(void)
{
	static pthread_mutex_t lock = INVALID_HANDLE_VALUE;
	static uint64_t result = 0;
	static uint32_t previous_time = 0;

	/* Initialization is not thread safe. */
	if (lock == INVALID_HANDLE_VALUE) {
		pthread_mutex_init(&lock, NULL);
	}

	pthread_mutex_lock(&lock);

	uint32_t current_time = GetTickCount();

	if (current_time > previous_time) {
		result += current_time - previous_time;
	}

	previous_time = current_time;

	pthread_mutex_unlock(&lock);
	return result;
}

void msleep_approx(uint64_t ms)
{
	Sleep((DWORD)ms);
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

int pthread_create(pthread_t *tid, void *attr, LPTHREAD_START_ROUTINE start, void *arg)
{
	*tid = CreateThread(NULL, 0, start, arg, 0, NULL);
	if (!*tid) {
		return (int)GetLastError();
	}
	return 0;
}

int pthread_join(pthread_t tid, void **value_ptr)
{
	while (1) {
		DWORD ExitCode = 0;
		if (!GetExitCodeThread(tid, &ExitCode)) {
			return (int)GetLastError();
		}
		if (ExitCode != STILL_ACTIVE) {
			return 0;
		}
	}
}

void pthread_mutex_init(pthread_mutex_t *mutex, void *attr)
{
	*mutex = CreateMutex(NULL, FALSE, NULL);
}

void pthread_mutex_lock(pthread_mutex_t *mutex)
{
	WaitForSingleObject(*mutex, INFINITE);
}

void pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	ReleaseMutex(*mutex);
}

/*
 * The console output format should be set to UTF-8, however in XP and Vista this breaks batch file processing.
 * Attempting to restore on exit fails to restore if the program is terminated by the user.
 * Solution - set the output format each printf.
 */
void console_vprintf(const char *fmt, va_list ap)
{
	UINT cp = GetConsoleOutputCP();
	SetConsoleOutputCP(CP_UTF8);
	vprintf(fmt, ap);
	SetConsoleOutputCP(cp);
}

void console_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	console_vprintf(fmt, ap);
	va_end(ap);
}
