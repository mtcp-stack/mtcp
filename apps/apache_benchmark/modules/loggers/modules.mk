libmod_log_config.la: mod_log_config.lo
	$(MOD_LINK) mod_log_config.lo $(MOD_LOG_CONFIG_LDADD)
DISTCLEAN_TARGETS = modules.mk
static =  libmod_log_config.la
shared = 
