# 原本是没有 makefile ，这里是自己写的，主要是用习惯了
NUWRITER=/Users/yangxiyuan/Projects/rtt-projects/tools/NUC980_NuWriter_CMD/install/bin/nuwriter
RAM_INI=./install/bin/ram.ini
NAND_INI=./install/bin/nand.ini
GEN_PACK_INI=./install/bin/gen_pack.ini
BURN_PACK_INI=./install/bin/burn_pack.ini
ERASE_INI=./install/bin/erase.ini

VERSION_STRING=0.0.1
COMMENT="转换器开发"
APP_NAME=gw
DIST_NAME=rtthread.bin

all: auto_version
	scons -j 8

menuconfig:
	scons --menuconfig

update:
	pkgs --update

auto_version:
	zsh auto_version.zsh ${APP_NAME} $(VERSION_STRING) $(COMMENT)
ram:
	scons -j 8
	# 加上 - 表示连接超时继续
	- curl --connect-timeout 1 -m 2 http://192.168.1.100/cgi-bin/reset
	${NUWRITER} ${RAM_INI}
image:
	@echo "use space Not tab before ifneq and endif"
    ifneq ($(wildcard "images/uffs.bin"),)
		@rm ./images/uffs.bin
    endif
	./mkuffs -c -v --file ./images/uffs.bin --page-size 2048 --spare-size 64 --block-pages 64 --total-blocks 16 --ecc-option none --mount /,24,50 --exec script.ufs
nand:
	${NUWRITER} ${NAND_INI}
gpack:
	@rm -f ./rtthread_pack.bin
	@${NUWRITER} ${GEN_PACK_INI}	
bpack:
	${NUWRITER} ${BURN_PACK_INI}		
erase:
	${NUWRITER} ${ERASE_INI}  
deploy:
	zsh ./deploy.zsh ${DIST_NAME}