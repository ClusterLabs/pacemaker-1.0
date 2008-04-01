#!/bin/bash

# Mar 2008, Dominik Klein
# Display scores of Linux-HA resources

# Known issues:
# * cannot get resource[_failure]_stickiness values for master/slave and clone resources
#   if those values are configured as meta attributes of the master/slave or clone resource
#   instead of as meta attributes of the encapsulated primitive

if [ `crmadmin -D | cut -d' ' -f4` != `uname -n|tr "[:upper:]" "[:lower:]"` ] 
  then echo "Warning: Script is not running on DC. This will be slow."
fi

sortby=1
[ -n "$1" ] && [ "$1" = "node" ] && sortby=3

export default_stickiness=`cibadmin -Q -o crm_config 2>/dev/null|grep "default[_-]resource[_-]stickiness"|grep -o -E 'value ?= ?"[^ ]*"'|cut -d '"' -f 2|grep -v "^$"`
export default_failurestickiness=`cibadmin -Q -o crm_config 2>/dev/null|grep "resource[_-]failure[_-]stickiness"|grep -o -E 'value ?= ?"[^ ]*"'|cut -d '"' -f 2|grep -v "^$"`

# Heading
printf "%-20s%-10s%-16s%-11s%-9s%-16s\n" "Resource" "Score" "Node" "Stickiness" "#Fail" "Fail-Stickiness"

2>&1 ptest -LVs | grep -v group | while read line
do
	node=`echo $line|cut -d "=" -f 1|sed 's/  *$//g'|grep -o -E "[^\ ]*$"|grep -o "[^\.]*$"|sed 's/\.$//'`
	res=`echo $line|cut -d "=" -f 1|sed 's/  *$//g'|grep -o -E "[^\ ]*$"|grep -o "^.*\."|sed 's/\.$//'`
	score=`echo $line|grep -o -E "[-0-9]*$"|sed 's/1000000/INFINITY/g'`

	# get meta attribute resource_stickiness
	if ! stickiness=`crm_resource -g resource_stickiness -r $res --meta 2>/dev/null`
	then
		# if that doesnt exist, get syntax like <primitive resource-stickiness="100"
		if ! stickiness=`crm_resource -x -r $res 2>/dev/null | grep -E "<master|<primitive|<clone" | grep -o "resource[_-]stickiness=\"[0-9]*\"" | cut -d '"' -f 2 | grep -v "^$"`
		then 
			# if no resource-specific stickiness is confiugured, use the default value
			stickiness="$default_stickiness"
		fi	
	fi

	# get meta attribute resource_failure_stickiness
	if ! failurestickiness=`crm_resource -g resource_failure_stickiness -r $res --meta 2>/dev/null`
	then
		# if that doesnt exist, use the default value
		failurestickiness="$default_failurestickiness"
	fi	

	failcount=`crm_failcount -G -r $res -U $node 2>/dev/null|grep -o -E 'value ?= ?INFINITY|value ?= ?[0-9]*'|cut -d '=' -f 2|grep -v "^$"`

	if echo $line | grep -q clone_color
	then
		res=$res"_(master)"
	else if echo $res | grep -q -E ':[0-9]{1,2}'
		then
			res=$res"_(clone)"
		fi
	fi

	printf "%-20s%-10s%-16s%-11s%-9s%-16s\n" $res $score $node $stickiness $failcount $failurestickiness
done|sort -k $sortby
