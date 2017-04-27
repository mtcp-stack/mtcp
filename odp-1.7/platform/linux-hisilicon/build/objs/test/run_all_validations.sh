#!/bin/sh
./odp_atomic > validation_log.txt
echo "######atomic######"
tail validation_log.txt  --lines=6
if cat validation_log.txt | grep FAILED>/dev/null
then
        echo "atomic validation fail!!!"
       	echo "######atomic######"
        exit 1;
else
        echo "atomic validation passed!"
       	echo "######atomic######"
fi
echo "                                     "
./odp_classification >> validation_log.txt
echo "######classification######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "classification validation failed!!!"
	echo "######classification######"
        exit 1;
}||{
        echo "classification validation passed!"
	echo "######classification######"
}
echo "                                     "
./odp_barrier >> validation_log.txt
echo "######barrier######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "barrier validation failed!!!"
	echo "######barrier######"
        exit 1;
}||{
        echo "barrier validation passed!"
	echo "######barrier######"
}
echo "                                     "
./odp_buffer >> validation_log.txt
echo "######buffer######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "buffer validation failed!!!"
	echo "######buffer######"
        exit 1;
}||{
        echo "buffer validation passed!"
	echo "######buffer######"
}
echo "                                     "
./odp_config >> validation_log.txt
echo "######config######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "config validation failed!!!"
	echo "######config######"
        exit 1;
}||{
        echo "config validation passed!"
	echo "######config######"
}
echo "                                     "
./odp_cpumask >> validation_log.txt
echo "######cpumask######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "cpumask validation failed!!!"
	echo "######cpumask######"
        exit 1;
}||{
        echo "cpumask validation passed!"
	echo "######cpumask######"
}
echo "                                     "
./odp_crypto >> validation_log.txt
echo "######crypto######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "crypto validation failed!!!"
	echo "######crypto######"
        exit 1;
}||{
        echo "crypto validation passed!"
	echo "######crypto######"
}
echo "                                     "
./odp_errno >> validation_log.txt
echo "######errno######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "errno validation failed!!!"
	echo "######errno######"
        exit 1;
}||{
        echo "errno validation passed!"
	echo "######errno######"
}
echo "                                     "
./odp_hash >> validation_log.txt
echo "######hash######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "hash validation failed!!!"
	echo "######hash######"
        exit 1;
}||{
        echo "hash validation passed!"
	echo "######hash######"
}
echo "                                     "
./odp_lock >> validation_log.txt
echo "######lock######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "lock validation failed!!!"
	echo "######lock######"
        exit 1;
}||{
        echo "lock validation passed!"
	echo "######lock######"
}
echo "                                     "
./odp_packet >> validation_log.txt
echo "######packet######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "packet validation failed!!!"
	echo "######packet######"
        exit 1;
}||{
        echo "packet validation passed!"
	echo "######packet######"
}
echo "                                     "
./odp_pktio >> validation_log.txt
echo "######pktio######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "pktio validation failed!!!"
	echo "######pktio######"
        exit 1;
}||{
        echo "pktio validation passed!"
	echo "######pktio######"
}
echo "                                     "
./odp_pool >> validation_log.txt
echo "######pool######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "pool validation failed!!!"
	echo "######pool######"
        exit 1;
}||{
        echo "pool validation passed!"
	echo "######pool######"
}
echo "                                     "
./odp_queue >> validation_log.txt
echo "######queue######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "queue validation failed!!!"
	echo "######queue######"
        exit 1;
}||{
        echo "queue validation passed!"
	echo "######queue######"
}
echo "                                     "
./odp_random >> validation_log.txt
echo "######random######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "random validation failed!!!"
	echo "######random######"
        exit 1;
}||{
        echo "random validation passed!"
	echo "######random######"
}
echo "                                     "
./odp_scheduler >> validation_log.txt
echo "######scheduler######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "scheduler validation failed!!!"
	echo "######scheduler######"
        exit 1;
}||{
        echo "scheduler validation passed!"
	echo "######scheduler######"
}
echo "                                     "
./odp_shmem >> validation_log.txt
echo "######shmem######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "shmem validation failed!!!"
	echo "######shmem######"
        exit 1;
}||{
        echo "shmem validation passed!"
	echo "######shmem######"
}
echo "                                     "
./odp_std_clib >> validation_log.txt
echo "######std_clib######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "std_clib validation failed!!!"
	echo "######std_clib######"
        exit 1;
}||{
        echo "std_clib validation passed!"
	echo "######std_clib######"
}
echo "                                     "
./odp_system >> validation_log.txt
echo "######system######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "system validation failed!!!"
	echo "######system######"
        exit 1;
}||{
        echo "system validation passed!"
	echo "######system######"
}
echo "                                     "
./odp_thread >> validation_log.txt
echo "######thread######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "thread validation failed!!!"
	echo "######thread######"
        exit 1;
}||{
        echo "thread validation passed!"
	echo "######thread######"
}
echo "                                     "
./odp_time >> validation_log.txt
echo "######time######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "time validation failed!!!"
	echo "######time######"
        exit 1;
}||{
        echo "time validation passed!"
	echo "######time######"
}
echo "                                     "
./odp_timer >> validation_log.txt
echo "######timer######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "timer validation failed!!!"
	echo "######timer######"
        exit 1;
}||{
        echo "timer validation passed!"
	echo "######timer######"
}
echo "                                     "
./odp_init_log >> validation_log.txt
echo "######init_log######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "init_log validation failed!!!"
	echo "######init_log######"
        exit 1;
}||{
        echo "init_log validation passed!"
	echo "######init_log######"
}
echo "                                     "
./odp_init_abort >> validation_log.txt
echo "######init_abort######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "init_abort validation failed!!!"
	echo "######init_abort######"
        exit 1;
}||{
        echo "init_abort validation passed!"
	echo "######init_abort######"
}
echo "                                     "
./odp_init_ok >> validation_log.txt
echo "######init_ok######"
tail validation_log.txt  --lines=6
grep  "FAILED" validation_log.txt &&{
        echo "init_ok validation failed!!!"
	echo "######init_ok######"
        exit 1;
}||{
        echo "init_ok validation passed!"
	echo "######init_ok######"
}






