echo "##############################################################"
echo "Event Service API_TEST">result
TOPDIR=/projects/linux-ha/telecom/eventd/test/api_test
rm -f log


for j in a b c d e f g h i j 
do
	./$j>temp
	cat temp>>log
	OK=`grep -c "success" temp`
	FAIL=`grep -c "fail" temp`
	echo "$j">>result
	echo "	OK=$OK">>result
	echo "	FAIL=$FAIL">>result
	echo "$j"
	echo "	OK=$OK"
	echo "	FAIL=$FAIL"
done

	OK=`grep -c "success" log`
	FAIL=`grep -c "fail" log`
	echo "Total">>result
	echo "	OK=$OK">>result
	echo "	FAIL=$FAIL">>result
	echo "Total"
	echo "	OK=$OK"
	echo "	FAIL=$FAIL"

cat log|grep "fail">fail 

echo "##############################################################"

