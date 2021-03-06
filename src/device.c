/*
 * device.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>

#include "options.h"
#include "defines.h"
#include "device.h"
#include "args.h"
#include "str.h"

#ifdef OPTION_EFM
#include "efm.h"
#endif

//#define DEBUG

/* type, cish_string, linux_string, web_string */
dev_family _devices[] = {
	{ eth, "ethernet", "eth", "Ethernet" },
	{ lo, "loopback", "lo", "Loopback" },
#ifdef OPTION_TUNNEL
	{ tun, "tunnel", "tunnel", "Tunnel" },
#endif
#ifdef OPTION_IPSEC
	{ ipsec, "ipsec", "ipsec", "IPSec" },
#endif
#ifdef OPTION_PPP
	{ ppp, "m3G", "ppp", "3GModem" },
#endif
#ifdef OPTION_EFM
	{ efm, "efm", "eth", "EFM" },
#endif
#ifdef OPTION_PPTP
	{ pptp, "pptp", "ppp", "PPTP" },
#endif
#ifdef OPTION_PPPOE
	{ pppoe, "pppoe", "ppp", "PPPoE" },
#endif
#ifdef OPTION_BRIDGE
	{ bridge, "bridge", "bridge", "Bridge"},
#endif
#ifdef OPTION_WIFI
	{ wlan, "wlan", "wlan", "Wlan" },
#endif
	{ none, NULL, NULL, NULL }
};

/**
 * librouter_device_get_family	Discover family based on name
 *
 *
 * @param name
 * @param type
 * @return pointer to family struct if success, NULL otherwise.
 */
dev_family *librouter_device_get_family_by_name(const char *name, string_type type)
{
	int i;

	char *string;
	char offset = 0;
	dev_family *fam = _devices;

	/* Discover the offset inside the family structure */
	switch(type) {
	case str_cish:
		offset = offsetof(dev_family, cish_string);
		break;
	case str_linux:
		offset = offsetof(dev_family, linux_string);
		break;
	case str_web:
		offset = offsetof(dev_family, web_string);
		break;
	default:
		break;
	}

	/* Use the offset to access the desired string */
	for (; *(char **)(((void *)fam) + offset) != NULL; fam++) {
		char *string = *(char **)(((void *)fam) + offset);

		if (!strncmp(name, string, strlen(string)))
			return fam;
	}

	return NULL;
}

/**
 * librouter_device_get_family_by_type	Discover family based on type
 *
 * @param type
 * @return pointer to family structure, NULL if not found
 */
dev_family *librouter_device_get_family_by_type(device_type type)
{
	int i;


	for (i = 0; _devices[i].type != none; i++) {
		if (type == _devices[i].type)
			return &_devices[i];
	}

	return NULL;
}

/**
 * librouter_device_get_minor	Get Minor number from interface name
 * e.g. : ethernet0.100 -> 100
 *
 * @param name
 * @param type
 * @return interface minor number or -1 if failure
 */
int librouter_device_get_minor(const char *name, string_type type)
{
	dev_family *fam = librouter_device_get_family_by_name(name, type);
	int minor;
	char *n;

	if (fam == NULL)
		return -1;

	n = strstr(name, "."); /* All sub-interfaces have a dot, e.g. eth1.50 */
	if (n == NULL)
		return -1;

	minor = atoi(++n);

	return minor;
}

/**
 * librouter_device_get_major	Get Major number from interface name
 * e.g. : ethernet0 -> 0
 *
 * @param name
 * @param type
 * @return interface major number or -1 if failure
 */
int librouter_device_get_major(const char *name, string_type type)
{
	int major;
	dev_family *fam = librouter_device_get_family_by_name(name, type);
	int len;

	if (fam == NULL)
		return -1;

	len = strlen(fam->linux_string);
	major = atoi(&name[len]);

	return major;
}

/**
 * librouter_device_cli_to_linux	Convert a cish interface string to linux interface string
 *
 * FIXME: Maybe the function name should indicate that it is a cish to
 * linux name convertion.
 *
 * ex.: device = 'serial', major = 0, minor = 16
 * retorna 'serial0.16'
 *
 * @param device
 * @param major
 * @param minor
 * @return
 */
char *librouter_device_cli_to_linux(const char *device, int major, int minor)
{
	char *result;
	dev_family *fam = librouter_device_get_family_by_name(device, str_cish);

	if (fam) {
		switch (fam->type) {
		case pptp:
			major += PPTP_PPP_START;
			break;
		case pppoe:
			major += PPPOE_PPP_START;
			break;
		default:
			break;
		}

		if (minor >= 0) {
			result = (char *) malloc(strlen(fam->linux_string) + 12);
			sprintf(result, "%s%i.%i", fam->linux_string, major, minor);
			return result;
		} else {
			result = (char *) malloc(strlen(fam->linux_string) + 6);
			sprintf(result, "%s%i", fam->linux_string, major);
			return result;
		}
	} else {
		fprintf(stderr, "%% Unknown device family: %s\n", device);
		return (strdup("null0"));
	}
}

/**
 * ppp_convert_management	Convert PPP interface to the deserved type, like PPTP, PPPOE...
 *
 * @param osdev
 * @param crsr
 * @param ppp_index
 * @return cishdev
 */
static const char * ppp_convert_management(const char *osdev, int * crsr, int ppp_index)
{
	int j;
	const char *cishdev;

	if ( (atoi(osdev+*crsr) > M3G_PPP_END) ){

		switch ( (osdev+*crsr)[0] ){

			case '2':
				/*PPTP*/
				for (j = 0; _devices[j].type != none; j++) {
					if (_devices[j].type == pptp){
						cishdev = _devices[j].cish_string;
						++*crsr; /* shift++ para pegar só a unidade do ppp -> (ex: ppp20 -> ppp0) */
						break;
					}
				}
				break;

			case '3':
				/*PPPOE*/
				for (j = 0; _devices[j].type != none; j++) {
					if (_devices[j].type == pppoe){
						cishdev = _devices[j].cish_string;
						++*crsr; /* shift++ para pegar só a unidade do ppp -> (ex: ppp30 -> ppp0) */
						break;
					}
				}
				break;

			default:
				break;

		}

	}
	else
		cishdev = _devices[ppp_index].cish_string;

	return cishdev;
}

/**
 * librouter_device_linux_to_cli	Convert a linux string to cish string
 * ex.: osdev = 'serial0.16'
 * retorna 'serial 0.16' se mode=0,
 * ou      'serial0.16'  se mode=1.
 *
 * @param osdev
 * @param mode
 * @return
 */
char *librouter_device_linux_to_cli(const char *osdev, int mode)
{
	static char dev[64];
	char odev[16];
	int i, crsr;
	const char *cishdev;
	int iface_index = -1, subiface_index = -1;

	crsr = 0;
	while ((crsr < 8) && (osdev[crsr] > 32) && (!isdigit(osdev[crsr])))
		++crsr;
	memcpy(odev, osdev, crsr);
	odev[crsr] = 0;

	while ((osdev[crsr]) && (!isdigit(osdev[crsr])))
		++crsr; /* skip space! */

	iface_index = atoi(&osdev[crsr]);

	/* Search for Sub-Interfaces (VLANs, Frame-Relay DLCIs, etc...) */
	if (strstr(osdev, ".") != NULL)
		subiface_index = atoi(strstr(osdev, ".") + 1); /* skip the dot */

	cishdev = NULL;

	for (i = 0; _devices[i].linux_string != NULL; i++) {
		if (strcmp(_devices[i].linux_string, odev) == 0) {
			if ( !strcmp(odev,"ppp") )
				cishdev = ppp_convert_management(osdev, &crsr, i);
			else
				cishdev = _devices[i].cish_string;
			break;
		}
	}

	if (!cishdev)
		return NULL;

#ifdef OPTION_IPSEC
	if (_devices[i].type == ipsec) {
		char filename[32];
		char iface[16];
		FILE *f;

		sprintf(filename, "/var/run/%s", osdev);
		if ((f = fopen(filename, "r")) != NULL) {
			fgets(iface, 16, f);
			fclose(f);
			librouter_str_striplf(iface);
			crsr = 0;
			while ((crsr < 8) && (iface[crsr] > 32) && (!isdigit(
			                iface[crsr])))
				++crsr; /* !!! ethernet == 8 */
			memcpy(odev, iface, crsr);
			odev[crsr] = 0;
			sprintf(dev, "crypto-%s%s%s", odev, mode ? "" : " ",
			                iface + crsr);
		} else
			return NULL;
	}
#endif
#ifdef OPTION_EFM
	/* Need to check if this eth interface is not in EFM for the CLI */
	else if ((_devices[i].type == eth) && (iface_index >= EFM_INDEX_OFFSET) &&
			iface_index <= (EFM_INDEX_OFFSET + EFM_NUM_INTERFACES)) {
		dev_family *fam = librouter_device_get_family_by_type(efm);

		/* Search for VLANs */
		if (subiface_index >= 0)
			sprintf(dev, "%s%s%d.%d", fam->cish_string, mode ? "" : " ", iface_index - EFM_INDEX_OFFSET, subiface_index);
		else
			sprintf(dev, "%s%s%d", fam->cish_string, mode ? "" : " ", iface_index - EFM_INDEX_OFFSET);

	}
#endif
	else
		sprintf(dev, "%s%s%s", cishdev, mode ? "" : " ", osdev + crsr);

	/* Make ethernet -> Ethernet */
	if (mode)
		dev[0] = toupper(dev[0]);

	return dev;
}

/**
 * Recebe uma linha de comando com interfaces no estilo cish (ex.: 'ethernet 0')
 * e devolve a linha de comando com as interfaces traduzidos para estilo linux
 * (ex.: 'eth0').
 *
 * @param cmdline
 * @return
 */
char *librouter_device_to_linux_cmdline(char *cmdline)
{
	static char new_cmdline[2048];
	arglist *args;
	int i;
	dev_family *fam;
	char * ppp_init = NULL;

	new_cmdline[0] = 0;
	args = librouter_make_args(cmdline);

	for (i = 0; i < args->argc; i++) {
		fam = librouter_device_get_family_by_name(args->argv[i], str_cish);
		if (fam) {
			strcat(new_cmdline, fam->linux_string);
			i++;
			if (fam->type == pptp) {
				ppp_init = malloc(2);
				sprintf(ppp_init, "%d", PPTP_PPP_START);
				strcat(new_cmdline, ppp_init);
				strcat(new_cmdline, " "); /* Deve fazer algum sentido */
				i++;
				free(ppp_init);
			}
			if (fam->type == pppoe) {
				ppp_init = malloc(2);
				sprintf(ppp_init, "%d", PPPOE_PPP_START);
				strcat(new_cmdline, ppp_init);
				strcat(new_cmdline, " ");
				i++;
				free(ppp_init);
			}
#ifdef OPTION_EFM
			if (fam->type == efm) {
				int idx = atoi(args->argv[i]);
				char idx_str[8];
				idx += EFM_INDEX_OFFSET;
				sprintf(idx_str, "%d ", idx);
				strcat(new_cmdline, idx_str);
				i++;
			}
#endif
			if (i >= args->argc)
				break;
		}
		strcat(new_cmdline, args->argv[i]);
		strcat(new_cmdline, " ");
	}

	librouter_destroy_args(args);
	return new_cmdline;
}

/**
 * Recebe uma linha de comando com interfaces no estilo linux (ex.: 'eth0')
 * e devolve a linha de comando com as interfaces traduzidos para estilo cish
 * (ex.: 'ethernet 0').
 *
 * @param cmdline
 * @return
 */
char *librouter_device_from_linux_cmdline(char *cmdline)
{
	static char new_cmdline[2048];
	arglist *args;
	int i;
	char *dev;

	new_cmdline[0] = 0;
	if (librouter_str_is_empty(cmdline))
		return new_cmdline;
	args = librouter_make_args(cmdline);
	for (i = 0; i < args->argc; i++) {
		dev = librouter_device_linux_to_cli(args->argv[i], 0);
		if (dev)
			strcat(new_cmdline, dev);
		else
			strcat(new_cmdline, args->argv[i]);
		strcat(new_cmdline, " ");
	}
	librouter_destroy_args(args);
	return new_cmdline;
}


/**
 * Recebe uma linha de comando com redes no estilo zebra (ex.: '10.0.0.0/8')
 * e devolve a linha de comando com as redes traduzidas para estilo linux
 * (ex.: '10.0.0.0 255.0.0.0').
 *
 * @param cmdline
 * @return
 */
char *librouter_zebra_to_linux_cmdline(char *cmdline)
{
	static char new_cmdline[2048];
	arglist *args;
	int i;
	char addr_net[64];

	new_cmdline[0] = 0;
	if (librouter_str_is_empty(cmdline))
		return new_cmdline;

	args = librouter_make_args(cmdline);

	for (i = 0; i < args->argc; i++) {
		if (librouter_quagga_cidr_to_classic(args->argv[i], addr_net) == 0)
			strcat(new_cmdline, addr_net);
		else
			strcat(new_cmdline, args->argv[i]);
		strcat(new_cmdline, " ");
	}

	librouter_destroy_args(args);
	return new_cmdline;
}

/**
 * Recebe uma linha de comando com redes no estilo linux
 * (ex.: '10.0.0.0 255.0.0.0') e devolve a linha de comando
 * com as redes traduzidas para estilo zebra (ex.: '10.0.0.0/8')
 *
 * @param cmdline
 * @return
 */
char *librouter_zebra_from_linux_cmdline(char *cmdline)
{
	static char new_cmdline[2048];
	arglist *args;
	int i;
	char buf[64];

	new_cmdline[0] = 0;
	if (librouter_str_is_empty(cmdline))
		return new_cmdline;

	args = librouter_make_args(cmdline);

	for (i = 0; i < (args->argc - 1); i++) {
		if ((librouter_quagga_validate_ip(args->argv[i]) == 0)
		                && (librouter_quagga_classic_to_cidr(args->argv[i], args->argv[i + 1], buf) == 0)) {
			strcat(new_cmdline, buf);
			i++;
		} else {
			strcat(new_cmdline, args->argv[i]);
		}
		strcat(new_cmdline, " ");
	}
	if (i < args->argc)
		strcat(new_cmdline, args->argv[i]);

	librouter_destroy_args(args);
	return new_cmdline;
}
