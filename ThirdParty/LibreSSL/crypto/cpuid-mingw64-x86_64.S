#if defined(__CET__)
#include <cet.h>
#else
#define _CET_ENDBR
#endif

#include "x86_arch.h"
.text	
.globl	OPENSSL_ia32_cpuid
.def	OPENSSL_ia32_cpuid;	.scl 2;	.type 32;	.endef
.p2align	4
OPENSSL_ia32_cpuid:
_CET_ENDBR
	movq	%rbx,%r8

	xorl	%eax,%eax
	cpuid
	movl	%eax,%r11d

	xorl	%eax,%eax
	cmpl	$1970169159,%ebx
	setne	%al
	movl	%eax,%r9d
	cmpl	$1231384169,%edx
	setne	%al
	orl	%eax,%r9d
	cmpl	$1818588270,%ecx
	setne	%al
	orl	%eax,%r9d
	jz	.Lintel

	cmpl	$1752462657,%ebx
	setne	%al
	movl	%eax,%r10d
	cmpl	$1769238117,%edx
	setne	%al
	orl	%eax,%r10d
	cmpl	$1145913699,%ecx
	setne	%al
	orl	%eax,%r10d
	jnz	.Lintel


	movl	$2147483648,%eax
	cpuid
	cmpl	$2147483649,%eax
	jb	.Lintel
	movl	%eax,%r10d
	movl	$2147483649,%eax
	cpuid
	orl	%ecx,%r9d
	andl	$IA32CAP_MASK1_AMD_XOP,%r9d
	orl	$1,%r9d

	cmpl	$2147483656,%r10d
	jb	.Lintel

	movl	$2147483656,%eax
	cpuid
	movzbq	%cl,%r10
	incq	%r10

	movl	$1,%eax
	cpuid
	btl	$IA32CAP_BIT0_HT,%edx
	jnc	.Lgeneric
	shrl	$16,%ebx
	cmpb	%r10b,%bl
	ja	.Lgeneric
	xorl	$IA32CAP_MASK0_HT,%edx
	jmp	.Lgeneric

.Lintel:
	cmpl	$4,%r11d
	movl	$-1,%r10d
	jb	.Lnocacheinfo

	movl	$4,%eax
	movl	$0,%ecx
	cpuid
	movl	%eax,%r10d
	shrl	$14,%r10d
	andl	$4095,%r10d

.Lnocacheinfo:
	movl	$1,%eax
	cpuid

	andl	$(~(IA32CAP_MASK0_INTELP4 | IA32CAP_MASK0_INTEL)),%edx
	cmpl	$0,%r9d
	jne	.Lnotintel

	orl	$IA32CAP_MASK0_INTEL,%edx
	andb	$15,%ah
	cmpb	$15,%ah
	jne	.Lnotintel

	orl	$IA32CAP_MASK0_INTELP4,%edx
.Lnotintel:
	btl	$IA32CAP_BIT0_HT,%edx
	jnc	.Lgeneric
	xorl	$IA32CAP_MASK0_HT,%edx
	cmpl	$0,%r10d
	je	.Lgeneric

	orl	$IA32CAP_MASK0_HT,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	.Lgeneric
	xorl	$IA32CAP_MASK0_HT,%edx

.Lgeneric:
	andl	$IA32CAP_MASK1_AMD_XOP,%r9d
	andl	$(~IA32CAP_MASK1_AMD_XOP),%ecx
	orl	%ecx,%r9d

	movl	%edx,%r10d
	btl	$IA32CAP_BIT1_OSXSAVE,%r9d
	jnc	.Lclear_avx
	xorl	%ecx,%ecx
.byte	0x0f,0x01,0xd0		
	andl	$6,%eax
	cmpl	$6,%eax
	je	.Ldone
.Lclear_avx:
	movl	$(~(IA32CAP_MASK1_AVX | IA32CAP_MASK1_FMA3 | IA32CAP_MASK1_AMD_XOP)),%eax
	andl	%eax,%r9d
.Ldone:
	shlq	$32,%r9
	movl	%r10d,%eax
	movq	%r8,%rbx
	orq	%r9,%rax
	retq

