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
#include <regex.h>

#include <clplumbing/cl_malloc.h>

#include "cmpi_cluster.h"
#include "cmpi_utils.h"

int main(void)
{
	char ** std_out = NULL;
	int rc;
	int i;

        rc = regex_search(": (.*) \\((.*)\\)", 
                          "member node: hadev4 (ac4142a9-2ee2-4c2b-81eb-8101373d03c4)", 
                          &std_out);

	i = 0;
	while (rc == HA_OK && std_out[i] != NULL){
                 printf("%d\t%s\n", i, std_out[i]);
                 i++;
	
	}

        free_2d_array(std_out);
	printf("Return value is %d\n", rc);

	return 0;
}
