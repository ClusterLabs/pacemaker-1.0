#!/bin/sh

# test.sh: cms daemon test script
# 
# Copyright (c) 2004 Intel Corp.
# 
# Author: Zhu Yi (yi.zhu@intel.com)
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


CMSTOOLS=../utils/cmstools
VERBOSE="-v"

CHECK_RESULT()
{
	errno=$?
	expect=$2
	expect=${expect:=1}

	if [ $errno -eq $expect ]; then
		echo $1 passed
	else
		echo $1 failed "(errno = $errno)"
	fi
}

mqueue_test()
{
	$CMSTOOLS $VERBOSE -c createqueue
	CHECK_RESULT "create-queue"
	
	$CMSTOOLS $VERBOSE -c status
	CHECK_RESULT "status-queue"
	
	$CMSTOOLS $VERBOSE -c unlink
	CHECK_RESULT "unlink-queue"

	$CMSTOOLS $VERBOSE -c createqueue -p -n close_mqueue \
		-c closequeue -n close_mqueue
	CHECK_RESULT "close-queue"
	
	$CMSTOOLS $VERBOSE -c status -n close_mqueue
	CHECK_RESULT "status-queue-not-exist" 12

	$CMSTOOLS $VERBOSE -c unlink -n close_mqueue
	CHECK_RESULT "unlink-queue-not-exist"
}

mqgroup_test()
{
	MQNAME="mqueue_in_group"

	$CMSTOOLS $VERBOSE -c creategroup
	CHECK_RESULT "create-group"

	$CMSTOOLS $VERBOSE -c deletegroup
	CHECK_RESULT "delete-group"

	$CMSTOOLS $VERBOSE -c creategroup -c createqueue -n $MQNAME \
	       -c insert -q $MQNAME
	CHECK_RESULT "insert-group"

	$CMSTOOLS $VERBOSE -c remove -q $MQNAME
	CHECK_RESULT "remove-group"

	$CMSTOOLS $VERBOSE -c unlink -n $MQNAME
	CHECK_RESULT "unlink-queue"

	$CMSTOOLS $VERBOSE -c deletegroup
	CHECK_RESULT "delete-mqueue-group"
}

sendget_test()
{
	MQNAME="sendget_mqueue"

	$CMSTOOLS $VERBOSE -c createqueue -n $MQNAME -c send -n $MQNAME
	CHECK_RESULT "send-queue"

	$CMSTOOLS $VERBOSE -c openqueue -n $MQNAME -c get -n $MQNAME \
		-c unlink -n $MQNAME
	CHECK_RESULT "get-queue"
}

#
# Main
#
#mqueue_test
#mqgroup_test
sendget_test
