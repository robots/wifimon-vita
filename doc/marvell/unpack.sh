# /bin/sh

# unpack vendor drop into git

for i in SD-BT-8787-*.tgz
do
	tar xf $i --strip 1
done
