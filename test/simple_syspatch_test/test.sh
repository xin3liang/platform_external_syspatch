#! /bin/sh

# preemptive cleanup
rm a
rm b
rm a_to_b.patch
rm a_to_b.patch.orig
rm a_to_b.patch.xz

# setup the files
echo "hello world" > a
echo "Hello, world" > b

# make and verify the patch
xdelta3 -e -s a b a_to_b.patch
xdelta3 -d -s a a_to_b.patch c
cmp b c
if [ $? -ne 0 ]; then
	echo "Couldn't verify patch"
fi
rm c

# compress it and verify the compressed patch
xz -zk -9 --check=crc32 a_to_b.patch
mv a_to_b.patch a_to_b.patch.orig
xz -dk a_to_b.patch.xz
cmp a_to_b.patch a_to_b.patch.orig
if [ $? -ne 0 ]; then
	echo "Couldn't verify compression"
fi
rm a_to_b.patch
rm a_to_b.patch.orig

# run the test
$ANDROID_BUILD_TOP/out/host/linux-x86/bin/syspatch a a_to_b.patch.xz a
cmp a b
if [ $? -ne 0 ]; then
	echo "Test failed"
fi

# cleanup
rm a
rm b
rm a_to_b.patch
rm a_to_b.patch.orig
rm a_to_b.patch.xz
