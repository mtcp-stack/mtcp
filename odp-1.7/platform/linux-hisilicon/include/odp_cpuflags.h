/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Huawei Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ODP_CPUFLAGS_X86_64_H_
#define _ODP_CPUFLAGS_X86_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include "generic/odp_cpuflags.h"

enum odp_cpu_flag_t {
	/* (EAX 01h) ECX features*/
	ODP_CPUFLAG_SSE3 = 0,        /**< SSE3 */
	ODP_CPUFLAG_PCLMULQDQ,       /**< PCLMULQDQ */
	ODP_CPUFLAG_DTES64,          /**< DTES64 */
	ODP_CPUFLAG_MONITOR,         /**< MONITOR */
	ODP_CPUFLAG_DS_CPL,          /**< DS_CPL */
	ODP_CPUFLAG_VMX,             /**< VMX */
	ODP_CPUFLAG_SMX,             /**< SMX */
	ODP_CPUFLAG_EIST,            /**< EIST */
	ODP_CPUFLAG_TM2,             /**< TM2 */
	ODP_CPUFLAG_SSSE3,           /**< SSSE3 */
	ODP_CPUFLAG_CNXT_ID,         /**< CNXT_ID */
	ODP_CPUFLAG_FMA,             /**< FMA */
	ODP_CPUFLAG_CMPXCHG16B,      /**< CMPXCHG16B */
	ODP_CPUFLAG_XTPR,            /**< XTPR */
	ODP_CPUFLAG_PDCM,            /**< PDCM */
	ODP_CPUFLAG_PCID,            /**< PCID */
	ODP_CPUFLAG_DCA,             /**< DCA */
	ODP_CPUFLAG_SSE4_1,          /**< SSE4_1 */
	ODP_CPUFLAG_SSE4_2,          /**< SSE4_2 */
	ODP_CPUFLAG_X2APIC,          /**< X2APIC */
	ODP_CPUFLAG_MOVBE,           /**< MOVBE */
	ODP_CPUFLAG_POPCNT,          /**< POPCNT */
	ODP_CPUFLAG_TSC_DEADLINE,    /**< TSC_DEADLINE */
	ODP_CPUFLAG_AES,             /**< AES */
	ODP_CPUFLAG_XSAVE,           /**< XSAVE */
	ODP_CPUFLAG_OSXSAVE,         /**< OSXSAVE */
	ODP_CPUFLAG_AVX,             /**< AVX */
	ODP_CPUFLAG_F16C,            /**< F16C */
	ODP_CPUFLAG_RDRAND,          /**< RDRAND */

	/* (EAX 01h) EDX features */
	ODP_CPUFLAG_FPU,             /**< FPU */
	ODP_CPUFLAG_VME,             /**< VME */
	ODP_CPUFLAG_DE,              /**< DE */
	ODP_CPUFLAG_PSE,             /**< PSE */
	ODP_CPUFLAG_TSC,             /**< TSC */
	ODP_CPUFLAG_MSR,             /**< MSR */
	ODP_CPUFLAG_PAE,             /**< PAE */
	ODP_CPUFLAG_MCE,             /**< MCE */
	ODP_CPUFLAG_CX8,             /**< CX8 */
	ODP_CPUFLAG_APIC,            /**< APIC */
	ODP_CPUFLAG_SEP,             /**< SEP */
	ODP_CPUFLAG_MTRR,            /**< MTRR */
	ODP_CPUFLAG_PGE,             /**< PGE */
	ODP_CPUFLAG_MCA,             /**< MCA */
	ODP_CPUFLAG_CMOV,            /**< CMOV */
	ODP_CPUFLAG_PAT,             /**< PAT */
	ODP_CPUFLAG_PSE36,           /**< PSE36 */
	ODP_CPUFLAG_PSN,             /**< PSN */
	ODP_CPUFLAG_CLFSH,           /**< CLFSH */
	ODP_CPUFLAG_DS,              /**< DS */
	ODP_CPUFLAG_ACPI,            /**< ACPI */
	ODP_CPUFLAG_MMX,             /**< MMX */
	ODP_CPUFLAG_FXSR,            /**< FXSR */
	ODP_CPUFLAG_SSE,             /**< SSE */
	ODP_CPUFLAG_SSE2,            /**< SSE2 */
	ODP_CPUFLAG_SS,              /**< SS */
	ODP_CPUFLAG_HTT,             /**< HTT */
	ODP_CPUFLAG_TM,              /**< TM */
	ODP_CPUFLAG_PBE,             /**< PBE */

	/* (EAX 06h) EAX features */
	ODP_CPUFLAG_DIGTEMP,         /**< DIGTEMP */
	ODP_CPUFLAG_TRBOBST,         /**< TRBOBST */
	ODP_CPUFLAG_ARAT,            /**< ARAT */
	ODP_CPUFLAG_PLN,             /**< PLN */
	ODP_CPUFLAG_ECMD,            /**< ECMD */
	ODP_CPUFLAG_PTM,             /**< PTM */

	/* (EAX 06h) ECX features */
	ODP_CPUFLAG_MPERF_APERF_MSR, /**< MPERF_APERF_MSR */
	ODP_CPUFLAG_ACNT2,           /**< ACNT2 */
	ODP_CPUFLAG_ENERGY_EFF,      /**< ENERGY_EFF */

	/* (EAX 07h, ECX 0h) EBX features */
	ODP_CPUFLAG_FSGSBASE,        /**< FSGSBASE */
	ODP_CPUFLAG_BMI1,            /**< BMI1 */
	ODP_CPUFLAG_HLE,             /**< Hardware Lock elision */
	ODP_CPUFLAG_AVX2,            /**< AVX2 */
	ODP_CPUFLAG_SMEP,            /**< SMEP */
	ODP_CPUFLAG_BMI2,            /**< BMI2 */
	ODP_CPUFLAG_ERMS,            /**< ERMS */
	ODP_CPUFLAG_INVPCID,         /**< INVPCID */
	ODP_CPUFLAG_RTM,             /**< Transactional memory */

	/* (EAX 80000001h) ECX features */
	ODP_CPUFLAG_LAHF_SAHF,       /**< LAHF_SAHF */
	ODP_CPUFLAG_LZCNT,           /**< LZCNT */

	/* (EAX 80000001h) EDX features */
	ODP_CPUFLAG_SYSCALL,         /**< SYSCALL */
	ODP_CPUFLAG_XD,              /**< XD */
	ODP_CPUFLAG_1GB_PG,          /**< 1GB_PG */
	ODP_CPUFLAG_RDTSCP,          /**< RDTSCP */
	ODP_CPUFLAG_EM64T,           /**< EM64T */

	/* (EAX 80000007h) EDX features */
	ODP_CPUFLAG_INVTSC,          /**< INVTSC */

	/* The last item */
	ODP_CPUFLAG_NUMFLAGS,        /**< This should always be the last! */
};

enum cpu_register_t {
	ODP_REG_EAX = 0,
	ODP_REG_EBX,
	ODP_REG_ECX,
	ODP_REG_EDX,
};

static const struct feature_entry cpu_feature_table[] = {
	FEAT_DEF(SSE3,		  0x00000001, 0, ODP_REG_ECX, 0)
	FEAT_DEF(PCLMULQDQ,	  0x00000001, 0, ODP_REG_ECX, 1)
	FEAT_DEF(DTES64,	  0x00000001, 0, ODP_REG_ECX, 2)
	FEAT_DEF(MONITOR,	  0x00000001, 0, ODP_REG_ECX, 3)
	FEAT_DEF(DS_CPL,	  0x00000001, 0, ODP_REG_ECX, 4)
	FEAT_DEF(VMX,		  0x00000001, 0, ODP_REG_ECX, 5)
	FEAT_DEF(SMX,		  0x00000001, 0, ODP_REG_ECX, 6)
	FEAT_DEF(EIST,		  0x00000001, 0, ODP_REG_ECX, 7)
	FEAT_DEF(TM2,		  0x00000001, 0, ODP_REG_ECX, 8)
	FEAT_DEF(SSSE3,		  0x00000001, 0, ODP_REG_ECX, 9)
	FEAT_DEF(CNXT_ID,	  0x00000001, 0, ODP_REG_ECX, 10)
	FEAT_DEF(FMA,		  0x00000001, 0, ODP_REG_ECX, 12)
	FEAT_DEF(CMPXCHG16B,	  0x00000001, 0, ODP_REG_ECX, 13)
	FEAT_DEF(XTPR,		  0x00000001, 0, ODP_REG_ECX, 14)
	FEAT_DEF(PDCM,		  0x00000001, 0, ODP_REG_ECX, 15)
	FEAT_DEF(PCID,		  0x00000001, 0, ODP_REG_ECX, 17)
	FEAT_DEF(DCA,		  0x00000001, 0, ODP_REG_ECX, 18)
	FEAT_DEF(SSE4_1,	  0x00000001, 0, ODP_REG_ECX, 19)
	FEAT_DEF(SSE4_2,	  0x00000001, 0, ODP_REG_ECX, 20)
	FEAT_DEF(X2APIC,	  0x00000001, 0, ODP_REG_ECX, 21)
	FEAT_DEF(MOVBE,		  0x00000001, 0, ODP_REG_ECX, 22)
	FEAT_DEF(POPCNT,	  0x00000001, 0, ODP_REG_ECX, 23)
	FEAT_DEF(TSC_DEADLINE,	  0x00000001, 0, ODP_REG_ECX, 24)
	FEAT_DEF(AES,		  0x00000001, 0, ODP_REG_ECX, 25)
	FEAT_DEF(XSAVE,		  0x00000001, 0, ODP_REG_ECX, 26)
	FEAT_DEF(OSXSAVE,	  0x00000001, 0, ODP_REG_ECX, 27)
	FEAT_DEF(AVX,		  0x00000001, 0, ODP_REG_ECX, 28)
	FEAT_DEF(F16C,		  0x00000001, 0, ODP_REG_ECX, 29)
	FEAT_DEF(RDRAND,	  0x00000001, 0, ODP_REG_ECX, 30)

	FEAT_DEF(FPU,		  0x00000001, 0, ODP_REG_EDX, 0)
	FEAT_DEF(VME,		  0x00000001, 0, ODP_REG_EDX, 1)
	FEAT_DEF(DE,		  0x00000001, 0, ODP_REG_EDX, 2)
	FEAT_DEF(PSE,		  0x00000001, 0, ODP_REG_EDX, 3)
	FEAT_DEF(TSC,		  0x00000001, 0, ODP_REG_EDX, 4)
	FEAT_DEF(MSR,		  0x00000001, 0, ODP_REG_EDX, 5)
	FEAT_DEF(PAE,		  0x00000001, 0, ODP_REG_EDX, 6)
	FEAT_DEF(MCE,		  0x00000001, 0, ODP_REG_EDX, 7)
	FEAT_DEF(CX8,		  0x00000001, 0, ODP_REG_EDX, 8)
	FEAT_DEF(APIC,		  0x00000001, 0, ODP_REG_EDX, 9)
	FEAT_DEF(SEP,		  0x00000001, 0, ODP_REG_EDX, 11)
	FEAT_DEF(MTRR,		  0x00000001, 0, ODP_REG_EDX, 12)
	FEAT_DEF(PGE,		  0x00000001, 0, ODP_REG_EDX, 13)
	FEAT_DEF(MCA,		  0x00000001, 0, ODP_REG_EDX, 14)
	FEAT_DEF(CMOV,		  0x00000001, 0, ODP_REG_EDX, 15)
	FEAT_DEF(PAT,		  0x00000001, 0, ODP_REG_EDX, 16)
	FEAT_DEF(PSE36,		  0x00000001, 0, ODP_REG_EDX, 17)
	FEAT_DEF(PSN,		  0x00000001, 0, ODP_REG_EDX, 18)
	FEAT_DEF(CLFSH,		  0x00000001, 0, ODP_REG_EDX, 19)
	FEAT_DEF(DS,		  0x00000001, 0, ODP_REG_EDX, 21)
	FEAT_DEF(ACPI,		  0x00000001, 0, ODP_REG_EDX, 22)
	FEAT_DEF(MMX,		  0x00000001, 0, ODP_REG_EDX, 23)
	FEAT_DEF(FXSR,		  0x00000001, 0, ODP_REG_EDX, 24)
	FEAT_DEF(SSE,		  0x00000001, 0, ODP_REG_EDX, 25)
	FEAT_DEF(SSE2,		  0x00000001, 0, ODP_REG_EDX, 26)
	FEAT_DEF(SS,		  0x00000001, 0, ODP_REG_EDX, 27)
	FEAT_DEF(HTT,		  0x00000001, 0, ODP_REG_EDX, 28)
	FEAT_DEF(TM,		  0x00000001, 0, ODP_REG_EDX, 29)
	FEAT_DEF(PBE,		  0x00000001, 0, ODP_REG_EDX, 31)

	FEAT_DEF(DIGTEMP,	  0x00000006, 0, ODP_REG_EAX, 0)
	FEAT_DEF(TRBOBST,	  0x00000006, 0, ODP_REG_EAX, 1)
	FEAT_DEF(ARAT,		  0x00000006, 0, ODP_REG_EAX, 2)
	FEAT_DEF(PLN,		  0x00000006, 0, ODP_REG_EAX, 4)
	FEAT_DEF(ECMD,		  0x00000006, 0, ODP_REG_EAX, 5)
	FEAT_DEF(PTM,		  0x00000006, 0, ODP_REG_EAX, 6)

	FEAT_DEF(MPERF_APERF_MSR, 0x00000006, 0, ODP_REG_ECX, 0)
	FEAT_DEF(ACNT2,		  0x00000006, 0, ODP_REG_ECX, 1)
	FEAT_DEF(ENERGY_EFF,	  0x00000006, 0, ODP_REG_ECX, 3)

	FEAT_DEF(FSGSBASE,	  0x00000007, 0, ODP_REG_EBX, 0)
	FEAT_DEF(BMI1,		  0x00000007, 0, ODP_REG_EBX, 2)
	FEAT_DEF(HLE,		  0x00000007, 0, ODP_REG_EBX, 4)
	FEAT_DEF(AVX2,		  0x00000007, 0, ODP_REG_EBX, 5)
	FEAT_DEF(SMEP,		  0x00000007, 0, ODP_REG_EBX, 6)
	FEAT_DEF(BMI2,		  0x00000007, 0, ODP_REG_EBX, 7)
	FEAT_DEF(ERMS,		  0x00000007, 0, ODP_REG_EBX, 8)
	FEAT_DEF(INVPCID,	  0x00000007, 0, ODP_REG_EBX, 10)
	FEAT_DEF(RTM,		  0x00000007, 0, ODP_REG_EBX, 11)

	FEAT_DEF(LAHF_SAHF,	  0x80000001, 0, ODP_REG_ECX, 0)
	FEAT_DEF(LZCNT,		  0x80000001, 0, ODP_REG_ECX, 4)

	FEAT_DEF(SYSCALL,	  0x80000001, 0, ODP_REG_EDX, 11)
	FEAT_DEF(XD,		  0x80000001, 0, ODP_REG_EDX, 20)
	FEAT_DEF(1GB_PG,	  0x80000001, 0, ODP_REG_EDX, 26)
	FEAT_DEF(RDTSCP,	  0x80000001, 0, ODP_REG_EDX, 27)
	FEAT_DEF(EM64T,		  0x80000001, 0, ODP_REG_EDX, 29)

	FEAT_DEF(INVTSC,	  0x80000007, 0, ODP_REG_EDX, 8)
};

static inline void odp_cpu_get_features(uint32_t leaf, uint32_t subleaf,
					cpuid_registers_t out)
{
	asm volatile ("cpuid"
		      : "=a" (out[ODP_REG_EAX]),
		      "=b" (out[ODP_REG_EBX]),
		      "=c" (out[ODP_REG_ECX]),
		      "=d" (out[ODP_REG_EDX])
		      : "a" (leaf), "c" (subleaf));
}

static inline int odp_cpu_get_flag_enabled(enum odp_cpu_flag_t feature)
{
	const struct feature_entry *feat;
	cpuid_registers_t regs;

	if (feature >= ODP_CPUFLAG_NUMFLAGS)
		/* Flag does not match anything in the feature tables */
		return -ENOENT;

	feat = &cpu_feature_table[feature];

	if (!feat->leaf)
		/* This entry in the table wasn't filled out! */
		return -EFAULT;

	odp_cpu_get_features(feat->leaf & 0xffff0000, 0, regs);
	if (((regs[ODP_REG_EAX] ^ feat->leaf) & 0xffff0000) ||
	    (regs[ODP_REG_EAX] < feat->leaf))
		return 0;

	/* get the cpuid leaf containing the desired feature */
	odp_cpu_get_features(feat->leaf, feat->subleaf, regs);

	/* check if the feature is enabled */
	return (regs[feat->reg] >> feat->bit) & 1;
}

#ifdef __cplusplus
}
#endif
#endif /* _ODP_CPUFLAGS_X86_64_H_ */
