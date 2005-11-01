#!/bin/sh

TMP=`mktemp`

WBEMCAT=`which wbemcat`

if test $? != 0; then
        echo wbemcat not found!
        exit
fi

wbemcat CreateFilter.xml > $TMP
wbemcat CreateHandler.xml > $TMP
wbemcat CreateSubscription.xml > $TMP
echo
echo Create subscription,
echo Press any key to terminate ... 
read
wbemcat DeleteFilter.xml > $TMP
wbemcat DeleteSubscription.xml > $TMP
wbemcat DeleteHandler.xml > $TMP

rm -rf $TMP 
