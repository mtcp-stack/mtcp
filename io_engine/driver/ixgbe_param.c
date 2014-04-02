/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2009 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/types.h>
#include <linux/module.h>

#include "ixgbe.h"

/* This is the only thing that needs to be changed to adjust the
 * maximum number of ports that the driver can manage.
 */

#define IXGBE_MAX_NIC 8

#define OPTION_UNSET    -1
#define OPTION_DISABLED 0
#define OPTION_ENABLED  1

/* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */

#define IXGBE_PARAM_INIT { [0 ... IXGBE_MAX_NIC] = OPTION_UNSET }
#ifndef module_param_array
/* Module Parameters are always initialized to -1, so that the driver
 * can tell the difference between no user specified value or the
 * user asking for the default value.
 * The true default values are loaded in when ixgbe_check_options is called.
 *
 * This is a GCC extension to ANSI C.
 * See the item "Labeled Elements in Initializers" in the section
 * "Extensions to the C Language Family" of the GCC documentation.
 */

#define IXGBE_PARAM(X, desc) \
	static const int __devinitdata X[IXGBE_MAX_NIC+1] = IXGBE_PARAM_INIT; \
	MODULE_PARM(X, "1-" __MODULE_STRING(IXGBE_MAX_NIC) "i"); \
	MODULE_PARM_DESC(X, desc);
#else
#define IXGBE_PARAM(X, desc) \
	static int __devinitdata X[IXGBE_MAX_NIC+1] = IXGBE_PARAM_INIT; \
	static unsigned int num_##X; \
	module_param_array_named(X, X, int, &num_##X, 0); \
	MODULE_PARM_DESC(X, desc);
#endif

IXGBE_PARAM(RXKernel, "Disable(0) or enable(1) Linux TCP/IP stack RX, default 0");

/* Interrupt Type
 *
 * Valid Range: 0-2
 *  - 0 - Legacy Interrupt
 *  - 1 - MSI Interrupt
 *  - 2 - MSI-X Interrupt(s)
 *
 * Default Value: 2
 */
IXGBE_PARAM(InterruptType, "Change Interrupt Mode (0=Legacy, 1=MSI, 2=MSI-X), default 2");
#define IXGBE_INT_LEGACY		      0
#define IXGBE_INT_MSI			      1
#define IXGBE_INT_MSIX			      2
#define IXGBE_DEFAULT_INT	 IXGBE_INT_MSIX

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
/* DCA - Direct Cache Access (DCA) Control
 *
 * This option allows the device to hint to DCA enabled processors
 * which CPU should have its cache warmed with the data being
 * transferred over PCIe.  This can increase performance by reducing
 * cache misses.  ixgbe hardware supports DCA for:
 * tx descriptor writeback
 * rx descriptor writeback
 * rx data
 * rx data header only (in packet split mode)
 *
 * enabling option 2 can cause cache thrash in some tests, particularly
 * if the CPU is completely utilized
 *
 * Valid Range: 0 - 2
 *  - 0 - disables DCA
 *  - 1 - enables DCA
 *  - 2 - enables DCA with rx data included
 *
 * Default Value: 2
 */

#define IXGBE_MAX_DCA 0

IXGBE_PARAM(DCA, "Disable or enable Direct Cache Access, 0=disabled, 1=descriptor only, 2=descriptor and data");

#endif

/* RXQ - The number of RX queues with RSS distribution
 *
 * Valid Range: 0-16
 *  - 0 - Default, min(16, num_online_cpus())
 *  - 1-16 - sets the Desc. Q's to the specified value.
 *
 * Default Value: 0
 */

IXGBE_PARAM(RXQ, "Number of RX queues, default 0=number of cpus");

/* TXQ - The number of TX queues
 *
 * Valid Range: 0-16
 *  - 0 - Default, min(16, num_online_cpus())
 *  - 1-16 - sets the Desc. Q's to the specified value.
 *
 * Default Value: 0
 */

IXGBE_PARAM(TXQ, "Number of TX queues, default 0=number of cpus");

/* Interrupt Throttle Rate (interrupts/sec)
 *
 * Valid Range: 956-488281 (0=off, 1=dynamic)
 *
 * Default Value: 8000
 */
#define DEFAULT_ITR                 956
IXGBE_PARAM(InterruptThrottleRate, "Maximum interrupts per second, per vector, (956-488281), default 8000");
#define MAX_ITR       IXGBE_MAX_INT_RATE
#define MIN_ITR       IXGBE_MIN_INT_RATE

#ifndef IXGBE_NO_LLI
/* LLIPort (Low Latency Interrupt TCP Port)
 *
 * Valid Range: 0 - 65535
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIPort, "Low Latency Interrupt TCP Port (0-65535)");

#define DEFAULT_LLIPORT                0
#define MAX_LLIPORT               0xFFFF
#define MIN_LLIPORT                    0

/* LLIPush (Low Latency Interrupt on TCP Push flag)
 *
 * Valid Range: 0,1
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIPush, "Low Latency Interrupt on TCP Push flag (0,1)");

#define DEFAULT_LLIPUSH                0
#define MAX_LLIPUSH                    1
#define MIN_LLIPUSH                    0

/* LLISize (Low Latency Interrupt on Packet Size)
 *
 * Valid Range: 0 - 1500
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLISize, "Low Latency Interrupt on Packet Size (0-1500)");

#define DEFAULT_LLISIZE                0
#define MAX_LLISIZE                 1500
#define MIN_LLISIZE                    0

/* LLIEType (Low Latency Interrupt Ethernet Type)
 *
 * Valid Range: 0 - 0x8fff
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIEType, "Low Latency Interrupt Ethernet Protocol Type");

#define DEFAULT_LLIETYPE               0
#define MAX_LLIETYPE              0x8fff
#define MIN_LLIETYPE                   0

/* LLIVLANP (Low Latency Interrupt on VLAN priority threshold)
 *
 * Valid Range: 0 - 7
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIVLANP, "Low Latency Interrupt on VLAN priority threshold");

#define DEFAULT_LLIVLANP               0
#define MAX_LLIVLANP                   7
#define MIN_LLIVLANP                   0

#endif /* IXGBE_NO_LLI */
/* Rx buffer mode
 *
 * Valid Range: 0-2 0 = 1buf_mode_always, 1 = ps_mode_always and 2 = optimal
 *
 * Default Value: 2
 */
IXGBE_PARAM(RxBufferMode, "0=1 descriptor per packet,\n"
                          "\t\t\t1=use packet split, multiple descriptors per jumbo frame\n"
                          "\t\t\t2 (default)=use 1buf mode for 1500 mtu, packet split for jumbo");

#define IXGBE_RXBUFMODE_1BUF_ALWAYS			0
#define IXGBE_RXBUFMODE_PS_ALWAYS			1
#define IXGBE_RXBUFMODE_OPTIMAL				2
#define IXGBE_DEFAULT_RXBUFMODE	  IXGBE_RXBUFMODE_1BUF_ALWAYS

/* Flow Director filtering mode
 *
 * Valid Range: 0-2  0 = off, 1 = Hashing (ATR), and 2 = perfect filters
 *
 * Default Value: 1 (ATR)
 */
IXGBE_PARAM(FdirMode, "Flow Director filtering modes:\n"
	              "\t\t\t0 = Filtering off\n"
	              "\t\t\t1 = Signature Hashing filters (SW ATR)\n"
	              "\t\t\t2 = Perfect Filters");

#define IXGBE_FDIR_FILTER_OFF				0
#define IXGBE_FDIR_FILTER_HASH				1
#define IXGBE_FDIR_FILTER_PERFECT			2
/* #define IXGBE_DEFAULT_FDIR_FILTER  IXGBE_FDIR_FILTER_HASH */
#define IXGBE_DEFAULT_FDIR_FILTER  IXGBE_FDIR_FILTER_OFF

/* Flow Director packet buffer allocation level
 *
 * Valid Range: 0-2  0 = 8k hash/2k perfect, 1 = 16k hash/4k perfect,
 *                   2 = 32k hash/8k perfect
 *
 * Default Value: 0
 */
IXGBE_PARAM(FdirPballoc, "Flow Director packet buffer allocation level:\n"
	                 "\t\t\t0 = 8k hash filters or 2k perfect filters\n"
	                 "\t\t\t1 = 16k hash filters or 4k perfect filters\n"
	                 "\t\t\t2 = 32k hash filters or 8k perfect filters");

#define IXGBE_FDIR_PBALLOC_64K				0
#define IXGBE_FDIR_PBALLOC_128K				1
#define IXGBE_FDIR_PBALLOC_256K				2
#define IXGBE_DEFAULT_FDIR_PBALLOC IXGBE_FDIR_PBALLOC_64K

/* Software ATR packet sample rate
 *
 * Valid Range: 0-100  0 = off, 1-100 = rate of Tx packet inspection
 *
 * Default Value: 20
 */
IXGBE_PARAM(AtrSampleRate, "Software ATR Tx packet sample rate");

#define IXGBE_MAX_ATR_SAMPLE_RATE	100
#define IXGBE_MIN_ATR_SAMPLE_RATE	  1
#define IXGBE_ATR_SAMPLE_RATE_OFF	  0
#define IXGBE_DEFAULT_ATR_SAMPLE_RATE	 20

struct ixgbe_option {
	enum { enable_option, range_option, list_option } type;
	const char *name;
	const char *err;
	int def;
	union {
		struct { /* range_option info */
			int min;
			int max;
		} r;
		struct { /* list_option info */
			int nr;
			const struct ixgbe_opt_list {
				int i;
				char *str;
			} *p;
		} l;
	} arg;
};

static int __devinit ixgbe_validate_option(unsigned int *value,
                                           struct ixgbe_option *opt)
{
	if (*value == OPTION_UNSET) {
		*value = opt->def;
		return 0;
	}

	switch (opt->type) {
	case enable_option:
		switch (*value) {
		case OPTION_ENABLED:
			printk(KERN_INFO "ixgbe: %s Enabled\n", opt->name);
			return 0;
		case OPTION_DISABLED:
			printk(KERN_INFO "ixgbe: %s Disabled\n", opt->name);
			return 0;
		}
		break;
	case range_option:
		if (*value >= opt->arg.r.min && *value <= opt->arg.r.max) {
			printk(KERN_INFO "ixgbe: %s set to %d\n", opt->name, *value);
			return 0;
		}
		break;
	case list_option: {
		int i;
		const struct ixgbe_opt_list *ent;

		for (i = 0; i < opt->arg.l.nr; i++) {
			ent = &opt->arg.l.p[i];
			if (*value == ent->i) {
				if (ent->str[0] != '\0')
					printk(KERN_INFO "%s\n", ent->str);
				return 0;
			}
		}
	}
		break;
	default:
		BUG();
	}

	printk(KERN_INFO "ixgbe: Invalid %s specified (%d),  %s\n",
	       opt->name, *value, opt->err);
	*value = opt->def;
	return -1;
}

#define LIST_LEN(l) (sizeof(l) / sizeof(l[0]))

/**
 * ixgbe_check_options - Range Checking for Command Line Parameters
 * @adapter: board private structure
 *
 * This routine checks all command line parameters for valid user
 * input.  If an invalid value is given, or if no user specified
 * value exists, a default value is used.  The final value is stored
 * in a variable in the adapter structure.
 **/
void __devinit ixgbe_check_options(struct ixgbe_adapter *adapter)
{
	int bd = adapter->bd_number;
	u32 *aflags = &adapter->flags;
	struct ixgbe_ring_feature *feature = adapter->ring_feature;

	if (bd >= IXGBE_MAX_NIC) {
		printk(KERN_NOTICE
		       "Warning: no configuration for board #%d\n", bd);
		printk(KERN_NOTICE "Using defaults for all values\n");
#ifndef module_param_array
		bd = IXGBE_MAX_NIC;
#endif
	}

	{ /* Linux RX Stack Support */
		static struct ixgbe_option opt = {
			.type = enable_option,
			.name = "Linux TCP/IP stack RX",
			.err  = "defaulting to Disabled",
			.def  = OPTION_DISABLED
		};

#ifdef module_param_array
		if (num_RXKernel > bd) {
#endif
			unsigned int tmp = RXKernel[bd];
			ixgbe_validate_option(&tmp, &opt);
			if (tmp)
				*aflags |= IXGBE_FLAG_RX_KERNEL_ENABLE;
			else
				*aflags &= ~IXGBE_FLAG_RX_KERNEL_ENABLE;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_ENABLED)
				*aflags |= IXGBE_FLAG_RX_KERNEL_ENABLE;
			else
				*aflags &= ~IXGBE_FLAG_RX_KERNEL_ENABLE;
		}
#endif
	}
	{ /* Interrupt Type */
		unsigned int i_type;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Interrupt Type",
			.err =
			  "using default of "__MODULE_STRING(IXGBE_DEFAULT_INT),
			.def = IXGBE_DEFAULT_INT,
			.arg = { .r = { .min = IXGBE_INT_LEGACY,
					.max = IXGBE_INT_MSIX}}
		};

#ifdef module_param_array
		if (num_InterruptType > bd) {
#endif
			i_type = InterruptType[bd];
			ixgbe_validate_option(&i_type, &opt);
			switch (i_type) {
			case IXGBE_INT_MSIX:
				if (!(*aflags & IXGBE_FLAG_MSIX_CAPABLE))
					printk(KERN_INFO
					       "Ignoring MSI-X setting; "
					       "support unavailable\n");
				break;
			case IXGBE_INT_MSI:
				if (!(*aflags & IXGBE_FLAG_MSI_CAPABLE)) {
					printk(KERN_INFO
					       "Ignoring MSI setting; "
					       "support unavailable\n");
				} else {
					*aflags &= ~IXGBE_FLAG_MSIX_CAPABLE;
					*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
				}
				break;
			case IXGBE_INT_LEGACY:
			default:
				*aflags &= ~IXGBE_FLAG_MSIX_CAPABLE;
				*aflags &= ~IXGBE_FLAG_MSI_CAPABLE;
				*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
				break;
			}
#ifdef module_param_array
		} else {
			*aflags |= IXGBE_FLAG_MSIX_CAPABLE;
			*aflags |= IXGBE_FLAG_MSI_CAPABLE;
		}
#endif
	}
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
	{ /* Direct Cache Access (DCA) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Direct Cache Access (DCA)",
			.err  = "defaulting to Enabled",
			.def  = OPTION_DISABLED,
			.arg  = { .r = { .min = OPTION_DISABLED,
					 .max = IXGBE_MAX_DCA}}
		};
		unsigned int dca = opt.def;

#ifdef module_param_array
		if (num_DCA > bd) {
#endif
			dca = DCA[bd];
			ixgbe_validate_option(&dca, &opt);
			if (!dca)
				*aflags &= ~IXGBE_FLAG_DCA_CAPABLE;

			/* Check Interoperability */
			if (!(*aflags & IXGBE_FLAG_DCA_CAPABLE)) {
				DPRINTK(PROBE, INFO, "DCA is disabled\n");
				*aflags &= ~IXGBE_FLAG_DCA_ENABLED;
			}

			if (dca == IXGBE_MAX_DCA) {
				DPRINTK(PROBE, INFO,
				        "DCA enabled for rx data\n");
				adapter->flags |= IXGBE_FLAG_DCA_ENABLED_DATA;
			}
#ifdef module_param_array
		} else {
			/* make sure to clear the capability flag if the
			 * option is disabled by default above */
			if (opt.def == OPTION_DISABLED)
				*aflags &= ~IXGBE_FLAG_DCA_CAPABLE;
		}
#endif
		if (dca == IXGBE_MAX_DCA)
			adapter->flags |= IXGBE_FLAG_DCA_ENABLED_DATA;
	}
#endif /* CONFIG_DCA or CONFIG_DCA_MODULE */
	{ /* # of RX queues with RSS (RXQ) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "RX queues (RXQ)",
			.err  = "using default.",
			.def  = 0,
			.arg  = { .r = { .min = 0,
					 .max = IXGBE_MAX_RSS_INDICES}}
		};
		unsigned int rxq = RXQ[bd];

#ifdef module_param_array
		if (num_RXQ > bd) {
#endif
			switch (rxq) {
			case 0:
				/*
				 * Base it off num_online_cpus() with
				 * a hardware limit cap.
				 */
				rxq = min(IXGBE_MAX_RSS_INDICES,
				          (int)num_online_cpus());
				break;
			default:
				ixgbe_validate_option(&rxq, &opt);
				break;
			}
			feature[RING_F_RXQ].indices = rxq;
			*aflags |= IXGBE_FLAG_RSS_ENABLED;
#ifdef module_param_array
		} else {
			rxq = min(IXGBE_MAX_RSS_INDICES,
				  (int)num_online_cpus());
			feature[RING_F_RXQ].indices = rxq;
			*aflags |= IXGBE_FLAG_RSS_ENABLED;
		}
#endif
	}
	{ /* # of TX queues (TXQ) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "TX queues (TXQ)",
			.err  = "using default.",
			.def  = 0,
			.arg  = { .r = { .min = 0,
					 .max = IXGBE_MAX_RSS_INDICES}}
		};
		unsigned int txq = TXQ[bd];

#ifdef module_param_array
		if (num_TXQ > bd) {
#endif
			switch (txq) {
			case 0:
				/*
				 * Base it off num_online_cpus() with
				 * a hardware limit cap.
				 */
				txq = min(IXGBE_MAX_RSS_INDICES,
				          (int)num_online_cpus());
				break;
			default:
				ixgbe_validate_option(&txq, &opt);
				break;
			}
			feature[RING_F_TXQ].indices = txq;
#ifdef module_param_array
		} else {
			txq = min(IXGBE_MAX_RSS_INDICES,
				  (int)num_online_cpus());
			feature[RING_F_TXQ].indices = txq;
		}
#endif
	}
	{ /* Interrupt Throttling Rate */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Interrupt Throttling Rate (ints/sec)",
			.err  = "using default of "__MODULE_STRING(DEFAULT_ITR),
			.def  = DEFAULT_ITR,
			.arg  = { .r = { .min = MIN_ITR,
					 .max = MAX_ITR }}
		};

#ifdef module_param_array
		if (num_InterruptThrottleRate > bd) {
#endif
			u32 eitr = InterruptThrottleRate[bd];
			switch (eitr) {
			case 0:
				DPRINTK(PROBE, INFO, "%s turned off\n",
				        opt.name);
				/*
				 * zero is a special value, we don't want to
				 * turn off ITR completely, just set it to an
				 * insane interrupt rate
				 */
				adapter->eitr_param = IXGBE_MAX_INT_RATE;
				adapter->itr_setting = 0;
				break;
			case 1:
				DPRINTK(PROBE, INFO, "dynamic interrupt "
                                        "throttling enabled\n");
				adapter->eitr_param = 20000;
				adapter->itr_setting = 1;
				break;
			default:
				ixgbe_validate_option(&eitr, &opt);
				adapter->eitr_param = eitr;
				/* the first bit is used as control */
				adapter->itr_setting = eitr & ~1;
				break;
			}
#ifdef module_param_array
		} else {
			adapter->eitr_param = DEFAULT_ITR;
			adapter->itr_setting = DEFAULT_ITR;
		}
#endif
	}
#ifndef IXGBE_NO_LLI
	{ /* Low Latency Interrupt TCP Port*/
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt TCP Port",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLIPORT),
			.def  = DEFAULT_LLIPORT,
			.arg  = { .r = { .min = MIN_LLIPORT,
					 .max = MAX_LLIPORT }}
		};

#ifdef module_param_array
		if (num_LLIPort > bd) {
#endif
			adapter->lli_port = LLIPort[bd];
			if (adapter->lli_port) {
				ixgbe_validate_option(&adapter->lli_port, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_port = opt.def;
		}
#endif
	}
	{ /* Low Latency Interrupt on Packet Size */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on Packet Size",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLISIZE),
			.def  = DEFAULT_LLISIZE,
			.arg  = { .r = { .min = MIN_LLISIZE,
					 .max = MAX_LLISIZE }}
		};

#ifdef module_param_array
		if (num_LLISize > bd) {
#endif
			adapter->lli_size = LLISize[bd];
			if (adapter->lli_size) {
				ixgbe_validate_option(&adapter->lli_size, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_size = opt.def;
		}
#endif
	}
	{ /*Low Latency Interrupt on TCP Push flag*/
		static struct ixgbe_option opt = {
			.type = enable_option,
			.name = "Low Latency Interrupt on TCP Push flag",
			.err  = "defaulting to Disabled",
			.def  = OPTION_DISABLED
		};

#ifdef module_param_array
		if (num_LLIPush > bd) {
#endif
			unsigned int lli_push = LLIPush[bd];
			ixgbe_validate_option(&lli_push, &opt);
			if (lli_push)
				*aflags |= IXGBE_FLAG_LLI_PUSH;
			else
				*aflags &= ~IXGBE_FLAG_LLI_PUSH;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_ENABLED)
				*aflags |= IXGBE_FLAG_LLI_PUSH;
			else
				*aflags &= ~IXGBE_FLAG_LLI_PUSH;
		}
#endif
	}
	{ /* Low Latency Interrupt EtherType*/
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on Ethernet Protocol Type",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLIETYPE),
			.def  = DEFAULT_LLIETYPE,
			.arg  = { .r = { .min = MIN_LLIETYPE,
					 .max = MAX_LLIETYPE }}
		};

#ifdef module_param_array
		if (num_LLIEType > bd) {
#endif
			adapter->lli_etype = LLIEType[bd];
			if (adapter->lli_etype) {
				ixgbe_validate_option(&adapter->lli_etype, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_etype = opt.def;
		}
#endif
	}
	{ /* LLI VLAN Priority */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on VLAN priority threashold",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLIVLANP),
			.def  = DEFAULT_LLIVLANP,
			.arg  = { .r = { .min = MIN_LLIVLANP,
					 .max = MAX_LLIVLANP }}
		};

#ifdef module_param_array
		if (num_LLIVLANP > bd) {
#endif
			adapter->lli_vlan_pri = LLIVLANP[bd];
			if (adapter->lli_vlan_pri) {
				ixgbe_validate_option(&adapter->lli_vlan_pri, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_vlan_pri = opt.def;
		}
#endif
	}
#endif /* IXGBE_NO_LLI */
	{ /* Rx buffer mode */
		unsigned int rx_buf_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Rx buffer mode",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_RXBUFMODE),
			.def = IXGBE_DEFAULT_RXBUFMODE,
			.arg = {.r = {.min = IXGBE_RXBUFMODE_1BUF_ALWAYS,
				      .max = IXGBE_RXBUFMODE_OPTIMAL}}
		};

#ifdef module_param_array
		if (num_RxBufferMode > bd) {
#endif
			rx_buf_mode = RxBufferMode[bd];
			ixgbe_validate_option(&rx_buf_mode, &opt);
			switch (rx_buf_mode) {
			case IXGBE_RXBUFMODE_OPTIMAL:
				*aflags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
				*aflags |= IXGBE_FLAG_RX_PS_CAPABLE;
				break;
			case IXGBE_RXBUFMODE_PS_ALWAYS:
				*aflags |= IXGBE_FLAG_RX_PS_CAPABLE;
				break;
			case IXGBE_RXBUFMODE_1BUF_ALWAYS:
				*aflags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
			default:
				break;
			}
#ifdef module_param_array
		} else {
			*aflags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
			*aflags |= IXGBE_FLAG_RX_PS_CAPABLE;
		}
#endif
	}
	{ /* Flow Director filtering mode */
		unsigned int fdir_filter_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Flow Director filtering mode",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_FDIR_FILTER),
			.def = IXGBE_DEFAULT_FDIR_FILTER,
			.arg = {.r = {.min = IXGBE_FDIR_FILTER_OFF,
				      .max = IXGBE_FDIR_FILTER_PERFECT}}
		};

		*aflags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
		*aflags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
		if (adapter->hw.mac.type == ixgbe_mac_82598EB)
			goto no_flow_director;
#ifdef module_param_array
		if (num_FdirMode > bd) {
#endif
#ifdef HAVE_TX_MQ
			fdir_filter_mode = FdirMode[bd];
#else
			fdir_filter_mode = IXGBE_FDIR_FILTER_OFF;
#endif /* HAVE_TX_MQ */
			ixgbe_validate_option(&fdir_filter_mode, &opt);

			switch (fdir_filter_mode) {
			case IXGBE_FDIR_FILTER_OFF:
				DPRINTK(PROBE, INFO, "Flow Director disabled\n");
				break;
			case IXGBE_FDIR_FILTER_HASH:
				*aflags |= IXGBE_FLAG_FDIR_HASH_CAPABLE;
				*aflags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
				feature[RING_F_FDIR].indices =
					IXGBE_MAX_FDIR_INDICES;
				DPRINTK(PROBE, INFO,
				        "Flow Director hash filtering enabled\n");
				break;
			case IXGBE_FDIR_FILTER_PERFECT:
				*aflags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
				*aflags |= IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
				feature[RING_F_FDIR].indices =
					IXGBE_MAX_FDIR_INDICES;
				spin_lock_init(&adapter->fdir_perfect_lock);
				DPRINTK(PROBE, INFO,
				        "Flow Director perfect filtering enabled\n");
				break;
			default:
				break;
			}
#ifdef module_param_array
		} else {
#ifdef HAVE_TX_MQ
			if (opt.def != IXGBE_FDIR_FILTER_OFF) {
				*aflags |= IXGBE_FLAG_FDIR_HASH_CAPABLE;
				feature[RING_F_FDIR].indices = IXGBE_MAX_FDIR_INDICES;
				DPRINTK(PROBE, INFO,
					"Flow Director hash filtering enabled\n");
			} else {
#endif /* HAVE_TX_MQ */
				*aflags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
				*aflags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
				feature[RING_F_FDIR].indices = 0;
				DPRINTK(PROBE, INFO,
					"Flow Director hash filtering disabled\n");
#ifdef HAVE_TX_MQ
			}
#endif /* HAVE_TX_MQ */
		}
		/* Check interoperability */
		if ((*aflags & IXGBE_FLAG_FDIR_HASH_CAPABLE) ||
		    (*aflags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)) {
			if (!(*aflags & IXGBE_FLAG_MQ_CAPABLE)) {
				DPRINTK(PROBE, INFO,
					"Flow Director is not supported "
					"while multiple queues are disabled. "
					"Disabling Flow Director\n");
				*aflags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
				*aflags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
			}
		}
#endif
no_flow_director:
		/* empty code line with semi-colon */ ;
	}
	{ /* Flow Director packet buffer allocation */
		unsigned int fdir_pballoc_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Flow Director packet buffer allocation",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_FDIR_PBALLOC),
			.def = IXGBE_DEFAULT_FDIR_PBALLOC,
			.arg = {.r = {.min = IXGBE_FDIR_PBALLOC_64K,
				      .max = IXGBE_FDIR_PBALLOC_256K}}
		};
		char pstring[10];

		if ((adapter->hw.mac.type == ixgbe_mac_82598EB) ||
		    (!(*aflags & (IXGBE_FLAG_FDIR_HASH_CAPABLE |
		                  IXGBE_FLAG_FDIR_PERFECT_CAPABLE))))
			goto no_fdir_pballoc;
#ifdef module_param_array
		if (num_FdirPballoc > bd) {
#endif
			fdir_pballoc_mode = FdirPballoc[bd];
			ixgbe_validate_option(&fdir_pballoc_mode, &opt);
			switch (fdir_pballoc_mode) {
			case IXGBE_FDIR_PBALLOC_64K:
				adapter->fdir_pballoc = IXGBE_FDIR_PBALLOC_64K;
				sprintf(pstring, "64kB");
				break;
			case IXGBE_FDIR_PBALLOC_128K:
				adapter->fdir_pballoc = IXGBE_FDIR_PBALLOC_128K;
				sprintf(pstring, "128kB");
				break;
			case IXGBE_FDIR_PBALLOC_256K:
				adapter->fdir_pballoc = IXGBE_FDIR_PBALLOC_256K;
				sprintf(pstring, "256kB");
				break;
			default:
				break;
			}
			DPRINTK(PROBE, INFO,
			        "Flow Director allocated %s of packet buffer\n",
			        pstring);

#ifdef module_param_array
		} else {
			adapter->fdir_pballoc = opt.def;
			DPRINTK(PROBE, INFO,
			     "Flow Director allocated 64kB of packet buffer\n");

		}
#endif
no_fdir_pballoc:
		/* empty code line with semi-colon */ ;
	}
	{ /* Flow Director ATR Tx sample packet rate */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Software ATR Tx packet sample rate",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_ATR_SAMPLE_RATE),
			.def = IXGBE_DEFAULT_ATR_SAMPLE_RATE,
			.arg = {.r = {.min = IXGBE_ATR_SAMPLE_RATE_OFF,
				      .max = IXGBE_MAX_ATR_SAMPLE_RATE}}
		};
		static const char atr_string[] =
		                            "ATR Tx Packet sample rate set to";

		adapter->atr_sample_rate = IXGBE_ATR_SAMPLE_RATE_OFF;
		if (adapter->hw.mac.type == ixgbe_mac_82598EB)
			goto no_fdir_sample;

		/* no sample rate for perfect filtering */
		if (*aflags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
			goto no_fdir_sample;
#ifdef module_param_array
		if (num_AtrSampleRate > bd) {
#endif
			/* Only enable the sample rate if hashing (ATR) is on */
			if (*aflags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
				adapter->atr_sample_rate = AtrSampleRate[bd];

			if (adapter->atr_sample_rate) {
				ixgbe_validate_option(&adapter->atr_sample_rate,
				                      &opt);
				DPRINTK(PROBE, INFO, "%s %d\n", atr_string,
				        adapter->atr_sample_rate);
			}
#ifdef module_param_array
		} else {
			/* Only enable the sample rate if hashing (ATR) is on */
			if (*aflags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
				adapter->atr_sample_rate = opt.def;

			DPRINTK(PROBE, INFO, "%s default of %d\n", atr_string,
			        adapter->atr_sample_rate);
		}
#endif
no_fdir_sample:
		/* empty code line with semi-colon */ ;
	}
}
