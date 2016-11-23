// this file was taken from libogc, see http://www.devkitpro.org/ 

#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include <gctypes.h>

#define __stringify(rn)					#rn
#define ATTRIBUTE_ALIGN(v)				__attribute__((aligned(v)))

#define _sync() asm volatile("sync")
#define _nop() asm volatile("nop")
#define ppcsync() asm volatile("sc")
#define ppchalt() ({					\
	asm volatile("sync");				\
	while(1) {							\
		asm volatile("nop");			\
		asm volatile("li 3,0");			\
		asm volatile("nop");			\
	}									\
})

#define mfdcr(_rn) ({register u32 _rval; \
		asm volatile("mfdcr %0," __stringify(_rn) \
             : "=r" (_rval)); _rval;})
#define mtdcr(rn, val)  asm volatile("mtdcr " __stringify(rn) ",%0" : : "r" (val))

#define mfmsr()   ({register u32 _rval; \
		asm volatile("mfmsr %0" : "=r" (_rval)); _rval;})
#define mtmsr(val)  asm volatile("mtmsr %0" : : "r" (val))

#define mfdec()   ({register u32 _rval; \
		asm volatile("mfdec %0" : "=r" (_rval)); _rval;})
#define mtdec(_val)  asm volatile("mtdec %0" : : "r" (_val))

#define mfspr(_rn) \
({	register u32 _rval = 0; \
	asm volatile("mfspr %0," __stringify(_rn) \
	: "=r" (_rval));\
	_rval; \
})

#define mtspr(_rn, _val) asm volatile("mtspr " __stringify(_rn) ",%0" : : "r" (_val))

#define mfwpar()		mfspr(WPAR)
#define mtwpar(_val)	mtspr(WPAR,_val)

#define mfmmcr0()		mfspr(MMCR0)
#define mtmmcr0(_val)	mtspr(MMCR0,_val)
#define mfmmcr1()		mfspr(MMCR1)
#define mtmmcr1(_val)	mtspr(MMCR1,_val)

#define mfpmc1()		mfspr(PMC1)
#define mtpmc1(_val)	mtspr(PMC1,_val)
#define mfpmc2()		mfspr(PMC2)
#define mtpmc2(_val)	mtspr(PMC2,_val)
#define mfpmc3()		mfspr(PMC3)
#define mtpmc3(_val)	mtspr(PMC3,_val)
#define mfpmc4()		mfspr(PMC4)
#define mtpmc4(_val)	mtspr(PMC4,_val)

#define mfhid0()		mfspr(HID0)
#define mthid0(_val)	mtspr(HID0,_val)
#define mfhid1()		mfspr(HID1)
#define mthid1(_val)	mtspr(HID1,_val)
#define mfhid2()		mfspr(HID2)
#define mthid2(_val)	mtspr(HID2,_val)
#define mfhid4()		mfspr(HID4)
#define mthid4(_val)	mtspr(HID4,_val)

#define cntlzw(_val) ({register u32 _rval; \
					  asm volatile("cntlzw %0, %1" : "=r"((_rval)) : "r"((_val))); _rval;})

#define _CPU_MSR_GET( _msr_value ) \
  do { \
    _msr_value = 0; \
    asm volatile ("mfmsr %0" : "=&r" ((_msr_value)) : "0" ((_msr_value))); \
  } while (0)

#define _CPU_MSR_SET( _msr_value ) \
{ asm volatile ("mtmsr %0" : "=&r" ((_msr_value)) : "0" ((_msr_value))); }

#define _CPU_ISR_Enable() \
	{ register u32 _val = 0; \
	  asm volatile ("mfmsr %0; ori %0,%0,0x8000; mtmsr %0" : \
					"=&r" (_val) : "0" (_val));\
	}

#define _CPU_ISR_Disable( _isr_cookie ) \
  { register u32 _disable_mask = MSR_EE; \
    _isr_cookie = 0; \
    asm volatile ( \
	"mfmsr %0; andc %1,%0,%1; mtmsr %1" : \
	"=&r" ((_isr_cookie)), "=&r" ((_disable_mask)) : \
	"0" ((_isr_cookie)), "1" ((_disable_mask)) \
	); \
  }

#define _CPU_ISR_Restore( _isr_cookie )  \
  { \
     asm volatile ( "mtmsr %0" : \
		   "=r" ((_isr_cookie)) : \
                   "0" ((_isr_cookie))); \
  }

#define _CPU_ISR_Flash( _isr_cookie ) \
  { register u32 _disable_mask = MSR_EE; \
    asm volatile ( \
      "mtmsr %0; andc %1,%0,%1; mtmsr %1" : \
      "=r" ((_isr_cookie)), "=r" ((_disable_mask)) : \
      "0" ((_isr_cookie)), "1" ((_disable_mask)) \
    ); \
  }

#endif
