echo "##############################################################"
echo "Check Point API_TEST">result
TOPDIR=/projects/linux-ha-20040421/lib/checkpointd/test/api_test
rm -f log

echo "">>result
echo "Libary Lifecycle Test ">>result
echo "Libary Lifecycle Test "
for j in init finalize fdget dispatch
do
	#killall lt-checkpointd
	#/projects/linux-ha/telecom/checkpointd/checkpointd -d
	
	$TOPDIR/lib_life/$j>temp
	cat temp>>log
	OK=`grep -c "OK" temp`
	FAIL=`grep -c "FAIL" temp`
	echo "$j">>result
	echo "	OK=$OK">>result
	echo "	FAIL=$FAIL">>result
	echo "$j"
	echo "	OK=$OK"
	echo "	FAIL=$FAIL"
done

echo "Section Management Test ">>result
echo "Section Management Test "
for j in create delete expire_time_set iterator_init iterator_next  iterator_finalize
#Expire_time_set
do
	#killall lt-checkpointd
	#/projects/linux-ha/telecom/checkpointd/checkpointd -d
	
	$TOPDIR/sect_mg/$j>temp
	cat temp>>log
	OK=`grep -c "OK" temp`
	FAIL=`grep -c "FAIL" temp`
	echo "$j">>result
	echo "	OK=$OK">>result
	echo "	FAIL=$FAIL">>result
	echo "$j"
	echo "	OK=$OK"
	echo "	FAIL=$FAIL"
done

echo "Check Point Management Test ">>result
echo "Check Point Management Test "
for j in open close set_active set_duration unlink get_status 
do
	#killall lt-checkpointd
	#/projects/linux-ha/telecom/checkpointd/checkpointd -d
	
	$TOPDIR/ckpt_mg/$j>temp
	cat temp>>log
	OK=`grep -c "OK" temp`
	FAIL=`grep -c "FAIL" temp`
	echo "$j">>result
	echo "	OK=$OK">>result
	echo "	FAIL=$FAIL">>result
	echo "$j"
	echo "	OK=$OK"
	echo "	FAIL=$FAIL"
done

echo "">>result
echo "Data Access Test ">>result
echo "Data Access Test "
for j in write overwrite read synchronize asynchronize
do
	#killall lt-checkpointd
	#/projects/linux-ha/telecom/checkpointd/checkpointd -d
	
	$TOPDIR/data_access/$j>temp
	cat temp>>log
	OK=`grep -c "OK" temp`
	FAIL=`grep -c "FAIL" temp`
	echo "$j">>result
	echo "	OK=$OK">>result
	echo "	FAIL=$FAIL">>result
	echo "$j"
	echo "	OK=$OK"
	echo "	FAIL=$FAIL"
done

cat log|grep "FAIL">fail 

echo "##############################################################"

