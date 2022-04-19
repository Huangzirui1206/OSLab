#include "x86.h"
#include "device.h"

SegDesc gdt[NR_SEGMENTS];       // the new GDT, NR_SEGMENTS=10, defined in x86/memory.h
// Once open macro  TABLE_ENABLE, the NR_SEGMENTS is 6
TSS tss;
#ifdef PAGE_ENABLED
#define PAGE_START 0x100000
#define pf_to_pa(pf) (PAGE_START+pf*PAGE_SIZE)
#define nr_pageFrame  (MAX_PCB_NUM*(MAX_PROC_SIZE/PAGE_SIZE))

	int pageFrameNxt[nr_pageFrame];
	int freePageFrameFirst;
	uint32_t freePageFrameCnt;

	PageFrame pageDir; // only page[0],page[1],page[2] is used
	PageFrame pageFrame;

#define ph_w(flag) ((flag&2)>>1)
#endif


void eax_get_eip(){
#ifdef DEBUG
	putStr("The eip for break should be ");
	asm volatile("movl %ebp,%ebx");
	asm volatile("addl $4,%ebx");
	asm volatile("movl (%ebx),%eax");
	uint32_t eip;
	asm volatile("movl %%eax,%0":"=m"(eip));
	putNumX(eip);
	putChar('\n');
#endif
}


ProcessTable pcb[MAX_PCB_NUM];
int runnableFirst;
int runnableTail;
int runnableNxt[MAX_PCB_NUM]; //for STATE_RUNNABLE to round-robin
int current; // current process
int freeFirst;
int freeNxt[MAX_PCB_NUM]; //for allocating free pcb to a new precess

#ifdef PAGE_ENABLED
	static void freePageEnqueue(int pg){
		assert(pg>=0&&pg<nr_pageFrame);
		pageFrameNxt[pg] = freePageFrameFirst;
		freePageFrameFirst = pg;
	}

	static int freePageDequeue(){
		int oriFirst = freePageFrameFirst;
		if(freePageFrameFirst != -1)freePageFrameFirst = pageFrameNxt[freePageFrameFirst];
		return oriFirst;
	}

	void busyPageEnqueue(int* busyPageFrameFirst, int pg){
		assert(pg>=0&&pg<nr_pageFrame);
		pageFrameNxt[pg] = *busyPageFrameFirst;
		*busyPageFrameFirst = pg;
	}

	void releaseBusyPage(int busyPageFrameFirst){
		for(;busyPageFrameFirst!=-1;busyPageFrameFirst = pageFrameNxt[busyPageFrameFirst]){
			freePageEnqueue(busyPageFrameFirst);
		}
	}
#endif

#ifdef PAGE_ENABLED
void allocatePageFrame(uint32_t pid, uint32_t vaddr, uint32_t size, uint32_t rw){
	int cnt = size / PAGE_SIZE;
	if(size%PAGE_SIZE)cnt++;
	if(freePageFrameCnt < cnt){
		putStr("The space is not enough for allocation. Process crushed.\n");
		assert(0);
	}
	int i;
	vaddr = (vaddr&0x003ff000)>>12;
	for(i=0;i<cnt;i++){
		int pf = freePageDequeue();
		busyPageEnqueue(&pcb[pid].busyPageFrameFirst,pf);
		pcb[pid].pageTb[vaddr + i].val = PAGE_DESC_BUILD(0,1,rw,1,pf_to_pa(pf));
		pcb[pid].procSize += PAGE_SIZE;

	}
	freePageFrameCnt -= cnt;
}

void allocateStack(uint32_t pid){
	allocatePageFrame(pid,MAX_PROC_SIZE - STACK_SIZE, STACK_SIZE, 1);
}

void clearPageTable(uint32_t pid){
	for(int i = 0;i<NR_PAGES_PER_PROC;i++){
		pcb[pid].pageTb[i].val = 0;
	}
	pcb[pid].procSize = 0;
}

void freshPageFrame(int num, int proc){
	assert(proc == 0 || proc == 2);
	uint32_t proc_base = proc * NR_PAGES_PER_PROC;
	for(int i = 0; i<NR_PAGES_PER_PROC; i++){
		pageFrame.content[proc_base + i] = pcb[num].pageTb[i];
	}
}
#endif


/*
MACRO
SEG(type, base, lim, dpl) (SegDesc) {...};
SEG_KCODE=1
SEG_KDATA=2
SEG_TSS=NR_SEGMENTS-1
DPL_KERN=0
DPL_USER=3
KSEL(desc) (((desc)<<3) | DPL_KERN)
USEL(desc) (((desc)<<3) | DPL_UERN)
asm [volatile] (AssemblerTemplate : OutputOperands [ : InputOperands [ : Clobbers ] ])
asm [volatile] (AssemblerTemplate : : InputOperands : Clobbers : GotoLabels)

triggger PAGE_ENABLED:
KERN_CODE_SEG  = 1
KERN_DATA_SEG = 2
USER_CODE_SEG = 3
USER_DATA_SEG = 4
NR_SEGMENTS = 6
PAGE_SIZE = 0x1000
*/
#ifdef PAGE_ENABLED
void initPage(){//set up page frames
	int i;
	for(i=0;i<nr_pageFrame;i++){
		pageFrameNxt[i] = i+1;
	}
	pageFrameNxt[nr_pageFrame-1] = -1;
	freePageFrameFirst = 0;
	freePageFrameCnt = nr_pageFrame;
	for(i=0;i<MAX_PCB_NUM;i++){
		clearPageTable(i);
		pcb[i].procSize = 0;
		pcb[i].active_mm = i;
		pcb[i].copyNum = 0;
	}
	//init kernel proc table
	pageDir.content[0].val =PAGE_DESC_BUILD(0,1,1,1,(unsigned int)pageFrame.content); //pageDir shared by user and kernel
	pcb[0].procSize = MAX_PROC_SIZE ;
	for(i=0;i<NR_PAGES_PER_PROC;i++){
		pcb[0].pageTb[i].val = PAGE_DESC_BUILD(0,1,1,0,pf_to_pa(i));
		pageFrame.content[i + NR_PAGES_PER_PROC].val = PAGE_DESC_BUILD(0,1,1,0,pf_to_pa(i));
 	}
	pcb[0].procSize += MAX_PROC_SIZE;
	freePageFrameFirst = NR_PAGES_PER_PROC;
	freePageFrameCnt -= NR_PAGES_PER_PROC;
	// enable page
	uint32_t cr3 = (uint32_t)pageDir.content;
	putStr("Prevent compiler from optimizing initPage().\n");
	asm volatile("movl %0,%%eax":"=m"(cr3));
	asm volatile("movl %eax, %cr3");
	asm volatile("movl %cr0,%eax");
	asm volatile("orl $0x80000000, %eax");
	asm volatile("movl %eax, %cr0");
}
#endif

void initSeg() { // setup kernel segements
#ifndef PAGE_ENABLED
	int i;
	//gdt[1] and gdt[2]
	gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0,       0xffffffff, DPL_KERN);
	gdt[SEG_KDATA] = SEG(STA_W,         0,       0xffffffff, DPL_KERN);

	//user processes' gdt
	for (i = 1; i < MAX_PCB_NUM; i++) {
		gdt[1+i*2] = SEG(STA_X | STA_R, (i+1)*0x100000,0x00100000, DPL_USER); //code
		gdt[2+i*2] = SEG(STA_W,         (i+1)*0x100000,0x00100000, DPL_USER); //data
	}
	
	gdt[SEG_TSS] = SEG16(STS_T32A,      &tss, sizeof(TSS)-1, DPL_KERN);
	gdt[SEG_TSS].s = 0;//system

#else
	gdt[SEG_KCODE] =  SEG(STA_X | STA_R, 0,       0xffffffff, DPL_KERN);
	gdt[SEG_KDATA] = SEG(STA_W,         0,       0xffffffff, DPL_KERN);
	gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0,       0xffffffff, DPL_USER);
	gdt[SEG_UDATA] = SEG(STA_W,         0,       0xffffffff, DPL_USER);
	gdt[SEG_TSS] = SEG16(STS_T32A,      &tss, sizeof(TSS)-1, DPL_KERN);
	gdt[SEG_TSS].s = 0;//system
#endif
	setGdt(gdt, sizeof(gdt)); // gdt is set in bootloader, here reset gdt in kernel

	/* initialize TSS */
	tss.ss0 = KSEL(SEG_KDATA);
#ifdef PAGE_ENABLED
	tss.cr3 = (uint32_t)pageDir.content;
#endif
	asm volatile("ltr %%ax":: "a" (KSEL(SEG_TSS)));
	/* reassign segment register */
	asm volatile("movw %%ax,%%ds":: "a" (KSEL(SEG_KDATA)));
	asm volatile("movw %%ax,%%ss":: "a" (KSEL(SEG_KDATA)));

	lLdt(0);
}

uint32_t loadUMain();

void initProc() {
	int i = 0;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		pcb[i].state = STATE_DEAD;
	}
	for(i = 0;i < MAX_PCB_NUM; i++){
			runnableNxt[i] = -1;
			freeNxt[i] = i+1;
	}
	runnableFirst = 0;
	runnableTail = 0;
	freeFirst = 0;
	freeNxt[MAX_PCB_NUM - 1] = -1;
	// kernel process
	pcb[0].stackTop = (uint32_t)&(pcb[0].stackTop);
	pcb[0].state = STATE_RUNNING;
	pcb[0].timeCount = MAX_TIME_COUNT;
	pcb[0].sleepTime = 0;
	pcb[0].pid = 0;

	// user process
	pcb[1].stackTop = (uint32_t)&(pcb[1].regs);
	pcb[1].state = STATE_RUNNABLE;
	pcb[1].timeCount = 0;
	pcb[1].sleepTime = 0;
	pcb[1].pid = 1;
#ifndef PAGE_ENABLED
	pcb[1].regs.ss = USEL(4);
	pcb[1].regs.cs = USEL(3);
	pcb[1].regs.ds = USEL(4);
	pcb[1].regs.es = USEL(4);
	pcb[1].regs.fs = USEL(4);
	pcb[1].regs.gs = USEL(4);
#else
	
	allocateStack(1);
	pcb[1].regs.ss = USEL(SEG_UDATA);
	pcb[1].regs.cs = USEL(SEG_UCODE);
	pcb[1].regs.ds = USEL(SEG_UDATA);
	pcb[1].regs.es = USEL(SEG_UDATA);
	pcb[1].regs.fs = USEL(SEG_UDATA);
	pcb[1].regs.gs = USEL(SEG_UDATA);
#endif
	pcb[1].regs.esp = 0x100000;
	pcb[1].regs.eip = loadUMain();	
	asm volatile("pushfl");
	asm volatile("popl %0":"=r"(pcb[1].regs.eflags));
	pcb[1].regs.eflags = pcb[1].regs.eflags | 0x200;
	current = 0; // kernel idle process
	runnableFirst = runnableTail = 1; //user process is waiting in queue
	freeFirst = 2;
	asm volatile("movl %0, %%esp"::"m"(pcb[0].stackTop)); // switch to kernel stack for kernel idle process
	enableInterrupt();
	asm volatile("int $0x20"); // trigger irqTimer
	while(1) 
		waitForInterrupt();
}

/*
kernel is loaded to location 0x100000, i.e., 1MB
size of kernel is not greater than 200*512 bytes, i.e., 100KB
user program is loaded to location 0x200000, i.e., 2MB
size of user program is not greater than 200*512 bytes, i.e., 100KB
*/

//自己编的memcpy
void *loadmemcpy(void *dst,void *src,unsigned len){
	int i = 0;
	for(i = 0; i < len; i++){
		*((unsigned char *)(dst+i)) = *((unsigned char *)(src+i));
	}
	return dst;
}

//自己编的memset
void *loadmemset(void *dst,unsigned char ch,unsigned len){
	int i = 0;
	for(i = 0; i < len; i++){
		*((unsigned char *)(dst+i)) = ch;
	}
	return dst;
}


//把elf文件加载到Pysaddr开始的地址，即将加载的程序以编号为Secstart的扇区开始，占据Secnum个扇区（LBA），传入的entry是个指针
#ifndef PAGE_ENABLED
int loadelf(uint32_t Secstart, uint32_t Secnum,uint32_t Pysaddr,uint32_t *entry){
	int i = 0;
	int phoff = 0x34;
	int phnum = 0;
	char buf[Secnum*512];
	unsigned int elf = (unsigned int)buf;
	struct ProgramHeader *thisph = 0x0;

	for (i = 0; i < Secnum; i++) {
		readSect((void*)(elf + i*512), Secstart + i);
	}

	*entry = (uint32_t)((struct ELFHeader *)elf)->entry;
	phoff = ((struct ELFHeader *)elf)->phoff;
	phnum = ((struct ELFHeader *)elf)->phnum;
	for(i = 0; i < phnum; i++){
		thisph = ((struct ProgramHeader *)(elf + phoff) + i);
		if(thisph->type == PT_LOAD){
			loadmemcpy((void *)Pysaddr + thisph->vaddr, ((void *)elf+thisph->off), thisph->filesz);
			loadmemset((void *)Pysaddr + thisph->vaddr+thisph->filesz, 0, thisph->memsz-thisph->filesz);
		}
	}
	return 0;
}
#else
int loadelf(uint32_t Secstart, uint32_t Secnum,uint32_t pid,uint32_t *entry){
	int i = 0;
	int phoff = 0x34;
	int phnum = 0;
	char buf[Secnum*512];
	unsigned int elf = (unsigned int)buf;
	struct ProgramHeader *thisph = 0x0;

	for (i = 0; i < Secnum; i++) {
		readSect((void*)(elf + i*512), Secstart + i);
	}
	*entry = (uint32_t)((struct ELFHeader *)elf)->entry;
	phoff = ((struct ELFHeader *)elf)->phoff;
	phnum = ((struct ELFHeader *)elf)->phnum;
	for(i = 0; i < phnum; i++){
		thisph = ((struct ProgramHeader *)(elf + phoff) + i);
		if(thisph->type == PT_LOAD){
			allocatePageFrame(pid,thisph->vaddr,thisph->memsz,ph_w(thisph->flags)); // allocate space for the PT_LOAD contents first, macro ph_w judge whether writable or not
		}
	}
	freshPageFrame(pid,2);
	for(i = 0; i < phnum; i++){
		thisph = ((struct ProgramHeader *)(elf + phoff) + i);
		if(thisph->type == PT_LOAD){
			// Inside kernel, all page tables are reachable.
			uint32_t kernelVaddr = 0x200000 +  thisph->vaddr;
			loadmemcpy((void *)kernelVaddr, ((void *)elf+thisph->off), thisph->filesz);
			loadmemset((void *)kernelVaddr+thisph->filesz, 0, thisph->memsz-thisph->filesz);
		}
	}
	return 0;
}
#endif

uint32_t loadUMain(){
	uint32_t entry = 0;
#ifndef PAGE_ENABLED
	loadelf(201, 20, 0x200000, &entry);
#else
	loadelf(201,20,1,&entry);
#endif
	putStr("loadelf() should not be optimized.\n"); 
	//额外的运算会让编译器放弃优化这个函数，注释此行可以重现bug
	return entry;
}
