echo "##############################################################"
echo "TCQ depth=$1"

READ=0
for j in 1 2 3 4 5 
do
	TMP=`cat $1.$j|grep "aeken2,"|awk -F "," '{print $5}'`
	let "READ+=$TMP"
done
let "READ/=5"
echo "READ=$READ"	

WRITE=0
for j in 1 2 3 4 5 
do
	TMP=`cat $1.$j|grep "aeken2,"|awk -F "," '{print $11}'`
	let "WRITE+=$TMP"
done
let "WRITE/=5"
echo "WRITE=$WRITE"	

SEEK=0
for j in 1 2 3 4 5 
do
	TMP=`cat $1.$j|grep "aeken2,"|awk -F "," '{print $13}'` 
	SEEK=$(echo "$SEEK+$TMP" |bc -l)
done
SEEK=$(echo "$SEEK/5"|bc -l)
echo "SEEK=$SEEK"

echo "##############################################################"
