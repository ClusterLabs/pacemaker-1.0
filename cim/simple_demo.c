/*
 * Tests 
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include "cmpi_utils.h"
#include "cluster_info.h"
#include <ha_msg.h>
#include <hb_api.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
        char * msg;
        int i;
        char * result;

        if ( argc < 2 ) {
                return 0;
        }

        ci_lib_initialize();
        if ( (msg = mgmt_new_msg(argv[1], NULL)) == NULL ) {
                return 0;
        }

        for (i = 2; i < argc; i++ ) {
                msg = mgmt_msg_append(msg, argv[i]);
        }

        printf("msg: [%s]\n", msg);
        result = process_msg(msg);
        printf("result: [%s]\n", result);
        
        ci_lib_finalize();

        mgmt_del_msg(msg);
        mgmt_del_msg(result);

        return 0;
}
