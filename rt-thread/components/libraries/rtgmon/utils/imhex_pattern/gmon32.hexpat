#pragma endian little

/* author: liu2guang 2021-06-28 */ 
/* glibc : https://gitee.com/mirrors/glibc_source.git 2c16cb88a6e5ace0fb7cedca86860ea7bde522a7 */ 

/* gmon\sys\gmon_out.h */ 
enum GMON_Record_Tag : u8
{
    GMON_TAG_TIME_HIST = 0, 
    GMON_TAG_CG_ARC = 1, 
    GMON_TAG_BB_COUNT = 2
}; 

/* gmon\sys\gmon_out.h */ 
/* gmon\gmon.c */ 
struct gmon_hdr
{
    char cookie[4];     /* gmon */ 
    u32 version;        /* 1 */ 
    char spare[3*4];
};

/* gmon\sys\gmon_out.h */ 
/* gmon\gmon.c */ 
struct gmon_hist_hdr
{
    u32 low_pc; 
    u32 high_pc; 
    s32 hist_size;
    s32 prof_rate;
    char dimen[15];
    char dimen_abbrev;
}; 

struct gmon_hist
{
    GMON_Record_Tag tag; 
    gmon_hist_hdr hist_hdr; 
    u16 hist_content[hist_hdr.hist_size];
}; 

struct gmon_cg_arc_record
{   
    u32 from_pc; 
    u32 self_pc; 
    u32 count; 
};

struct gmon_cg
{
    GMON_Record_Tag tag;
    gmon_cg_arc_record arc_record; 
}; 

#define GMON_GC_NUM 2

struct gmon
{
    gmon_hdr ghdr; 
    gmon_hist hist; 
    gmon_cg cg[GMON_GC_NUM]; 
}; 

gmon gmon_out @ 0x00000000; 
