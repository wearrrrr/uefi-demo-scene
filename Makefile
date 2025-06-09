TARGET = BOOTX64.EFI

# leave the hard work and all the rest to posix-uefi

# set this if you want GNU gcc + ld + objcopy instead of LLVM Clang + Lld
#USE_GCC = 1
include uefi/Makefile

all:
	cp ./BOOTX64.EFI ../UEFI-GPT-image-creator/
	cp okayu.bmp ../UEFI-GPT-image-creator/

	cd ../UEFI-GPT-image-creator/ && ./write_gpt -ae /EFI/BOOT/ ./okayu.bmp && ./qemu.sh
