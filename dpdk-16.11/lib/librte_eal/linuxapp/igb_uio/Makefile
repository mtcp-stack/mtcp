#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

#
# module name and path
#
MODULE = igb_uio
MODULE_PATH = drivers/net/igb_uio

include $(RTE_SDK)/lib/librte_eal/linuxapp/igb_uio/i40e/common.mk

#
# CFLAGS
#
MODULE_CFLAGS += -I$(SRCDIR) -I$(SRCDIR)/i40e
MODULE_CFLAGS += -I$(RTE_OUTPUT)/include
MODULE_CFLAGS += -Wall -Werror
MODULE_CFLAGS += -include $(RTE_OUTPUT)/include/rte_config.h

ifeq ($(shell lsb_release -si 2>/dev/null),Ubuntu)
MODULE_CFLAGS += -DUBUNTU_RELEASE_CODE=$(shell lsb_release -sr | tr -d .)
UBUNTU_KERNEL_CODE := $(shell echo `grep UTS_RELEASE $(RTE_KERNELDIR)/include/generated/utsrelease.h \
	 | cut -d '"' -f2 | cut -d- -f1,2 | tr .- ,`,1)
MODULE_CFLAGS += -D"UBUNTU_KERNEL_CODE=UBUNTU_KERNEL_VERSION($(UBUNTU_KERNEL_CODE))"
endif

#
# all source are stored in SRCS-y
#
SRCS-y := igbuio.c
SRCS-y += i40e/i40e_main.c
SRCS-y += i40e/i40e_adminq.c
SRCS-y += i40e/i40e_dcb.c
SRCS-y += i40e/i40e_diag.c
SRCS-y += i40e/i40e_hmc.c
SRCS-y += i40e/i40e_nvm.c
SRCS-y += i40e/i40e_virtchnl_pf.c
SRCS-y += i40e/i40e_client.c
SRCS-y += i40e/i40e_dcb_nl.c
SRCS-y += i40e/i40e_ethtool.c
SRCS-y += i40e/i40e_lan_hmc.c
SRCS-y += i40e/i40e_ptp.c
SRCS-y += i40e/kcompat.c
SRCS-y += i40e/i40e_common.c
SRCS-y += i40e/i40e_debugfs.c
SRCS-y += i40e/i40e_fcoe.c
SRCS-y += i40e/i40e_txrx.c

include $(RTE_SDK)/mk/rte.module.mk
