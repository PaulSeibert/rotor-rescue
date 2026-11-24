; ROTOR RESCUE - the per-picture BOB loop in 68000 assembly (objectsQueue in
; main.c; its C version is beside it, OBJ_BLITS_C). (c) 2026 Desert Palms Software.
;
; Every object the logic draws becomes a queued blit (agk/blitq.h): its
; template, shifted, at its place in the frame buffer, and an erase area for
; next time; after each, the next queued blit starts if the blitter is free.
; This runs in the 6-plane lines from slow RAM, where the bus is full (bitplanes
; 55%, the blitter 15%) and the C loop cost ~6 raster lines an object: here
; everything lives in registers, so it fetches about half the words.
;
; In:  a0 = tObjBlits (main.c, offsets below; checked there by _Static_assert)
; Out: its uwRoom, uwStarted, uwDrawn (bit 15: the queue was full)

FB_X0		equ	48		; (main.c: the frame buffer's left margin)
FB_ROW_BYTES	equ	162		; (main.c: 3 interleaved planes of 54 bytes)

; tObjBlits
OB_DRAW		equ	0		; const tDraw *: x.w, y.w, bob.b, frame.b (6 bytes)
OB_QUEUE	equ	4		; tAgkBlit *: the first free slot
OB_NEXT		equ	8		; tAgkBlit *: the next queued blit to start
OB_ERASE	equ	12		; tAgkBlitArea *: pD.l, size.w, mod.w
OB_FB		equ	16		; UBYTE *: the frame buffer's first plane, row 0
OB_INFO		equ	20		; tBobInfo[] (16 bytes): templates.l, max x.w, max y.w, slack.b
OB_N		equ	24		; UWORD: objects
OB_ROOM		equ	26		; UWORD: free queue slots (in), left (out)
OB_STARTED	equ	28		; UWORD out: blits started
OB_DRAWN	equ	30		; UWORD out: blits queued (bit 15: full)

; tAgkBlit (agk/blitq.h), 24 bytes
BL_CON0		equ	0
BL_CON1		equ	2
BL_ALWM		equ	4
BL_SIZE		equ	6
BL_A		equ	8
BL_B		equ	12
BL_CD		equ	16
BL_SRCMOD	equ	20
BL_DSTMOD	equ	22
BL_BYTES	equ	24

; custom chip registers
DMACONR		equ	$002
BLTCON0		equ	$040
BLTAFWM		equ	$044
BLTALWM		equ	$046
BLTCPT		equ	$048
BLTBPT		equ	$04C
BLTAPT		equ	$050
BLTDPT		equ	$054
BLTSIZE		equ	$058
BLTCMOD		equ	$060
BLTBMOD		equ	$062
BLTAMOD		equ	$064
BLTDMOD		equ	$066

		section	.text,code
		xdef	_objBlitsAsm

; a0 draw, a1 queue slot, a2 erase, a3 next to start, a4 BOB info, a5 custom
; d4 frame buffer, d5 free slots (high word: blits started), d6 queued, d7 count
_objBlitsAsm:
		movem.l	d2-d7/a2-a6,-(sp)
		move.l	a0,-(sp)
		moveq	#0,d5
		move.w	OB_ROOM(a0),d5
		moveq	#0,d6
		move.w	OB_N(a0),d7
		beq	.done
		subq.w	#1,d7
		move.l	OB_FB(a0),d4
		move.l	OB_QUEUE(a0),a1
		move.l	OB_ERASE(a0),a2
		move.l	OB_NEXT(a0),a3
		move.l	OB_INFO(a0),a4
		lea	$dff000,a5
		move.l	OB_DRAW(a0),a0
.loop:
		moveq	#0,d0
		move.b	4(a0),d0		; bob
		lsl.w	#4,d0			; its info
		move.w	(a0),d1
		add.w	#FB_X0,d1		; buffer x
		cmp.w	4(a4,d0.w),d1		; past max x, or x < 0 (unsigned): off
		bhi	.next
		move.w	2(a0),d2		; y
		cmp.w	6(a4,d0.w),d2
		bhi	.next
		move.l	(a4,d0.w),a6		; frame 0's two templates
		moveq	#0,d3
		move.b	5(a0),d3		; frame
		mulu.w	#2*BL_BYTES,d3
		adda.w	d3,a6
		moveq	#15,d3
		and.w	d1,d3			; the shift
		cmp.b	8(a4,d0.w),d3		; past the last word's slack: the wider one
		bls.s	.narrow
		lea	BL_BYTES(a6),a6
.narrow:
		move.l	(a6)+,(a1)		; BLTCON0, BLTCON1
		move.l	(a6)+,BL_ALWM(a1)	; ALWM, size
		move.l	(a6)+,BL_A(a1)
		move.l	(a6)+,BL_B(a1)
		move.l	4(a6),BL_SRCMOD(a1)	; source and destination modulos
		ror.w	#4,d3			; shift << 12
		or.w	d3,(a1)			; ASHIFT
		move.w	d3,BL_CON1(a1)		; BSHIFT
		mulu.w	#FB_ROW_BYTES,d2	; (< 64 KB: the buffer's rows)
		lsr.w	#4,d1
		add.w	d1,d1			; its word's byte
		add.w	d1,d2
		add.l	(a6),d2			; + the template's blank rows (main.c: bobsCrop)
		add.l	d4,d2			; the destination
		move.l	d2,BL_CD(a1)
		move.l	d2,(a2)+		; the erase next time: where, size, modulo
		move.w	BL_SIZE(a1),(a2)+
		move.w	BL_DSTMOD(a1),(a2)+
		addq.w	#1,d6
		tst.w	d5
		bne.s	.room
		bset	#15,d6			; full: the slot is written again (agkBlitqFull)
		bra.s	.poll
.room:
		subq.w	#1,d5
		lea	BL_BYTES(a1),a1
.poll:
		cmpa.l	a1,a3			; nothing queued to start
		bhs.s	.next
		tst.w	DMACONR(a5)		; (the first read after a start can lie on OCS)
		btst	#6,DMACONR(a5)		; blitter busy
		bne.s	.next
		move.l	(a3)+,BLTCON0(a5)	; BLTCON0, BLTCON1
		move.w	#$ffff,BLTAFWM(a5)
		move.w	(a3)+,BLTALWM(a5)
		move.w	(a3)+,d0		; (the size starts it: last)
		move.l	(a3)+,BLTAPT(a5)
		move.l	(a3)+,BLTBPT(a5)
		move.l	(a3),BLTCPT(a5)
		move.l	(a3)+,BLTDPT(a5)
		move.w	(a3),BLTAMOD(a5)
		move.w	(a3)+,BLTBMOD(a5)
		move.w	(a3),BLTCMOD(a5)
		move.w	(a3)+,BLTDMOD(a5)
		move.w	d0,BLTSIZE(a5)
		add.l	#$10000,d5		; one more started
.next:
		addq.l	#6,a0
		dbra	d7,.loop
.done:
		move.l	(sp)+,a0
		move.w	d5,OB_ROOM(a0)
		swap	d5
		move.w	d5,OB_STARTED(a0)
		move.w	d6,OB_DRAWN(a0)
		movem.l	(sp)+,d2-d7/a2-a6
		rts

; erasesMergeAsm: erase areas side by side on the same rows near the ground (the people at a
; barracks) become one clear of the box around them (main.c: erasesMerge, its C twin; read
; the reasons there). Entries are tAgkBlitArea: pD.l, size.w (rows << 6 | words), mod.w.
; In:  a0 = the entries, d0.w = how many, a1 = the floor: only areas at or below it merge
; Out: d0.w = how many are left
ER_BYTES	equ	24		; (main.c ERASE_MERGE_BYTES) the widest box
ER_ROWS		equ	(14*3)<<6	; (ERASE_MERGE_ROWS << 6) the tallest rows
PLANE_BYTES	equ	54		; (FB_PLANE_BYTES)

		xdef	_erasesMergeAsm
_erasesMergeAsm:
		cmp.w	#2,d0
		bcc.s	.go
		rts
.go:
		movem.l	d2-d6/a2-a3,-(sp)
		move.l	a0,a2			; the last kept entry
		lea	8(a0),a3		; the next to look at
		move.l	a1,d3			; the floor
		subq.w	#2,d0
		move.w	d0,d5			; n - 1 to look at (dbra)
.eloop:
		move.l	(a3),d1			; this one's start
		cmp.l	d3,d1
		blo.s	.ekeep
		move.l	(a2),d4			; the last kept one's start
		cmp.l	d3,d4
		blo.s	.ekeep
		move.w	4(a3),d2
		move.w	4(a2),d6
		eor.w	d2,d6
		and.w	#$ffc0,d6		; the same rows?
		bne.s	.ekeep
		move.w	d2,d0
		and.w	#$ffc0,d0		; rows << 6
		cmp.w	#ER_ROWS,d0
		bhi.s	.ekeep
		moveq	#63,d6
		and.w	d2,d6			; its words
		add.w	d6,d6
		add.l	d1,d6			; its end
		moveq	#63,d2
		and.w	4(a2),d2		; the last one's words
		add.w	d2,d2
		add.l	d4,d2			; its end
		cmp.l	d2,d6
		bhs.s	.e1
		move.l	d2,d6			; the later end
.e1:
		move.l	d4,d2
		cmp.l	d2,d1
		bhs.s	.s1
		move.l	d1,d2			; the earlier start
.s1:
		sub.l	d2,d6			; the box's bytes (a different row: a row's bytes more, too wide)
		cmp.l	#ER_BYTES,d6
		bhi.s	.ekeep
		move.l	d2,(a2)			; the last one grows to the box
		moveq	#PLANE_BYTES,d4
		sub.w	d6,d4
		move.w	d4,6(a2)		; modulo
		lsr.w	#1,d6
		or.w	d0,d6
		move.w	d6,4(a2)		; rows, words
		bra.s	.enext
.ekeep:
		addq.l	#8,a2
		move.l	(a3),(a2)
		move.l	4(a3),4(a2)
.enext:
		addq.l	#8,a3
		dbra	d5,.eloop
		move.l	a2,d0
		sub.l	a0,d0
		lsr.l	#3,d0
		addq.w	#1,d0
		movem.l	(sp)+,d2-d6/a2-a3
		rts
