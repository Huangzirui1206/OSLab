#include "common.h"
#include "x86.h"
#include "device.h"

#ifndef PAGE_ENABLED
#define va_to_pa(va) (va + (current + 1) * 0x100000)
#define pa_to_va(pa) (pa - (current + 1) * 0x100000)
#define desc_to_pbase(desc) ((desc+1)*0x100000 ) 
#define proc_size 0x100000
#endif

extern TSS tss;
extern int displayRow;
extern int displayCol;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int runnableFirst;
extern int runnableTail;
extern int runnableNxt[MAX_PCB_NUM]; //for STATE_RUNNABLE to round-robin
extern int current; // current process
extern int freeFirst;
extern int freeNxt[MAX_PCB_NUM]; //for allocating free pcb to a new precess

extern void eax_get_eip();

#ifdef PAGE_ENABLED
extern void releaseBusyPage(int* busyPageFrameFirst);
extern void allocatePageFrame(uint32_t pid, uint32_t vaddr, uint32_t size, uint32_t rw);
extern void allocateStack(uint32_t pid);
extern void clearPageTable(uint32_t pid);
extern void freshPageFrame(int pid, int proc);
extern void copyPageFrame(int src_pid, int dst_pid, int pf);
extern PageFrame pageDir; 
extern PageFrame pageFrame;
#endif

static void runnableEnqueue(uint32_t num){
	assert(num<MAX_PCB_NUM);
	pcb[num].state = STATE_RUNNABLE;
	if(runnableFirst!=-1){//runnableQueue is not empty
		runnableNxt[runnableTail] = num;
		runnableNxt[num] = -1;
		runnableTail = num;
	}
	else{
		runnableTail = runnableFirst = num;
		runnableNxt[num] = -1;
	}
}

static int runnableDequeue(){
	int oriFirst = runnableFirst;
	if(oriFirst!=-1){
		runnableFirst = runnableNxt[oriFirst];
		if(runnableFirst==-1)runnableTail = runnableFirst;
	}
	if(oriFirst==0 && runnableNxt[0] != -1){
		oriFirst = runnableFirst;
		runnableFirst = runnableNxt[oriFirst];
		if(runnableFirst==-1)runnableTail = runnableFirst;
		runnableEnqueue(0);
	}
	return oriFirst;
}

static void freeEnqueue(uint32_t num){
	assert(num<MAX_PCB_NUM);
	pcb[num].state = STATE_DEAD;
	freeNxt[num] = freeFirst;
	freeFirst = num;
}

static int freeDequeue(){
	int oriFirst = freeFirst;
	if(freeFirst!=-1)freeFirst = freeNxt[freeFirst];
	return oriFirst;
}

void GProtectFaultHandle(struct StackFrame *sf);
void PageFaultHandle(struct StackFrame *sf);

void timerHandle(struct StackFrame *sf);

void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);

void syscallThreadCreate(struct StackFrame *sf);
void syscallThreadExit(struct StackFrame *sf);
void syscallThreadJoin(struct StackFrame *sf);

static void switchProc(uint32_t num){
	current = num;
	pcb[current].state = STATE_RUNNING;
#ifdef PAGE_ENABLED
	if(num!=0) freshPageFrame(current,0);
	else{
		enableInterrupt();
		while (1)
			waitForInterrupt();
	}
#else
	if(num==0) {
		enableInterrupt();
		while (1)
			waitForInterrupt();
	}
	putStr("Avoid switchProc() from being optimized.\n");
#endif
	int tmpStackTop=pcb[num].stackTop;
    tss.esp0=(uint32_t)&(pcb[num].stackTop); //use as kernel stack
	asm volatile("movl %0,%%eax":"=m"(current));
    asm volatile("movl %0,%%esp"::"m"(tmpStackTop));
    asm volatile("popl %gs");
    asm volatile("popl %fs");
    asm volatile("popl %es");
    asm volatile("popl %ds");
    asm volatile("popal");
    asm volatile("addl $8,%esp");
    asm volatile("iret");
}

#ifdef PAGE_ENABLED
static void pageTableReadOnly(uint32_t pid){
	for(int i=0;i<NR_PAGES_PER_PROC;i++){
		pcb[pid].pageTb[i].real_rw = pcb[pid].pageTb[i].rw;
		pcb[pid].pageTb[i].rw = 0;
	}
}
#endif
#ifdef PTHREAD_ENABLED
static void stackPageTableReadOnly(uint32_t pid){
	for(int i=NR_PAGES_PER_PROC - STACK_SIZE;i<NR_PAGES_PER_PROC;i++){
		pcb[pid].pageTb[i].real_rw = pcb[pid].pageTb[i].rw;
		pcb[pid].pageTb[i].rw = 0;
	}
}
#endif

static void do_exit(uint32_t num){
#ifdef PAGE_ENABLED
	if(pcb[num].copyNum==0){
		uint32_t active_pcb = pcb[num].active_mm;
		if(active_pcb!=num){
			pcb[active_pcb].copyNum-=1;
			if(pcb[active_pcb].state == STATE_ZOMBIE){
				do_exit(active_pcb);
			}
		}
		clearPageTable(num);
		releaseBusyPage(&pcb[num].busyPageFrameFirst); //Release the process space.
		pcb[num].state = STATE_DEAD;
		freeEnqueue(num);
	}
	else{
		pcb[current].state = STATE_ZOMBIE;
	}
#else
	pcb[num].state = STATE_DEAD;
	freeEnqueue(num);
#endif
}

void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	
	/* Save esp to stackTop */
	//为了中断嵌套
	pcb[current].stackTop=(uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		case 0xe:
			PageFaultHandle(sf);
			break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct StackFrame *sf) {
	putStr("The general protect fault happened with return ");
	putNum(sf->error);
	putStr(" in process ");
	putNum(current);
	putChar('\n');
	assert(0);
	return;
}

#ifdef PAGE_ENABLED
#define pf_error_info(eip,pid,error)\
putStr("#PF(");\
putNumX(error);\
putStr(") at eip: ");\
putNumX(eip);\
putStr(" in process ");\
putNum(pid);\
putChar('\n');

static int copyOnWrite(uint32_t pid, int fault_addr){
	int fault_pf = (fault_addr & 0x003ff000)>>12;
	// check present
	if(pcb[pid].pageTb[fault_pf].p==0){
		putStr("Page Fault: page doesn't exit! Programme exits with code -1.\n");
		return -1;
	}
	// check if read-only
	uint32_t active_pcb =  pcb[pid].active_mm;
	if(pcb[active_pcb].pageTb[fault_pf].real_rw == 0){
		putStr("Page Fault: write on read-only page ! Programme exits with code -1.\n");
		return -1;
	}
	// copy-on-write
	if(pcb[pid].active_mm != pcb[pid].pid){
		allocatePageFrame(pid,fault_addr,PAGE_SIZE,1);
		copyPageFrame(active_pcb, pid,fault_pf);
		pcb[active_pcb].pageTb[fault_pf].rw = 1;
	}
	else if(pcb[pid].copyNum != 0){
		pcb[pid].pageTb[fault_pf].rw = 1;
		// O(n)
		for(int i = 0; i<MAX_PCB_NUM;i++){
			if(i == pid || i == 0)continue;
			else if(pcb[i].state == STATE_DEAD || pcb[i].state == STATE_ZOMBIE)continue;
			else if(pcb[i].active_mm == pid && pcb[i].pageTb[fault_pf].rw == 0){
				allocatePageFrame(i,fault_addr,PAGE_SIZE,1);
				// copy page frame
				copyPageFrame(pid, i,fault_pf);
			}
		}
	}
	return 0;
}
#endif

void PageFaultHandle(struct StackFrame *sf){
#ifdef PAGE_ENABLED
	uint32_t fault_addr;
	asm volatile("movl %cr2,%eax");
	asm volatile("movl %%eax, %0":"=m"(fault_addr));
	int cow = copyOnWrite(current, fault_addr);
	if(cow == -1){
		pf_error_info(sf->eip,current,sf->error)
		assert(0);
	}
	//fresh pageFrame and TLB
	freshPageFrame(current, 0);
#else
	assert(0);
	return;
#endif
}

void timerHandle(struct StackFrame *sf){
	//handle sleep precesses
	for(int i =0;i<MAX_PCB_NUM;i++){
		if(pcb[i].sleepTime)pcb[i].sleepTime--;
#ifndef PTHREAD_ENABLED
		if(!pcb[i].sleepTime && pcb[i].state == STATE_BLOCKED)runnableEnqueue(i);
#else
		if(!pcb[i].sleepTime && pcb[i].join_pid == -1 && pcb[i].state == STATE_BLOCKED)runnableEnqueue(i);
#endif
	}
	//handle precess switch
	if(pcb[current].state == STATE_RUNNING){
			pcb[current].timeCount++;
		if(pcb[current].timeCount>=MAX_TIME_COUNT){
			int nxtProc = runnableDequeue();
			if(nxtProc==-1) {// no runnable process
				pcb[current].timeCount = 0;
			}
			else{
				runnableEnqueue(current);
				//switch process
     	    	switchProc(nxtProc);
		    } 
		}
	}else{
			int nxtProc = runnableDequeue();
			if(nxtProc!=-1) switchProc(nxtProc);
	}
}


void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case 0:
			syscallWrite(sf);
			break; // for SYS_WRITE
		/* Add Fork,Sleep... */
		case 1:
			syscallFork(sf);
			break;
		case 2:
			syscallExec(sf);
			break;
		case 3:
			syscallSleep(sf);
			break;
		case 4:
			syscallExit(sf);
			break;
		// pthread functions
		case 5:
			syscallThreadCreate(sf);
			break;
		case 6:
			syscallThreadExit(sf);
			break;
		case 7:
			syscallThreadJoin(sf);
			break;
		default:break;
	}
}

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case 0:
			syscallPrint(sf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct StackFrame *sf) {
#ifndef PAGE_ENABLED
	int sel = sf->ds; // segment selector for user data, need further modification
#endif
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
#ifndef PAGE_ENABLED
	asm volatile("movw %0, %%es"::"m"(sel));
#else
	pageFrame.content[0x2b8].val = PAGE_DESC_BUILD(0,1,1,0,0xb8000);
#endif
	for (i = 0; i < size; i++) {
#ifndef PAGE_ENABLED
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
#else
		asm volatile("movb (%1), %0":"=r"(character):"r"(str+i));
#endif
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==25){
				displayRow=24;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (80*displayRow+displayCol)*2;
#ifndef PAGE_ENABLED
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
#else
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0x2b8000));
#endif
			displayCol++;
			if(displayCol==80){
				displayRow++;
				displayCol=0;
				if(displayRow==25){
					displayRow=24;
					displayCol=0;
					scrollScreen();
				}
			}
		}

	}
	updateCursor(displayRow, displayCol);
	sf->eax=size;
	return;
}	

void memcpy(void* dst,void* src,size_t size){
	for(uint32_t j=0;j<size;j++){
		*(uint8_t*)(dst+j)=*(uint8_t*)(src+j);
	}
}

void syscallFork(struct StackFrame *sf){
	//search for free pcb, if there is not, return -1 to father process
	int i=freeDequeue();
	sf->eax = i;
	if(i==-1){
		sf->eax = -1;
		return;
	}
	//copy segment address space
#ifndef PAGE_ENABLED
	memcpy((void*)desc_to_pbase(i),(void*)va_to_pa(0),SEG_SIZE);
	memcpy(&pcb[i],&pcb[current],sizeof(ProcessTable));
#else
	// copy_on_write
	pageTableReadOnly(current);
	memcpy(&pcb[i],&pcb[current],sizeof(ProcessTable) - 4 * sizeof(int));
	pcb[i].active_mm = current;
	pcb[current].copyNum +=1; 
	//fresh Page Table and TLB
	freshPageFrame(current,0);
#endif	
	// return 0 to child process
	pcb[i].regs.eax = 0;
#ifndef PAGE_ENABLED
	pcb[i].regs.cs = USEL(1 + i * 2);
	pcb[i].regs.ds = USEL(2 + i * 2);
	pcb[i].regs.es = USEL(2 + i * 2);
	pcb[i].regs.fs = USEL(2 + i * 2);
	pcb[i].regs.gs = USEL(2 + i * 2);
	pcb[i].regs.ss = USEL(2 + i * 2);
#endif
#ifdef PTHREAD_ENABLED
	pcb[i].join_pid = -1;
#endif
	pcb[i].stackTop = (uint32_t)&(pcb[i].regs);
	pcb[i].state = STATE_RUNNABLE;
	runnableEnqueue(i);
	pcb[i].timeCount = MAX_TIME_COUNT - pcb[i].timeCount;
	pcb[i].sleepTime = 0;
	pcb[i].pid = i;
}	

void syscallExec(struct StackFrame *sf) {
	// use elf to realise exec()
	uint32_t entry = 0;
	uint32_t secstart = sf->ecx;
	uint32_t secnum =  sf->edx;
	sf->eax = 0;
#ifndef PAGE_ENABLED
	loadelf(secstart,secnum,va_to_pa(0),&entry);
	sf->esp = SEG_SIZE - 0x10; // spare some space for safety
#else
	if(pcb[current].copyNum != 0){
		putStr("The process still has children using its copy, can't exec()\n");
		sf->eax = -1;
		return;
	}
	do_exit(current);
	pcb[current].active_mm = current;
	pcb[current].state = STATE_RUNNING;
	loadelf(secstart,secnum,current,&entry); 
	allocateStack(current);
	freshPageFrame(current, 0);
	sf->esp = NR_PAGES_PER_PROC*PAGE_SIZE - 0x10; // spare some space for safety
#endif
	sf->eax = 0;
	sf->eflags = sf->eflags | 0x200;
	sf->eip = entry;
	pcb[current].stackTop = (uint32_t)&(pcb[current].regs);
	putStr("loadelf() should not be optimized.\n"); 
}

void syscallSleep(struct StackFrame *sf){
	pcb[current].state = STATE_BLOCKED;
	pcb[current].sleepTime = sf->ecx;
	pcb[current].timeCount = 0;
	int nxtProc = runnableDequeue();
	assert(nxtProc!=-1);
	pcb[current].state = STATE_BLOCKED;
	//switch process
    switchProc(nxtProc);
}	

void syscallExit(struct StackFrame *sf){
	int error = sf->ecx;
	if(error)assert(error!=0);
	do_exit(current);
	int nxtProc = runnableDequeue();
	assert(nxtProc != -1);
	switchProc(nxtProc);
}

// kernel thread
void syscallThreadCreate(struct StackFrame *sf){
#ifndef PAGE_ENABLED
	putStr("Can't invoke pthread.h when macro PAGE_ENABLED is set.\n");
	sf->eax = -1;
	return;
#elif defined PTHREAD_ENABLED
	//search for free pcb, if there is not, return -1 to father process
	int i=freeDequeue();
	sf->eax = i;
	if(i==-1){
		sf->eax = -1;
		return;
	}
	// copy_on_write
	stackPageTableReadOnly(current);
	memcpy(&pcb[i],&pcb[current],sizeof(ProcessTable) - 4 * sizeof(int));
	pcb[i].active_mm = current;
	pcb[current].copyNum +=1; 
	// return 0 to thread
	pcb[i].regs.eax = 0;
	// set thread id 
	*(uint32_t*)sf->ecx = i;	
	pcb[i].regs.eip = sf->edx;
	//build argv
	pcb[i].regs.esp = NR_PAGES_PER_PROC*PAGE_SIZE- 0x10;
	int cow = copyOnWrite(i, pcb[i].regs.esp);
	if(cow == -1){
		putStr("Copy-On-Write goes wrong in pthread_create.\n");
		sf->eax = -1;
		return;
	}
	pageFrame.content[0x2ff].val = pcb[i].pageTb[0xff].val;
	uint32_t argNum  = sf->ebx;
	uint32_t* argvSrc = (uint32_t*)sf->esi;
	uint32_t* argvDst = (uint32_t*)(0x200000 + pcb[i].regs.esp - 4 * argNum);
	//fresh Page Table and TLB
	freshPageFrame(current,0);
	for (int i = 0;i<argNum;i++){
		argvDst[i] = argvSrc[i]; 
	}
	pageFrame.content[0x2ff].val = 0;
	pcb[i].stackTop = (uint32_t)&(pcb[i].regs);
	pcb[i].state = STATE_RUNNABLE;
	runnableEnqueue(i);
	pcb[i].timeCount = MAX_TIME_COUNT - pcb[i].timeCount;
	pcb[i].sleepTime = 0;
	pcb[i].pid = i;
#else
	putStr("Can't invoke pthread.h when macro PTHREAD_ENABLED is set.\n");
	sf->eax = -1;
	return;
#endif
}

void syscallThreadExit(struct StackFrame *sf){
#ifdef PTHREAD_ENABLED
	// set retval to waiter porcess if there is one.
	int waiter_pid = pcb[current].waiter_pid;
	if(waiter_pid>0&&waiter_pid<MAX_PCB_NUM){
		assert(pcb[waiter_pid].state==STATE_BLOCKED);
		pcb[waiter_pid].join_retval = sf->ecx;
		runnableEnqueue(waiter_pid);
	}
	sf->eax = 0;
	// do exit
	do_exit(current);
	// Switch proc, int $0x20 is actually better, but here we won't run into nexted interrupt.
	int nxtProc = runnableDequeue();
	assert(nxtProc != -1);
	switchProc(nxtProc);
#else
	sf->eax = -1;
#endif
}

void syscallThreadJoin(struct StackFrame *sf){
#ifdef PTHREAD_ENABLED
	int join_pid = sf->ecx;
	// handle thread_join failure
	if(join_pid!=STATE_RUNNABLE&&join_pid!=STATE_BLOCKED){
		//join failed
		sf->eax = -1;
		return;
	}
	if(pcb[join_pid].active_mm!=current){
		//join failed
		sf->eax = -1;
		return;
	}
	// set join information
	pcb[current].join_pid = join_pid;
	pcb[current].state = STATE_BLOCKED;
	pcb[current].timeCount = 0;
	asm volatile ("int $0x20"); // To  handle nested interrupt, and switch process
	*(uint32_t*)sf->edx = pcb[current].join_pid;
	sf->eax = 0;
#else	
	sf->eax = -1;
#endif
}