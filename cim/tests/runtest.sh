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
USERNAME=root
PASSWD=hadev
HOST=localhost
NAMESPACE=root/cimv2

WBEMCLI=`which wbemcli`

if test $? != 0; then
        echo "wbemcli not found, please install sblim-wbemcli"
        exit 1
fi

WBEMCLI="$WBEMCLI -nl"


INST_CLASSES="LinuxHA_Cluster 
              LinuxHA_SoftwareIdentity
              LinuxHA_ClusterNode
              LinuxHA_ClusterResource
              LinuxHA_ClusterResourceGroup"

ASSOC_CLASSES="LinuxHA_ParticipatingNode
               LinuxHA_HostedResource
               LinuxHA_SubResource
               LinuxHA_InstalledSoftwareIdentity"


ALL_CLASSES="$INST_CLASSES $ASSOC_CLASSES"

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
zero=0
total=0


function call_zero () 
{
        echo -e $'\t'$HL$YELLOW"[ ZERO ]"$END
        zero=`expr $zero + 1`
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


ZERO=0
ERROR=1
SUCCESS=2

function check_result () 
{
        echo
        if test $quite = 0; then
                cat $TMP
        fi

        # zero
        lc=`cat $TMP | wc -l`
        if [ $lc -eq 0 ]; then
                call_zero
                return $ZERO 
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

function test_relation () 
{
        assoc_class=$1
        inst_class=$2
        l=""
        r=""

        case $assoc_class in
                LinuxHA_ParticipatingNode) 
                        l="LinuxHA_Cluster"
                        r="LinuxHA_ClusterNode";;
                LinuxHA_HostedResource)
                        l="LinuxHA_ClusterNode"
                        r="LinuxHA_ClusterResource";;
                LinuxHA_SubResource)
                        l="LinuxHA_ClusterResource"
                        r="LinuxHA_ClsterResourceGroup";;
                LinuxHA_InstalledSoftwareIdentity)
                        l="LinuxHA_Cluster"
                        r="LinuxHA_SoftwareIdentity";;
        esac

        if test x"$inst_class" = x"$l"; then
                return $SUCCESS
        fi

        if test x"$inst_class" = x"$r"; then
                return $SUCCESS
        fi

        return $ERROR 
}


################################################################

function cim_get_instance () 
{
        ref=$1
        echo -en gi$'\t'$HL"http://$USERNAME:$PASSWD@"$ref$END 
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

        echo -en $op$'\t'$HL$assoc_class$'\t'http://$USERNAME:$PASSWD@$ref$END 
        result=`$WBEMCLI $op http://$USERNAME:$PASSWD@$ref -ac $assoc_class >$TMP 2>&1`

        check_result

        return $?

}

function cim_enum_references () 
{
        op=$1
        ref=$2
        assoc_class=$3

        echo -en $op$'\t'$HL$assoc_class$'\t'http://$USERNAME:$PASSWD@$ref$END 
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

        for assoc_class in $ASSOC_CLASSES; do
                for inst_class in $INST_CLASSES; do
                        test_relation $assoc_class $inst_class
                        if [ ! $? -eq $SUCCESS ]; then
                                continue
                        fi

                        result=""
                        cim_enum_instances "ein" $inst_class
                        if [ $? -eq $SUCCESS ]; then
                                result=`cat $TMP`
                        else
                                echo Failed to enum instances: $inst_class
                                continue
                        fi
                        
                        for ref in $result; do
                                cim_enum_associators ain $ref $assoc_class
                                cim_enum_associators ai $ref $assoc_class

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

total=`expr $zero + $failure + $success`
echo Total: $total, Success: $success, Failure: $failure, Zero: $zero

rm -rf $TMP
