#!/bin/sh

cmd=openssl

T=testcert
KEY=512
CNF=${1:-test.cnf}
CA=${2:-../certs/testca.pem}
DSA=${3:-../apps/dsa512.pem}

/bin/rm -f $T.1 $T.2 $T.key

echo "generating certificate request"

echo "string to make the random number generator think it has entropy" >> ./.rnd

if $cmd no-rsa; then
  req_new="-newkey dsa:$DSA"
else
  req_new='-new'
  echo "There should be a 2 sequences of .'s and some +'s."
  echo "There should not be more that at most 80 per line"
fi

echo "This could take some time."

rm -f testkey.pem testreq.pem

$cmd req -config $CNF $req_new -out testreq.pem
if [ $? != 0 ]; then
echo problems creating request
exit 1
fi

$cmd req -config $CNF -verify -in testreq.pem -noout
if [ $? != 0 ]; then
echo signature on req is wrong
exit 1
fi

exit 0
