#!/bin/bash

RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
LIGHTRED='\033[1;31m'
NC='\033[0m' # No Color
#str=$(grep -o '^flags\b.*: .*\bht\b' /proc/cpuinfo | tail -1)
nproc=$(grep -i "processor" /proc/cpuinfo | sort -u | wc -l)
phycore=$(cat /proc/cpuinfo | egrep "core id|physical id" | tr -d "\n" | sed s/physical/\\nphysical/g | grep -v ^$ | sort | uniq | wc -l)

if [ -z "$(echo "$phycore *2" | bc | grep $nproc)" ]
then
    str=""
else
    str="ht-enabled"
fi


if [ "$str" ]
then
    printf "${RED}mTCP works best when hyperthreading is DISABLED. Please disable this feature from BIOS.${NC}\n"
fi

printf "${CYAN}Type ${YELLOW}make${CYAN} to compile mTCP ${LIGHTRED}src/${CYAN} and ${LIGHTRED}apps/${CYAN}.${NC}\n"
