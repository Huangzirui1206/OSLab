#This part should be the .text section in mbr.elf, but objdump takes it as .data, so the objdump result is wrong. However it also shows that .bin files, compared whith elf files, are simpler with infomation like program headers dropped. If possible, it's better to use hexdump to compare the two parts of .text in mbr.elf and mbr.bin. Actually, this's exactly the result od instruction: objcopy -S -j .text -O binary mbr.elf mbr.bin

mbr.bin：     文件格式 binary


Disassembly of section .data:

00000000 <.data>:
   0:	d88ec88c 	stmle	lr, {r2, r3, r7, fp, lr, pc}
   4:	d08ec08e 	addle	ip, lr, lr, lsl #1
   8:	897d00b8 	ldmdbhi	sp!, {r3, r4, r5, r7}^
   c:	680d6ac4 	stmdavs	sp, {r2, r6, r7, r9, fp, sp, lr}
  10:	12e87c17 	rscne	r7, r8, #5888	; 0x1700
  14:	48feeb00 	ldmmi	lr!, {r8, r9, fp, sp, lr, pc}^
  18:	6f6c6c65 	svcvs	0x006c6c65
  1c:	6f57202c 	svcvs	0x0057202c
  20:	21646c72 	smccs	18114	; 0x46c2
  24:	5500000a 	strpl	r0, [r0, #-10]
  28:	24448b67 	strbcs	r8, [r4], #-2919	; 0xfffff499
  2c:	67c58904 	strbvs	r8, [r5, r4, lsl #18]
  30:	06244c8b 	strteq	r4, [r4], -fp, lsl #25
  34:	bb1301b8 	bllt	0x4c071c
  38:	00ba000c 	adcseq	r0, sl, ip
  3c:	5d10cd00 	ldcpl	13, cr12, [r0, #-0]
  40:	000000c3 	andeq	r0, r0, r3, asr #1
	...
 1fc:	aa550000 	bge	0x1540204
