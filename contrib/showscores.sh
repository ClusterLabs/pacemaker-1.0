#!/bin/bash

# Dec 2007, Dominik Klein
# Display scores of Linux-HA resources

# Known issues:
# * cannot get resource[_failure]_stickiness values for master/slave and clone resources
#   if those values are configured as meta attributes of the master/slave or clone resource
#   instead of as meta attributes of the encapsulated primitive

if [ `crmadmin -D | cut -d' ' -f4` != `uname -n|tr "[:upper:]" "[:lower:]"` ] 
  then echo "Warning: Script running not on DC. Might be slow(!)"
fi

# Heading
printf "%-16s%-16s%-16s%-16s%-16s%-16s\n" "Score" "Resource" "Node" "Stickiness" "Failcount" "Failure-Stickiness"

2>&1 ptest -LVVVVVVV|grep -E "assign_node|rsc_location"|grep -w -E "\ [-]{0,1}[0-9]*$"|while read line
do
	node=`echo $line|cut -d ' ' -f 8|cut -d ':' -f 1`
	res=`echo $line|cut -d ' ' -f 6|tr -d ","`
	score=`echo $line|cut -d ' ' -f 9|sed 's/1000000/INFINITY/g'`

	# get meta attribute resource_stickiness
	if ! stickiness=`crm_resource -g resource_stickiness -r $res --meta 2>/dev/null`
	then
		# if that doesnt exist, get syntax like <primitive resource-stickiness="100"
		if ! stickiness=`crm_resource -x -r $res 2>/dev/null | grep -E "<master|<primitive|<clone" | grep -o "resource[_-]stickiness=\"[0-9]*\"" | cut -d '"' -f 2 | grep -v "^$"`
		then 
			# if no resource-specific stickiness is confiugured, grep the default value
			stickiness=`cibadmin -Q -o crm_config 2>/dev/null|grep "default[_-]resource[_-]stickiness"|grep -o -E 'value ?= ?"[^ ]*"'|cut -d '"' -f 2|grep -v "^$"`
		fi	
	fi

	# get meta attribute resource_failure_stickiness
	if ! failurestickiness=`crm_resource -g resource_failure_stickiness -r $res --meta 2>/dev/null`
	then 
		# if that doesnt exist, get the default value
		failurestickiness=`cibadmin -Q -o crm_config 2>/dev/null|grep "resource[_-]failure[_-]stickiness"|grep -o -E 'value ?= ?"[^ ]*"'|cut -d '"' -f 2|grep -v "^$"`
	fi	

	failcount=`crm_failcount -G -r $res -U $node 2>/dev/null|grep -o -E 'value ?= ?[0-9]*'|cut -d '=' -f 2|grep -v "^$"`

	printf "%-16s%-16s%-16s%-16s%-16s%-16s\n" $res $score $node $stickiness $failcount $failurestickiness
done|sort -k 1
