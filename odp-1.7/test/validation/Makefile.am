ODP_MODULES = atomic \
	      barrier \
	      buffer \
	      classification \
	      config \
	      cpumask \
	      crypto \
	      errno \
	      hash \
	      init \
	      lock \
	      queue \
	      packet \
	      pktio \
	      pool \
	      random \
	      scheduler \
	      std_clib \
	      thread \
	      time \
	      timer \
	      shmem \
	      system

SUBDIRS = common $(ODP_MODULES)

#The tests will need to retain the deprecated test implementation
AM_CFLAGS += -Wno-deprecated-declarations
