/*
 * lan.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/autoconf.h>
#include <netinet/in.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/hdlc.h>
#include <linux/sockios.h>
#include <linux/mii.h>
#include <linux/ethtool.h>

#include "typedefs.h"
#include "args.h"
#include "error.h"
#include "ppp.h"
#include "dev.h"
#include "defines.h"
#include "ppcio.h"
#include "lan.h"

//#define PHY_DEBUG

int librouter_lan_get_status(char *ifname, struct lan_status *st)
{
	int fd, err;
	char *p;
	struct ifreq ifr;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_get_status: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_data = (void *) st;

	/* vlan uses ethernetX status! */
	if ((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;

	err = ioctl(fd, SIOCGPHYSTATUS, &ifr);
	close(fd);

	if (err < 0) {
		if (errno != ENODEV)
			return 0;

		librouter_pr_error(1, "SIOCGPHYSTATUS");
		return -1;
	}

	return 0;
}

int librouter_lan_get_phy_reg(char *ifname, u16 regnum)
{
	int fd;
	char *p;
	struct ifreq ifr;
	struct mii_ioctl_data mii;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_get_phy_reg: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	/* vlan uses ethernetX status! */
	if ((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;

	ifr.ifr_data = (char *) &mii;
	mii.reg_num = regnum;

	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		close(fd);
		librouter_pr_error(1, "Error reading PHY register for %s: SIOCGMIIPHY", ifname);
		return -1;
	}
#ifdef PHY_DEBUG
	printf("SIOCGMIIPHY (0x%02x=0x%04x)\n", mii.reg_num, mii.val_out);
#endif
	close(fd);

	return (mii.val_out);
}

int librouter_lan_set_phy_reg(char *ifname, u16 regnum, u16 data)
{
	int fd;
	char *p;
	struct ifreq ifr;
	struct mii_ioctl_data mii;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_set_phy_reg: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	/* vlan uses ethernetX status! */
	if ((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;

	ifr.ifr_data = (char *) &mii;
	mii.reg_num = regnum;

	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		close(fd);
		librouter_pr_error(1, "lan_set_phy_reg: SIOCGMIIPHY");
		return -1;
	}

	mii.val_in = data;

	if (ioctl(fd, SIOCSMIIREG, &ifr) < 0) {
		close(fd);
		librouter_pr_error(1, "lan_set_phy_reg: SIOCSMIIREG");
		return -1;
	}

	close(fd);
	return 0;
}

#ifdef CONFIGURE_MDIX
static void _configure_auto_mdix(char *dev)
{
	int gpcr;

	if ((gpcr = librouter_lan_get_phy_reg(dev, MII_ADM7001_GPCR)) < 0)
	return;

	gpcr |= MII_ADM7001_GPCR_XOVEN;
	librouter_lan_set_phy_reg(dev, MII_ADM7001_GPCR, gpcr);
}
#endif

#undef ADVERTISE

int librouter_fec_autonegotiate_link(char *dev)
{
	int bmcr;
#ifdef ADVERTISE
	int advertise
#endif

#ifdef CONFIG_MDIX
	_configure_auto_mdix(dev);
#endif
#ifdef ADVERTISE
	if ((advertise = librouter_lan_get_phy_reg(dev, MII_ADVERTISE)) < 0)
	return -1;

	advertise |= (ADVERTISE_10HALF | ADVERTISE_10FULL | ADVERTISE_100HALF | ADVERTISE_100FULL);

	librouter_lan_set_phy_reg(dev, MII_ADVERTISE, advertise);
#endif
	if ((bmcr = librouter_lan_get_phy_reg(dev, MII_BMCR)) < 0)
		return -1;

	bmcr |= (BMCR_ANENABLE | BMCR_ANRESTART);

	if (librouter_lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
		return -1;

	return 0;
}

#undef VERIFY_LP

int librouter_fec_config_link(char *dev, int speed100, int duplex)
{
	int bmcr;
#ifdef VERIFY_LP
	int lpa , impossible = 0;
#endif

#ifdef CONFIG_MDIX
	_configure_auto_mdix(dev);
#endif
#ifdef VERIFY_LP
	/* Verify link partner */
	if ((lpa = librouter_lan_get_phy_reg(dev, MII_LPA)) < 0)
	return -1;

	if (speed100) {
		if (!(lpa & LPA_100))
		impossible++;
		else {
			if (duplex) {
				if (!(lpa & LPA_100FULL))
				impossible++;
			}
		}
	} else {
		if (duplex) {
			if (!(lpa & LPA_10FULL))
			impossible++;
		}
	}

	if (impossible) {
		printf("%% Link partner abilities are not enough for this mode\n");
		printf("%% Forcing mode anyway\n");
	}
#endif
	if ((bmcr = librouter_lan_get_phy_reg(dev, MII_BMCR)) < 0)
		return -1;

	if (!(bmcr & BMCR_PDOWN)) {
		/* backups */
		int advertise, icsr;

		if ((advertise = librouter_lan_get_phy_reg(dev, MII_ADVERTISE)) < 0)
			return -1;

		if ((icsr = librouter_lan_get_phy_reg(dev, 0x1b)) < 0)
			return -1;

		/* alert partner! */
		bmcr |= BMCR_PDOWN;

		if (librouter_lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
			return -1;

		sleep(1);
		bmcr &= (~BMCR_PDOWN);

		/* wake-up! (delay!) */
		if (librouter_lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
			return -1;

		/* KS8721 re-sample io pins! */
		/* restore! */
		if (librouter_lan_set_phy_reg(dev, 0x1b, icsr) < 0)
			return -1;

		/* restore! */
		if (librouter_lan_set_phy_reg(dev, MII_ADVERTISE, advertise) < 0)
			return -1;
	}

	/* disable auto-negotiate */
	bmcr &= ~BMCR_ANENABLE;

	if (speed100)
		bmcr |= BMCR_SPEED100;
	else
		bmcr &= (~BMCR_SPEED100);

	if (duplex)
		bmcr |= BMCR_FULLDPLX;
	else
		bmcr &= (~BMCR_FULLDPLX);

	if (librouter_lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
		return -1;

	return 0;
}
