# 原本是没有 makefile ，这里是自己写的，主要是用习惯了
NUWRITER=../NuWriter/bin/nuwriter
RAM_INI=./NuWriterIni/ram.ini
NAND_INI=/Users/yangxiyuan/Projects/rtt-projects/tools/NUC980_NuWriter_CMD/install/bin/nand.ini
GEN_PACK_INI=/Users/yangxiyuan/Projects/rtt-projects/tools/NUC980_NuWriter_CMD/install/bin/gen_pack.ini
BURN_PACK_INI=/Users/yangxiyuan/Projects/rtt-projects/tools/NUC980_NuWriter_CMD/install/bin/burn_pack.ini
ERASE_INI=/Users/yangxiyuan/Projects/rtt-projects/tools/NUC980_NuWriter_CMD/install/bin/erase.ini

VERSION_STRING=0.0.1
COMMENT="转换器开发"
APP_NAME=gw
DIST_NAME=rtthread.bin

all: 
	scons -j 8

ram: r0
	scons -j 8
	${NUWRITER} ${RAM_INI}

reset:
	# 加上 - 表示连接超时继续
	- curl --connect-timeout 0.1 -m 2 http://192.168.1.100/cgi-bin/reset
r0:
	- curl --connect-timeout 0.1 -m 2 http://192.168.1.140/cgi-bin/reset

build:
	scons -j 8

nand:reset erase
	${NUWRITER} ${NAND_INI}

erase:
	${NUWRITER} ${ERASE_INI}  

menuconfig:
	scons --menuconfig

upload:
	../tftp.sh 1.mp3
update:
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
	@rm -f ./rtthread_pack.bin
	@${NUWRITER} ${GEN_PACK_INI}	
bpack:
	${NUWRITER} ${BURN_PACK_INI}		

deploy:
	zsh ./deploy.zsh ${DIST_NAME}
clear:
	scons -c
clean:
	scons -c
