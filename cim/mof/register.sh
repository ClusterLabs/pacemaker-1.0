#!/bin/sh

TMP=`mktemp`

sfcb_register()
{
        mof_file=$1
        reg_file=$2
        out_file=$TMP

        cat $reg_file | grep -v '^[[:space:]]*#.*' | while read classname namespace providername providermodule type
                do
                        echo    "############################
                                [$classname]
                                provider: $providername
                                location: $providermodule
                                type: $type
                                namespace: $namespace
                                " >> $out_file
        done


        ## register
        echo register ...
        sfcbstage -r $out_file $mof_file
        
        if [ $? -eq 1 ];
        then
                echo failed to register, exit
                exit
        fi

        echo rebuild ...
        sfcbrepos -f
        if [ $? -eq 1 ];
        then
                echo failed to rebuild sfcb, exit
                exit
        fi
 
}


sfcb_install () {
        ps -C sfcbd > /dev/null 2>&1
        if [ $? -eq 0 ];
        then
                echo "sfcbd is running, please stop it first."
                exit
        fi

        sfcb_register $1 $2

} 

############ MAIN ###############

if [ ! $# -eq 2 ];
then 
        echo "usage:  register.sh MOF REG"
        exit
fi

mof_file=$1
reg_file=$2


sfcb_install $mof_file $reg_file
rm -rf $TMP
