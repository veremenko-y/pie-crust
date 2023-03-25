.feature string_escapes
.include "crust.inc"

knownRts := $ff58

.segment "CODE"
; ProDOS 6.3.1 - ROM Code Conventions
.scope DriveHeader
    lda     #$20
    lda     #$00
    lda     #$03
    lda     #$3C
.endscope
; ---------------------------------
ColdBoot:
    sei                      ; disable interrutps
    jsr     knownRts         ; acquire high byte of the card address (c0,c1...c7)
    tsx
    lda     $100,x
    sta     ProDOS::bufferHi ; store high byte of card address
    asl                      ; << 4 to get card slot number (00,10...70)
    asl
    asl
    asl
    sta     ProDOS::unitNo
    tax
    lda     $cfff            ; disable $c800 range
    lda     IO_RAMEXEC,x     ; enable $c800 range
    lda     #0               ; store low byte of card address
    sta     ProDOS::bufferLo

    ; Copy bootstrap loader into $c800 RAM on the card
    ldy     #<Loader
    ldx     #0
:
    lda     (ProDOS::buffer),y
    sta     LOADER_ADDR,x
    iny
    inx
    cpx     #LoaderSize
    bne :-
    ; Fixup IO addresses
    lda     ProDOS::unitNo
    sta     CardIoOffset
    lda     ProDOS::bufferHi
    sta     CardAddr
    clc
:
    lda     LOADER_ADDR,x
    cmp     #$ff
    beq :+
    tay
    lda     LOADER_ADDR,y
    adc     ProDOS::unitNo   ; add card number in high nibble (00,10...70)
    sta     LOADER_ADDR,y
    ; beq PANIC ; should never happen
    bcc :-                   ; force branch
:
    jmp     LOADER_ADDR

; WarmBoot:
;     lda     ProDOS::unitNo
;     pha
;     lda     ProDOS::bufferLo
;     pha
;     lda     ProDOS::bufferHi
; ---------------------------
; Firmware loader
FIRMWARE_BUFFER_ADDR := $cd00
RELOC_TABLE_BUFFER_ADDR := $ce00
LOADER_ADDR := $cf00
.org LOADER_ADDR
RELOC_IO_IDX .set 0
.macro RELOC_IO op
    op
    .ident(.sprintf ("RELOC_IO_%d", RELOC_IO_IDX)):
    RELOC_IO_IDX .set (RELOC_IO_IDX + 1)
.endmacro
RELOC_CARD_ADDR_IDX .set 0
.macro RELOC_CARD_ADDR op
    op
    .ident(.sprintf ("RELOC_CARD_ADDR_%d", RELOC_CARD_ADDR_IDX)):
    RELOC_CARD_ADDR_IDX .set (RELOC_CARD_ADDR_IDX + 1)
.endmacro
Loader:
CardIoOffset: .res 1
CardAddr: .res 1
FirmwareDriver: .byte 1
    jmp     WriteRAMEXEC
    jmp     ReadCMD
    jmp     WriteCMD
    jmp     ReadDATA
    jmp     WriteDATA
LoaderStart:
    lda     #0               ; disable ram exec, enable ram write
    jsr     WriteRAMEXEC
    lda     #CMD_WRITEDATA   ; open write into card
    jsr     WriteCMD
    lda     FirmwareDriver   ; write firmware number into card
    jsr     WriteDATA
    ldx     #0               ; copy 255 bytes of firmware into the car space
LoadFirmware:
    jsr     ReadDATA
    sta     FIRMWARE_BUFFER_ADDR,x
    inx
    bne     LoadFirmware

LoadRelocTable:
    jsr     ReadDATA
    sta     RELOC_TABLE_BUFFER_ADDR,x
    inx
    bne     LoadRelocTable

    ldx     #0
RelocateIOAddr:
    lda     RELOC_TABLE_BUFFER_ADDR,x
    cmp     #$ff
    beq :+
    tay
    lda     FIRMWARE_BUFFER_ADDR,y
    clc
    adc     CardIoOffset
    sta     FIRMWARE_BUFFER_ADDR,y
    inx
    bne     RelocateIOAddr
:

RelocateCardAddr:
    lda     RELOC_TABLE_BUFFER_ADDR,x
    cmp     #$ff
    beq :+
    tay
    lda     CardAddr
    sta     FIRMWARE_BUFFER_ADDR,y
    inx
    bne     RelocateIOAddr
:

    ldx     #0
CopyFirmware:
    lda     FIRMWARE_BUFFER_ADDR,x
    RELOC_CARD_ADDR { sta $ff00 }
    inx
    bne     CopyFirmware
Run:
    RELOC_CARD_ADDR { jmp $ff00 }
; -------------------------------
; IO endpoints
.macro WAIT_COMPLETE
:
    RELOC_IO { lda IO_CMD }
    bmi :-
.endmacro
WriteRAMEXEC:
    RELOC_IO { sta IO_RAMEXEC }
    rts
ReadCMD:
    RELOC_IO { lda IO_CMD }
    rts
WriteCMD:
    RELOC_IO { sta IO_CMD }
    WAIT_COMPLETE
    rts
ReadDATA:
    WAIT_COMPLETE
    RELOC_IO { lda IO_DATA }
    rts
WriteDATA:
    RELOC_IO { sta IO_DATA }
    WAIT_COMPLETE
    rts
LoaderSize := * - Loader
; -----------------------------
; Reloc table
RELOC_IO_TABLE:
.repeat RELOC_IO_IDX,I
    ; -2 offset to replace low byte of the address
    ; e.g. LDA $ABCD => A9 >>CD<< AB
    .byte <(.ident(.sprintf ("RELOC_IO_%d", I))-2)
.endrepeat
    .byte $FF
RELOC_CARD_ADDR_TABLE:
.repeat RELOC_CARD_ADDR_IDX,I
    ; -1 offset to replace high byte of the address
    .byte <(.ident(.sprintf ("RELOC_CARD_ADDR_%d", I))-2)
.endrepeat
    .byte $FF

.reloc
.segment "FOOTER"
.res 8
driver_EntryAddress:
    sec
    lda     #ProDOS::IO_ERR::NO_DEVICE_CONNECTED
    rts
.scope DriveFooter
    BLOCKS_STATUS_REQUIRED = 0

    STATUS_REMOVABLE = 1 << 7
    STATUS_INTERRUPTABLE = 1 << 6
    STATUS_FORMATTABLE = 1 << 3
    STATUS_WRITTABLE = 1 << 2
    STATUS_READABLE = 1 << 1
    STATUS_STATUS = 1 << 0
    .word BLOCKS_STATUS_REQUIRED
    .byte STATUS_WRITTABLE | STATUS_READABLE | STATUS_STATUS
    .byte <driver_EntryAddress
.endscope

.segment "RELOC_TABLE"
