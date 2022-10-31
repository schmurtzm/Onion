#!/bin/sh
echo $0 $*
progdir=`dirname "$0"`
homedir=`dirname "$1"`
cd $progdir
./cpufreq.sh

# Timer initialisation
cd /mnt/SDCARD/App/PlayActivity
./playActivity "init"

cd /mnt/SDCARD/RetroArch/
HOME=/mnt/SDCARD/RetroArch/ $progdir/../../RetroArch/retroarch -v --log-file retroarch.log -L $progdir/../../RetroArch/.retroarch/cores/fake08_libretro.so "$1"


# Timer registration
cd /mnt/SDCARD/App/PlayActivity
./playActivity "$1"