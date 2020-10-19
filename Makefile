# 
# 	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
#	Copyright 2013 Michael Hughes <squirmyworms@embarqmail.com>
#
#	This file is part of simavr.
#
#	simavr is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	simavr is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with simavr.  If not, see <http://www.gnu.org/licenses/>.

target=	FIGsimavr
#firm_src = ${wildcard at*${board}.c}
#firmware = ${firm_src:.c=.axf}
simavr = ../../

IPATH = .
IPATH += ./include
#VPATH += ./source
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ./source
VPATH += ../parts
VPATH += ../parts/dtime
VPATH += ../parts/spi_sram
VPATH += ../parts/spi_flash

LDFLAGS += -lpthread
LDFLAGS += -lSDL

CFLAGS += -Og -g

all: obj ${target}

include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

# board module parts
${board} : ${OBJ}/dtime.o
${board} : ${OBJ}/amic_a25l040_spi_flash.o
${board} : ${OBJ}/microchip_23k640_spi_sram.o

${board} : ${OBJ}/fig_init.o
${board} : ${OBJ}/fig_avr.o

${board} : ${OBJ}/fig_kbd.o
${board} : ${OBJ}/fig_spi.o

${board} : ${OBJ}/fig_sdl.o
${board} : ${OBJ}/fig_video.o

${board} : ${OBJ}/fig_main.o

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.hex *.a *.axf ${target} *.vcd .*.swo .*.swp .*.swm .*.swn

#******

run: ${OBJ}/${target}.elf
	${OBJ}/${target}.elf

run-out: ${OBJ}/${target}.elf
	${OBJ}/${target}.elf | tee run.out

cut-diff:
	dd if=run.out of=run.cut count=8192
	diff core_run.out run.cut > diff.out

diff:
	diff core_run.out run.out > diff.out
