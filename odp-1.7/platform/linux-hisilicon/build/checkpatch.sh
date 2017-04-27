#!/bin/sh
cd ../../../
./scripts/checkpatch.pl -f ./example/*/*.c
./scripts/checkpatch.pl -f ./example/*/*.h
./scripts/checkpatch.pl -f ./example/*.h
./scripts/checkpatch.pl -f ./example/*.c

./scripts/checkpatch.pl -f ./helper/*.c
./scripts/checkpatch.pl -f ./helper/*.h
./scripts/checkpatch.pl -f ./helper/test/*.c
./scripts/checkpatch.pl -f ./helper/test/*.h
./scripts/checkpatch.pl -f ./helper/*/*/*/*.h

./scripts/checkpatch.pl -f ./platform/linux-generic/*.c
./scripts/checkpatch.pl -f ./platform/linux-generic/*.h
./scripts/checkpatch.pl -f ./platform/linux-generic/*/*.c
./scripts/checkpatch.pl -f ./platform/linux-generic/*/*.h
./scripts/checkpatch.pl -f ./platform/linux-generic/*/*/*.h
./scripts/checkpatch.pl -f ./platform/linux-generic/*/*/*/*.h
./scripts/checkpatch.pl -f ./platform/linux-generic/*/*/*/*.c

./scripts/checkpatch.pl -f ./platform/linux-hisilicon/*.c
./scripts/checkpatch.pl -f ./platform/linux-hisilicon/*.h
./scripts/checkpatch.pl -f ./platform/linux-hisilicon/*/*.c
./scripts/checkpatch.pl -f ./platform/linux-hisilicon/*/*.h
