libmod_env.la: mod_env.lo
	$(MOD_LINK) mod_env.lo $(MOD_ENV_LDADD)
libmod_setenvif.la: mod_setenvif.lo
	$(MOD_LINK) mod_setenvif.lo $(MOD_SETENVIF_LDADD)
libmod_version.la: mod_version.lo
	$(MOD_LINK) mod_version.lo $(MOD_VERSION_LDADD)
DISTCLEAN_TARGETS = modules.mk
static =  libmod_env.la libmod_setenvif.la libmod_version.la
shared = 
