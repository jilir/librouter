/*
 * error.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "error.h"

void librouter_pr_error(int output_strerror, char *fmt, ...)
{
	va_list args;
	char buf[1024];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	printf("%% %s", buf);

	if (output_strerror)
		printf(" (%s)", strerror(errno));

	printf("\n");
}

void librouter_logerr(char *fmt, ...)
{
	va_list args;
	char buf[1024];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	syslog(LOG_ERR, "%s : %d => %s", __FUNCTION__, __LINE__, buf);
}

