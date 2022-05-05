#include "x86.h"
#include "device.h"

//#define DEBUG

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_FORK 2
#define SYS_EXEC 3
#define SYS_SLEEP 4
#define SYS_EXIT 5
#define SYS_SEM 6
#define SYS_CRI_MEM 7

#define STD_OUT 0
#define STD_IN 1

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3


// For writer and reader problem, stimulate critical area
#define CR_RCNT_R 0
#define CR_RCNT_INC 1
#define CR_RCNT_DEC 2
#define CR_kernelFile_W 3
#define CR_kernelFile_R 4
#define CR_WCNT_R 5
#define CR_WCNT_INC 6
#define CR_WCNT_DEC 7


extern TSS tss;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

extern int Rcnt;
extern int Wcnt;
extern char kernelFile[128];

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void keyboardHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallRead(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void syscallSem(struct StackFrame *sf);
void syscallCriArea(struct StackFrame *sf);



void syscallWriteStdOut(struct StackFrame *sf);

void syscallReadStdIn(struct StackFrame *sf);

void syscallSemInit(struct StackFrame *sf);
void syscallSemWait(struct StackFrame *sf);
void syscallSemPost(struct StackFrame *sf);
void syscallSemDestroy(struct StackFrame *sf);

void syscallRcntRead(struct StackFrame *sf);
void syscallRcntInc(struct StackFrame *sf);
void syscallRcntDec(struct StackFrame *sf);
void syscallkernelFileRead(struct StackFrame *sf);
void syscallkernelFileWrite(struct StackFrame *sf);
void syscallWcntRead(struct StackFrame *sf);
void syscallWcntInc(struct StackFrame *sf);
void syscallWcntDec(struct StackFrame *sf);

void eax_get_eip();
void putNumX(uint32_t);
void eax_get_eip(){
#ifdef DEBUG
	asm volatile("leal 4(%ebp), %ebx");
	asm volatile("movl (%ebx), %eax");
	int eip;
	asm volatile("movl %%eax, %0":"=m"(eip));
	putNumX(eip);
	putChar('\n');
#endif
}
void putNumX(uint32_t num){
	 static char buf[30] = { 0 };  
    char* p = buf + sizeof(buf) - 1;  
    if (num == 0){ putChar('0'); return;}  
   // if (num < 0){ putChar('-'); num = -num;}  
    while(num){ 
		int i =  (num % 16);
		if(i<10){
			*--p = i + '0';  
		}
	    else{
			*--p = i - 10 + 'a';  
		}
         num /= 16;  
    } 
	putChar('0');
	putChar('x'); 
    while(*p) putChar(*(p++));  
}


void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	/* Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x21:
			keyboardHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		default:assert(0);
	}
	/* Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf) {
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf) {
	int i;
	uint32_t tmpStackTop;
	i = (current+1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED && pcb[i].sleepTime != -1) {
			pcb[i].sleepTime --;
			if (pcb[i].sleepTime == 0)
				pcb[i].state = STATE_RUNNABLE;
		}
		i = (i+1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
		pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		
		i = (current+1) % MAX_PCB_NUM;
		while (i != current) {
			if (i !=0 && pcb[i].state == STATE_RUNNABLE)
				break;
			i = (i+1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE)
			i = 0;
		current = i;
		/* echo pid of selected process */
		//putChar('0'+current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		/* recover stackTop of selected process */
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop); // setting tss for user process
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
}

void keyboardHandle(struct StackFrame *sf) {
	ProcessTable *pt = NULL;
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0) // illegal keyCode
		return;
	putChar(getChar(keyCode));
	//keyBuffer[bufferTail] = getChar(keyCode);
	//bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;
	if(keyCode == 0xe){ // 退格符
		if(displayCol>0){
			keyBuffer[bufferTail]=0xe;
			bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;
			displayCol--;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(keyCode == 0x1c){ // 回车符
		keyBuffer[bufferTail]='\n';
		bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;
		displayRow++;
		displayCol=0;
	
		if(displayRow==25){
			scrollScreen();
			displayRow=24;
			displayCol=0;
		}
	}else if(keyCode < 0x81){ // 正常字符
		char character=getChar(keyCode);
		if(character!=0){
			//putChar(character);//put char into serial
			uint16_t data=character|(0x0c<<8);
			keyBuffer[bufferTail]=character;
			bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;
			int pos=(80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol+=1;
			if(displayCol==80){
				displayCol=0;
				displayRow++;
				if(displayRow==25){
					scrollScreen();
					displayRow=24;
					displayCol=0;
				}
			}
		}
	}
	updateCursor(displayRow, displayCol);
	

	if (dev[STD_IN].state==1&&dev[STD_IN].value < 0) { // with process blocked
		//  deal with blocked situation
		pt = (ProcessTable*)((uint32_t)(dev[STD_IN].pcb.prev) - (uint32_t)&(((ProcessTable*)0)->blocked));
		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
		pt->state=STATE_RUNNABLE;
		dev[STD_IN].value=1;
		asm volatile("int $0x20");
	}

	return;
}

void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case SYS_WRITE:
			syscallWrite(sf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(sf);
			break; // for SYS_READ
		case SYS_FORK:
			syscallFork(sf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(sf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(sf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(sf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(sf);
			break; // for SYS_SEM
		case SYS_CRI_MEM:
			syscallCriArea(sf);
			break;
		default:break;
	}
}

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // kernelFile descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1)
				syscallWriteStdOut(sf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallWriteStdOut(struct StackFrame *sf) {
	int sel = sf->ss; // segment selector for user data, need further modification
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
			if(displayRow==MAX_ROW){
				displayRow=MAX_ROW-1;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (MAX_COL*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==MAX_COL){
				displayRow++;
				displayCol=0;
				if(displayRow==MAX_ROW){
					displayRow=MAX_ROW-1;
					displayCol=0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
	return;
}

void syscallRead(struct StackFrame *sf) {
	switch(sf->ecx) {
		case STD_IN:
			if (dev[STD_IN].state == 1)
				syscallReadStdIn(sf);
			break; // for STD_IN
		default:
			break;
	}
}

void syscallReadStdIn(struct StackFrame *sf) {
	int16_t sel = sf->ds;
	asm volatile("movw %0, %%es"::"m"(sel));
	char *str = (char *)sf->edx;
	int i = 0;
	uint32_t size = sf->ebx;
	char character = 0;
	if(dev[STD_IN].value<0){
		sf->eax = -1;
	}
	else if(dev[STD_IN].value==0){
		dev[STD_IN].value --;
		/* read STD_IN */
		do
		{
			/* add current process to the dev[STD_IN]'s blocked list */
			dev[STD_IN].value  = -1;
			pcb[current].blocked.next = dev[STD_IN].pcb.next;
        	pcb[current].blocked.prev = &(dev[STD_IN].pcb);
       		dev[STD_IN].pcb.next = &(pcb[current].blocked);
        	(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
			pcb[current].state = STATE_BLOCKED;
			pcb[current].sleepTime = -1;
			asm volatile("int $0x20");
			if(bufferHead!=bufferTail){
				character = keyBuffer[bufferHead];
				bufferHead = (bufferHead+1)%MAX_KEYBUFFER_SIZE;
				if(character == 0xe){
					i--;
					character = '\0';
					asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str + i));
				}
				else if(character != '\n'){
					asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str + i));
					i++;
					if(i>=size)break;
				}
			}
		} while (character!='\n');
		asm volatile("movb $0, %%es:(%0)"::"r"(str + i));
		sf->eax = i + 1;
		dev[STD_IN].value++;
	}
	else if(dev[STD_IN].value>0){
		dev[STD_IN].value = 0;
		sf->eax = -1;
	}
}

void syscallFork(struct StackFrame *sf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD)
			break;
	}
	if (i != MAX_PCB_NUM) {
		/* copy userspace
		   enable interrupt
		 */
		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i+1)*0x100000) = *(uint8_t *)(j + (current+1)*0x100000);
			//asm volatile("int $0x20"); // Testing irqTimer during syscall
		}
		/* disable interrupt
		 */
		disableInterrupt();
		/* set pcb
		   pcb[i]=pcb[current] doesn't work
		*/
		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		/* set regs */
		pcb[i].regs.ss = USEL(2+i*2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1+i*2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2+i*2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/* set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct StackFrame *sf) {
	return;
}

void syscallSleep(struct StackFrame *sf) {
	if (sf->ecx == 0)
		return;
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = sf->ecx;
		asm volatile("int $0x20");
		return;
	}
}

void syscallExit(struct StackFrame *sf) {
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct StackFrame *sf) {
	switch(sf->ecx) {
		case SEM_INIT:
			syscallSemInit(sf);
			break;
		case SEM_WAIT:
			syscallSemWait(sf);
			break;
		case SEM_POST:
			syscallSemPost(sf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(sf);
			break;
		default:break;
	}
}

void syscallSemInit(struct StackFrame *sf) {
	int value = sf->edx;
	if(value < 0){
		sf->eax = -1;
		return;
	}
	int i =0;
	for(;i<MAX_SEM_NUM;i++){
		if(sem[i].state == 0)break;
	}
	if(i==MAX_SEM_NUM){
		sf->eax = -1;
		return;
	}
	sem[i].state = 1;
	sem[i].value = value;
	sf->eax = i;
	return;
}

void syscallSemWait(struct StackFrame *sf) {
	int i = (int)sf->edx;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if(sem[i].state == 0){
		sf->eax = -1;
		return;
	}
	sem[i].value --;
	if(sem[i].value<0){
		pcb[current].blocked.next = sem[i].pcb.next;
        pcb[current].blocked.prev = &(sem[i].pcb);
        sem[i].pcb.next = &(pcb[current].blocked);
        (pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = -1;
		sf->eax = 0;
		asm volatile("int $0x20");
	}
}

void syscallSemPost(struct StackFrame *sf) {
	int i = (int)sf->edx;
	ProcessTable *pt = NULL;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if(sem[i].state == 0){
		sf->eax = -1;
		return;
	}
	sem[i].value ++;
	if(sem[i].value<=0){
		 pt = (ProcessTable*)((uint32_t)(sem[i].pcb.prev) -
                    (uint32_t)&(((ProcessTable*)0)->blocked));
        sem[i].pcb.prev = (sem[i].pcb.prev)->prev;
        (sem[i].pcb.prev)->next = &(sem[i].pcb);
		pt->state = STATE_RUNNABLE;
		sf->eax = 0;
	}
}

void syscallSemDestroy(struct StackFrame *sf) {
	int i = (int)sf->edx;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if(sem[i].state == 0){
		sf->eax = -1;
		return;
	}
	if(sem[i].pcb.prev==&(sem[i].pcb)&&sem[i].pcb.next==&(sem[i].pcb)){
		sem[i].state = 0;
		sf->eax = 0;
	}
	else{
		sf->eax = -1;
	}
}

void syscallCriArea(struct StackFrame* sf){
	switch(sf->ecx){
		case CR_RCNT_R:
			syscallRcntRead(sf);
			break;
		case CR_RCNT_INC:
			syscallRcntInc(sf);
			break;
		case CR_RCNT_DEC:
			syscallRcntDec(sf);
			break;
		case CR_kernelFile_R:
			syscallkernelFileRead(sf);
			break;
		case CR_kernelFile_W:
			syscallkernelFileWrite(sf);
			break;
		case CR_WCNT_R:
			syscallWcntRead(sf);
			break;
		case CR_WCNT_INC:
			syscallWcntInc(sf);
			break;
		case CR_WCNT_DEC:
			syscallWcntDec(sf);
			break;
		default:
			break;
	}
}

void syscallRcntRead(struct StackFrame *sf){
	sf->eax = Rcnt;
}
void syscallRcntInc(struct StackFrame *sf){
	Rcnt++;
	sf->eax = Rcnt;
}
void syscallRcntDec(struct StackFrame *sf){
	Rcnt--;
	sf->eax = Rcnt;
}
void syscallkernelFileRead(struct StackFrame *sf){
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	asm volatile("movw %0, %%es"::"m"(sel));
	char character = '\0';
	int i = 0;
	do{
		character = kernelFile[i];
		asm volatile("movb %0,%%es:(%1) "::"r"(character),"r"(str+i));
		i++;
	}while(character);
	sf->eax = i;
}
void syscallkernelFileWrite(struct StackFrame *sf){
	int sel = sf->ds; // segment selector for user data, need further modification
	const char *str = (char*)sf->edx;
	asm volatile("movw %0, %%es"::"m"(sel));
	char character = '\0';
	int i = 0;
	do{
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		kernelFile[i] = character;
		i++;
	}while(character);
	sf->eax = i;
}
void syscallWcntRead(struct StackFrame *sf){
	sf->eax = Wcnt;
}
void syscallWcntInc(struct StackFrame *sf){
	Wcnt++;
	sf->eax = Wcnt;
}
void syscallWcntDec(struct StackFrame *sf){
	Wcnt--;
	sf->eax = Wcnt;
}