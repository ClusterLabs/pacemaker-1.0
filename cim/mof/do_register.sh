#!/bin/sh
#
# Linux-HA: CIM Provider register tool 
#
# Author: Jia Ming Pan <jmltc@cn.ibm.com>
# Copyright (c) 2005 International Business Machines
# Licensed under the GNU GPL.
#
#

#---------- openwbem functions ---------------------------------------
openwbem_register ()
{
        mof_file=$1
        reg_file=$2

        ps -C owcimomd > /dev/null 2>&1
        if test $? != 0; then
                echo "owcimomd not running, start it first"
                exit
        fi

        OWMOFC=`which owmofc` 
        if test $? != 0; then
                echo "owmofc not found, OpenWbem not installed?"
        fi

        $OWMOFC $mof_file
}

openwbem_unregister ()
{
        mof_file=$1
        reg_file=$2

        ps -C owcimomd > /dev/null 2>&1
        if test $? != 0; then
                echo "owcimomd not running, start it first"
                exit
        fi

        OWMOFC=`which owmofc`
        if test $? != 0; then
                echo "owmofc not found, OpenWbem not installed?"
        fi

        $OWMOFC -r $mof_file
}

#---------- pegasus functions ----------------------------------------

pegasus_add_module ()
{
        mof_file=$1
        reg_file=$2
        out_file=$REGFILE

        modules=`cat $reg_file 2> /dev/null | grep -v '^[[:space:]]*#.*' | cut -d ' ' -f 4 | sort | uniq`
        for module in $modules; do
cat >> $out_file <<EOF
instance of PG_ProviderModule
{
   Name = "$module";
   Location = "$module";
   Vendor = "LinuxHA";
   Version = "2.0.0";
   InterfaceType = "CMPI";
   InterfaceVersion = "2.0.0";
};
EOF
        done

}

pegasus_add_provider ()
{
        mof_file=$1
        reg_file=$2
        out_file=$REGFILE

        providers=`cat $reg_file 2> /dev/null | grep -v '^[[:space:]]*#.*' | cut -d ' ' -f 3-4 | sort | uniq`
        set -- $providers
        while test x$1 != x
        do
cat >> $out_file <<EOF
instance of PG_Provider
{
   Name = "$1";
   ProviderModuleName = "$2";
};

EOF
                shift 2
        done

}


pegasus_add_capability ()
{
        mof_file=$1
        reg_file=$2
        out_file=$REGFILE

        capid=0
        cat $reg_file | grep -v '^[[:space:]]*#.*' | \
        while read classname namespace providername providermodule types
        do
                capid=`expr $capid + 1`
                provider_types=""
                type_no=0
                for type in $types; do
                        case $type in
                                instance)    type_no=2;;
                                association) type_no=3;;
                                indication)  type_no=4;;
                                method)      type_no=5;;
                                *)           echo "Unknown provider type: $type"
                                             exit;;
                        esac
                        if test "x$provider_types" = "x"; then
                                provider_types="$type_no"
                        else
                                provider_types="$provider_types, $type_no" 
                        fi
                done  # for

cat >> $out_file << EOF
instance of PG_ProviderCapabilities
{
   ProviderModuleName = "$providermodule";
   ProviderName = "$providername";
   ClassName = "$classname";
   ProviderType = { $provider_types };
   Namespaces = {"$namespace"};
   SupportedProperties = NULL;
   SupportedMethods = NULL;
   CapabilityID = "$capid";
};

EOF
        done  # while

}

pegasus_register ()
{
        mof_file=$1
        reg_file=$2

        if ps -C cimserver > /dev/null 2>&1
        then
                CIMMOF=cimmof
                state=active
        else
                CIMMOF=cimmofl
                PEGASUSREPOSITORY="/opt/tog-pegasus/repository"
                CIMMOF="$CIMMOF -R $PEGASUSREPOSITORY"
                state=inactive
        fi

        pegasus_add_module $mof_file $reg_file
        pegasus_add_provider $mof_file $reg_file
        pegasus_add_capability $mof_file $reg_file

        $CIMMOF -n root/cimv2 $mof_file
        $CIMMOF -n root/PG_Interop $REGFILE
}

pegasus_unregister ()
{
        echo "FixME"
        exit
}


#---------- sfcb functions -------------------------------------------
sfcb_transform ()
{
	old_file=$1
	new_file=$2

        cat $old_file | grep -v '^[[:space:]]*#.*' | \
        while read classname namespace providername providermodule types
        do
cat >> $new_file <<EOF
[$classname]
    provider: $providername
    location: $providermodule
    type: $types
    namespace: $namespace

EOF
        done


}

sfcb_running_check () 
{
        ps -C sfcbd > /dev/null 2>&1
        if [ $? -eq 0 ];
        then
                echo "sfcbd is running, please stop it first."
                exit
        fi
}

sfcb_register () 
{
	mof_file=$1
	reg_file=$2
	
        sfcb_running_check

        new_reg_file=/tmp/`basename $2`.reg
	sfcb_transform $reg_file $new_reg_file  

        ## register
        echo register to sfcb ...
        sfcbstage -r $new_reg_file $mof_file
        
        if [ $? -eq 1 ]; then
                echo failed to register, exit
                exit
        fi

        echo rebuild sfcb repository ...
        sfcbrepos -f
        if [ $? -eq 1 ]; then
                echo failed to rebuild sfcb, exit
                exit
        fi

	rm -rf $new_reg_file 
} 

sfcb_unregister () 
{
        mof_file=$1
        reg_file=$2

        sfcb_running_check

        new_reg_file=/tmp/`basename $2`.reg

        sfcb_transform $reg_file $new_reg_file

        ## register
        echo unregister to sfcb ...
        sfcbunstage -r $new_reg_file $mof_file

        if [ $? -eq 1 ]; then
                echo failed to unregister, exit
                exit
        fi

        echo rebuild sfcb repository ...
        sfcbrepos -f
        if [ $? -eq 1 ]; then
                echo failed to rebuild sfcb, exit
                exit
        fi

	rm -rf $new_reg_file
}

#========== main ============================================

mof_file=""
reg_file=""
unregister="no"
cimom=""

usage () {
        echo "usage: $0 [-t cimserver] [-u] -r regfile -m mof"
        echo $'\t'-t cimserver: specify cimserver "[pegasus|openwbem|sfcb]".
        echo $'\t'-u: Unregister 
        echo $'\t'-r regfile: specify reg file.
        echo $'\t'-m moffile: specify mof file. 
 
        exit 0
}

args=`getopt hut:r:m: $*`


if test $? != 0; then
        usage
fi

while [ -n "$1" ]; do
        case $1 in
                -h) usage;;
                -u) unregister="yes"
                    shift;;
                -t) shift
                    if test "x$cimom" = "x"; then
                        cimom=$1
                    else
                        echo CIM server already set, ignore [$1]
                    fi
                    shift;;
                -m) shift
                    if test "x$mof_file" = "x"; then
                        mof_file=$1
                    else
                        echo MOF file already set, ignore [$1]
                    fi
                    shift;;
                -r) shift
                    if test "x$reg_file" = "x"; then
                        reg_file=$1
                    else
                        echo Reg file already set, ignore [$1]
                    fi
                    shift;;
                --) break;;
        esac
done


if test "x$unregister" != "xyes"; then
echo registering providers ...
else
echo unregistering providers ...
fi

echo CIM Server: $cimom
echo mof file  :  $mof_file
echo registration file  :  $reg_file

case $cimom in
        pegasus) ;;
        sfcb)    ;;
        openwbem);;
        *)  usage;;
esac

REG_FUNC=${cimom}_register
UNREG_FUNC=${cimom}_unregister

if test "x$unregister" != "xyes"; then
        $REG_FUNC $mof_file $reg_file
else
        $UNREG_FUNC $mof_file $reg_file
fi


#============== end of file =======================================
