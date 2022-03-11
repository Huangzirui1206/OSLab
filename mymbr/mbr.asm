
OS2022/mbr.bin：     文件格式 binary


Disassembly of section .data:

00000000 <.data>:
   0:	8c c8                	mov    %cs,%ax
   2:	8e d8                	mov    %ax,%ds
   4:	8e c0                	mov    %ax,%es
   6:	8e d0                	mov    %ax,%ss
   8:	b8 00 7d             	mov    $0x7d00,%ax
   b:	89 c4                	mov    %ax,%sp
   d:	6a 0d                	push   $0xd
   f:	68 17 7c             	push   $0x7c17
  12:	e8 12 00             	call   0x27
  15:	eb fe                	jmp    0x15
  17:	48                   	dec    %ax
  18:	65 6c                	gs insb (%dx),%es:(%di)
  1a:	6c                   	insb   (%dx),%es:(%di)
  1b:	6f                   	outsw  %ds:(%si),(%dx)
  1c:	2c 20                	sub    $0x20,%al
  1e:	57                   	push   %di
  1f:	6f                   	outsw  %ds:(%si),(%dx)
  20:	72 6c                	jb     0x8e
  22:	64 21 0a             	and    %cx,%fs:(%bp,%si)
  25:	00 00                	add    %al,(%bx,%si)
  27:	55                   	push   %bp
  28:	67 8b 44 24 04       	mov    0x4(%esp),%ax
  2d:	89 c5                	mov    %ax,%bp
  2f:	67 8b 4c 24 06       	mov    0x6(%esp),%cx
  34:	b8 01 13             	mov    $0x1301,%ax
  37:	bb 0c 00             	mov    $0xc,%bx
  3a:	ba 00 00             	mov    $0x0,%dx
  3d:	cd 10                	int    $0x10
  3f:	5d                   	pop    %bp
  40:	c3                   	ret    
	...
 1fd:	00 55 aa             	add    %dl,-0x56(%di)
