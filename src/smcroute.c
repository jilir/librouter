/*
 * smcroute.c
 *
 *  Created on: May 31, 2010
 *      Author: PD3 Tecnologia (tgrande@pd3.com.br)
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>

#include "options.h"
#include "error.h"
#include "exec.h"
#include "defines.h"
#include "device.h"
#include "dev.h"
#include "ip.h"
#include "smcroute.h"

#ifdef OPTION_SMCROUTE

void librouter_smc_route_hup(void)
{
	FILE *F;
	char buf[32];

	F = fopen("/var/run/smcroute.pid", "r");
	if (F) {
		fgets(buf, 32, F);
		kill((pid_t) atoi(buf), SIGHUP);
		fclose(F);
	}
}

int librouter_smc_route(int add, char *origin, char *group, char *in, char *out)
{
	FILE *f;
	int i, n, t;
	struct smc_route_database database[SMC_ROUTE_MAX];
	char cmd[768];

	/* database clean */
	memset(&database[0], 0, sizeof(database));
	f = fopen(SMC_ROUTE_CONF, "rb");

	if (f) {
		fread(&database[0], sizeof(database), 1, f);
		fclose(f);
	}

	for (i = 0, n = -1, t = -1; i < SMC_ROUTE_MAX; i++) {
		if (database[i].valid) {
			if (!strcmp(origin, database[i].origin) && !strcmp(
			                group, database[i].group) && !strcmp(
			                in, database[i].in) && !strcmp(out,
			                database[i].out))
				n = i; /* match found */
		} else {
			if (t == -1)
				t = i; /* empty space */
		}
	}

	if (add) {
		if (n != -1) {
			//pr_error(0, "mroute allready added");
			return (-1);
		}

		if (t != -1) {
			database[t].valid = 1; /* add */
			strcpy(database[t].origin, origin);
			strcpy(database[t].group, group);
			strcpy(database[t].in, in);
			strcpy(database[t].out, out);
		} else {
			librouter_pr_error(0, "mroute table full");
			return (-1);
		}
	} else {
		if (n != -1) {
			memset(&database[n], 0,
			                sizeof(struct smc_route_database)); /* del */
		} else {
			librouter_pr_error(0, "mroute not found");
			return (-1);
		}
	}

	f = fopen(SMC_ROUTE_CONF, "wb");
	if (f) {
		fwrite(&database[0], sizeof(database), 1, f);
		fclose(f);
	}

	/* add/del */
	for (i = 0; i < SMC_ROUTE_MAX; i++)
		if (database[i].valid == 1)
			break;

	if (i == SMC_ROUTE_MAX) {
		librouter_exec_init_program(0, SMC_DAEMON);
	} else {
		if (!librouter_exec_check_daemon(SMC_DAEMON)) {
			librouter_exec_init_program(1, SMC_DAEMON);
			sleep(1);
		}

		if (add)
			sprintf(cmd, "/bin/smcroute -a %s %s %s %s >/dev/null 2>/dev/null", in, origin, group, out);
		else
			sprintf(cmd, "/bin/smcroute -r %s %s %s >/dev/null 2>/dev/null", in, origin, group);

		system(cmd);
	}

	return 0;
}

void librouter_smc_route_dump(FILE *out)
{
	int i, print = 0;
	struct smc_route_database database[SMC_ROUTE_MAX];
	char buf[1024];
	FILE *f;

	/* database clean */
	memset(&database[0], 0, sizeof(database));
	f = fopen(SMC_ROUTE_CONF, "rb");

	if (f) {
		fread(&database[0], sizeof(database), 1, f);
		fclose(f);
	}

	for (i = 0; i < SMC_ROUTE_MAX; i++) {
		if (database[i].valid) {
			sprintf(buf, "ip mroute %s %s in %s out %s",
			                database[i].origin, database[i].group,
			                database[i].in, database[i].out);
			fprintf(out, "%s\n", librouter_device_from_linux_cmdline(buf));
			print = 1;
		}
	}

	if (print)
		fprintf(out, "!\n");
}

#endif

