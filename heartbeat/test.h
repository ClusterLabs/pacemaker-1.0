/*
 * test.h: header for special test code inside of heartbeat
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __TEST_H
#	define __TEST_H 1

#include <stdlib.h>
struct TestParms {
	int	enable_send_pkt_loss;
	int	enable_rcv_pkt_loss;
	float	send_loss_prob;
	float	rcv_loss_prob;
	char	allow_nodes[512];
};

extern struct TestParms *	TestOpts;

#define	TESTSEND	(TestOpts && TestOpts->enable_send_pkt_loss)
#define	TESTRCV		(TestOpts && TestOpts->enable_rcv_pkt_loss)

#ifdef  __GNUC__
#define RandThresh(p) ((1.0*rand()) <= ((((double)RAND_MAX) * ((double)p))))
#else
#define RandThresh(p) ((double)(rand()) <= ((((double)RAND_MAX) * ((double)p))))
#endif

#define TestRand(field)	(TestOpts && RandThresh(TestOpts->field))
int ParseTestOpts(void);

#endif /* __TEST_H */
