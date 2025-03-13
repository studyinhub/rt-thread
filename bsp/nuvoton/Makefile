# 原本是没有 makefile ，这里是自己写的，主要是用习惯了

makefile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_path :=  $(patsubst %/,%,$(dir $(makefile_path)))
current_dir := $(notdir $(current_path))
# ifconfig e0 192.168.2.200 192.168.2.1 255.255.255.0
# ip += $(if $(filter $(current_dir),gw),192.168.1.100,192.168.2.100 )
ip+=192.168.2.200
include ../Makefile_common

VERSION_STRING=0.0.2
COMMENT="PT980"
APP_NAME=$(current_dir)
DIST_NAME=rtthread.bin

all: auto_version
	scons -j 8 

test1: 
	@echo "hello" $(pack_file)

ram: reset
	scons -j 8 
	# nuwriter ${RAM_INI}
	nutool ram

reset:
	# 加上 - 表示连接超时继续
	- curl --connect-timeout 1 -m 2 http://$(ip)/cgi-bin/reset

build:
	scons -j 8

nand:reset erase
	${NUWRITER} ${NAND_INI}

menuconfig:
	scons --menuconfig

upload:
	../tftp.sh config.html
	
update:
	# source ~/.env/env.sh
	pkgs --update

auto_version:
	zsh auto_version.zsh ${APP_NAME} $(VERSION_STRING) $(COMMENT)

image:
	@echo "use space Not tab before ifneq and endif"
    ifneq ($(wildcard "images/uffs.bin"),)
		@rm ./images/uffs.bin
    endif
	./mkuffs -c -v --file ./images/uffs.bin --page-size 2048 --spare-size 64 --block-pages 64 --total-blocks 16 --ecc-option none --mount /,24,50 --exec script.ufs

gpack:
	# nuwriter $(GEN_PACK_INI)
	-nutool gpack
	echo "重命名" $(dateSuffix)
	@mv -f ./rtthread_pack.bin ./$(pack_file)

bpack: 
	${NUWRITER} ${BURN_PACK_INI}		

deploy:
	zsh ./deploy.zsh ${DIST_NAME}
clear:
	scons -c
