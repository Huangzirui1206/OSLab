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

#ifdef PAGE_ENABLED
extern void releaseBusyPage(int busyPageFrameFirst);
extern void allocatePageFrame(uint32_t pid, uint32_t vaddr, uint32_t size, uint32_t rw);
extern void allocateStack(uint32_t pid);
extern void clearPageTable(uint32_t pid);
extern void freshPageFrame(int num, int pf);
extern PageFrame pageDir; 
extern PageFrame pageFrame;

extern void eax_get_eip();
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
	if(runnableFirst!=-1)runnableFirst = runnableNxt[runnableFirst];
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


static void switchProc(uint32_t num){
	current = num;
	pcb[current].state = STATE_RUNNING;
#ifdef PAGE_ENABLED
	freshPageFrame(current,0);
#endif
	int tmpStackTop=pcb[num].stackTop;
    tss.esp0=(uint32_t)&(pcb[num].stackTop); //use as kernel stack
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

static void procPageTableCopy(uint32_t dst, uint32_t src){
	for(int i = 0;i<NR_PAGES_PER_PROC;i++){
		pcb[dst].pageTb[i] = pcb[src].pageTb[i];
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
		releaseBusyPage(pcb[num].busyPageFrameFirst); //Release the process space.
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
#endif
void PageFaultHandle(struct StackFrame *sf){
#ifdef PAGE_ENABLED
	uint32_t fault_addr;
	asm volatile("movl %cr2,%eax");
	asm volatile("movl %%eax, %0":"=m"(fault_addr));
	fault_addr = (fault_addr & 0x003ff000)>>12;
	// check present
	if(pcb[current].pageTb[fault_addr].p==0){
		putStr("Page Fault: page doesn't exit! Programme exits with code -1.\n");
		pf_error_info(sf->eip,current,sf->error)
		assert(0);
	}
	// check if read-only
	uint32_t active_pcb =  pcb[current].active_mm;
	if(pcb[active_pcb].pageTb[fault_addr].real_rw == 0){
		putStr("Page Fault: write on read-only page ! Programme exits with code -1.\n");
		pf_error_info(sf->eip,current,sf->error)
		assert(0);
	}
	// copy-on-write
	if(pcb[current].active_mm != pcb[current].pid){
		allocatePageFrame(current,fault_addr,PAGE_SIZE,1);
		pcb[active_pcb].pageTb[fault_addr].rw = 1;
	}
	else if(pcb[current].copyNum != 0){
		pcb[current].pageTb[fault_addr].rw = 1;
		// O(n)
		for(int i = 0; i<MAX_PCB_NUM;i++){
			if(i == current || i == 0)continue;
			else if(pcb[i].state == STATE_DEAD || pcb[i].state == STATE_ZOMBIE)continue;
			else if(pcb[i].active_mm == current && pcb[i].pageTb[fault_addr].rw == 0){
				allocatePageFrame(current,fault_addr,PAGE_SIZE,1);
			}
		}
	}
	//fresh pageFrame[0]
	freshPageFrame(current, 0);
	// fresh TLB
	tss.cr3 = (uint32_t)pageDir.content;
#else
	assert(0);
	return;
#endif
}

void timerHandle(struct StackFrame *sf){
	//handle sleep precesses
	for(int i =0;i<MAX_PCB_NUM;i++){
		if(pcb[i].sleepTime)pcb[i].sleepTime--;
		if(!pcb[i].sleepTime && pcb[i].state == STATE_BLOCKED)runnableEnqueue(i);
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
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
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
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
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
	if(i==-1)return;
	//copy segment address space
#ifndef PAGE_ENABLED
	memcpy((void*)desc_to_pbase(i),(void*)va_to_pa(0),SEG_SIZE*2);
	memcpy(&pcb[i],&pcb[current],sizeof(ProcessTable));
#else
	// copy_on_write
	pageTableReadOnly(current);
	pcb[i].active_mm = current;
	pcb[current].copyNum +=1; 
	// copy page tables
	procPageTableCopy(i,current);
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
#else
	pcb[i].regs.cs = USEL(SEG_UCODE);
	pcb[i].regs.ds = USEL(SEG_UDATA);
	pcb[i].regs.es = USEL(SEG_UDATA);
	pcb[i].regs.fs = USEL(SEG_UDATA);
	pcb[i].regs.gs = USEL(SEG_UDATA);
	pcb[i].regs.ss = USEL(SEG_UDATA);
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
#ifndef PAGE_ENABLED
	loadelf(secstart,secnum,va_to_pa(0),&entry);
#else
	if(pcb[current].copyNum != 0){
		assert(0);
		for(int i = 0;i<MAX_PCB_NUM;i++){
			if(i==0||i==current) continue;
			else if(pcb[i].state == STATE_DEAD || pcb[i].state == STATE_ZOMBIE)continue;
			else if(pcb[i].active_mm == current){
				//TODO:handle this.
				
			}
		}
	}
	do_exit(current);
	pcb[current].active_mm = current;
	pcb[current].state = STATE_RUNNING;
	loadelf(secstart,secnum,current,&entry); // Don't care Physaddr under page system.
	allocateStack(current);
#endif
	sf->esp = SEG_SIZE;
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
	if(nxtProc == -1){
		enableInterrupt();
		while(nxtProc == -1){//no runalbe process
			nxtProc = runnableDequeue();
		}
		disableInterrupt();
	}
	pcb[current].state = STATE_BLOCKED;
	//switch process
    switchProc(nxtProc);
}	

void syscallExit(struct StackFrame *sf){
	int error = sf->ecx;
	if(error)assert(error!=0);
	do_exit(current);
	int nxtProc = runnableDequeue();
	if(nxtProc == -1){
		enableInterrupt();
		while(nxtProc == -1){//no runalbe process
			nxtProc = runnableDequeue();
		}
		disableInterrupt();
	}
	switchProc(nxtProc);
}
