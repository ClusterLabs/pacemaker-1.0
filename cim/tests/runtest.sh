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

HL="\33[1m"

RED="\33[31m"
GREEN="\33[32m"
BLUE="\33[34m"
YELLOW="\33[33m"
END="\33[0m"

TMP=`mktemp`

success=0
failure=0
zero=0
total=0


call_zero () {
        echo -e $'\t'$HL$YELLOW"[ ZERO ]"$END
        zero=`expr $zero + 1`
}

call_ok () {
        echo -e $'\t'$HL$GREEN"[ OK ]"$END
        success=`expr $success + 1`
}

call_failed () {
        echo -e $'\t'$HL$RED"[ FAILED ]"$END
        cat $TMP
        failure=`expr $failure + 1`
}        

check_zero () {
        echo
        if [ $quite -eq 0 ]; 
        then
                cat $TMP
        fi

        lc=`cat $TMP | wc -l`
        if [ $lc -eq 0 ]
        then
                return 0 
        fi
        return 1
}

check_success () {
        grep -e ERR -e ERROR -e error $TMP >/dev/null 2>&1
        if [ $? -eq 0 ];
        then
                return 1 
        fi

        return 0
}


relation_between () {
        ref=$1
        assoc_class=$2

        return 0
}


################################################################

cim_get_instance () {
        ref=$1
        echo -en gi$'\t'$HL"http://$USERNAME:$PASSWD@"$ref$END 
        result=`$WBEMCLI gi http://$USERNAME:$PASSWD@$ref >$TMP 2>&1`

        if check_zero 
        then
                check=2
        else
                if check_success
                then
                        check=0
                else
                        check=1
                fi
        fi
        case $check in
                0) call_ok 
                   return 0;;
                1) call_failed ;;
                2) call_zero ;;
        esac
        return 1
}

cim_enum_instances () {
        op=$1
        class=$2

        echo -en $op$'\t'$HL$class$END 
        result=`$WBEMCLI $op http://$USERNAME:$PASSWD@$HOST/$NAMESPACE:$class >$TMP 2>&1`
        
        if check_zero 
        then
                check=2
        else
                if check_success
                then
                        check=0
                else
                        check=1
                fi
        fi
        case $check in
                0) call_ok 
                   return 0;;
                1) call_failed ;;
                2) call_zero ;;
        esac
        return 1
}


cim_enum_associators () {
        op=$1
        ref=$2
        assoc_class=$3

        echo -en $op$'\t'$HL$assoc_class$'\t'http://$ref$END 
        result=`$WBEMCLI $op http://$ref -ac $assoc_class >$TMP 2>&1`

        if check_zero 
        then
                check=2
        else
                if check_success
                then
                        check=0
                else
                        check=1
                fi
        fi
 
        case $check in
                0) call_ok
                   return 0 ;;
                1) call_failed ;;
                2) call_zero ;;
        esac
        return 1
}

cim_enum_references () {
        op=$1
        ref=$2
        assoc_class=$3

        echo -en $op$'\t'$HL$assoc_class$'\t'http://$ref$END 
        result=`$WBEMCLI $op http://$ref -arc $assoc_class >$TMP 2>&1`
        
        if check_zero 
        then
                check=2
        else
                if check_success
                then
                        check=0
                else
                        check=1
                fi
        fi
 
        case $check in
                0) call_ok 
                   return 0;;
                1) call_failed ;;
                2) call_zero ;;
        esac
        return 1
}

### EnumerateInstanceNames, EnuerateInstances, GetInstance. 
instance_test () {
        for class in $ALL_CLASSES; do
                if cim_enum_instances "ein" $class
                then
                        result=`cat $TMP`
                        for ref in $result; do
                                cim_get_instance "$ref"
                        done
                fi
                cim_enum_instances "ei" $class 
       done
}

assoc_test () {

        for inst_class in $INST_CLASSES; do

                result=""

                if cim_enum_instances "ein" $inst_class
                then
                        result=`cat $TMP`
                fi

                for assoc_class in $ASSOC_CLASSES; do
                        for ref in $result; do
                                if relation_between $ref $assoc_class
                                then 
                                        cim_enum_associators ain $ref $assoc_class
                                        cim_enum_associators ai $ref $assoc_class

                                        cim_enum_references rin $ref $assoc_class
                                        cim_enum_references ri $ref $assoc_class
                                fi
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
