#! /bin/sh

rm temp.img
rm to.img
rm from.img
rm *patch*

cp from.orig from.img
cp to.orig to.img

# build the patch
xdelta3 -0 -B 67108864 -e -s from.img to.img a_to_b.patch
/usr/bin/time -v xdelta3 -B 67108864 -d -s from.img a_to_b.patch temp.img
cmp to.img temp.img
if [ $? -ne 0 ]; then
	echo "Couldn't verify patch"
else
	echo "Built patch"
fi

# compress it and verify the compressed patch
xz -zk --check=crc32 a_to_b.patch
mv a_to_b.patch a_to_b.patch.orig
xz -dk a_to_b.patch.xz
cmp a_to_b.patch a_to_b.patch.orig
if [ $? -ne 0 ]; then
	echo "Couldn't verify compression"
fi
rm a_to_b.patch.orig

# run the test
STARTTIME=$(date +%s)
$ANDROID_BUILD_TOP/out/host/linux-x86/bin/syspatch from.img a_to_b.patch.xz temp.img
ENDTIME=$(date +%s)
cmp to.img temp.img
if [ $? -ne 0 ]; then
	echo "Different file test failed"
else
	echo "Different file test passed, took $((ENDTIME - STARTTIME)) seconds"
fi
STARTTIME=$(date +%s)
/usr/bin/time -v $ANDROID_BUILD_TOP/out/host/linux-x86/bin/syspatch from.img a_to_b.patch.xz from.img
ENDTIME=$(date +%s)
cmp from.img to.img
if [ $? -ne 0 ]; then
	echo "Same file test failed"
else
	echo "Same file test passed, took $((ENDTIME - STARTTIME)) seconds"
fi

rm temp.img
rm to.img
rm from.img
rm *patch*
