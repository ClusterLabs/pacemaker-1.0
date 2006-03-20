#!/bin/sh
#
# Linux-HA: CIM Provider tests 
# 
# Author: Jia Ming Pan <jmltc@cn.ibm.com>
# Copyright (c) 2005 International Business Machines
# Licensed under the GNU GPL.
#
#

# set quite to 0 to disable result outputs
quite=0
color=1

# loop iteration
iteration=$1

if [ X"$iteration" = "X" ]; 
then
    iteration=1
fi

#set to your username, password, host
echo -en "user: "
read USERNAME
echo -en "password: "
read PASSWD
HOST=localhost
NAMESPACE=root/cimv2

WBEMCLI=`which wbemcli`

if test $? != 0; then
        echo "wbemcli not found, please install sblim-wbemcli"
        exit 1
fi

WBEMCLI="$WBEMCLI -nl"


INST_CLASSES="HA_Cluster 
              HA_SoftwareIdentity
              HA_ClusterNode
              HA_ResourceGroup
              HA_ResourceClone
              HA_PrimitiveResource
	      HA_MasterSlaveResource
	      HA_OrderConstraint
	      HA_LocationConstraint
	      HA_ColocationConstraint"

ASSOC_CLASSES="HA_ParticipatingNode
               HA_InstalledSoftwareIdentity
	       HA_SubResource
               HA_ResourceInstance
               HA_OperationOnResource
               HA_HostedResource
               HA_ResourceInstance"

ALL_CLASSES="$INST_CLASSES $ASSOC_CLASSES"

function get_assoc_classes_of ()
{
       case $1 in
        HA_Cluster) 
                echo "HA_ParticipatingNode HA_InstalledSoftwareIdentity";;
        HA_ClusterNode) 
                echo "HA_ParticipatingNode HA_HostedResource";;
        HA_PrimitiveResource) 
               echo "HA_SubResource HA_HostedResource
                     HA_ResourceInstance HA_OperationOnResource";;
        HA_MasterSlaveResource|HA_ResourceClone|HA_ResourceGroup) 
                echo "HA_SubResource HA_ResourceInstance
                      HA_OperationOnResource";;
        HA_SoftwareIdentity) 
                echo "HA_InstalledSoftwareIdentity";;
        *) echo ""
       esac
}

if test $color = 1; then 
        HL="\33[1m"   #high light

        RED="\33[31m"
        GREEN="\33[32m"
        BLUE="\33[34m"
        YELLOW="\33[33m"
        END="\33[0m"
fi

TMP=`mktemp`

success=0
failure=0
empty=0
total=0


function call_empty () 
{
        echo -e $'\t'$HL$YELLOW"[ EMPTY ]"$END
        empty=`expr $empty + 1`
}

function call_ok () 
{
        echo -e $'\t'$HL$GREEN"[ OK ]"$END
        success=`expr $success + 1`
}

function call_failed () 
{
        echo -e $'\t'$HL$RED"[ FAILED ]"$END
        cat $TMP
        failure=`expr $failure + 1`
}        


EMPTY=0
ERROR=1
SUCCESS=2

function check_result () 
{
        echo
        if test $quite = 0; then
                cat $TMP
        fi

        # empty
        lc=`cat $TMP | wc -l`
        if [ $lc -eq 0 ]; then
                call_empty
                return $EMPTY 
        fi
       
        # error
        grep -e FAILED -e ERR -e "*" -e Exception -e error $TMP >/dev/null 2>&1
        if [ $? -eq 0 ]; then
                call_failed
                return $ERROR 
        fi
 
        # success
        call_ok
        return $SUCCESS
}

################################################################

function cim_get_instance () 
{
        ref=$1
        echo -en gi$'\t'$HL"http://$USERNAME:"*PASSWD*"@"$ref$END 
        result=`$WBEMCLI gi http://$USERNAME:$PASSWD@$ref >$TMP 2>&1`

        check_result
        return $?
}

function cim_enum_instances () 
{
        op=$1
        class=$2

        echo -en $op$'\t'$HL$class$END 
        result=`$WBEMCLI $op http://$USERNAME:$PASSWD@$HOST/$NAMESPACE:$class >$TMP 2>&1`
        
        check_result
        return $?
}


function cim_enum_associators () 
{
        op=$1
        ref=$2
        assoc_class=$3
        echo -en $op$'\t'$HL"http://"$USERNAME:"*PASSWD*"@$ref$END -ac $assoc_class
        result=`$WBEMCLI $op http://$USERNAME:$PASSWD@$ref -ac $assoc_class >$TMP 2>&1`
        check_result
        return $?
}

function cim_enum_references () 
{
        op=$1
        ref=$2
        assoc_class=$3
        echo -en $op$'\t'$HL"http://"$USERNAME:"*PASSWD*"@$ref$END -arc $assoc_class 
        result=`$WBEMCLI $op http://$USERNAME:$PASSWD@$ref -arc $assoc_class >$TMP 2>&1`
        check_result
        return $?
}

### EnumerateInstanceNames, EnuerateInstances, GetInstance. 
function instance_test () 
{
        for class in $ALL_CLASSES; do
                cim_enum_instances "ein" $class
                if [ $? -eq $SUCCESS ]; then
                        result=`cat $TMP`
                        for ref in $result; do
                                cim_get_instance "$ref"
                        done
                fi
                cim_enum_instances "ei" $class 
       done
}

function assoc_test () 
{

        for inst_class in $INST_CLASSES; do
                result=""
                cim_enum_instances "ein" $inst_class
                if [ $? -eq $SUCCESS ]; then
                        result=`cat $TMP`
                else
                        continue
                fi
                        
                for ref in $result; do
                        assoc_classes=`get_assoc_classes_of $inst_class`
                        for assoc_class in $assoc_classes; do
                                cim_enum_associators ain $ref $assoc_class
                                cim_enum_associators ai $ref  $assoc_class

                                cim_enum_references rin $ref $assoc_class
                                cim_enum_references ri $ref $assoc_class
                        done
                done
        done
}



## Main ################

for i in `seq $iteration`; do
        instance_test
        assoc_test
done

total=`expr $empty + $failure + $success`
echo Total: $total, Success: $success, Failure: $failure, Empty: $empty

rm -rf $TMP
