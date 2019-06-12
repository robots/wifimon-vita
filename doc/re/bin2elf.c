#include <arpa/inet.h>
#include <time.h>
#include <math.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <fcntl.h>
#include <ctype.h>
#include <elf.h>

static char *file_start;

#pragma pack(1)

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;


#if defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)

#define le32_to_cpu(x) (\
	(((x)>>24)&0xff)\
	|\
	(((x)>>8)&0xff00)\
	|\
	(((x)<<8)&0xff0000)\
	|\
	(((x)<<24)&0xff000000)\
)

#define le16_to_cpu(x) ( (((x)>>8)&0xff) | (((x)<<8)&0xff00) )

#else

#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)

#endif

#define cpu_to_le32(x) le32_to_cpu(x)
#define cpu_to_le16(x) le16_to_cpu(x)

//////////////////

static void hexdump(char *prefix, char *data, int size){
  if(size > 640)
  	size = 640;
  while(size){
    int amt = (size>16) ? 16 : size;
    char samp[20];
    char hex[80];
    samp[amt]='\0';
    hex[0] = '\0';
    memcpy(samp,data,amt);
    int i = 0;
    char *hp = hex;
    while(i<amt){
      snprintf(hp,42,"%02x ",(unsigned char)samp[i]);
      hp += 3;
      if((i&3)==3){
        hp[0]=' ';
        hp[1]='\0';
        hp++;
      }
      if(!isprint(samp[i]))
        samp[i]='.';
      i++;
    }
    fprintf(stderr,"%s%-52.52s<%s>\n",prefix,hex,samp);
    data += amt;
    size -= amt;
  }
}

/////////////////////////////////////

static unsigned crctable[256];

#define POLYNOMIAL 0x04C11DB7

static void init_crc(void){
	unsigned dividend = 0;
	do{
		unsigned remainder = dividend << 24;
		unsigned bit = 0;
		do{
			if(remainder & 0x80000000){
				remainder <<= 1;
				remainder ^= POLYNOMIAL;
			}else{
				remainder <<= 1;
			}
		}while(++bit<8);
		crctable[dividend++] = remainder;
	}while(dividend<256);
}

// for Marvell, pass 0 as the initial remainder
static unsigned do_crc(unsigned remainder, const unsigned char *p, unsigned n){
	unsigned i = 0;
	while(i < n){
		unsigned char data = *p ^ (remainder >> 24);
		remainder = crctable[data] ^ (remainder << 8);
		p++;
		i++;
	}
	return remainder;
}

///////////////////////////////////////

#define need(size,msg)({        \
	if(mapsize<(int)(size)){   \
		fprintf(stderr,"not enough bytes, have %d of %d for %s\n",(int)mapsize,(int)(size),msg);     \
		return;     \
	}                    \
})

////////////////////////////////////

static char elfdata[4096+(1024+128+128)*1024];

static unsigned entrypoint;

static unsigned upper0;
static unsigned upperC;
static unsigned upper04;
static unsigned lower0 = ~0u;
static unsigned lowerC = ~0u;
static unsigned lower04 = ~0u;

static unsigned shsz;
static char shstrtab[4096];

static int newstr(const char *restrict const s){
	int len = strlen(s)+1;
	memcpy(shstrtab+shsz,s,len);
	shsz += len;
	return shsz-len;
}

#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))

static int phi;
static int shi;
static struct{
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdr[2];
	Elf32_Shdr shdr[10];
	char shstrtab[10*8];
}e;

static void ss(const char *restrict const name, int w, int x, unsigned memaddr, unsigned memsize, unsigned fileaddr, const unsigned *restrict const filesize){
	unsigned u;

	if(name[0] && memsize && filesize){
		e.phdr[phi].p_type = cpu_to_le32(PT_LOAD);
		e.phdr[phi].p_offset = cpu_to_le32(fileaddr); // where in the file
		e.phdr[phi].p_vaddr = cpu_to_le32(memaddr);
		e.phdr[phi].p_paddr = cpu_to_le32(memaddr);
		e.phdr[phi].p_filesz = cpu_to_le32(filesize ? *filesize : 0);
		e.phdr[phi].p_memsz = cpu_to_le32(memsize);
		u = 0;
		if(w)
			u |= PF_R|PF_W;
		if(x)
			u |= PF_R|PF_X;
		e.phdr[phi].p_flags = cpu_to_le32(u);
		e.phdr[phi].p_align = cpu_to_le32(0);
		phi++;
	}

	u = newstr(name);
	e.shdr[shi].sh_name = cpu_to_le32(u);
	u = SHT_PROGBITS;
	if(!name[0]){
		u = SHT_NULL;
	} else if(!memsize){
		u = SHT_STRTAB;
		fileaddr = sizeof e - sizeof e.shstrtab;
	} else if(!filesize){
		u = SHT_NOBITS;
	}
	e.shdr[shi].sh_type = cpu_to_le32(u);
	u = 0;
	if(w)
		u |= SHF_ALLOC|SHF_WRITE;
	if(x)
		u |= SHF_ALLOC|SHF_EXECINSTR;
	e.shdr[shi].sh_flags = cpu_to_le32(u);
	e.shdr[shi].sh_addr = cpu_to_le32(memaddr);
	e.shdr[shi].sh_offset = cpu_to_le32(fileaddr);
	e.shdr[shi].sh_size = cpu_to_le32(filesize ? *filesize : memsize);
	e.shdr[shi].sh_link = cpu_to_le32(0);
	e.shdr[shi].sh_info = cpu_to_le32(0);
	e.shdr[shi].sh_addralign = cpu_to_le32(0);
	e.shdr[shi].sh_entsize = cpu_to_le32(0);
	shi++;
}

static void mkelf(void){
	if(sizeof e > 4096){
		fprintf(stderr, "shit, %d bytes\n", (int)(sizeof e));
		exit(45);
	}

	unsigned x0sz = upper0-lower0;
	unsigned xcsz = upperC-lowerC;
	unsigned x04sz = upper04-lower04;
	ss("",         0,0,0x00000000,0x00000,0x00000,NULL); // broken GNU crap expects this
	ss(".000",     0,1,lower0,    0x100000,0x01000,&x0sz);// 1st chunk of code
	if (lower04 > 0x04000000)
		ss(".04l",     1,0,lower04,   0x20000,0x121000,NULL); // boot code sets SP to 0x04002000
	unsigned gap04 = lower04 - 0x04000000u;
	ss(".040",     1,0,lower04,   0x20000-gap04,0x121000+gap04,&x04sz); // boot code sets SP to 0x04002000
	ss(".800",     1,0,0x80000000,0x10000,0x101000,NULL); // IO memory, probably including USB
	ss(".900",     1,0,0x90000000,0x0c000,0x101000,NULL); // IO memory
	if(lowerC>0xc0000000)
		ss(".clo",     1,0,0xc0000000,lowerC-0xc0000000,0x101000,NULL);
	unsigned cgap = lowerC - 0xc0000000u;
	ss(".c00",     1,1,lowerC,    0x20000-cgap,0x101000+cgap,&xcsz);// 2nd chunk of code, initialized data, uninitialized data, heap
	ss(".fff",     0,1,0xffff0000,0x0fff0,0x11000+cgap+xcsz,NULL); // there might be a ROM in here somewhere
	ss(".shstrtab",0,0,0x00000000,0x00000,42, &shsz); // must be last for e_shstrndx patchup below

	memcpy(&e.ehdr,"\177ELF", 4);
	e.ehdr.e_ident[4] = ELFCLASS32;
	e.ehdr.e_ident[5] = ELFDATA2LSB;
	e.ehdr.e_ident[6] = EV_CURRENT;
	e.ehdr.e_ident[7] = ELFOSABI_STANDALONE;
	e.ehdr.e_ident[8] = 0;
	e.ehdr.e_ident[9] = 0;
	e.ehdr.e_ident[10] = 0;
	e.ehdr.e_ident[11] = 0;
	e.ehdr.e_ident[12] = 0;
	e.ehdr.e_ident[13] = 0;
	e.ehdr.e_ident[14] = 0;
	e.ehdr.e_ident[15] = 0;
	e.ehdr.e_type = cpu_to_le16(ET_EXEC);
	e.ehdr.e_machine = cpu_to_le16(EM_ARM);
	e.ehdr.e_version = cpu_to_le32(EV_CURRENT);
	e.ehdr.e_entry = cpu_to_le32(entrypoint);
	e.ehdr.e_phoff = cpu_to_le32(sizeof e.ehdr); // offset to program header table
	e.ehdr.e_shoff = cpu_to_le32(sizeof e.ehdr + sizeof e.phdr); // offset to section header table
	e.ehdr.e_flags = cpu_to_le32(0);
	e.ehdr.e_ehsize = cpu_to_le16(sizeof e.ehdr);
	e.ehdr.e_phentsize = cpu_to_le16(sizeof e.phdr[0]);
	e.ehdr.e_phnum = cpu_to_le16(phi);
	e.ehdr.e_shentsize = cpu_to_le16(sizeof e.shdr[0]);
	e.ehdr.e_shnum = cpu_to_le16(shi);
	e.ehdr.e_shstrndx = cpu_to_le16(shi-1);

	memcpy(e.shstrtab, shstrtab, sizeof e.shstrtab);
	memcpy(elfdata, &e, sizeof e);
}

////////////////////////////////////

#define FW_HAS_DATA_TO_RECV		0x00000001
#define FW_HAS_LAST_BLOCK		0x00000004

struct fwheader {
	u32 dnldcmd;    // 1, except 4 for the 0-sized block at the end
	u32 baseaddr;   // usually 0x0000xxxx or 0xc00xxxxx. For FW_HAS_LAST_BLOCK, it's the entry point.
	u32 datalength; // The driver accepts 0 to 600. We see 512, plus short blocks at the end of a segment/section.
	u32 CRC;
};
// for sending to the chip, an extra u32 seq num goes here (before data)

///////////////////////////////////////

static void spew(char *map, ssize_t mapsize){
	for(;;){
		if(!mapsize){
			fprintf(stderr,"ran out at file offset %zd 0x%08zx\n",map-file_start,map-file_start);
			exit(19);
		}
		if(mapsize<0){
			fprintf(stderr,"INTERNAL ERROR NEGATIVE REMAINDER %zd 0x%08zx\n",map-file_start,map-file_start);
			exit(7);
		}

		struct fwheader fwheader;
		unsigned tmp;
		char *saved = map;

		need(sizeof fwheader, "fwheader");
		memcpy(&fwheader,map,sizeof fwheader);
		map += sizeof fwheader;
		mapsize -= sizeof fwheader;
		if((unsigned)mapsize>=le32_to_cpu(fwheader.datalength)){
			memcpy(&tmp,map+le32_to_cpu(fwheader.datalength)-4,4);
		}
		fprintf(stderr,
			"%08x  cmd:%08x addr:%08x len:%08x crc:%08x end:%08x\n",
			(unsigned)(saved-file_start),
			le32_to_cpu(fwheader.dnldcmd),
			le32_to_cpu(fwheader.baseaddr),
			le32_to_cpu(fwheader.datalength),
			le32_to_cpu(fwheader.CRC),
			le32_to_cpu(tmp)
		);
		if(do_crc(0,(unsigned char*)&fwheader,16)){
			fprintf(stderr,"bad header CRC\n");
			exit(91);
		}
		if(le32_to_cpu(fwheader.dnldcmd)==FW_HAS_LAST_BLOCK && fwheader.datalength==0){
			entrypoint = le32_to_cpu(fwheader.baseaddr);
			return;
		}
		if(le32_to_cpu(fwheader.dnldcmd)!=FW_HAS_DATA_TO_RECV || le32_to_cpu(fwheader.datalength) > 0x400){
			fprintf(stderr,"oh crap\n");
			hexdump("x ",saved,mapsize+sizeof fwheader);
		}
		need(le32_to_cpu(fwheader.datalength), "data");
		unsigned len = le32_to_cpu(fwheader.datalength) - 4;
		unsigned addr = le32_to_cpu(fwheader.baseaddr);
		unsigned past = (addr & 0x003fffff) + len;
/*		if(past>0x28000 || past<500) {
			fprintf(stderr, "tu\n");
			exit(89);
		}*/
		if(do_crc(0,(unsigned char*)map,len+4)){
			fprintf(stderr,"bad body CRC\n");
			exit(90);
		}
		switch(addr & ~0x003fffff){
		case 0xc0000000:
			{
				if(addr+len>upperC)
					upperC = addr+len;
				if(addr<lowerC)
					lowerC = addr;
				memcpy(elfdata+4096+1024*1024+(addr & 0x003fffff),map,len);
			}
			break;
		case 0x00000000:
			{
				if(addr+len>upper0)
					upper0 = addr+len;
				if(addr<lower0)
					lower0 = addr;
				memcpy(elfdata+4096+(addr & 0x003fffff),map,len);
			}
			break;
		case 0x04000000:
			{
				if(addr+len>upper04)
					upper04 = addr+len;
				if(addr<lower04)
					lower04 = addr;
				memcpy(elfdata+4096+(1024+128)*1024+(addr & 0x003fffff),map,len);
			hexdump("x ",saved,mapsize+sizeof fwheader);
			}
			break;
		default:
			fprintf(stderr,"le32_to_cpu(fwheader.baseaddr) is 0x%08x %x\n",le32_to_cpu(fwheader.baseaddr), addr);
			exit(9);
		}
//		hexdump("data ",map,le32_to_cpu(fwheader.datalength));
		map += le32_to_cpu(fwheader.datalength);
		mapsize -= le32_to_cpu(fwheader.datalength);

//				hexdump("junk ",map,mapsize);
	}
}

static void mapit(char *name){
  int fd = open(name, O_RDONLY);
  if(fd<3){
    perror("open");
    exit(88);
  }
  struct stat sbuf;
  fstat(fd,&sbuf);
  char *map;
  map = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  file_start = map;

  if(!map || map==(char*)-1){
  	fprintf(stderr,"%d is a bad size or mapping failed\n",(int)sbuf.st_size);
  	exit(3);
  }
  int mapsize = sbuf.st_size;
  fprintf(stderr,"mapped    %6d bytes\n",mapsize);
  spew(map,mapsize);
  mkelf();
  fwrite(elfdata,sizeof elfdata,1,stdout);
}

int main(int argc, char *argv[]){
  init_crc();
//  fprintf(stderr,"filename %s\n",argv[1]);
  mapit(argv[1]);
  return 0;
}
