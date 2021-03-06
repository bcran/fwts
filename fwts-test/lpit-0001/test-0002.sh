#!/bin/bash
#
TEST="Test acpitables against invalid LPIT"
NAME=test-0001.sh
TMPLOG=$TMP/lpit.log.$$

$FWTS --show-tests | grep LPIT > /dev/null
if [ $? -eq 1 ]; then
	echo SKIP: $TEST, $NAME
	exit 77
fi

$FWTS --log-format="%line %owner " -w 80 --dumpfile=$FWTSTESTDIR/lpit-0001/acpidump-0002.log lpit - | cut -c7- | grep "^lpit" > $TMPLOG
diff $TMPLOG $FWTSTESTDIR/lpit-0001/lpit-0002.log >> $FAILURE_LOG
ret=$?
if [ $ret -eq 0 ]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi

rm $TMPLOG
exit $ret
