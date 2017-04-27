#########################################################################
# Check for libpcap availability
#########################################################################
have_pcap=no
AC_CHECK_HEADER(pcap/pcap.h,
    [AC_CHECK_HEADER(pcap/bpf.h,
        [AC_CHECK_LIB(pcap, pcap_open_offline, have_pcap=yes, [])],
    [])],
[])

if test $have_pcap == yes; then
    AM_CFLAGS="$AM_CFLAGS -DHAVE_PCAP"
    LIBS="$LIBS -lpcap"
fi
