.macro incfirmware name
    .section  .rodata.\name
    .global \name
    .type \name, %object
    .balign 4
    \name:
        .incbin "../firmware_files/\name\().bin"
.endm

incfirmware drv0boot
incfirmware drv1drive
