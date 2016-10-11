#ifdef MULTI_THREADED
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <numa.h>
#include <sched.h>
#endif

#include "server.h"
#include "buffer.h"
#include "network.h"
#include "log.h"
#include "keyvalue.h"
#include "response.h"
#include "request.h"
#include "chunk.h"
#include "http_chunk.h"
#include "fdevent.h"
#include "connections.h"
#include "stat_cache.h"
#include "plugin.h"
#include "joblist.h"
#include "network_backends.h"
#include "version.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <locale.h>

#include <stdio.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_PWD_H
# include <grp.h>
# include <pwd.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
# include <sys/prctl.h>
#endif

#ifdef USE_OPENSSL
# include <openssl/err.h> 
#endif
/*----------------------------------------------------------------------------*/
#ifndef __sgi
/* IRIX doesn't like the alarm based time() optimization */
/* #define USE_ALARM */
#endif

#ifdef HAVE_GETUID
# ifndef HAVE_ISSETUGID

static int
l_issetugid(void) {
	return (geteuid() != getuid() || getegid() != getgid());
}

#  define issetugid l_issetugid
# endif
#endif

static volatile sig_atomic_t srv_shutdown = 0;
static volatile sig_atomic_t graceful_shutdown = 0;
static volatile sig_atomic_t handle_sig_alarm = 1;
static volatile sig_atomic_t handle_sig_hup = 0;
static volatile sig_atomic_t forwarded_sig_hup = 0;

#if defined(HAVE_SIGACTION) && defined(SA_SIGINFO)
static volatile siginfo_t last_sigterm_info;
static volatile siginfo_t last_sighup_info;
/*----------------------------------------------------------------------------*/
static void
sigaction_handler(int sig, siginfo_t *si, void *context) {
	static siginfo_t empty_siginfo;
	UNUSED(context);
	
	if (!si) si = &empty_siginfo;

	switch (sig) {
	case SIGTERM:
		srv_shutdown = 1;
		last_sigterm_info = *si;
		break;
	case SIGINT:
		if (graceful_shutdown) {
			srv_shutdown = 1;
		} else {
			graceful_shutdown = 1;
		}
		last_sigterm_info = *si;

		break;
	case SIGALRM: 
		handle_sig_alarm = 1; 
		break;
	case SIGHUP:
		/** 
		 * we send the SIGHUP to all procs in the process-group
		 * this includes ourself
		 * 
		 * make sure we only send it once and don't create a 
		 * infinite loop
		 */
		if (!forwarded_sig_hup) {
			handle_sig_hup = 1;
			last_sighup_info = *si;
		} else {
			forwarded_sig_hup = 0;
		}
		break;
	case SIGCHLD:
		break;
	}
}
/*----------------------------------------------------------------------------*/
#elif defined(HAVE_SIGNAL) || defined(HAVE_SIGACTION)
static void
signal_handler(int sig) {
	switch (sig) {
	case SIGTERM: srv_shutdown = 1; break;
	case SIGINT:
	     if (graceful_shutdown) srv_shutdown = 1;
	     else graceful_shutdown = 1;
	     break;
	case SIGALRM: handle_sig_alarm = 1; break;
	case SIGHUP:  handle_sig_hup = 1; break;
	case SIGCHLD:  break;
	}
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef MULTI_THREADED
/* TODO - signal handling logic will be revised in future revisions */
static void
signal_handler(int sig) {
	switch (sig) {
	case SIGTERM: srv_shutdown = 1; break;
	case SIGINT:
		if (graceful_shutdown) srv_shutdown = 1;
		else graceful_shutdown = 1;
		
		break;
	case SIGALRM: handle_sig_alarm = 1; break;
	case SIGHUP:  handle_sig_hup = 1; break;
	case SIGCHLD:  break;
	}
#ifdef HAVE_LIBDPDK
        exit(EXIT_SUCCESS);
#endif
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef HAVE_FORK
static void
daemonize(void) {
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
	if (0 != fork()) exit(EXIT_SUCCESS);

	if (-1 == setsid()) exit(EXIT_SUCCESS);

	signal(SIGHUP, SIG_IGN);

	if (0 != fork()) exit(EXIT_SUCCESS);

	if (0 != chdir("/")) exit(EXIT_SUCCESS);
}
#endif
/*----------------------------------------------------------------------------*/
static server *
server_init(void) {
	int i;
	FILE *frandom = NULL;
	
	server *srv = calloc(1, sizeof(*srv));
	assert(srv);
#define CLEAN(x)				\
	srv->x = buffer_init();

	CLEAN(response_header);
	CLEAN(parse_full_path);
	CLEAN(ts_debug_str);
	CLEAN(ts_date_str);
	CLEAN(errorlog_buf);
	CLEAN(response_range);
	CLEAN(tmp_buf);
	srv->empty_string = buffer_init_string("");
	CLEAN(cond_check_buf);

	CLEAN(srvconf.errorlog_file);
	CLEAN(srvconf.breakagelog_file);
	CLEAN(srvconf.groupname);
	CLEAN(srvconf.username);
	CLEAN(srvconf.changeroot);
	CLEAN(srvconf.bindhost);
	CLEAN(srvconf.event_handler);
	CLEAN(srvconf.pid_file);

	CLEAN(tmp_chunk_len);
#undef CLEAN

#define CLEAN(x) \
	srv->x = array_init();

	CLEAN(config_context);
	CLEAN(config_touched);
	CLEAN(status);
#undef CLEAN

	for (i = 0; i < FILE_CACHE_MAX; i++) {
		srv->mtime_cache[i].mtime = (time_t)-1;
		srv->mtime_cache[i].str = buffer_init();
	}

	if ((NULL != (frandom = fopen("/dev/urandom", "rb")) || NULL != (frandom = fopen("/dev/random", "rb")))
	            && 1 == fread(srv->entropy, sizeof(srv->entropy), 1, frandom)) {
		unsigned int e;
		memcpy(&e, srv->entropy, sizeof(e) < sizeof(srv->entropy) ? sizeof(e) : sizeof(srv->entropy));
		srand(e);
		srv->is_real_entropy = 1;
	} else {
		unsigned int j;
		srand(time(NULL) ^ getpid());
		srv->is_real_entropy = 0;
		for (j = 0; j < sizeof(srv->entropy); j++)
			srv->entropy[j] = rand();
	}
	if (frandom) fclose(frandom);

	srv->cur_ts = time(NULL);
	srv->startup_ts = srv->cur_ts;

	srv->conns = calloc(1, sizeof(*srv->conns));
	assert(srv->conns);

	srv->joblist = calloc(1, sizeof(*srv->joblist));
	assert(srv->joblist);

	srv->fdwaitqueue = calloc(1, sizeof(*srv->fdwaitqueue));
	assert(srv->fdwaitqueue);

	srv->srvconf.modules = array_init();
	srv->srvconf.modules_dir = buffer_init_string(LIBRARY_DIR);
	srv->srvconf.network_backend = buffer_init();
	srv->srvconf.upload_tempdirs = array_init();
	srv->srvconf.reject_expect_100_with_417 = 1;

	/* use syslog */
	srv->errorlog_fd = STDERR_FILENO;
	srv->errorlog_mode = ERRORLOG_FD;

	srv->split_vals = array_init();

	return srv;
}
/*----------------------------------------------------------------------------*/
static void
server_free(server *srv) {
	size_t i;
	
	for (i = 0; i < FILE_CACHE_MAX; i++) {
		buffer_free(srv->mtime_cache[i].str);
	}

#define CLEAN(x) \
	buffer_free(srv->x);

	CLEAN(response_header);
	CLEAN(parse_full_path);
	CLEAN(ts_debug_str);
	CLEAN(ts_date_str);
	CLEAN(errorlog_buf);
	CLEAN(response_range);
	CLEAN(tmp_buf);
	CLEAN(empty_string);
	CLEAN(cond_check_buf);

	CLEAN(srvconf.errorlog_file);
	CLEAN(srvconf.breakagelog_file);
	CLEAN(srvconf.groupname);
	CLEAN(srvconf.username);
	CLEAN(srvconf.changeroot);
	CLEAN(srvconf.bindhost);
	CLEAN(srvconf.event_handler);
	CLEAN(srvconf.pid_file);
	CLEAN(srvconf.modules_dir);
	CLEAN(srvconf.network_backend);

	CLEAN(tmp_chunk_len);
#undef CLEAN

#if 0
	fdevent_unregister(srv->ev, srv->fd);
#endif
	fdevent_free(srv->ev);

	free(srv->conns);

	if (srv->config_storage) {
		for (i = 0; i < srv->config_context->used; i++) {
			specific_config *s = srv->config_storage[i];

			if (!s) continue;

			buffer_free(s->document_root);
			buffer_free(s->server_name);
			buffer_free(s->server_tag);
			buffer_free(s->ssl_pemfile);
			buffer_free(s->ssl_ca_file);
			buffer_free(s->ssl_cipher_list);
			buffer_free(s->ssl_dh_file);
			buffer_free(s->ssl_ec_curve);
			buffer_free(s->error_handler);
			buffer_free(s->errorfile_prefix);
			array_free(s->mimetypes);
			buffer_free(s->ssl_verifyclient_username);
#ifdef USE_OPENSSL
			SSL_CTX_free(s->ssl_ctx);
#endif
			free(s);
		}
		free(srv->config_storage);
		srv->config_storage = NULL;
	}

#define CLEAN(x) \
	array_free(srv->x);

	CLEAN(config_context);
	CLEAN(config_touched);
	CLEAN(status);
	CLEAN(srvconf.upload_tempdirs);
#undef CLEAN

	joblist_free(srv, srv->joblist);
	fdwaitqueue_free(srv, srv->fdwaitqueue);

	if (srv->stat_cache) {
		stat_cache_free(srv->stat_cache);
	}

	array_free(srv->srvconf.modules);
	array_free(srv->split_vals);

#ifdef USE_OPENSSL
	if (srv->ssl_is_init) {
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();
		ERR_remove_state(0);
		EVP_cleanup();
	}
#endif

	free(srv);
}
/*----------------------------------------------------------------------------*/
static inline void
load_plugins(server *srv)
{
	if (HANDLER_GO_ON != plugins_call_init(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", 
				"Initialization of plugins failed. Going down.");
		exit(EXIT_FAILURE);
	}	
	
}
/*----------------------------------------------------------------------------*/
static inline void
set_max_conns(server *srv, int restrict_limit)
{
	if (restrict_limit) {
		if (srv->srvconf.max_conns > srv->max_fds/2) {
			/* we can't have more connections than max-fds/2 */
			log_error_write(srv, __FILE__, __LINE__, "sdd", 
					"can't have more connections than fds/2: ", 
					srv->srvconf.max_conns, srv->max_fds);
			srv->max_conns = srv->max_fds/2;
		} else if (srv->srvconf.max_conns) {
			/* otherwise respect the wishes of the user */
			srv->max_conns = srv->srvconf.max_conns;
		} else {
			/* or use the default: we really don't want to hit max-fds */
			srv->max_conns = srv->max_fds/3;
		}
	} else {
		srv->max_conns = srv->srvconf.max_conns;
	}
}
/*----------------------------------------------------------------------------*/
static inline void
initialize_fd_framework(server *srv)
{
	size_t i;

	/* the 2nd arg of fdevent_init in case of libmtcp is ignored */
	if (NULL == (srv->ev = fdevent_init(srv, srv->max_fds + 1, srv->event_handler))) {
		log_error_write(srv, __FILE__, __LINE__,
				"s", "fdevent_init failed");
		exit(EXIT_FAILURE);
	}
	
	/*
	 * kqueue() is called here, select resets its internals,
	 * all server sockets get their handlers
	 *
	 * */
	if (0 != network_register_fdevents(srv)) {
		exit(EXIT_FAILURE);
	}

	/* might fail if user is using fam (not gamin) and famd isn't running */
	if (NULL == (srv->stat_cache = stat_cache_init())) {
		log_error_write(srv, __FILE__, __LINE__, "s",
			"stat-cache could not be setup, dieing.");
		exit(EXIT_FAILURE);
	}

#ifdef HAVE_FAM_H
i hope this does not work
	/* setup FAM */
	if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_FAM) {
		if (0 != FAMOpen2(srv->stat_cache->fam, "lighttpd")) {
			log_error_write(srv, __FILE__, __LINE__, "s",
					 "could not open a fam connection, dieing.");
			exit(EXIT_FAILURE);
		}
#ifdef HAVE_FAMNOEXISTS
		FAMNoExists(srv->stat_cache->fam);
#endif

		srv->stat_cache->fam_fcce_ndx = -1;
		fdevent_register(srv->ev, FAMCONNECTION_GETFD(srv->stat_cache->fam), stat_cache_handle_fdevent, NULL);
		fdevent_event_set(srv->ev, &(srv->stat_cache->fam_fcce_ndx), FAMCONNECTION_GETFD(srv->stat_cache->fam), FDEVENT_IN);
	}
#endif


	/* get the current number of FDs */
	srv->cur_fds = open("/dev/null", O_RDONLY);
	close(srv->cur_fds);

	for (i = 0; i < srv->srv_sockets.used; i++) {
		server_socket *srv_socket = srv->srv_sockets.ptr[i];
		if (-1 == fdevent_fcntl_set(srv->ev, srv_socket->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed:", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}
/*----------------------------------------------------------------------------*/
#ifdef MULTI_THREADED
#ifdef USE_MTCP
static inline void
set_listen_backlog(server *srv)
{
	fprintf(stderr, "Applying listen_backlog: %d\n", srv->srvconf.listen_backlog);
	srv->listen_backlog = srv->srvconf.listen_backlog;	
}
#endif
/*----------------------------------------------------------------------------*/
int 
core_affinitize(int cpu)
{
	cpu_set_t *cmask;
	struct bitmask *bmask;
	size_t n;
	int ret;

	n = sysconf(_SC_NPROCESSORS_ONLN);

	if (cpu < 0 || cpu >= (int) n) {
		errno = -EINVAL;
		return -1;
	}

	cmask = CPU_ALLOC(n);
	if (cmask == NULL)
		return -1;

	CPU_ZERO_S(n, cmask);
	CPU_SET_S(cpu, n, cmask);

	ret = sched_setaffinity(0, n, cmask);

	CPU_FREE(cmask);

	if (numa_max_node() == 0)
		return ret;

	bmask = numa_bitmask_alloc(16);
	assert(bmask);

	numa_bitmask_setbit(bmask, cpu % 2);
	numa_set_membind(bmask);
	numa_bitmask_free(bmask);

	return ret;
}
/*----------------------------------------------------------------------------*/
static int
get_num_cpus(const char *strnum)
{
	return strtol(strnum, (char **) NULL, 10);
}
/*----------------------------------------------------------------------------*/
static void
init_server_states(server ***srv_states, int cpus, 
		   server *first_entry, const char *conf_file)
{
	int i;

	/* initialize the array */
	*srv_states = (server **)calloc(cpus, sizeof(server *));
	if (NULL == *srv_states) {
		fprintf(stderr, "%s: %d(%s) - Can't allocate memory for srv_states\n",
			__FUNCTION__, __LINE__, __FILE__);
		exit(EXIT_FAILURE);
	}
	/* put the first entry on index 0 */
	(*srv_states)[0] = first_entry;
#if !(defined USE_MTCP || defined REUSEPORT)
	//#ifndef USE_MTCP
	((*srv_states)[0])->first_entry = first_entry;
#else
#ifdef USE_MTCP
	/* set listen-backlog */
	set_listen_backlog(first_entry);
#endif
#endif
	
	/* now do the same for all remaining reserved cpus */
	for (i = 1; i < cpus; i++) {
		/* initialize it */
		if (NULL == ((*srv_states)[i] = server_init())) {
			fprintf(stderr, "%s: %d(%s) - Can't allocate memory for %dth srv_state entry\n", 
				__FUNCTION__, __LINE__, __FILE__, i);
			goto release_everything;
		}
		((*srv_states)[i])->srvconf.port = 0;
		((*srv_states)[i])->srvconf.dont_daemonize = 
			first_entry->srvconf.dont_daemonize;
#if !(defined USE_MTCP || defined REUSEPORT)
		//#ifndef USE_MTCP
		((*srv_states)[i])->first_entry = first_entry;
#endif
		
		/* set the struct by reading the conf file again... */
		if (config_read(((*srv_states)[i]), conf_file))
			goto release_everything;
		buffer_copy_string(((*srv_states)[i])->srvconf.modules_dir,
				   first_entry->srvconf.modules_dir->ptr);
		/* ... and set the remaining as default. */
		if (0 != config_set_defaults((*srv_states)[i])) {
			log_error_write((*srv_states)[i], __FILE__, __LINE__, "s",
					"setting default values failed");
			goto release_everything;
		}
		/* load the plugins... i hope it doesn't mess things up */
		if (plugins_load((*srv_states)[i])) {
			log_error_write((*srv_states)[i], __FILE__, __LINE__, "s",
					"loading plugins finally failed");
			goto release_everything;
		}
		/* clone max_fds as well */
		((*srv_states)[i])->max_fds = first_entry->max_fds;
		/* clone max_conns as well */
		((*srv_states)[i])->max_conns = first_entry->max_conns;
#ifdef USE_MTCP
		/* clone listen backlog limit */
		((*srv_states)[i])->listen_backlog = first_entry->listen_backlog;
#endif
	}
	return;
 release_everything:
	/* release everything in reverse then */
	while (i >= 0) {
		server_free((*srv_states)[i]);
		plugins_free((*srv_states)[i]);
		i--;			
	}
	free(*srv_states);
	exit(EXIT_FAILURE);
}
/*----------------------------------------------------------------------------*/
void *
start_server(void *svrptr)
{
	server *srv = (server *)svrptr;
	size_t cpu = srv->cpu;
	size_t i;

	/* affinitize server to core `cpu' */
	core_affinitize(cpu);

#ifdef USE_MTCP
	/* initialize the per-cpu mctx context */
	/* creating mtcp context first */
	srv->mctx = mtcp_create_context(cpu);
	if (!srv->mctx) {
		fprintf(stderr, "Failed to create mtcp context!\n");
		exit(EXIT_FAILURE);
	}

	/* adjust max fds to max_conns * 3 */
	srv->max_fds = srv->max_conns * 3;
#else
	/* register SIGINT signal handler */
	signal(SIGINT, signal_handler);

#endif /* !USE_MTCP */

	/* network backend initialization */
	if (network_init(srv) == -1)
		exit(EXIT_FAILURE);
#ifdef USE_MTCP
	set_max_conns(srv, 0);
#else
	set_max_conns(srv, 1);
#endif
	load_plugins(srv);

	/* Close stderr ASAP in the child process to make sure that nothing
	 * is being written to that fd which may not be valid anymore. */
	if (-1 == log_error_open(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"Opening errorlog failed. Going down.");
		exit(EXIT_FAILURE);
	}

	if (HANDLER_GO_ON != plugins_call_set_defaults(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", 
				"Configuration of plugins failed. Going down.");
		exit(EXIT_FAILURE);
	}

	/* register fdevent framework first */
	initialize_fd_framework(srv);

	/* and... finally the 'main-loop' */
	/*-------------------------------------------------------------------------------*/
	/* main-loop */
	while (!srv_shutdown) {
		int n;
		size_t ndx;
		time_t min_ts;
		
		if (handle_sig_hup) {
			handler_t r;
			
			/* reset notification */
			handle_sig_hup = 0;


			/* cycle logfiles */

			switch(r = plugins_call_handle_sighup(srv)) {
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sd", "sighup-handler return with an error", r);
				break;
			}

			if (-1 == log_error_cycle(srv)) {
				log_error_write(srv, __FILE__, __LINE__, "s", "cycling errorlog failed, dying");

				exit(EXIT_FAILURE);
			} else {
#ifdef HAVE_SIGACTION
				log_error_write(srv, __FILE__, __LINE__, "sdsd", 
					"logfiles cycled UID =",
					last_sighup_info.si_uid,
					"PID =",
					last_sighup_info.si_pid);
#else
				log_error_write(srv, __FILE__, __LINE__, "s", 
					"logfiles cycled");
#endif
			}
		}

		if (handle_sig_alarm) {
			/* a new second */

#ifdef USE_ALARM
			/* reset notification */
			handle_sig_alarm = 0;
#endif

			/* get current time */
			min_ts = time(NULL);

			if (min_ts != srv->cur_ts) {
				int cs = 0;
				connections *conns = srv->conns;
				handler_t r;

				switch(r = plugins_call_handle_trigger(srv)) {
				case HANDLER_GO_ON:
					break;
				case HANDLER_ERROR:
					log_error_write(srv, __FILE__, __LINE__, "s", "one of the triggers failed");
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}

				/* trigger waitpid */
				srv->cur_ts = min_ts;

				/* cleanup stat-cache */
				stat_cache_trigger_cleanup(srv);
				/**
				 * check all connections for timeouts
				 *
				 */
				for (ndx = 0; ndx < conns->used; ndx++) {
					int changed = 0;
					connection *con;
					int t_diff;

					con = conns->ptr[ndx];

					if (con->state == CON_STATE_READ ||
					    con->state == CON_STATE_READ_POST) {
						if (con->request_count == 1) {
							if (srv->cur_ts - con->read_idle_ts > con->conf.max_read_idle) {
								/* time - out */
#if 0
								log_error_write(srv, __FILE__, __LINE__, "sd",
										"connection closed - read-timeout:", con->fd);
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						} else {
							if (srv->cur_ts - con->read_idle_ts > con->keep_alive_idle) {
								/* time - out */
#if 0
								log_error_write(srv, __FILE__, __LINE__, "sd",
										"connection closed - read-timeout:", con->fd);
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						}
					}

					if ((con->state == CON_STATE_WRITE) &&
					    (con->write_request_ts != 0)) {
#if 0
						if (srv->cur_ts - con->write_request_ts > 60) {
							log_error_write(srv, __FILE__, __LINE__, "sdd",
									"connection closed - pre-write-request-timeout:", con->fd, srv->cur_ts - con->write_request_ts);
						}
#endif

						if (srv->cur_ts - con->write_request_ts > con->conf.max_write_idle) {
							/* time - out */
							if (con->conf.log_timeouts) {
								log_error_write(srv, __FILE__, __LINE__, "sbsosds",
									"NOTE: a request for",
									con->request.uri,
									"timed out after writing",
									con->bytes_written,
									"bytes. We waited",
									(int)con->conf.max_write_idle,
									"seconds. If this a problem increase server.max-write-idle");
							}
							connection_set_state(srv, con, CON_STATE_ERROR);
							changed = 1;
						}
					}

					if (con->state == CON_STATE_CLOSE && (srv->cur_ts - con->close_timeout_ts > HTTP_LINGER_TIMEOUT)) {
						changed = 1;
					}

					/* we don't like div by zero */
					if (0 == (t_diff = srv->cur_ts - con->connection_start)) t_diff = 1;

					if (con->traffic_limit_reached &&
					    (con->conf.kbytes_per_second == 0 ||
					     ((con->bytes_written / t_diff) < con->conf.kbytes_per_second * 1024))) {
						/* enable connection again */
						con->traffic_limit_reached = 0;

						changed = 1;
					}

					if (changed) {
						connection_state_machine(srv, con);
					}
					con->bytes_written_cur_second = 0;
					*(con->conf.global_bytes_per_second_cnt_ptr) = 0;

#if 0
					if (cs == 0) {
						fprintf(stderr, "connection-state: ");
						cs = 1;
					}

					fprintf(stderr, "c[%d,%d]: %s ",
						con->fd,
						con->fcgi.fd,
						connection_get_state(con->state));
#endif
				}

				if (cs == 1) fprintf(stderr, "\n");
			}
		}

		if (srv->sockets_disabled) {
			/* our server sockets are disabled, why ? */

			if ((srv->cur_fds + srv->want_fds < srv->max_fds * 8 / 10) && /* we have enough unused fds */
			    (srv->conns->used <= srv->max_conns * 9 / 10) &&
			    (0 == graceful_shutdown)) {
				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_set(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd, FDEVENT_IN);
				}

				log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets enabled again");

				srv->sockets_disabled = 0;
			}
		} else {
			if ((srv->cur_fds + srv->want_fds > srv->max_fds * 9 / 10) || /* out of fds */
			    (srv->conns->used >= srv->max_conns) || /* out of connections */
			    (graceful_shutdown)) { /* graceful_shutdown */
				
				/* disable server-fds */
				
				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_del(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd);

					if (graceful_shutdown) {
						/* we don't want this socket anymore,
						 *
						 * closing it right away will make it possible for
						 * the next lighttpd to take over (graceful restart)
						 *  */

						fdevent_unregister(srv->ev, srv_socket->fd);
#ifdef USE_MTCP
						mtcp_close(srv->mctx, srv_socket->fd);
#else
						close(srv_socket->fd);
#endif
						srv_socket->fd = -1;

						/* network_close() will cleanup after us */

						if (srv->srvconf.pid_file->used &&
						    srv->srvconf.changeroot->used == 0) {
							if (0 != unlink(srv->srvconf.pid_file->ptr)) {
								if (errno != EACCES && errno != EPERM) {
									log_error_write(srv, __FILE__, __LINE__, "sbds",
											"unlink failed for:",
											srv->srvconf.pid_file,
											errno,
											strerror(errno));
								}
							}
						}
					}
				}

				if (graceful_shutdown) {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] graceful shutdown started");
				} else if (srv->conns->used >= srv->max_conns) {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets disabled, connection limit reached");
				} else {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets disabled, out-of-fds");
				}

				srv->sockets_disabled = 1;
			}
		}

		if (graceful_shutdown && srv->conns->used == 0) {
			/* we are in graceful shutdown phase and all connections are closed
			 * we are ready to terminate without harming anyone */
			srv_shutdown = 1;
		}

		/* we still have some fds to share */
		if (srv->want_fds) {
			/* check the fdwaitqueue for waiting fds */
			int free_fds = srv->max_fds - srv->cur_fds - 16;
			connection *con;

			for (; free_fds > 0 && NULL != (con = fdwaitqueue_unshift(srv, srv->fdwaitqueue)); free_fds--) {
				connection_state_machine(srv, con);

				srv->want_fds--;
			}
		}

		if ((n = fdevent_poll(srv->ev, -1/*1000*/)) > 0) {
			/* n is the number of events */
			int revents;
			int fd_ndx;
#if 0
			if (n > 0) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"polls:", n);
			}
#endif
			fd_ndx = -1;
			do {
				fdevent_handler handler;
				void *context;
				handler_t r;
				int fd;

				fd_ndx  = fdevent_event_next_fdndx (srv->ev, fd_ndx);
				if (-1 == fd_ndx) break; /* not all fdevent handlers know how many fds got an event */

				revents = fdevent_event_get_revent (srv->ev, fd_ndx);
				fd      = fdevent_event_get_fd     (srv->ev, fd_ndx);
				handler = fdevent_get_handler(srv->ev, fd);
				context = fdevent_get_context(srv->ev, fd);

				/* connection_handle_fdevent needs a joblist_append */
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sdd",
						"event for", fd, revents);
#endif
				switch (r = (*handler)(srv, context, revents)) {
				case HANDLER_FINISHED:
				case HANDLER_GO_ON:
				case HANDLER_WAIT_FOR_EVENT:
				case HANDLER_WAIT_FOR_FD:
					break;
				case HANDLER_ERROR:
					/* should never happen */
					SEGFAULT();
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}
			} while (--n > 0);
		} else if (n < 0 && errno != EINTR) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"fdevent_poll failed:",
					strerror(errno));
		}

		for (ndx = 0; ndx < srv->joblist->used; ndx++) {
			connection *con = srv->joblist->ptr[ndx];
			handler_t r;

			connection_state_machine(srv, con);

			switch(r = plugins_call_handle_joblist(srv, con)) {
			case HANDLER_FINISHED:
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "d", r);
				break;
			}

			con->in_joblist = 0;
		}
		
		srv->joblist->used = 0;
	} /* end of `while (!srv_shutdown)` */
	/*-------------------------------------------------------------------------------*/
#ifdef USE_MTCP
	/* TODO - this will go somewhere else */
	mtcp_destroy_context(srv->mctx);
#endif
	pthread_exit(NULL);

	return NULL;
}
#endif /* !MULTI_THREADED */
/*----------------------------------------------------------------------------*/
static void
show_version (void) {
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
	char *b = PACKAGE_DESC TEXT_SSL \
" - a light and fast webserver\n" \
"Build-Date: " __DATE__ " " __TIME__ "\n";
;
#undef TEXT_SSL
	write(STDOUT_FILENO, b, strlen(b));
}
/*----------------------------------------------------------------------------*/
static void
show_features(void) {
	const char features[] = ""
#ifdef USE_SELECT
		"\t+ select (generic)\n"
#else
		"\t- select (generic)\n"
#endif
#ifdef USE_POLL
		"\t+ poll (Unix)\n"
#else
		"\t- poll (Unix)\n"
#endif
#ifdef USE_LINUX_SIGIO
		"\t+ rt-signals (Linux 2.4+)\n"
#else
		"\t- rt-signals (Linux 2.4+)\n"
#endif
#ifdef USE_LINUX_EPOLL
		"\t+ epoll (Linux 2.6)\n"
#else
		"\t- epoll (Linux 2.6)\n"
#endif
#ifdef USE_SOLARIS_DEVPOLL
		"\t+ /dev/poll (Solaris)\n"
#else
		"\t- /dev/poll (Solaris)\n"
#endif
#ifdef USE_SOLARIS_PORT
		"\t+ eventports (Solaris)\n"
#else
		"\t- eventports (Solaris)\n"
#endif
#ifdef USE_FREEBSD_KQUEUE
		"\t+ kqueue (FreeBSD)\n"
#else
		"\t- kqueue (FreeBSD)\n"
#endif
#ifdef USE_LIBEV
		"\t+ libev (generic)\n"
#else
		"\t- libev (generic)\n"
#endif
		"\nNetwork handler:\n\n"
#if defined USE_LINUX_SENDFILE
		"\t+ linux-sendfile\n"
#else
		"\t- linux-sendfile\n"
#endif
#if defined USE_FREEBSD_SENDFILE
		"\t+ freebsd-sendfile\n"
#else
		"\t- freebsd-sendfile\n"
#endif
#if defined USE_SOLARIS_SENDFILEV
		"\t+ solaris-sendfilev\n"
#else
		"\t- solaris-sendfilev\n"
#endif
#if defined USE_WRITEV
		"\t+ writev\n"
#else
		"\t- writev\n"
#endif
		"\t+ write\n"
#ifdef USE_MMAP
		"\t+ mmap support\n"
#else
		"\t- mmap support\n"
#endif
		"\nFeatures:\n\n"
#ifdef HAVE_IPV6
		"\t+ IPv6 support\n"
#else
		"\t- IPv6 support\n"
#endif
#if defined HAVE_ZLIB_H && defined HAVE_LIBZ
		"\t+ zlib support\n"
#else
		"\t- zlib support\n"
#endif
#if defined HAVE_BZLIB_H && defined HAVE_LIBBZ2
		"\t+ bzip2 support\n"
#else
		"\t- bzip2 support\n"
#endif
#ifdef HAVE_LIBCRYPT
		"\t+ crypt support\n"
#else
		"\t- crypt support\n"
#endif
#ifdef USE_OPENSSL
		"\t+ SSL Support\n"
#else
		"\t- SSL Support\n"
#endif
#ifdef USE_MTCP
		"\t+ MTCP Support\n"
#else
		"\t- MTCP Support\n"
#endif
#ifdef HAVE_LIBPCRE
		"\t+ PCRE support\n"
#else
		"\t- PCRE support\n"
#endif
#ifdef HAVE_MYSQL
		"\t+ mySQL support\n"
#else
		"\t- mySQL support\n"
#endif
#if defined(HAVE_LDAP_H) && defined(HAVE_LBER_H) && defined(HAVE_LIBLDAP) && defined(HAVE_LIBLBER)
		"\t+ LDAP support\n"
#else
		"\t- LDAP support\n"
#endif
#ifdef HAVE_MEMCACHE_H
		"\t+ memcached support\n"
#else
		"\t- memcached support\n"
#endif
#ifdef HAVE_FAM_H
		"\t+ FAM support\n"
#else
		"\t- FAM support\n"
#endif
#ifdef HAVE_LUA_H
		"\t+ LUA support\n"
#else
		"\t- LUA support\n"
#endif
#ifdef HAVE_LIBXML_H
		"\t+ xml support\n"
#else
		"\t- xml support\n"
#endif
#ifdef HAVE_SQLITE3_H
		"\t+ SQLite support\n"
#else
		"\t- SQLite support\n"
#endif
#ifdef HAVE_GDBM_H
		"\t+ GDBM support\n"
#else
		"\t- GDBM support\n"
#endif
		"\n";
	show_version();
	printf("\nEvent Handlers:\n\n%s", features);
}
/*----------------------------------------------------------------------------*/
static void
show_help(void) {
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
	char *b = PACKAGE_DESC TEXT_SSL " ("__DATE__ " " __TIME__ ")" \
" - a light and fast webserver\n" \
"usage:\n" \
" -f <name>  filename of the config-file\n" \
" -m <name>  module directory (default: "LIBRARY_DIR")\n" \
" -p         print the parsed config-file in internal form, and exit\n" \
" -n <#cpus> number of cpu cores that lighttpd will use\n" \
" -t         test the config-file, and exit\n" \
" -D         don't go to background (default: go to background)\n" \
" -v         show version\n" \
" -V         show compile-time features\n" \
" -h         show this help\n" \
"\n"
;
#undef TEXT_SSL
#undef TEXT_IPV6
	write(STDOUT_FILENO, b, strlen(b));
}
/*----------------------------------------------------------------------------*/
int 
main(int argc, char **argv) {
#ifdef MULTI_THREADED
	server **srv_states = NULL;
	char *conf_file = NULL;
#ifdef USE_MTCP
	struct mtcp_conf mcfg;
#endif
#endif
	/* 
	 * The introduction of MTCP slightly changes the purpose of *srv.
	 * *srv will always hold the srv state info for core 0.
	 * 
	 * When compiled without --lib-mtcp, *srv is used as the default 
	 * version.
	 */
	server *srv = NULL;
	int print_config = 0;
	int test_config = 0;
	int i_am_root;
	int o;
	int num_childs = 0;
	int pid_fd = -1;
	size_t i;
	struct group *grp = NULL;
	struct passwd *pwd = NULL;
#ifdef HAVE_SIGACTION
	struct sigaction act;
#endif
#ifdef HAVE_GETRLIMIT
	struct rlimit rlim;
#endif

#ifdef USE_ALARM
	struct itimerval interval;

	interval.it_interval.tv_sec = 1;
	interval.it_interval.tv_usec = 0;
	interval.it_value.tv_sec = 1;
	interval.it_value.tv_usec = 0;
#endif
#ifdef MULTI_THREADED
	/* create the cpu variable to facilitate multi-threading framework */
	size_t cpus = -1;
#endif


	/* for nice %b handling in strfime() */
	setlocale(LC_TIME, "C");

	if (NULL == (srv = server_init())) {
		fprintf(stderr, "did this really happen?\n");
		return EXIT_FAILURE;
	}

	/* init structs done */
#ifdef HAVE_GETUID
	i_am_root = (getuid() == 0);
#else
	i_am_root = 0;
#endif

	srv->srvconf.port = 0;
	srv->srvconf.dont_daemonize = 0;

	while(-1 != (o = getopt(argc, argv, "f:m:n:hvVDpt"))) {
		switch(o) {
		case 'f':
			if (srv->config_storage) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"Can only read one config file. Use the include command to use multiple config files.");

				server_free(srv);
				return EXIT_FAILURE;
			}
#ifdef MULTI_THREADED
			/* store the path to conf file for populating other srv structs as well */
			conf_file = strdup(optarg);
			if (NULL == conf_file) {
				fprintf(stderr, "Can't duplicate conf_file string\n");
				return EXIT_FAILURE;
			}
#endif
			if (config_read(srv, optarg)) {
				server_free(srv);
				return EXIT_FAILURE;
			}
			break;
		case 'm':
			buffer_copy_string(srv->srvconf.modules_dir, optarg);
			break;
		case 'n':
#ifdef MULTI_THREADED
			cpus = get_num_cpus(optarg);
#else
			fprintf(stderr, "-n option only works with MTCP/MULTI_THREADED support!\n");
			exit(EXIT_FAILURE);
#endif
			break;
		case 'p': print_config = 1; break;
		case 't': test_config = 1; break;
		case 'D':
			srv->srvconf.dont_daemonize = 1; break;
		case 'v': show_version(); return EXIT_SUCCESS;
		case 'V': show_features(); return EXIT_SUCCESS;
		case 'h': show_help(); return EXIT_SUCCESS;
		default:
			show_help();
			server_free(srv);

			return EXIT_FAILURE;
		}
	}

	if (!srv->config_storage) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"No configuration available. Try using -f option.");

		server_free(srv);
		return EXIT_FAILURE;
	}

	if (print_config) {
		data_unset *dc = srv->config_context->data[0];
		if (dc) {
			dc->print(dc, 0);
			fprintf(stdout, "\n");
		} else {
			/* shouldn't happend */
			fprintf(stderr, "global config not found\n");
		}
	}

	if (test_config) {
		printf("Syntax OK\n");
	}

	if (test_config || print_config) {
		server_free(srv);
		return EXIT_SUCCESS;
	}

	/* close stdin and stdout, as they are not needed */
	openDevNull(STDIN_FILENO);
	openDevNull(STDOUT_FILENO);

	if (0 != config_set_defaults(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"setting default values failed");
		server_free(srv);
		return EXIT_FAILURE;
	}

	/* UID handling */
#ifdef HAVE_GETUID
	if (!i_am_root && issetugid()) {
		/* we are setuid-root */

		log_error_write(srv, __FILE__, __LINE__, "s",
				"Are you nuts ? Don't apply a SUID bit to this binary");

		server_free(srv);
		return EXIT_FAILURE;
	}
#endif

	/* check document-root */
	if (srv->config_storage[0]->document_root->used <= 1) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"document-root is not set\n");

		server_free(srv);

		return EXIT_FAILURE;
	}

	if (plugins_load(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"loading plugins finally failed");

		plugins_free(srv);
		server_free(srv);

		return EXIT_FAILURE;
	}

	/* open pid file BEFORE chroot */
	if (srv->srvconf.pid_file->used) {
		if (-1 == (pid_fd = open(srv->srvconf.pid_file->ptr, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
			struct stat st;
			if (errno != EEXIST) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"opening pid-file failed:", srv->srvconf.pid_file, strerror(errno));
				return EXIT_FAILURE;
			}

			if (0 != stat(srv->srvconf.pid_file->ptr, &st)) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"stating existing pid-file failed:", srv->srvconf.pid_file, strerror(errno));
			}

			if (!S_ISREG(st.st_mode)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"pid-file exists and isn't regular file:", srv->srvconf.pid_file);
				return EXIT_FAILURE;
			}

			if (-1 == (pid_fd = open(srv->srvconf.pid_file->ptr, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"opening pid-file failed:", srv->srvconf.pid_file, strerror(errno));
				return EXIT_FAILURE;
			}
		}
	}

	if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
		/* select limits itself
		 *
		 * as it is a hard limit and will lead to a segfault we add some safety
		 * */
		srv->max_fds = FD_SETSIZE - 200;
	} else {
		srv->max_fds = 4096;
	}

	if (i_am_root) {
		int use_rlimit = 1;

#ifdef HAVE_VALGRIND_VALGRIND_H
		if (RUNNING_ON_VALGRIND) use_rlimit = 0;
#endif

#ifdef HAVE_GETRLIMIT
		if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
			log_error_write(srv, __FILE__, __LINE__,
					"ss", "couldn't get 'max filedescriptors'",
					strerror(errno));
			return EXIT_FAILURE;
		}

		if (use_rlimit && srv->srvconf.max_fds) {
			/* set rlimits */

			rlim.rlim_cur = srv->srvconf.max_fds;
			rlim.rlim_max = srv->srvconf.max_fds;

			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
				log_error_write(srv, __FILE__, __LINE__,
						"ss", "couldn't set 'max filedescriptors'",
						strerror(errno));
				return EXIT_FAILURE;
			}
		}

		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			srv->max_fds = rlim.rlim_cur < ((int)FD_SETSIZE) - 200 ? rlim.rlim_cur : FD_SETSIZE - 200;
		} else {
			srv->max_fds = rlim.rlim_cur;
		}

		/* set core file rlimit, if enable_cores is set */
		if (use_rlimit && srv->srvconf.enable_cores && getrlimit(RLIMIT_CORE, &rlim) == 0) {
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}
#endif
		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			/* don't raise the limit above FD_SET_SIZE */
			if (srv->max_fds > ((int)FD_SETSIZE) - 200) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"can't raise max filedescriptors above",  FD_SETSIZE - 200,
						"if event-handler is 'select'. Use 'poll' or something else or reduce server.max-fds.");
				return EXIT_FAILURE;
			}
		}


#ifdef HAVE_PWD_H
		/* set user and group */
		if (srv->srvconf.username->used) {
			if (NULL == (pwd = getpwnam(srv->srvconf.username->ptr))) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"can't find username", srv->srvconf.username);
				return EXIT_FAILURE;
			}

			if (pwd->pw_uid == 0) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"I will not set uid to 0\n");
				return EXIT_FAILURE;
			}
		}

		if (srv->srvconf.groupname->used) {
			if (NULL == (grp = getgrnam(srv->srvconf.groupname->ptr))) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"can't find groupname", srv->srvconf.groupname);
				return EXIT_FAILURE;
			}
			if (grp->gr_gid == 0) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"I will not set gid to 0\n");
				return EXIT_FAILURE;
			}
		}
#endif
	} else {

#ifdef HAVE_GETRLIMIT
		if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
			log_error_write(srv, __FILE__, __LINE__,
					"ss", "couldn't get 'max filedescriptors'",
					strerror(errno));
			return EXIT_FAILURE;
		}

		/**
		 * we are not root can can't increase the fd-limit, but we can reduce it
		 */
		if (srv->srvconf.max_fds && srv->srvconf.max_fds < (int)rlim.rlim_cur) {
			/* set rlimits */

			rlim.rlim_cur = srv->srvconf.max_fds;

			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
				log_error_write(srv, __FILE__, __LINE__,
						"ss", "couldn't set 'max filedescriptors'",
						strerror(errno));
				return EXIT_FAILURE;
			}
		}

		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			srv->max_fds = rlim.rlim_cur < ((int)FD_SETSIZE) - 200 ? rlim.rlim_cur : FD_SETSIZE - 200;
		} else {
			srv->max_fds = rlim.rlim_cur;
		}

		/* set core file rlimit, if enable_cores is set */
		if (srv->srvconf.enable_cores && getrlimit(RLIMIT_CORE, &rlim) == 0) {
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}

#endif
		if (srv->event_handler == FDEVENT_HANDLER_SELECT) {
			/* don't raise the limit above FD_SET_SIZE */
			if (srv->max_fds > ((int)FD_SETSIZE) - 200) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"can't raise max filedescriptors above",  FD_SETSIZE - 200,
						"if event-handler is 'select'. Use 'poll' or something else or reduce server.max-fds.");
				return EXIT_FAILURE;
			}
		}
	}
#ifdef MULTI_THREADED

#if defined HAVE_FORK
	/* network is up, let's deamonize ourself */
	if (srv->srvconf.dont_daemonize == 0) daemonize();
#endif
#ifdef USE_MTCP
	/* set max-conns */
	set_max_conns(srv, 0);
#else
	/* set the first_entry field */
	srv->first_entry = srv;

	/* set max-conns */
	set_max_conns(srv, 1);
#endif
	/* thread-wide network initialization  */
	/* first initialize srv_states */
	init_server_states(&srv_states, cpus, srv, conf_file);

#ifdef USE_MTCP
	/** 
	 * it is important that core limit is set 
	 * before mtcp_init() is called. You can
	 * not set core_limit after mtcp_init()
	 */
	mtcp_getconf(&mcfg);
	mcfg.num_cores = cpus;
	mtcp_setconf(&mcfg);
	/* initialize the mtcp context */
	if (mtcp_init("mtcp.conf")) {
		fprintf(stderr, "Failed to initialize mtcp\n");
		goto clean_up;
	}

	mtcp_getconf(&mcfg);
	mcfg.max_concurrency = mcfg.max_num_buffers = srv_states[0]->max_conns;
	mtcp_setconf(&mcfg);

	/* register SIGINT signal handler */
	mtcp_register_signal(SIGINT, signal_handler);
#endif
	/* now spawn the threads and initialize the underlying networking layer */
	for (i = 0; i < cpus; i++) {
		srv_states[i]->cpu = i;
#if 0
		start_server((void *)srv_states[i]);
#endif
		if (pthread_create(&srv_states[i]->running_thread, NULL,
				   start_server, (void *)srv_states[i])) {
		  goto clean_up;
		}
	}

	/*
	 * ~~~ MTCP UPDATE ~~~
	 * From this point onwards, the per-core `engine' running_thread does not execute
	 * the following code.
	 * The main thread, however, executes the remaining system-wide initialization logic
	 * before it sleeps indefintely... (well... not quite... it waits till the threads
	 * commit suicide)
	 * ~~~ !MTCP UPDATE! ~~~
	 */
#else
	/* we need root-perms for port < 1024 */
	if (0 != network_init(srv)) {
		plugins_free(srv);
		server_free(srv);
		
		return EXIT_FAILURE;
	}
#endif /* !MULTI_THREADED */

	if (i_am_root) {
#ifdef HAVE_PWD_H
		/* 
		 * Change group before chroot, when we have access
		 * to /etc/group
		 * */
		if (NULL != grp) {
			setgid(grp->gr_gid);
			setgroups(0, NULL);
			if (srv->srvconf.username->used) {
				initgroups(srv->srvconf.username->ptr, grp->gr_gid);
			}
		}
#endif
#ifdef HAVE_CHROOT
		if (srv->srvconf.changeroot->used) {
			tzset();

			if (-1 == chroot(srv->srvconf.changeroot->ptr)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "chroot failed: ", strerror(errno));
				return EXIT_FAILURE;
			}
			if (-1 == chdir("/")) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "chdir failed: ", strerror(errno));
				return EXIT_FAILURE;
			}
		}
#endif
#ifdef HAVE_PWD_H
		/* drop root privs */
		if (NULL != pwd) {
			setuid(pwd->pw_uid);
		}
#endif
#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_DUMPABLE)
		/**
		 * on IRIX 6.5.30 they have prctl() but no DUMPABLE
		 */
		if (srv->srvconf.enable_cores) {
			prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
		}
#endif
	}

#ifndef MULTI_THREADED
	/* set max-conns */
	set_max_conns(srv, 1);
	load_plugins(srv);
#endif

#ifdef HAVE_FORK
#ifndef MULTI_THREADED
	/* network is up, let's deamonize ourself */
	if (srv->srvconf.dont_daemonize == 0) daemonize();
#endif
#endif

	srv->gid = getgid();
	srv->uid = getuid();

	/* write pid file */
	if (pid_fd != -1) {
		buffer_copy_long(srv->tmp_buf, getpid());
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("\n"));
		write(pid_fd, srv->tmp_buf->ptr, srv->tmp_buf->used - 1);
		close(pid_fd);
		pid_fd = -1;
	}

#ifndef MULTI_THREADED
	/* Close stderr ASAP in the child process to make sure that nothing
	 * is being written to that fd which may not be valid anymore. */
	if (-1 == log_error_open(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "Opening errorlog failed. Going down.");

		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return EXIT_FAILURE;
	}

	if (HANDLER_GO_ON != plugins_call_set_defaults(srv)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "Configuration of plugins failed. Going down.");

		plugins_free(srv);
		network_close(srv);
		server_free(srv);

		return EXIT_FAILURE;
	}
#endif
	/* dump unused config-keys */
	for (i = 0; i < srv->config_context->used; i++) {
		array *config = ((data_config *)srv->config_context->data[i])->value;
		size_t j;

		for (j = 0; config && j < config->used; j++) {
			data_unset *du = config->data[j];

			/* all var.* is known as user defined variable */
			if (strncmp(du->key->ptr, "var.", sizeof("var.") - 1) == 0) {
				continue;
			}

			if (NULL == array_get_element(srv->config_touched, du->key->ptr)) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"WARNING: unknown config-key:",
						du->key,
						"(ignored)");
			}
		}
	}

	if (srv->config_unsupported) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"Configuration contains unsupported keys. Going down.");
	}

	if (srv->config_deprecated) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"Configuration contains deprecated keys. Going down.");
	}

	if (srv->config_unsupported || srv->config_deprecated) {
		plugins_free(srv);
		network_close(srv);
		server_free(srv);

		return EXIT_FAILURE;
	}

#ifdef HAVE_SIGACTION
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);
# if defined(SA_SIGINFO)
	act.sa_sigaction = sigaction_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
# else
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
# endif
#ifndef USE_MTCP
	sigaction(SIGINT,  &act, NULL);
#endif
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);

#elif defined(HAVE_SIGNAL)
	Control is not coming here
	/* ignore the SIGPIPE from sendfile() */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGALRM, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP,  signal_handler);
	signal(SIGCHLD,  signal_handler);
	signal(SIGINT,  signal_handler);
#endif

#ifdef USE_ALARM
	signal(SIGALRM, signal_handler);

	/* setup periodic timer (1 second) */
	if (setitimer(ITIMER_REAL, &interval, NULL)) {
		log_error_write(srv, __FILE__, __LINE__, "s", "setting timer failed");
		return EXIT_FAILURE;
	}

	getitimer(ITIMER_REAL, &interval);
#endif

#ifdef HAVE_FORK
	/* MTCP/MULTI_THREADED UPDATE: num_childs is always zero here. Ignoring this entire code snippet */
	/* start watcher and workers */
	num_childs = srv->srvconf.max_worker;
	if (num_childs > 0) {
		int child = 0;
		while (!child && !srv_shutdown && !graceful_shutdown) {
			if (num_childs > 0) {
				switch (fork()) {
				case -1:
					return EXIT_FAILURE;
				case 0:
					child = 1;
					break;
				default:
					num_childs--;
					break;
				}
			} else {
				int status;

				if (-1 != wait(&status)) {
					/** 
					 * one of our workers went away 
					 */
					num_childs++;
				} else {
					switch (errno) {
					case EINTR:
						/**
						 * if we receive a SIGHUP we have to close our logs ourself as we don't 
						 * have the mainloop who can help us here
						 */
						if (handle_sig_hup) {
							handle_sig_hup = 0;

							log_error_cycle(srv);

							/**
							 * forward to all procs in the process-group
							 * 
							 * we also send it ourself
							 */
							if (!forwarded_sig_hup) {
								forwarded_sig_hup = 1;
								kill(0, SIGHUP);
							}
						}
						break;
					default:
						break;
					}
				}
			}
		}

		/**
		 * for the parent this is the exit-point 
		 */
		if (!child) {
			/** 
			 * kill all children too 
			 */
			if (graceful_shutdown) {
				kill(0, SIGINT);
			} else if (srv_shutdown) {
				kill(0, SIGTERM);
			}

			log_error_close(srv);
			network_close(srv);
			connections_free(srv);
			plugins_free(srv);
			server_free(srv);
			return EXIT_SUCCESS;
		}
	}
#endif /* !HAVE_FORK */

#ifndef MULTI_THREADED
	initialize_fd_framework(srv);

	/* libev backend overwrites our SIGCHLD handler and calls waitpid on SIGCHLD; we want our own SIGCHLD handling. */
#ifdef HAVE_SIGACTION
	sigaction(SIGCHLD, &act, NULL);
#elif defined(HAVE_SIGNAL)
	signal(SIGCHLD,  signal_handler);
#endif

	/* This part of code is only executed in the single-process, single-threaded version (non-mtcp/non-multithreaded) */
	/* Under USE_MTCP settings, each individual `running_thread' executes the `main-loop' */
	/* In USE_MTCP settings main thread will execute the flowing step */
	/* main-loop */
	while (!srv_shutdown) {
		int n;
		size_t ndx;
		time_t min_ts;

		if (handle_sig_hup) {
			handler_t r;

			/* reset notification */
			handle_sig_hup = 0;


			/* cycle logfiles */

			switch(r = plugins_call_handle_sighup(srv)) {
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sd", "sighup-handler return with an error", r);
				break;
			}

			if (-1 == log_error_cycle(srv)) {
				log_error_write(srv, __FILE__, __LINE__, "s", "cycling errorlog failed, dying");

				return EXIT_FAILURE;
			} else {
#ifdef HAVE_SIGACTION
				log_error_write(srv, __FILE__, __LINE__, "sdsd", 
					"logfiles cycled UID =",
					last_sighup_info.si_uid,
					"PID =",
					last_sighup_info.si_pid);
#else
				log_error_write(srv, __FILE__, __LINE__, "s", 
					"logfiles cycled");
#endif
			}
		}

		if (handle_sig_alarm) {
			/* a new second */

#ifdef USE_ALARM
			/* reset notification */
			handle_sig_alarm = 0;
#endif

			/* get current time */
			min_ts = time(NULL);

			if (min_ts != srv->cur_ts) {
				int cs = 0;
				connections *conns = srv->conns;
				handler_t r;

				switch(r = plugins_call_handle_trigger(srv)) {
				case HANDLER_GO_ON:
					break;
				case HANDLER_ERROR:
					log_error_write(srv, __FILE__, __LINE__, "s", "one of the triggers failed");
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}

				/* trigger waitpid */
				srv->cur_ts = min_ts;

				/* cleanup stat-cache */
				stat_cache_trigger_cleanup(srv);
				/**
				 * check all connections for timeouts
				 *
				 */
				for (ndx = 0; ndx < conns->used; ndx++) {
					int changed = 0;
					connection *con;
					int t_diff;

					con = conns->ptr[ndx];

					if (con->state == CON_STATE_READ ||
					    con->state == CON_STATE_READ_POST) {
						if (con->request_count == 1) {
							if (srv->cur_ts - con->read_idle_ts > con->conf.max_read_idle) {
								/* time - out */
#if 0
								log_error_write(srv, __FILE__, __LINE__, "sd",
										"connection closed - read-timeout:", con->fd);
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						} else {
							if (srv->cur_ts - con->read_idle_ts > con->keep_alive_idle) {
								/* time - out */
#if 0
								log_error_write(srv, __FILE__, __LINE__, "sd",
										"connection closed - read-timeout:", con->fd);
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						}
					}

					if ((con->state == CON_STATE_WRITE) &&
					    (con->write_request_ts != 0)) {
#if 0
						if (srv->cur_ts - con->write_request_ts > 60) {
							log_error_write(srv, __FILE__, __LINE__, "sdd",
									"connection closed - pre-write-request-timeout:", con->fd, srv->cur_ts - con->write_request_ts);
						}
#endif

						if (srv->cur_ts - con->write_request_ts > con->conf.max_write_idle) {
							/* time - out */
							if (con->conf.log_timeouts) {
								log_error_write(srv, __FILE__, __LINE__, "sbsosds",
									"NOTE: a request for",
									con->request.uri,
									"timed out after writing",
									con->bytes_written,
									"bytes. We waited",
									(int)con->conf.max_write_idle,
									"seconds. If this a problem increase server.max-write-idle");
							}
							connection_set_state(srv, con, CON_STATE_ERROR);
							changed = 1;
						}
					}

					if (con->state == CON_STATE_CLOSE && (srv->cur_ts - con->close_timeout_ts > HTTP_LINGER_TIMEOUT)) {
						changed = 1;
					}

					/* we don't like div by zero */
					if (0 == (t_diff = srv->cur_ts - con->connection_start)) t_diff = 1;

					if (con->traffic_limit_reached &&
					    (con->conf.kbytes_per_second == 0 ||
					     ((con->bytes_written / t_diff) < con->conf.kbytes_per_second * 1024))) {
						/* enable connection again */
						con->traffic_limit_reached = 0;

						changed = 1;
					}

					if (changed) {
						connection_state_machine(srv, con);
					}
					con->bytes_written_cur_second = 0;
					*(con->conf.global_bytes_per_second_cnt_ptr) = 0;

#if 0
					if (cs == 0) {
						fprintf(stderr, "connection-state: ");
						cs = 1;
					}

					fprintf(stderr, "c[%d,%d]: %s ",
						con->fd,
						con->fcgi.fd,
						connection_get_state(con->state));
#endif
				}

				if (cs == 1) fprintf(stderr, "\n");
			}
		}

		if (srv->sockets_disabled) {
			/* our server sockets are disabled, why ? */

			if ((srv->cur_fds + srv->want_fds < srv->max_fds * 8 / 10) && /* we have enough unused fds */
			    (srv->conns->used <= srv->max_conns * 9 / 10) &&
			    (0 == graceful_shutdown)) {
				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_set(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd, FDEVENT_IN);
				}

				log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets enabled again");

				srv->sockets_disabled = 0;
			}
		} else {
			if ((srv->cur_fds + srv->want_fds > srv->max_fds * 9 / 10) || /* out of fds */
			    (srv->conns->used >= srv->max_conns) || /* out of connections */
			    (graceful_shutdown)) { /* graceful_shutdown */

				/* disable server-fds */

				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_del(srv->ev, &(srv_socket->fde_ndx), srv_socket->fd);

					if (graceful_shutdown) {
						/* we don't want this socket anymore,
						 *
						 * closing it right away will make it possible for
						 * the next lighttpd to take over (graceful restart)
						 *  */

						fdevent_unregister(srv->ev, srv_socket->fd);
						close(srv_socket->fd);
						srv_socket->fd = -1;

						/* network_close() will cleanup after us */

						if (srv->srvconf.pid_file->used &&
						    srv->srvconf.changeroot->used == 0) {
							if (0 != unlink(srv->srvconf.pid_file->ptr)) {
								if (errno != EACCES && errno != EPERM) {
									log_error_write(srv, __FILE__, __LINE__, "sbds",
											"unlink failed for:",
											srv->srvconf.pid_file,
											errno,
											strerror(errno));
								}
							}
						}
					}
				}

				if (graceful_shutdown) {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] graceful shutdown started");
				} else if (srv->conns->used >= srv->max_conns) {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets disabled, connection limit reached");
				} else {
					log_error_write(srv, __FILE__, __LINE__, "s", "[note] sockets disabled, out-of-fds");
				}

				srv->sockets_disabled = 1;
			}
		}

		if (graceful_shutdown && srv->conns->used == 0) {
			/* we are in graceful shutdown phase and all connections are closed
			 * we are ready to terminate without harming anyone */
			srv_shutdown = 1;
		}

		/* we still have some fds to share */
		if (srv->want_fds) {
			/* check the fdwaitqueue for waiting fds */
			int free_fds = srv->max_fds - srv->cur_fds - 16;
			connection *con;

			for (; free_fds > 0 && NULL != (con = fdwaitqueue_unshift(srv, srv->fdwaitqueue)); free_fds--) {
				connection_state_machine(srv, con);

				srv->want_fds--;
			}
		}

		if ((n = fdevent_poll(srv->ev, 1000)) > 0) {
			/* n is the number of events */
			int revents;
			int fd_ndx;
#if 0
			if (n > 0) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"polls:", n);
			}
#endif
			fd_ndx = -1;
			do {
				fdevent_handler handler;
				void *context;
				handler_t r;
				int fd;

				fd_ndx  = fdevent_event_next_fdndx (srv->ev, fd_ndx);
				if (-1 == fd_ndx) break; /* not all fdevent handlers know how many fds got an event */

				revents = fdevent_event_get_revent (srv->ev, fd_ndx);
				fd      = fdevent_event_get_fd     (srv->ev, fd_ndx);
				handler = fdevent_get_handler(srv->ev, fd);
				context = fdevent_get_context(srv->ev, fd);

				/* connection_handle_fdevent needs a joblist_append */
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sdd",
						"event for", fd, revents);
#endif
				switch (r = (*handler)(srv, context, revents)) {
				case HANDLER_FINISHED:
				case HANDLER_GO_ON:
				case HANDLER_WAIT_FOR_EVENT:
				case HANDLER_WAIT_FOR_FD:
					break;
				case HANDLER_ERROR:
					/* should never happen */
					SEGFAULT();
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}
			} while (--n > 0);
		} else if (n < 0 && errno != EINTR) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"fdevent_poll failed:",
					strerror(errno));
		}

		for (ndx = 0; ndx < srv->joblist->used; ndx++) {
			connection *con = srv->joblist->ptr[ndx];
			handler_t r;

			connection_state_machine(srv, con);

			switch(r = plugins_call_handle_joblist(srv, con)) {
			case HANDLER_FINISHED:
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "d", r);
				break;
			}

			con->in_joblist = 0;
		}

		srv->joblist->used = 0;
	} /* end of `while (!srv_shutdown)` */

#if 0
	if (srv->srvconf.pid_file->used &&
	    srv->srvconf.changeroot->used == 0 &&
	    0 == graceful_shutdown) {
		if (0 != unlink(srv->srvconf.pid_file->ptr)) {
			if (errno != EACCES && errno != EPERM) {
				log_error_write(srv, __FILE__, __LINE__, "sbds",
						"unlink failed for:",
						srv->srvconf.pid_file,
						errno,
						strerror(errno));
			}
		}
	}
#endif
#ifdef HAVE_SIGACTION
	log_error_write(srv, __FILE__, __LINE__, "sdsd", 
			"server stopped by UID =",
			last_sigterm_info.si_uid,
			"PID =",
			last_sigterm_info.si_pid);
#else
	log_error_write(srv, __FILE__, __LINE__, "s", 
			"server stopped");
#endif
#endif /* !MULTI_THREADED */

#ifdef MULTI_THREADED
	/* main thread waits... */
	for (i = 0; i < cpus; i++)
		pthread_join(srv_states[i]->running_thread, NULL);
 clean_up:
#ifdef USE_MTCP
	/* destroy mtcp context */
	mtcp_destroy();
#endif
	for (i = 0; i < cpus; i++) {
		srv = srv_states[i];
#endif /* MULTI_THREADED */
		/* clean-up */
		log_error_close(srv);
		network_close(srv);
		connections_free(srv);
		plugins_free(srv);
		server_free(srv);
#ifdef MULTI_THREADED
	}
	free(conf_file);
	free(srv_states);
#endif /* !MULTI_THREADED */
	return EXIT_SUCCESS;
}
/*----------------------------------------------------------------------------*/
