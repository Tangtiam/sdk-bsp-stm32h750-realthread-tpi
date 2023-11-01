/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2018/08/29     Bernard         first version
 * 2021/04/23     chunyexixiaoyu  distinguish 32-bit and 64-bit
 */

#ifndef DL_ELF_H__
#define DL_ELF_H__

typedef rt_uint8_t              Elf_Byte;

typedef rt_uint32_t             Elf32_Addr;    /* Unsigned program address */
typedef rt_uint32_t             Elf32_Off;     /* Unsigned file offset */
typedef rt_int32_t              Elf32_Sword;   /* Signed large integer */
typedef rt_uint32_t             Elf32_Word;    /* Unsigned large integer */
typedef rt_uint16_t             Elf32_Half;    /* Unsigned medium integer */

typedef rt_uint64_t             Elf64_Addr;
typedef rt_uint16_t             Elf64_Half;
typedef rt_int16_t              Elf64_SHalf;
typedef rt_uint64_t             Elf64_Off;
typedef rt_int32_t              Elf64_Sword;
typedef rt_uint32_t             Elf64_Word;
typedef rt_uint64_t             Elf64_Xword;
typedef rt_int64_t              Elf64_Sxword;
typedef rt_uint16_t             Elf64_Section;

/* Fields in the e_ident array.  The EI_* macros are indices into the
   array.  The macros under each EI_* macro are the values the byte
   may have.  */

#define EI_MAG0     0       /* File identification byte 0 index */
#define ELFMAG0     0x7f        /* Magic number byte 0 */

#define EI_MAG1     1       /* File identification byte 1 index */
#define ELFMAG1     'E'     /* Magic number byte 1 */

#define EI_MAG2     2       /* File identification byte 2 index */
#define ELFMAG2     'L'     /* Magic number byte 2 */

#define EI_MAG3     3       /* File identification byte 3 index */
#define ELFMAG3     'F'     /* Magic number byte 3 */

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define ELFMAG      "\177ELF"
#define RTMMAG      "\177RTM" /* magic */
#define SELFMAG     4

#define EI_CLASS    4       /* File class byte index */
#define ELFCLASSNONE    0       /* Invalid class */
#define ELFCLASS32  1       /* 32-bit objects */
#define ELFCLASS64  2       /* 64-bit objects */
#define ELFCLASSNUM 3

#define EI_DATA     5       /* Data encoding byte index */
#define ELFDATANONE 0       /* Invalid data encoding */
#define ELFDATA2LSB 1       /* 2's complement, little endian */
#define ELFDATA2MSB 2       /* 2's complement, big endian */
#define ELFDATANUM  3

#define EI_VERSION  6       /* File version byte index */
                    /* Value must be EV_CURRENT */

#define EI_OSABI    7       /* OS ABI identification */
#define ELFOSABI_NONE       0   /* UNIX System V ABI */
#define ELFOSABI_SYSV       0   /* Alias.  */
#define ELFOSABI_HPUX       1   /* HP-UX */
#define ELFOSABI_NETBSD     2   /* NetBSD.  */
#define ELFOSABI_GNU        3   /* Object uses GNU ELF extensions.  */
#define ELFOSABI_LINUX      ELFOSABI_GNU /* Compatibility alias.  */
#define ELFOSABI_SOLARIS    6   /* Sun Solaris.  */
#define ELFOSABI_AIX        7   /* IBM AIX.  */
#define ELFOSABI_IRIX       8   /* SGI Irix.  */
#define ELFOSABI_FREEBSD    9   /* FreeBSD.  */
#define ELFOSABI_TRU64      10  /* Compaq TRU64 UNIX.  */
#define ELFOSABI_MODESTO    11  /* Novell Modesto.  */
#define ELFOSABI_OPENBSD    12  /* OpenBSD.  */
#define ELFOSABI_ARM_AEABI  64  /* ARM EABI */
#define ELFOSABI_ARM        97  /* ARM */
#define ELFOSABI_STANDALONE 255 /* Standalone (embedded) application */

#define EI_ABIVERSION   8       /* ABI version */

#define EI_PAD      9       /* Byte index of padding bytes */

/* Legal values for e_type (object file type).  */

#define ET_NONE     0       /* No file type */
#define ET_REL      1       /* Relocatable file */
#define ET_EXEC     2       /* Executable file */
#define ET_DYN      3       /* Shared object file */
#define ET_CORE     4       /* Core file */
#define ET_NUM      5       /* Number of defined types */
#define ET_LOOS     0xfe00      /* OS-specific range start */
#define ET_HIOS     0xfeff      /* OS-specific range end */
#define ET_LOPROC   0xff00      /* Processor-specific range start */
#define ET_HIPROC   0xffff      /* Processor-specific range end */

/* Legal values for e_machine (architecture).  */

#define EM_NONE      0      /* No machine */
#define EM_M32       1      /* AT&T WE 32100 */
#define EM_SPARC     2      /* SUN SPARC */
#define EM_386       3      /* Intel 80386 */
#define EM_68K       4      /* Motorola m68k family */
#define EM_88K       5      /* Motorola m88k family */
#define EM_860       7      /* Intel 80860 */
#define EM_MIPS      8      /* MIPS R3000 big-endian */
#define EM_S370      9      /* IBM System/370 */
#define EM_MIPS_RS3_LE  10      /* MIPS R3000 little-endian */

#define EM_PARISC   15      /* HPPA */
#define EM_VPP500   17      /* Fujitsu VPP500 */
#define EM_SPARC32PLUS  18      /* Sun's "v8plus" */
#define EM_960      19      /* Intel 80960 */
#define EM_PPC      20      /* PowerPC */
#define EM_PPC64    21      /* PowerPC 64-bit */
#define EM_S390     22      /* IBM S390 */

#define EM_V800     36      /* NEC V800 series */
#define EM_FR20     37      /* Fujitsu FR20 */
#define EM_RH32     38      /* TRW RH-32 */
#define EM_RCE      39      /* Motorola RCE */
#define EM_ARM      40      /* ARM */
#define EM_FAKE_ALPHA   41      /* Digital Alpha */
#define EM_SH       42      /* Hitachi SH */
#define EM_SPARCV9  43      /* SPARC v9 64-bit */
#define EM_TRICORE  44      /* Siemens Tricore */
#define EM_ARC      45      /* Argonaut RISC Core */
#define EM_H8_300   46      /* Hitachi H8/300 */
#define EM_H8_300H  47      /* Hitachi H8/300H */
#define EM_H8S      48      /* Hitachi H8S */
#define EM_H8_500   49      /* Hitachi H8/500 */
#define EM_IA_64    50      /* Intel Merced */
#define EM_MIPS_X   51      /* Stanford MIPS-X */
#define EM_COLDFIRE 52      /* Motorola Coldfire */
#define EM_68HC12   53      /* Motorola M68HC12 */
#define EM_MMA      54      /* Fujitsu MMA Multimedia Accelerator*/
#define EM_PCP      55      /* Siemens PCP */
#define EM_NCPU     56      /* Sony nCPU embeeded RISC */
#define EM_NDR1     57      /* Denso NDR1 microprocessor */
#define EM_STARCORE 58      /* Motorola Start*Core processor */
#define EM_ME16     59      /* Toyota ME16 processor */
#define EM_ST100    60      /* STMicroelectronic ST100 processor */
#define EM_TINYJ    61      /* Advanced Logic Corp. Tinyj emb.fam*/
#define EM_X86_64   62      /* AMD x86-64 architecture */
#define EM_PDSP     63      /* Sony DSP Processor */

#define EM_FX66     66      /* Siemens FX66 microcontroller */
#define EM_ST9PLUS  67      /* STMicroelectronics ST9+ 8/16 mc */
#define EM_ST7      68      /* STmicroelectronics ST7 8 bit mc */
#define EM_68HC16   69      /* Motorola MC68HC16 microcontroller */
#define EM_68HC11   70      /* Motorola MC68HC11 microcontroller */
#define EM_68HC08   71      /* Motorola MC68HC08 microcontroller */
#define EM_68HC05   72      /* Motorola MC68HC05 microcontroller */
#define EM_SVX      73      /* Silicon Graphics SVx */
#define EM_ST19     74      /* STMicroelectronics ST19 8 bit mc */
#define EM_VAX      75      /* Digital VAX */
#define EM_CRIS     76      /* Axis Communications 32-bit embedded processor */
#define EM_JAVELIN  77      /* Infineon Technologies 32-bit embedded processor */
#define EM_FIREPATH 78      /* Element 14 64-bit DSP Processor */
#define EM_ZSP      79      /* LSI Logic 16-bit DSP Processor */
#define EM_MMIX     80      /* Donald Knuth's educational 64-bit processor */
#define EM_HUANY    81      /* Harvard University machine-independent object files */
#define EM_PRISM    82      /* SiTera Prism */
#define EM_AVR      83      /* Atmel AVR 8-bit microcontroller */
#define EM_FR30     84      /* Fujitsu FR30 */
#define EM_D10V     85      /* Mitsubishi D10V */
#define EM_D30V     86      /* Mitsubishi D30V */
#define EM_V850     87      /* NEC v850 */
#define EM_M32R     88      /* Mitsubishi M32R */
#define EM_MN10300  89      /* Matsushita MN10300 */
#define EM_MN10200  90      /* Matsushita MN10200 */
#define EM_PJ       91      /* picoJava */
#define EM_OPENRISC 92      /* OpenRISC 32-bit embedded processor */
#define EM_ARC_A5   93      /* ARC Cores Tangent-A5 */
#define EM_XTENSA   94      /* Tensilica Xtensa Architecture */
#define EM_ALTERA_NIOS2 113     /* Altera Nios II */
#define EM_AARCH64  183     /* ARM AARCH64 */
#define EM_TILEPRO  188     /* Tilera TILEPro */
#define EM_MICROBLAZE   189     /* Xilinx MicroBlaze */
#define EM_TILEGX   191     /* Tilera TILE-Gx */
#define EM_NUM      192

/* If it is necessary to assign new unofficial EM_* values, please
   pick large random numbers (0x8523, 0xa7f2, etc.) to minimize the
   chances of collision with official or non-GNU unofficial values.  */

#define EM_ALPHA    0x9026

/* Legal values for e_version (version).  */

#define EV_NONE     0       /* Invalid ELF version */
#define EV_CURRENT  1       /* Current version */
#define EV_NUM      2


#ifdef ARCH_CPU_64BIT
#define elfhdr      Elf64_Ehdr
#define elf_phdr    Elf64_Phdr
#define elf_shdr    Elf64_Shdr
#define elf_note    elf64_note
#define elf_addr_t  Elf64_Off
#define Elf_Word    Elf64_Word
#define Elf_Addr    Elf64_Addr
#define Elf_Half    Elf64_Half
#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Phdr    Elf64_Phdr
#define Elf_Shdr    Elf64_Shdr
#else
#define elfhdr      Elf32_Ehdr
#define elf_phdr    Elf32_Phdr
#define elf_shdr    Elf32_Shdr
#define elf_note    elf32_note
#define elf_addr_t  Elf32_Off
#define Elf_Word    Elf32_Word
#define Elf_Addr    Elf32_Addr
#define Elf_Half    Elf32_Half
#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Phdr    Elf32_Phdr
#define Elf_Shdr    Elf32_Shdr
#endif

#define EI_NIDENT (16)

/* ELF Header */
typedef struct
{
    unsigned char e_ident[EI_NIDENT];          /* ELF Identification */
    Elf32_Half    e_type;                      /* object file type */
    Elf32_Half    e_machine;                   /* machine */
    Elf32_Word    e_version;                   /* object file version */
    Elf32_Addr    e_entry;                     /* virtual entry point */
    Elf32_Off     e_phoff;                     /* program header table offset */
    Elf32_Off     e_shoff;                     /* section header table offset */
    Elf32_Word    e_flags;                     /* processor-specific flags */
    Elf32_Half    e_ehsize;                    /* ELF header size */
    Elf32_Half    e_phentsize;                 /* program header entry size */
    Elf32_Half    e_phnum;                     /* number of program header entries */
    Elf32_Half    e_shentsize;                 /* section header entry size */
    Elf32_Half    e_shnum;                     /* number of section header entries */
    Elf32_Half    e_shstrndx;                  /* section header table's "section
                                                  header string table" entry offset */
} Elf32_Ehdr;

typedef struct
{
    unsigned char     e_ident[EI_NIDENT];        /* ELF Identification */
    Elf64_Half        e_type;                    /* object file type */
    Elf64_Half        e_machine;                 /* machine */
    Elf64_Word        e_version;                 /* object file version */
    Elf64_Addr        e_entry;                   /* virtual entry point */
    Elf64_Off         e_phoff;                   /* program header table offset */
    Elf64_Off         e_shoff;                   /* section header table offset */
    Elf64_Word        e_flags;                   /* processor-specific flags */
    Elf64_Half        e_ehsize;                  /* ELF header size */
    Elf64_Half        e_phentsize;               /* program header entry size */
    Elf64_Half        e_phnum;                   /* number of program header entries */
    Elf64_Half        e_shentsize;               /* section header entry size */
    Elf64_Half        e_shnum;                   /* number of section header entries */
    Elf64_Half        e_shstrndx;                /* section header table's "section
                                                header string table" entry offset */
} Elf64_Ehdr;

/* Section Header */
typedef struct
{
    Elf32_Word sh_name;                        /* name - index into section header
                                                  string table section */
    Elf32_Word sh_type;                        /* type */
    Elf32_Word sh_flags;                       /* flags */
    Elf32_Addr sh_addr;                        /* address */
    Elf32_Off  sh_offset;                      /* file offset */
    Elf32_Word sh_size;                        /* section size */
    Elf32_Word sh_link;                        /* section header table index link */
    Elf32_Word sh_info;                        /* extra information */
    Elf32_Word sh_addralign;                   /* address alignment */
    Elf32_Word sh_entsize;                     /* section entry size */
} Elf32_Shdr;

typedef struct
{
    Elf64_Word        sh_name;                   /* Section name (string tbl index) */
    Elf64_Word        sh_type;                   /* Section type */
    Elf64_Xword       sh_flags;                  /* Section flags */
    Elf64_Addr        sh_addr;                   /* Section virtual addr at execution */
    Elf64_Off         sh_offset;                 /* Section file offset */
    Elf64_Xword       sh_size;                   /* Section size in bytes */
    Elf64_Word        sh_link;                   /* Link to another section */
    Elf64_Word        sh_info;                   /* Additional section information */
    Elf64_Xword       sh_addralign;              /* Section alignment */
    Elf64_Xword       sh_entsize;                /* Entry size if section holds table */
} Elf64_Shdr;

/* Section names */
#define ELF_BSS                 ".bss"         /* uninitialized data */
#define ELF_DATA                ".data"        /* initialized data */
#define ELF_DEBUG               ".debug"       /* debug */
#define ELF_DYNAMIC             ".dynamic"     /* dynamic linking information */
#define ELF_DYNSTR              ".dynstr"      /* dynamic string table */
#define ELF_DYNSYM              ".dynsym"      /* dynamic symbol table */
#define ELF_FINI                ".fini"        /* termination code */
#define ELF_GOT                 ".got"         /* global offset table */
#define ELF_HASH                ".hash"        /* symbol hash table */
#define ELF_INIT                ".init"        /* initialization code */
#define ELF_REL_DATA            ".rel.data"    /* relocation data */
#define ELF_REL_FINI            ".rel.fini"    /* relocation termination code */
#define ELF_REL_INIT            ".rel.init"    /* relocation initialization code */
#define ELF_REL_DYN             ".rel.dyn"     /* relocaltion dynamic link info */
#define ELF_REL_RODATA          ".rel.rodata"  /* relocation read-only data */
#define ELF_REL_TEXT            ".rel.text"    /* relocation code */
#define ELF_RODATA              ".rodata"      /* read-only data */
#define ELF_SHSTRTAB            ".shstrtab"    /* section header string table */
#define ELF_STRTAB              ".strtab"      /* string table */
#define ELF_SYMTAB              ".symtab"      /* symbol table */
#define ELF_TEXT                ".text"        /* code */
#define ELF_RTMSYMTAB           "RTMSymTab"

/* Symbol Table Entry */
typedef struct elf32_sym
{
    Elf32_Word    st_name;                     /* name - index into string table */
    Elf32_Addr    st_value;                    /* symbol value */
    Elf32_Word    st_size;                     /* symbol size */
    unsigned char st_info;                     /* type and binding */
    unsigned char st_other;                    /* 0 - no defined meaning */
    Elf32_Half    st_shndx;                    /* section header index */
} Elf32_Sym;

typedef struct
{
    Elf64_Word        st_name;                   /* Symbol name (string tbl index) */
    unsigned char     st_info;                   /* Symbol type and binding */
    unsigned char     st_other;                  /* Symbol visibility */
    Elf64_Section     st_shndx;                  /* Section index */
    Elf64_Addr        st_value;                  /* Symbol value */
    Elf64_Xword       st_size;                   /* Symbol size */
} Elf64_Sym;

#define STB_LOCAL               0              /* BIND */
#define STB_GLOBAL              1
#define STB_WEAK                2
#define STB_NUM                 3

#define STB_LOPROC              13             /* processor specific range */
#define STB_HIPROC              15

#define STT_NOTYPE              0              /* symbol type is unspecified */
#define STT_OBJECT              1              /* data object */
#define STT_FUNC                2              /* code object */
#define STT_SECTION             3              /* symbol identifies an ELF section */
#define STT_FILE                4              /* symbol's name is file name */
#define STT_COMMON              5              /* common data object */
#define STT_TLS                 6              /* thread-local data object */
#define STT_NUM                 7              /* # defined types in generic range */
#define STT_LOOS                10             /* OS specific range */
#define STT_HIOS                12
#define STT_LOPROC              13             /* processor specific range */
#define STT_HIPROC              15

#define STN_UNDEF               0              /* undefined */

#define ELF_ST_BIND(info)       ((info) >> 4)
#define ELF_ST_TYPE(info)       ((info) & 0xf)
#define ELF_ST_INFO(bind, type) (((bind)<<4)+((type)&0xf))

/* Relocation entry with implicit addend */
typedef struct
{
    Elf32_Addr r_offset;                       /* offset of relocation */
    Elf32_Word r_info;                         /* symbol table index and type */
} Elf32_Rel;

typedef struct
{
    Elf64_Addr   r_offset;                     /* Address */
    Elf64_Xword  r_info;                       /* Relocation type and symbol index */
} Elf64_Rel;

/* Relocation entry with explicit addend */
typedef struct
{
    Elf32_Addr  r_offset;                      /* offset of relocation */
    Elf32_Word  r_info;                        /* symbol table index and type */
    Elf32_Sword r_addend;
} Elf32_Rela;

typedef struct
{
    Elf64_Addr      r_offset;                  /* Address */
    Elf64_Xword     r_info;                    /* Relocation type and symbol index */
    Elf64_Sxword    r_addend;                  /* Addend */
} Elf64_Rela;

/* Extract relocation info - r_info */
#define ELF32_R_SYM(i)          ((i) >> 8)
#define ELF32_R_TYPE(i)         ((unsigned char) (i))
#define ELF32_R_INFO(s,t)       (((s) << 8) + (unsigned char)(t))

#define ELF64_R_SYM(i)                        ((i) >> 32)
#define ELF64_R_TYPE(i)                        ((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type)                ((((Elf64_Xword) (sym)) << 32) + (type))

/*
 * Relocation type for arm
 */
#define R_ARM_NONE              0
#define R_ARM_PC24              1
#define R_ARM_ABS32             2
#define R_ARM_REL32             3
#define R_ARM_THM_CALL          10
#define R_ARM_GLOB_DAT          21
#define R_ARM_JUMP_SLOT         22
#define R_ARM_RELATIVE          23
#define R_ARM_GOT_BREL          26
#define R_ARM_PLT32             27
#define R_ARM_CALL              28
#define R_ARM_JUMP24            29
#define R_ARM_THM_JUMP24        30
#define R_ARM_V4BX              40

/*
 * Relocation type for x86
 */
#define R_386_NONE              0
#define R_386_32                1
#define R_386_PC32              2
#define R_386_GOT32             3
#define R_386_PLT32             4
#define R_386_COPY              5
#define R_386_GLOB_DAT          6
#define R_386_JMP_SLOT          7
#define R_386_RELATIVE          8
#define R_386_GOTOFF            9
#define R_386_GOTPC             10

/* Program Header */
typedef struct
{
    Elf32_Word p_type;                         /* segment type */
    Elf32_Off  p_offset;                       /* segment offset */
    Elf32_Addr p_vaddr;                        /* virtual address of segment */
    Elf32_Addr p_paddr;                        /* physical address - ignored? */
    Elf32_Word p_filesz;                       /* number of bytes in file for seg. */
    Elf32_Word p_memsz;                        /* number of bytes in mem. for seg. */
    Elf32_Word p_flags;                        /* flags */
    Elf32_Word p_align;                        /* memory alignment */
} Elf32_Phdr;

typedef struct
{
    Elf64_Word   p_type;                       /* Segment type */
    Elf64_Word   p_flags;                      /* Segment flags */
    Elf64_Off    p_offset;                     /* Segment file offset */
    Elf64_Addr   p_vaddr;                      /* Segment virtual address */
    Elf64_Addr   p_paddr;                      /* Segment physical address */
    Elf64_Xword  p_filesz;                     /* Segment size in file */
    Elf64_Xword  p_memsz;                      /* Segment size in memory */
    Elf64_Xword  p_align;                      /* Segment alignment */
} Elf64_Phdr;

/* p_type */
#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_DYNAMIC              2
#define PT_INTERP               3
#define PT_NOTE                 4
#define PT_SHLIB                5
#define PT_PHDR                 6
#define PT_TLS                  7
#define PT_NUM                  8
#define PT_LOOS                 0x60000000
#define PT_HIOS                 0x6fffffff
#define PT_LOPROC               0x70000000
#define PT_HIPROC               0x7fffffff

/* p_flags */
#define PF_X                    1
#define PF_W                    2
#define PF_R                    4

/* Legal values for note segment descriptor types for core files. */

#define NT_PRSTATUS 1       /* Contains copy of prstatus struct */
#define NT_FPREGSET 2       /* Contains copy of fpregset struct */
#define NT_PRPSINFO 3       /* Contains copy of prpsinfo struct */
#define NT_PRXREG   4       /* Contains copy of prxregset struct */
#define NT_TASKSTRUCT   4       /* Contains copy of task structure */
#define NT_PLATFORM 5       /* String from sysinfo(SI_PLATFORM) */
#define NT_AUXV     6       /* Contains copy of auxv array */
#define NT_GWINDOWS 7       /* Contains copy of gwindows struct */
#define NT_ASRS     8       /* Contains copy of asrset struct */
#define NT_PSTATUS  10      /* Contains copy of pstatus struct */
#define NT_PSINFO   13      /* Contains copy of psinfo struct */
#define NT_PRCRED   14      /* Contains copy of prcred struct */
#define NT_UTSNAME  15      /* Contains copy of utsname struct */
#define NT_LWPSTATUS    16      /* Contains copy of lwpstatus struct */
#define NT_LWPSINFO 17      /* Contains copy of lwpinfo struct */
#define NT_PRFPXREG 20      /* Contains copy of fprxregset struct */
#define NT_SIGINFO  0x53494749  /* Contains copy of siginfo_t,
                       size might increase */
#define NT_FILE     0x46494c45  /* Contains information about mapped
                       files */
#define NT_PRXFPREG 0x46e62b7f  /* Contains copy of user_fxsr_struct */
#define NT_PPC_VMX  0x100       /* PowerPC Altivec/VMX registers */
#define NT_PPC_SPE  0x101       /* PowerPC SPE/EVR registers */
#define NT_PPC_VSX  0x102       /* PowerPC VSX registers */
#define NT_386_TLS  0x200       /* i386 TLS slots (struct user_desc) */
#define NT_386_IOPERM   0x201       /* x86 io permission bitmap (1=deny) */
#define NT_X86_XSTATE   0x202       /* x86 extended state using xsave */
#define NT_S390_HIGH_GPRS   0x300   /* s390 upper register halves */
#define NT_S390_TIMER   0x301       /* s390 timer register */
#define NT_S390_TODCMP  0x302       /* s390 TOD clock comparator register */
#define NT_S390_TODPREG 0x303       /* s390 TOD programmable register */
#define NT_S390_CTRS    0x304       /* s390 control registers */
#define NT_S390_PREFIX  0x305       /* s390 prefix register */
#define NT_S390_LAST_BREAK  0x306   /* s390 breaking event address */
#define NT_S390_SYSTEM_CALL 0x307   /* s390 system call restart data */
#define NT_S390_TDB 0x308       /* s390 transaction diagnostic block */
#define NT_ARM_VFP  0x400       /* ARM VFP/NEON registers */
#define NT_ARM_TLS  0x401       /* ARM TLS register */
#define NT_ARM_HW_BREAK 0x402       /* ARM hardware breakpoint registers */
#define NT_ARM_HW_WATCH 0x403       /* ARM hardware watchpoint registers */

/* Legal values for the note segment descriptor types for object files.  */

#define NT_VERSION  1       /* Contains a version string.  */

/* sh_type */
#define SHT_NULL                0              /* inactive */
#define SHT_PROGBITS            1              /* program defined information */
#define SHT_SYMTAB              2              /* symbol table section */
#define SHT_STRTAB              3              /* string table section */
#define SHT_RELA                4              /* relocation section with addends*/
#define SHT_HASH                5              /* symbol hash table section */
#define SHT_DYNAMIC             6              /* dynamic section */
#define SHT_NOTE                7              /* note section */
#define SHT_NOBITS              8              /* no space section */
#define SHT_REL                 9              /* relocation section without addends */
#define SHT_SHLIB               10             /* reserved - purpose unknown */
#define SHT_DYNSYM              11             /* dynamic symbol table section */
#define SHT_NUM                 12             /* number of section types */
#define SHT_LOPROC              0x70000000     /* reserved range for processor */
#define SHT_HIPROC              0x7fffffff     /* specific section header types */
#define SHT_LOUSER              0x80000000     /* reserved range for application */
#define SHT_HIUSER              0xffffffff     /* specific indexes */

/* Section Attribute Flags - sh_flags */
#define SHF_WRITE               0x1            /* Writable */
#define SHF_ALLOC               0x2            /* occupies memory */
#define SHF_EXECINSTR           0x4            /* executable */
#define SHF_MASKPROC            0xf0000000     /* reserved bits for processor */
/* specific section attributes */

typedef struct
{
  Elf32_Word n_namesz;          /* Length of the note's name.  */
  Elf32_Word n_descsz;          /* Length of the note's descriptor.  */
  Elf32_Word n_type;            /* Type of the note.  */
} Elf32_Nhdr;

typedef struct
{
  Elf64_Word n_namesz;          /* Length of the note's name.  */
  Elf64_Word n_descsz;          /* Length of the note's descriptor.  */
  Elf64_Word n_type;            /* Type of the note.  */
} Elf64_Nhdr;

#define IS_PROG(s)        (s.sh_type == SHT_PROGBITS)
#define IS_NOPROG(s)      (s.sh_type == SHT_NOBITS)
#define IS_REL(s)         (s.sh_type == SHT_REL)
#define IS_RELA(s)        (s.sh_type == SHT_RELA)
#define IS_ALLOC(s)       (s.sh_flags == SHF_ALLOC)
#define IS_AX(s)          ((s.sh_flags & SHF_ALLOC) && (s.sh_flags & SHF_EXECINSTR))
#define IS_AW(s)          ((s.sh_flags & SHF_ALLOC) && (s.sh_flags & SHF_WRITE))

#if (defined(__arm__) || defined(__i386__) || (__riscv_xlen == 32))
#define elf_module        ((Elf32_Ehdr *)module_ptr)
#define shdr              ((Elf32_Shdr *)((rt_uint8_t *)module_ptr + elf_module->e_shoff))
#define phdr              ((Elf32_Phdr *)((rt_uint8_t *)module_ptr + elf_module->e_phoff))

typedef Elf32_Sym       Elf_Sym;
typedef Elf32_Rel       Elf_Rel;
typedef Elf32_Addr      Elf_Addr;
#elif (defined(__aarch64__) || defined(__x86_64__) || (__riscv_xlen == 64))
#define elf_module        ((Elf64_Ehdr *)module_ptr)
#define shdr              ((Elf64_Shdr *)((rt_uint8_t *)module_ptr + elf_module->e_shoff))
#define phdr              ((Elf64_Phdr *)((rt_uint8_t *)module_ptr + elf_module->e_phoff))

typedef Elf64_Sym       Elf_Sym;
typedef Elf64_Rela      Elf_Rel;
typedef Elf64_Addr      Elf_Addr;
#endif

int dlmodule_relocate(struct rt_dlmodule *module, Elf_Rel *rel, Elf_Addr sym_val);
rt_err_t dlmodule_load_shared_object(struct rt_dlmodule *module, void *module_ptr);
rt_err_t dlmodule_load_relocated_object(struct rt_dlmodule *module, void *module_ptr);

#endif
