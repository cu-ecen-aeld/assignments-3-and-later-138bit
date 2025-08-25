#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=${1:-/tmp/aeld}
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

CCBASE="${HOME}/repo/prive/course/linux-system-programming-introduction-to-buildroot/aesd-assignments/compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu"
if [[ -d ${CCBASE} ]]; then
	export PATH="${PATH}:${CCBASE}/bin/"
else 
	# Removes "bin/aarch64-none-linux-gnu-gcc" from path.
	CCBASE="$(dirname $(dirname $(which ${CROSS_COMPILE}gcc)))"
	cat <<-EOF
DEBUG DATA:
CROSS_COMPILE		: ${CROSS_COMPILE}
CC			: $(which ${CROSS_COMPILE}gcc)
CCBASE			: ${CCBASE}
EOF
	if [[ ! -d "${CCBASE}" ]]; then
		echo "Cant find ${CROSS_COMPILE}gcc in PATH ${PATH}"
		pwd
		ls
		exit 1
	fi
fi
export CCBASE
export CROSS_COMPILE
export ARCH

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
	make defconfig
	make -j $(nproc) Image
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"

# I don't need to run as root.
    rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
	make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j $(nproc)
make CONFIG_PREFIX=`pwd`/../rootfs install
cd `pwd`/../rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
mkdir lib
myld="$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | sed -r 's/^.* ([^ ]+)\]$/\1/')"
find ${CCBASE} -name $(basename ${myld}) -exec cp {} ./${myld} \;

${CROSS_COMPILE}readelf -a bin/busybox \
	| grep "Shared library" \
	| sed -r 's/.* \[([^ ]+)\]$/\1/;' \
	| while read -r line; do
		find ${CCBASE} -name ${line} -exec cp {} ./lib/. \;
done

# Can be cleaner.. Don't care for now.
cp -r ./lib ./lib64

# TODO: Make device nodes
mkdir proc dev sys

# No root in my docker container and the mknod utility is something
# for the present system, and not a target system.
ln -s /dev/null ./dev/

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
CROSS_COMPILE=aarch64-none-linux-gnu- make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
mkdir ${OUTDIR}/rootfs/home
for i in writer writer.sh finder.sh finder-test.sh autorun-qemu.sh; do
	chmod +x $i
	cp $i ${OUTDIR}/rootfs/home/.
done
cp -r ../conf ${OUTDIR}/rootfs/.

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
bash -c "find | cpio -R +0:+0 -LovH newc > ../initramfs.cpio"

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}
gzip -f initramfs.cpio

# Copy missing files to OUTDIR.
find ${OUTDIR}/linux-stable/ -name Image -exec cp {} ${OUTDIR}/. \;

# Copy binaries from /tmp to finder-app/ so it's out of my docker container:
rm -rf ${FINDER_APP_DIR}/out
mkdir ${FINDER_APP_DIR}/out
find ${OUTDIR}/linux-stable/ -name Image -exec cp {} ${FINDER_APP_DIR}/out/. \;
cp ${OUTDIR}/initramfs.cpio.gz ${FINDER_APP_DIR}/out/.

cat <<-EOF
Done!

To run "writer" in qemu, execute:
$ mount -t proc none /proc
$ mount -o remount,rw,size=1G /

cat <<-EOF
Done!

To run "writer" in qemu, execute:
$ mount -t proc none /proc
$ mount -o remount,rw,size=1G /
$ writer a/b/c/d.txt tralala
$ cat a/b/c/d.txt ; echo
EOF
