#!/bin/sh
#
#File: stonithd_test.sh.in
#Description: a test script for testing STONITH deamon.
#
# Author: Sun Jiang Dong <sunjd@cn.ibm.com>
# Copyright (c) 2004 International Business Machines
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# The requirement for the running environment is
# a) NODE1 and NODE2 can access each other via ssh without password authentication.
# b) You should run this test case in NODE1 under this 'test' directory.
#

NODE1=hadev1
NODE2=hadev2
NODE3=hadev3
NODE4=hadev4
LRMADMIN=/usr/lib/heartbeat/lrmadmin
APITEST=./apitest
RSH=ssh
RCP=scp
ERR_COUNT=0

$LRMADMIN -A myid1 stonith null NULL hostlist=$NODE2
[ $? == 0 ] || let ERR_COUNT++ 
$LRMADMIN -A myid2 stonith null NULL hostlist=$NODE3
[ $? == 0 ] || let ERR_COUNT++ 
$LRMADMIN -E myid1 start 0 0 0
[ $? == 0 ] || let ERR_COUNT++ 
$LRMADMIN -E myid2 start 0 0 0
[ $? == 0 ] || let ERR_COUNT++ 

$RSH root@$NODE2 $LRMADMIN -A myid3 stonith null NULL hostlist=$NODE1
[ $? == 0 ] || let ERR_COUNT++ 
$RSH root@$NODE2 $LRMADMIN -A myid4 stonith null NULL hostlist=$NODE3
[ $? == 0 ] || let ERR_COUNT++ 
$RSH root@$NODE2 $LRMADMIN -E myid3 start 0 0 0
[ $? == 0 ] || let ERR_COUNT++ 
$RSH root@$NODE2 $LRMADMIN -E myid4 start 0 0 0
[ $? == 0 ] || let ERR_COUNT++ 

$APITEST 0 $NODE3 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$APITEST 1 $NODE3 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$APITEST 1 $NODE1 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$APITEST 1 $NODE2 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$APITEST 3 $NODE4 4000 2
[ $? == 0 ] || let ERR_COUNT++ 

echo "will run test on the $NODE2"

$RCP .libs/$APITEST root@$NODE2:
[ $? == 0 ] || let ERR_COUNT++ 

$RSH root@$NODE2 $APITEST 0 $NODE3 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$RSH root@$NODE2 $APITEST 1 $NODE3 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$RSH root@$NODE2 $APITEST 1 $NODE1 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$RSH root@$NODE2 $APITEST 1 $NODE2 4000 0
[ $? == 0 ] || let ERR_COUNT++ 
$APITEST 2 $NODE4 4000 2
[ $? == 0 ] || let ERR_COUNT++ 

if [ $ERR_COUNT == 0 ]; then
	echo "All tests are ok."
else
	echo "There are $ERR_COUNT errors."
fi
