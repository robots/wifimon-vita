gcc bin2elf.c -o bin2elf
dd if=wlanbt_robin_img_ax.skprx.elf  of=8787.bin bs=304 skip=1
./bin2elf 8787.bin > 8787.elf

