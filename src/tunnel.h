/*
 * tunnel.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef TUNNEL_H_
#define TUNNEL_H_

typedef enum {
	TUNNEL_MODE,
	TUNNEL_SOURCE,
	TUNNEL_SOURCE_INTERFACE,
	TUNNEL_DESTINATION,
	TUNNEL_CHECKSUM,
	TUNNEL_SEQUENCE,
	TUNNEL_PMTU,
	TUNNEL_TTL,
	TUNNEL_KEY
} tunnel_param_type;

int librouter_tunnel_add(char *);
int librouter_tunnel_del(char *);
int librouter_tunnel_change(char *, tunnel_param_type, void *);
int librouter_tunnel_mode(char *, int);
void librouter_tunnel_dump_interface(FILE *, int, char *);

#endif /* TUNNEL_H_ */
