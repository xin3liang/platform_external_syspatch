#! /bin/sh

# initial cleanup
rm -f a
rm -f b
rm -f c
rm -f a_to_b.patch
rm -f a_to_b.patch.xz

# build the files
#./make_test_data.py
dd if=/dev/urandom of=b bs=1M count=100
dd if=/dev/full of=a bs=1M count=100

# build the patch
xdelta3 -0 -B 67108864 -e -s a b a_to_b.patch
/usr/bin/time -v xdelta3 -B 67108864 -d -s a a_to_b.patch c
cmp b c
if [ $? -ne 0 ]; then
	echo "Couldn't verify patch"
else
	echo "Built patch"
fi
rm c

# compress it and verify the compressed patch
xz -zk --check=crc32 a_to_b.patch
mv a_to_b.patch a_to_b.patch.orig
xz -dk a_to_b.patch.xz
cmp a_to_b.patch a_to_b.patch.orig
if [ $? -ne 0 ]; then
	echo "Couldn't verify compression"
fi
rm -f a_to_b.patch
rm -f a_to_b.patch.orig

# run the test
cp a c
$ANDROID_BUILD_TOP/out/host/linux-x86/bin/syspatch a a_to_b.patch.xz c
cmp c b
if [ $? -ne 0 ]; then
	echo "Different file test failed"
fi
/usr/bin/time -v $ANDROID_BUILD_TOP/out/host/linux-x86/bin/syspatch a a_to_b.patch.xz a
cmp a b
if [ $? -ne 0 ]; then
	echo "Same file test failed"
fi

# cleanup
rm -f a
rm -f b
rm -f c
rm -f a_to_b.patch.xz
