
; -----------------------------------------------------------------------------
; Apple II Monitor Routines
; =============================================================================

BASICIN := $C305 ; With 80-column firmware active, displays solid, blinking cursor; accepts character from keyboard
BASICOUT := $C307 ; Displays a character on the screen; used when the 80-column firmware is active (Chapter 3)
CLREOL := $FC9C ; Clears to end of line from current cursor position
CLEOLZ := $FC9E ; Clears to end of line using contents of Y register as cursor position
CLREOP := $FC42 ; Clears to bottom of window
CLRSCR := $F832 ; Clears the low-resolution screen
CLRTOP := $F836 ; Clears top 40 lines of low-resolution screen
COUT := $FDED ; Calls output routine whose address is stored in CSW (normally COUT1, Chapter 3)
COUT1 := $FDF0 ; Displays a character on the screen (Chapter 3)
CROUT := $FD8E ; Generates a carriage return character
CROUT1 := $FD8B ; Clears to end of line, then generates a carriage return character
GETLN := $FD6A ; Displays the prompt character; accepts a string of characters by means of RDKEY
HLINE := $F819 ; Draws a horizontal line of blocks
HOME := $FC58 ; Clears the window and puts cursor in upper-left corner of window
KEYIN := $FD1B ; With 80-column firmware inactive, displays checkerboard cursor; accepts character from keyboard
PLOT := $F800 ; Plots a single low-resolution block on the screen
PRBL2 := $F94A ; Sends 1 to 256 blank spaces to the output device
PRBYTE := $FDDA ; Prints a hexadecimal byte
PRERR := $FF2D ; Sends ERR and Control-G to the output device
PRHEX := $FDE3 ; Prints 4 bits as a hexadecimal number
PRNTAX := $F941 ; Prints contents of A and X in hexadecimal
RDKEY := $FD0C ; Displays blinking cursor; goes to standard input routine, normally KEYIN or BASICIN
SCRN := $F871 ; Reads color value of a low-resolution block
SETCOL := $F864 ; Sets the color for plotting in low resolution
VTABZ := $FC24 ; Sets cursor vertical position
VLINE := $F828 ; Draws a vertical line of low-resolution blocks

SOFTEV := $03F2   ; Vector for warm start
; -----------------------------------------------------------------------------


; -----------------------------------------------------------------------------
; ProDOS
; =============================================================================



; -----------------------------------------------------------------------------


IO_RAMEXEC = $C080
IO_CMD = $C082
IO_DATA = $C083

CMD_WRITEDATA = 0
CMD_WIFI = 1
CMD_READ_VEDRIVE_BLOCK = 2
CMD_WRITE_VEDRIVE_BLOCK = 3
CMD_GET_FIRMWARE = $FE
CMD_DEBUG = $FF

FIRMWARE_BOOT = 0
FIRMWARE_DRIVER = 1

STATUS_BUSY = (1 << 7)
STATUS_DATAREADY = (1 << 6)
STATUS_WIFICONNECTED = (1 << 5)
STATUS_ERROR = (1 << 4)

.scope ProDOS
	; ProDOS 6.3.2 - Call Parameters
	command := $42
	unitNo := $43
	buffer := $44
		bufferLo := buffer
		bufferHi := (buffer + 1)
	blockNo := $46
		blockNoLo := blockNo
		blockNoHi := (blockNo + 1)
	.scope IO_ERR
		; ProDOS 6.3.2 - Call Parameters
		IO_ERROR = $27
		NO_DEVICE_CONNECTED = $28
		WRITE_PROTECTED = $2B
	.endscope
	.scope DRIVE_CMD
		STATUS = 0
		READ = 1
		WRITE = 2
		FORMAT = 3
	.endscope
.endscope

.macro str str
	.repeat .strlen (str), c
	.byte .strat (str, c) | $80
	.endrep
	.byte 0
.endmacro

