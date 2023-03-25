.feature string_escapes
.org  $C700

.include "crust.inc"

; ProDOS 6.3.1 - ROM Code Conventions
.scope DriveHeader
	lda     #$20
	lda     #$00
	lda     #$03
	lda     #$3C
.endscope

Boot:
	ldx     #<WELCOME
:
	lda     $c700,x
	beq :+
	sta     $07D0-<WELCOME,y
	inx
	bne :-
:

	lda     #$08                                             ; Load block 0 at $800
	pha
	sta     ProDOS::bufferHi
	lda     #0
	pha
	sta     ProDOS::blockNoHi
	sta     ProDOS::blockNoLo
	sta     ProDOS::bufferLo

	lda     #1
	sta     ProDOS::command

.proc driver_Entry
	lda     ProDOS::command
	beq     driver_Status
	cmp     #ProDOS::DRIVE_CMD::READ
	beq     driver_Read
	bne     driver_Write
.endproc

.proc driver_Status
	; 6.3.1 - ROM Code Conventions
	; The STATUS call should perform a check to verify that the device is ready for a
	; READ or WRITE. If it is not, the carry should be set and the appropriate error code
	; returned in the accumulator. If the device is ready for a READ or WRITE, then the
	; driver should clear the carry, place a zero in the accumulator, and return the
	; number of blocks on the device in the X-register (low-byte) and Y-register (high-byte).
	clc
	lda     #$00
	ldx     #$ff
	ldy     #$ff
	rts
.endproc

.proc driver_Read
	lda     #CMD_WRITEDATA
    jsr     write_cmd
	lda     ProDOS::unitNo
    and     #$80
	jsr     write_data
	lda     ProDOS::blockNoLo
	jsr     write_data
	lda     ProDOS::blockNoHi
	jsr     write_data
	lda     #CMD_READ_VEDRIVE_BLOCK
    jsr     write_cmd

	ldx     #1
read_block:
	ldy     #0
read_page:
	jsr     read_data
	sta     (ProDOS::buffer),y
	iny
	bne     read_page
	inc     ProDOS::bufferHi
	dex
	beq     read_block
	dec     ProDOS::bufferHi

error:
	cpx     #1
	bne :+
	dec     ProDOS::bufferHi
:
	clc
	lda     #$00
	ldx     ProDOS::unitNo
	rts
.endproc

.proc driver_Write
	lda     #CMD_WRITEDATA
    jsr     write_cmd
	lda     ProDOS::unitNo
	and     #$80
	jsr     write_data
	lda     ProDOS::blockNoLo
	jsr     write_data
	lda     ProDOS::blockNoHi
	jsr     write_data

	ldx     #1
read_block:
	ldy     #0
read_page:
	lda     (ProDOS::buffer),y
	jsr     write_data
	iny
	bne     read_page
	inc     ProDOS::bufferHi
	dex
	beq     read_block
	dec     ProDOS::bufferHi

error:
	cpx     #1
	bne :+
	dec     ProDOS::bufferHi
:

    lda     #CMD_WRITE_VEDRIVE_BLOCK
	jsr     write_cmd

	clc
	lda     #$00
	ldx     ProDOS::unitNo
	rts
.endproc

.proc wait_io
:
	lda     $c0f2                                            ; IO_CMD,x
	bmi :-
    rts
.endproc

.proc write_cmd
	sta     $c0f2                                            ; IO_CMD,x
	jmp     wait_io
.endproc

.proc write_data
	sta     $c0f3                                            ; IO_DATA,x
	jmp     wait_io
.endproc

.proc read_data
	jsr     wait_io
	lda     $c0f3
    rts
.endproc

WELCOME:
	str "Pie Crust (c)2022    Yaroslav Veremenko"

.res $FF - 14 - <*, 0                   ; Pad until $FF leaving space for ProDOS drive footer
driver_EntryAddress:
	lda #$00
	sta $07f8
	lda $c0ff
	jmp $c800
.scope DriveFooter
	BLOCKS_STATUS_REQUIRED = 0

	STATUS_REMOVABLE = 1 << 7
	STATUS_INTERRUPTABLE = 1 << 6
	STATUS_FORMATTABLE = 1 << 3
	STATUS_WRITTABLE = 1 << 2
	STATUS_READABLE = 1 << 1
	STATUS_STATUS = 1 << 0
	.word BLOCKS_STATUS_REQUIRED          ; blocks
	.byte STATUS_WRITTABLE | STATUS_READABLE | STATUS_STATUS ; status
	.byte <driver_EntryAddress
.endscope
.assert * <> $100, error, "Boot ROM must be padded to 255 bytes"
