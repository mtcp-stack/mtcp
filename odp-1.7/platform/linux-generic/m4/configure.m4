AC_MSG_CHECKING(for GCC atomic builtins)
AC_LINK_IFELSE(
    [AC_LANG_SOURCE(
      [[int main() {
        int v = 1;
        __atomic_fetch_add(&v, 1, __ATOMIC_RELAXED);
        __atomic_fetch_sub(&v, 1, __ATOMIC_RELAXED);
        __atomic_store_n(&v, 1, __ATOMIC_RELAXED);
        __atomic_load_n(&v, __ATOMIC_RELAXED);
        return 0;
        }
    ]])],
    AC_MSG_RESULT(yes),
    AC_MSG_RESULT(no)
    echo "GCC-style __atomic builtins not supported by the compiler."
    echo "Use newer version. For gcc > 4.7.0"
    exit -1)

m4_include([platform/linux-generic/m4/odp_pthread.m4])
m4_include([platform/linux-generic/m4/odp_openssl.m4])
m4_include([platform/linux-generic/m4/odp_netmap.m4])
m4_include([platform/linux-generic/m4/odp_pcap.m4])

AC_CONFIG_FILES([platform/linux-generic/Makefile
		 platform/linux-generic/test/Makefile
		 platform/linux-generic/test/pktio/Makefile])
