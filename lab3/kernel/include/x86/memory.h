#ifndef __X86_MEMORY_H__
#define __X86_MEMORY_H__

//#define PAGE_ENABLED

//#define DEBUG

#define DPL_KERN                0
#define DPL_USER                3

// Application segment type bits
#define STA_X       0x8         // Executable segment
#define STA_W       0x2         // Writeable (non-executable segments)
#define STA_R       0x2         // Readable (executable segments)

// System segment type bits
#define STS_T32A    0x9         // Available 32-bit TSS
#define STS_IG32    0xE         // 32-bit Interrupt Gate
#define STS_TG32    0xF         // 32-bit Trap Gate

// GDT entries
#ifdef PAGE_ENABLED
	#define NR_SEGMENTS 6
	#define SEG_UCODE 3
	#define SEG_UDATA 4
	#define PAGE_SIZE 0x1000
	#define MAX_PROC_SIZE 0x100000
	#define STACK_SIZE 0x60000
	#define NR_PAGES_PER_PROC 0x100

	#define PAGE_DESC_BUILD(desc, p,rw,us,base) (desc| p | (rw<<1) | (us<<2) | (base&0xfffff000))
#else
	#define NR_SEGMENTS      10           // GDT size
#endif
#define SEG_KCODE   1           // Kernel code
#define SEG_KDATA   2           // Kernel data/stack
#define SEG_TSS     (NR_SEGMENTS-1)

// Selectors
#define KSEL(desc) (((desc) << 3) | DPL_KERN)
#define USEL(desc) (((desc) << 3) | DPL_USER)

//segmet size
#define SEG_SIZE 0x100000

struct GateDescriptor {
	uint32_t offset_15_0      : 16;
	uint32_t segment          : 16;
	uint32_t pad0             : 8;
	uint32_t type             : 4;
	uint32_t system           : 1;
	uint32_t privilege_level  : 2;
	uint32_t present          : 1;
	uint32_t offset_31_16     : 16;
};

struct StackFrame {
	uint32_t gs, fs, es, ds;
	uint32_t edi, esi, ebp, xxx, ebx, edx, ecx, eax;
	uint32_t irq, error;
	uint32_t eip, cs, eflags, esp, ss;
};


#ifdef PAGE_ENABLED 
union PageDescriptor{
	struct{
		uint32_t val;
	};
	struct{
		uint32_t p			 : 1;
		uint32_t rw 			: 1;
		uint32_t us			 : 1;
		uint32_t pwt			 : 1;
		uint32_t pcd 			: 1;
		uint32_t access 			: 1;
		uint32_t dirty 			: 1;
		// real_rw and dontcare is actually zero:0 in real linux descriptor
		uint32_t real_rw		:1;
		uint32_t dontcare		:1;
		uint32_t avl 			: 3;
		uint32_t base 			: 20;
	};
};
typedef union PageDescriptor PageDescriptor;
struct PageFrame{
	PageDescriptor content[1024];
};
typedef struct PageFrame PageFrame __attribute__ ((aligned (0x1000))); // set align type for page tables
#endif


#define MAX_STACK_SIZE 2048
#ifndef PAGE_ENABLED
	#define MAX_PCB_NUM ((NR_SEGMENTS-2)/2)
#else
	#define MAX_PCB_NUM 16
#endif


#define STATE_RUNNABLE 0
#define STATE_RUNNING 1
#define STATE_BLOCKED 2
#define STATE_DEAD 3
#ifdef PAGE_ENABLED
#define STATE_ZOMBIE 4 
#endif

#define MAX_TIME_COUNT 16

struct ProcessTable {
	uint32_t stack[MAX_STACK_SIZE]; //kernel stack
	struct StackFrame regs;
	uint32_t stackTop;
	int state;
	int timeCount;
	int sleepTime;
	uint32_t pid;
	char name[32];

#ifdef PAGE_ENABLED
	PageDescriptor pageTb[NR_PAGES_PER_PROC];
	uint32_t procSize;
	int busyPageFrameFirst;
	uint32_t active_mm;//other:copy_on_write; pid:reserve own memory
	uint32_t copyNum;
#endif

	// For kernel thread 
	//int count;
	//int parent;
};
typedef struct ProcessTable ProcessTable;

/*
1. The number of bits in a bit field sets the limit to the range of values it can hold
2. Multiple adjacent bit fields are usually packed together (although this behavior is implementation-defined)

Refer: en.cppreference.com/w/cpp/language/bit_field
*/
struct SegDesc {
	uint32_t lim_15_0 : 16;  // Low bits of segment limit
	uint32_t base_15_0 : 16; // Low bits of segment base address
	uint32_t base_23_16 : 8; // Middle bits of segment base address
	uint32_t type : 4;       // Segment type (see STS_ constants)
	uint32_t s : 1;          // 0 = system, 1 = application
	uint32_t dpl : 2;        // Descriptor Privilege Level
	uint32_t p : 1;          // Present
	uint32_t lim_19_16 : 4;  // High bits of segment limit
	uint32_t avl : 1;        // Unused (available for software use)
	uint32_t rsv1 : 1;       // Reserved
	uint32_t db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
	uint32_t g : 1;          // Granularity: limit scaled by 4K when set
	uint32_t base_31_24 : 8; // High bits of segment base address
};
typedef struct SegDesc SegDesc;

#define SEG(type, base, lim, dpl) (SegDesc)                   \
{	((lim) >> 12) & 0xffff, (uint32_t)(base) & 0xffff,        \
	((uint32_t)(base) >> 16) & 0xff, type, 1, dpl, 1,         \
	(uint32_t)(lim) >> 28, 0, 0, 1, 1, (uint32_t)(base) >> 24 }

#define SEG16(type, base, lim, dpl) (SegDesc)                 \
{	(lim) & 0xffff, (uint32_t)(base) & 0xffff,                \
	((uint32_t)(base) >> 16) & 0xff, type, 0, dpl, 1,         \
	(uint32_t)(lim) >> 16, 0, 0, 1, 0, (uint32_t)(base) >> 24 }
	
// Task state segment format
struct TSS {
	uint32_t link;         // old ts selector
	uint32_t esp0;         // Ring 0 Stack pointer and segment selector
	uint32_t ss0;          // after an increase in privilege level
	union{
		struct{
			char dontcare[88];
		};
		struct{
			uint32_t esp1,ss1,esp2,ss2;
			uint32_t cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
			uint32_t es, cs, ss, ds, fs, gs, ldt;
		};
    };
};
typedef struct TSS TSS;

static inline void setGdt(SegDesc *gdt, uint32_t size) {
	volatile static uint16_t data[3];
	data[0] = size - 1;
	data[1] = (uint32_t)gdt;
	data[2] = (uint32_t)gdt >> 16;
	asm volatile("lgdt (%0)" : : "r"(data));
}

static inline void lLdt(uint16_t sel)
{
	asm volatile("lldt %0" :: "r"(sel));
}

#endif
