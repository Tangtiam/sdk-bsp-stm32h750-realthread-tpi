/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author      Notes
 * 2018/08/29     Bernard     first version
 */

#include <rthw.h>

#include "dlfcn.h"
#include "dlmodule.h"
#include "dlelf.h"

#ifdef RT_USING_POSIX_FS
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#endif

#ifdef RT_USING_GDBSERVER
#include "gdbserver.h"
#endif

#ifdef RT_USING_GPROF
#include <rtgmon.h>
#endif

#define DBG_TAG    "DLMD"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>          // must after of DEBUG_ENABLE or some other options

#define RT_MID_NUM_MAX 32
struct dlmodule_ids
{
    struct rt_dlmodule* map[RT_MID_NUM_MAX];
    int last;
};

static struct dlmodule_ids mids;
static struct rt_module_symtab *_rt_module_symtab_begin = RT_NULL;
static struct rt_module_symtab *_rt_module_symtab_end   = RT_NULL;
static void* load_address = RT_NULL;

#if defined(__IAR_SYSTEMS_ICC__) /* for IAR compiler */
    #pragma section="RTMSymTab"
#endif

/* set the name of module */
static void _dlmodule_set_name(struct rt_dlmodule *module, const char *path)
{
    int size;
    struct rt_object *object;
    const char *first, *end, *ptr;

    object = &(module->parent);
    ptr   = first = (char *)path;
    end   = path + rt_strlen(path);

    while (*ptr != '\0')
    {
        if (*ptr == '/')
            first = ptr + 1;
        if (*ptr == '.')
            end = ptr - 1;

        ptr ++;
    }

    size = end - first + 1;
    if (size > RT_NAME_MAX) size = RT_NAME_MAX;

    rt_strncpy(object->name, first, size);
    object->name[size] = '\0';
}

#define RT_MODULE_ARG_MAX    8
static int _rt_module_split_arg(char *cmd, rt_size_t length, char *argv[])
{
    int argc = 0;
    char *ptr = cmd;

    while ((ptr - cmd) < length)
    {
        /* strip bank and tab */
        while ((*ptr == ' ' || *ptr == '\t') && (ptr - cmd) < length)
            *ptr++ = '\0';
        /* check whether it's the end of line */
        if ((ptr - cmd) >= length) break;

        /* handle string with quote */
        if (*ptr == '"')
        {
            argv[argc++] = ++ptr;

            /* skip this string */
            while (*ptr != '"' && (ptr - cmd) < length)
                if (*ptr ++ == '\\')  ptr ++;
            if ((ptr - cmd) >= length) break;

            /* skip '"' */
            *ptr ++ = '\0';
        }
        else
        {
            argv[argc++] = ptr;
            while ((*ptr != ' ' && *ptr != '\t') && (ptr - cmd) < length)
                ptr ++;
        }

        if (argc >= RT_MODULE_ARG_MAX) break;
    }

    return argc;
}

static void _dlmodule_thread_entry(void* parameter)
{
    int argc = 0;
    char *argv[RT_MODULE_ARG_MAX];

    struct rt_dlmodule *module = (struct rt_dlmodule*)parameter;

    if (module == RT_NULL || module->cmd_line == RT_NULL)
        /* malloc for module_cmd_line failed. */
        return;

    if (module->cmd_line)
    {
        rt_memset(argv, 0x00, sizeof(argv));
        argc = _rt_module_split_arg((char *)module->cmd_line, rt_strlen(module->cmd_line), argv);
        rt_free(module->cmd_line);
        if (argc == 0) return;
    }

#ifdef RT_USING_GDBSERVER
    if (module->debug)
    {
        uint32_t addr = (uint32_t)module->entry;
        if((addr) & 1)
        {
            module->bak_first_ins = *(uint32_t*)(addr - 1);
            *(uint32_t*)(addr - 1) = dbg_get_ins();
        }
        else
        {
            module->bak_first_ins = *(uint32_t*)addr;
            *(uint32_t*)(addr) = dbg_get_ins();
        }
    }
#endif
    load_address = module->mem_space;
    rt_kprintf("run main entry: %p load address:%p\n", module->entry, load_address);

    if (module->entry)
        module->entry(argc, argv);
}

static struct rt_dlmodule *dlmodule_new(void)
{
    struct rt_dlmodule *module = RT_NULL;

    module = (struct rt_dlmodule*) rt_object_allocate(RT_Object_Class_Module, "module");

    /* set initial priority and stack size */
    module->priority = RT_THREAD_PRIORITY_MAX - 1;
    module->stack_size = 2048;

    rt_list_init(&(module->object_list));
    rt_list_init(&(module->t_grp));

    return module;
}

rt_err_t dlmodule_free(struct rt_dlmodule* module)
{
    int i;

    RT_ASSERT(module != RT_NULL)

    RT_DEBUG_NOT_IN_INTERRUPT;

    /* do module cleanup */
    if (module->cleanup_func)
    {
        rt_enter_critical();
        module->cleanup_func(module);
        rt_exit_critical();
    }

#ifdef RT_USING_GPROF
    moncleanup();
#endif

    /* cleanup for all kernel objects inside module*/
    {
        struct rt_object *object = RT_NULL;
        struct rt_list_node *node = RT_NULL;

        /* detach/delete all threads in this module */
        for (node = module->object_list.next; node != &(module->object_list); )
        {
            int object_type;

            object = rt_list_entry(node, struct rt_object, list);
            object_type = object->type & ~RT_Object_Class_Static;

            /* to next node */
            node = node->next;

            if (object->type & RT_Object_Class_Static)
            {
                switch (object_type)
                {
#ifdef RT_USING_SEMAPHORE
                case RT_Object_Class_Semaphore:
                    rt_sem_detach((rt_sem_t)object);
                    break;
#endif
#ifdef RT_USING_MUTEX
                case RT_Object_Class_Mutex:
                    rt_mutex_detach((rt_mutex_t)object);
                    break;
#endif
#ifdef RT_USING_EVENT
                case RT_Object_Class_Event:
                    rt_event_detach((rt_event_t)object);
                    break;
#endif
#ifdef RT_USING_MAILBOX
                case RT_Object_Class_MailBox:
                    rt_mb_detach((rt_mailbox_t)object);
                    break;
#endif
#ifdef RT_USING_MESSAGEQUEUE
                case RT_Object_Class_MessageQueue:
                    rt_mq_detach((rt_mq_t)object);
                    break;
#endif
#ifdef RT_USING_MEMHEAP
                case RT_Object_Class_MemHeap:
                    rt_memheap_detach((struct rt_memheap*)object);
                    break;
#endif
#ifdef RT_USING_MEMPOOL
                case RT_Object_Class_MemPool:
                    rt_mp_detach((struct rt_mempool*)object);
                    break;
#endif
                case RT_Object_Class_Timer:
                    rt_timer_detach((rt_timer_t)object);
                    break;
                default:
                    LOG_E("Unsupported oject type in module.");
                    break;
                }
            }
            else
            {
                switch (object_type)
                {
                case RT_Object_Class_Thread:
                    rt_list_remove(&(object->list));
                    break;
#ifdef RT_USING_SEMAPHORE
                case RT_Object_Class_Semaphore:
                    rt_sem_delete((rt_sem_t)object);
                    break;
#endif
#ifdef RT_USING_MUTEX
                case RT_Object_Class_Mutex:
                    rt_mutex_delete((rt_mutex_t)object);
                    break;
#endif
#ifdef RT_USING_EVENT
                case RT_Object_Class_Event:
                    rt_event_delete((rt_event_t)object);
                    break;
#endif
#ifdef RT_USING_MAILBOX
                case RT_Object_Class_MailBox:
                    rt_mb_delete((rt_mailbox_t)object);
                    break;
#endif
#ifdef RT_USING_MESSAGEQUEUE
                case RT_Object_Class_MessageQueue:
                    rt_mq_delete((rt_mq_t)object);
                    break;
#endif
#ifdef RT_USING_MEMHEAP
                /* no delete operation */
#endif
#ifdef RT_USING_MEMPOOL
                case RT_Object_Class_MemPool:
                    rt_mp_delete((struct rt_mempool*)object);
                    break;
#endif
                case RT_Object_Class_Timer:
                    rt_timer_delete((rt_timer_t)object);
                    break;
                default:
                    LOG_E("Unsupported oject type in module.");
                    break;
                }
            }
        }
    }

    if (module->cmd_line) 
    {
        rt_free(module->cmd_line);
    }
    /* release module symbol table */
    for (i = 0; i < module->nsym; i ++)
    {
        rt_free((void *)module->symtab[i].name);
    }
    if (module->symtab)
    {
        rt_free(module->symtab);
    }

    /* destory module */
    rt_free(module->mem_space);
    /* delete module object */
    rt_object_delete((rt_object_t)module);

    return RT_EOK;
}

static void _delete_subthread(rt_thread_t tid)
{
    struct rt_thread    *thread = RT_NULL;
    struct rt_list_node *list = RT_NULL;
    struct rt_dlmodule* module;

    module = tid->module;

    rt_enter_critical();

    /* delete all sub threads */
    while ((list = tid->sibling.prev) != &module->t_grp)
    {
        thread = rt_list_entry(list, struct rt_thread, sibling);
        /* stop timer */
        if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_CLOSE &&
            (thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT)
        {
            rt_timer_stop(&(thread->thread_timer));
        }
        rt_list_remove(&thread->sibling);
        rt_thread_control(thread, RT_THREAD_CTRL_CLOSE, RT_NULL);
    }

    rt_exit_critical();
}

#ifdef RT_USING_EXCEPTION_HANDLE
static rt_err_t _dlmodule_exception_handle(void *context)
{
    if (dlmodule_self())
    {
        /* close assertion module */
        dlmodule_exit(-1);
    }

    return RT_EOK;
}
#endif

static void _dlmodule_assert_hook(const char *ex, const char *func, rt_size_t line)
{
    volatile char dummy = 0;

    rt_kprintf("(%s) assertion failed at function:%s, line number:%d \n", ex, func, line);
    if (dlmodule_self())
    {
        /* close assertion module */
        dlmodule_exit(-1);
    }
    else
    {
        while (dummy == 0);
    }
}

void dlmodule_cleanup(struct rt_thread *tid)
{
    struct rt_dlmodule* module;

    RT_ASSERT(tid != RT_NULL);
    module = tid->module;
    RT_ASSERT(module != RT_NULL);

    if(tid == module->t_main && module->nref > 1)
    {
        _delete_subthread(tid);
    }
    else
    {
        rt_enter_critical();
        rt_list_remove(&tid->sibling);
        rt_exit_critical();
    }

    dlmodule_ref_dec(module);

#if RT_VER_NUM <= 0x50000
    LOG_I("cleanup thread: %s, stack_addr: %08X", tid->name, tid->stack_addr);
#else
    LOG_I("cleanup thread: %s, stack_addr: %08X", tid->parent.name, tid->stack_addr);
#endif

    return;
}

void dlmodule_add_subthread(struct rt_dlmodule* module, rt_thread_t thread)
{
    rt_base_t level;
    RT_ASSERT(module != RT_NULL);
    RT_ASSERT(thread != RT_NULL);

    level = rt_hw_interrupt_disable();
    rt_list_insert_after(&(module->t_grp), &(thread->sibling));
    rt_hw_interrupt_enable(level);    
}

int dlmodule_ref_inc(struct rt_dlmodule* module)
{
    rt_base_t level;
    RT_ASSERT(module != RT_NULL);

    level = rt_hw_interrupt_disable();
    module->nref++;
    rt_hw_interrupt_enable(level);

    return 0;
}

int dlmodule_ref_dec(struct rt_dlmodule* module)
{
    rt_base_t level;
    int ref = -1;
    RT_ASSERT(module != RT_NULL);

    level = rt_hw_interrupt_disable();
    if (module->nref)
    {
        module->nref--;
        ref = module->nref;
    }
    rt_hw_interrupt_enable(level);
    if (!ref)
    {
        dlmodule_free(module);

        return 0;
    }

    return -1;
}

struct rt_dlmodule *dlmodule_self(void)
{
    rt_thread_t tid;
    struct rt_dlmodule *ret = RT_NULL;

    tid = rt_thread_self();
    if (tid)
    {
        ret = tid->module;
    }

    return ret;
}

/*
 * Compatible with old API
 */
struct rt_dlmodule *rt_module_self(void)
{
    return dlmodule_self();
}

static rt_uint8_t _is_dlmodule_load(const char* path)
{
    int index = 0;
    struct rt_dlmodule *mid = RT_NULL;
    int size = 0;
    const char *first = RT_NULL, *end = RT_NULL, *ptr = RT_NULL;
    char name[RT_NAME_MAX] = {'\0'};

    ptr   = first = (char *)path;
    end   = path + rt_strlen(path);

    while (*ptr != '\0')
    {
        if (*ptr == '/')
            first = ptr + 1;
        if (*ptr == '.')
            end = ptr - 1;

        ptr ++;
    }

    size = end - first + 1;
    if (size > RT_NAME_MAX)
        size = RT_NAME_MAX;

    rt_strncpy(name, first, size);
    name[size] = '\0';
    
    for (index = 0; index < RT_MID_NUM_MAX; index++)
    {
        mid = mids.map[index];
        if (mid != RT_NULL)
        {
            if (rt_strcmp(mid->parent.name, name) == 0)
            {
                return 1;
            }
        }
    }

    return 0;
}

static int _module_name2mid(const char* name)
{
    int index = 0;
    rt_base_t level = 0;

    level = rt_hw_interrupt_disable();
    for (index = 0; index < RT_MID_NUM_MAX; index++)
    {
        if (mids.map[index])
        {
            if(!rt_strncmp(name, mids.map[index]->parent.name, RT_NAME_MAX))
            {
                rt_hw_interrupt_enable(level);
                return index;
            }
        }
    }

    rt_hw_interrupt_enable(level);
    return -1;
}

struct rt_dlmodule* dlmodule_load(const char* path)
{
#ifdef RT_USING_POSIX_FS
    int fd = -1, length = 0;
#endif
    rt_uint8_t i = 0;
    rt_base_t level;
    rt_err_t ret = RT_EOK;
    rt_uint8_t *module_ptr = RT_NULL;
    struct rt_dlmodule *module = RT_NULL;

    if(_is_dlmodule_load(path))
    {
        rt_kprintf("module [%s] has already been loaded!\n", path);

        //return RT_NULL;
    }

#ifdef RT_USING_POSIX_FS
    fd = open(path, O_RDONLY, 0);
    if (fd >= 0)
    {
        length = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        if (length == 0) goto __exit;

        module_ptr = (uint8_t*) rt_malloc (length);
        if (!module_ptr) goto __exit;

        if (read(fd, module_ptr, length) != length)
            goto __exit;

        /* close file and release fd */
        close(fd);
        fd = -1;
    }
    else
    {
        goto __exit;
    }
#endif

    if (!module_ptr) goto __exit;

    /* check ELF header */
    if (rt_memcmp(elf_module->e_ident, RTMMAG, SELFMAG) != 0 &&
        rt_memcmp(elf_module->e_ident, ELFMAG, SELFMAG) != 0)
    {
        rt_kprintf("Module: magic error\n");
        goto __exit;
    }

    /* check ELF class */
    if ((elf_module->e_ident[EI_CLASS] != ELFCLASS32)&&(elf_module->e_ident[EI_CLASS] != ELFCLASS64))
    {
        rt_kprintf("Module: ELF class error\n");
        goto __exit;
    }

    module = dlmodule_new();
    if (!module) goto __exit;

    /* set the name of module */
    _dlmodule_set_name(module, path);

    LOG_D("rt_module_load: %.*s", RT_NAME_MAX, module->parent.name);

    if (elf_module->e_type == ET_REL)
    {
        ret = dlmodule_load_relocated_object(module, module_ptr);
    }
    else if (elf_module->e_type == ET_DYN)
    {
        ret = dlmodule_load_shared_object(module, module_ptr);
    }
    else
    {
        rt_kprintf("Module: unsupported elf type\n");
        goto __exit;
    }

    /* check return value */
    if (ret != RT_EOK) goto __exit;

    /* release module data */
    rt_free(module_ptr);

    /* set module initialization and cleanup function */
    module->init_func = dlsym(module, "module_init");
    module->cleanup_func = dlsym(module, "module_cleanup");
    /* do module initialization */
    if (module->init_func)
    {
        module->init_func(module);
    }

    level = rt_hw_interrupt_disable();
    for (i = mids.last; i < RT_MID_NUM_MAX; i++)
    {
        if (!mids.map[i])
        {
            break;
        }
    }

    if (i >= RT_MID_NUM_MAX)
    {
        for (i = 0; i < mids.last; i++)
        {
            if (!mids.map[i])
            {
                break;
            }
        }
    }

    mids.last = (i + 1) % RT_MID_NUM_MAX;
    module->mid = i;
    mids.map[i] = module;
    rt_hw_interrupt_enable(level);
    
    return module;

__exit:
#ifdef RT_USING_POSIX_FS
    if (fd >= 0) close(fd);
#endif
    if (module_ptr) rt_free(module_ptr);
    if (module) dlmodule_free(module);

    return RT_NULL;
}

rt_err_t dlmoudle_run(struct rt_dlmodule* module)
{
    rt_thread_t tid;
    rt_err_t ret = RT_EOK;

    RT_ASSERT(module != RT_NULL);

    /* check stack size and priority */
    if (module->priority > RT_THREAD_PRIORITY_MAX) module->priority = RT_THREAD_PRIORITY_MAX - 1;
    if (module->stack_size < 2048 || module->stack_size > (1024 * 32)) module->stack_size = 2048;

    tid = rt_thread_create(module->parent.name, _dlmodule_thread_entry, (void*)module,
        module->stack_size, module->priority, 10);
    if (tid)
    {
        tid->module = module;
        module->t_main = tid;
        rt_list_init(&(tid->sibling));
        dlmodule_add_subthread(module, tid); 
        dlmodule_ref_inc(module);

        /* install thread cleanup handler */
        tid->cleanup = dlmodule_cleanup;

#ifdef RT_USING_EXCEPTION_HANDLE
        /* install exception handler */
        rt_hw_exception_install(_dlmodule_exception_handle);
#endif
        /* install assert hook hander */
#ifdef RT_DEBUG
         rt_assert_set_hook(_dlmodule_assert_hook);
#endif

        rt_thread_startup(tid);
    }
    else
    {
        /* destory dl module */
        dlmodule_free(module);
        module = RT_NULL;
        ret = -RT_ERROR;
    }

    return ret;
}

struct rt_dlmodule* dlmodule_exec(const char* pgname, int debug, const char* cmd, int length)
{
    struct rt_dlmodule *module;

    module = dlmodule_load(pgname);
    if (module && module->entry)
    {
        if (debug)
        {
            module->debug = debug;
        }
        module->cmd_line = rt_strdup(cmd);
        dlmoudle_run(module);
    }

    return module;
}

static void _cmd_load(const char *path)
{
    struct rt_dlmodule *module = RT_NULL;

    module = dlmodule_load(path);
    if (module)
    {
        load_address = module->mem_space;
        rt_kprintf("entry addr[0x%p]\n", module->entry);
    }
    else
    {
        rt_kprintf("load module failed!\n");
    }
}

static void _cmd_run(const char *name)
{
    struct rt_dlmodule *module = RT_NULL;
    int mid = 0;

    mid = _module_name2mid(name);
    if (mid < 0)
    {
        rt_kprintf("can't find [%s] module\n", name);
        return;
    }

    module = mids.map[mid];
    if (module->entry)
    {
        module->debug = 0;
        module->cmd_line = rt_strdup(name);
        dlmoudle_run(module);
    }
}

static void _cmd_remove(const char *name)
{
    struct rt_dlmodule *module = RT_NULL;
    int mid = 0;

    mid = _module_name2mid(name);
    if (mid < 0)
    {
        rt_kprintf("can't find [%s] module\n", name);
        return;
    }

    module = mids.map[mid];
    if (module)
    {
        dlmodule_delete(module);
    }
}

int app(int argc, char **argv)
{
    const char *operator = argv[1];
    if(argc == 3)
    {
        if (!rt_strcmp(operator, "load"))
        {
            _cmd_load(argv[2]);
        }
        else if(!rt_strcmp(operator, "kill"))
        {
            _cmd_remove(argv[2]);
        }
        else if(!rt_strcmp(operator, "run"))
        {
            _cmd_run(argv[2]);
        }
        else
        {
            goto _app_usage;
        }
        
        return 0;
    }

_app_usage:
    rt_kprintf("Usage: app [options] [app_name]\n");
    rt_kprintf("[options]:\n");
    rt_kprintf("    load - Load app\n");
    rt_kprintf("    run - Run app\n");
    rt_kprintf("    kill - Remove app\n");

    return 0;
}
MSH_CMD_EXPORT(app, app(module) debug commond);

void dlmodule_delete(struct rt_dlmodule *module)
{
    RT_ASSERT(module != RT_NULL);

    if(module->t_main == RT_NULL)
    {
        dlmodule_free(module);
        module = RT_NULL;
    }
    else
    {
        /* delete thread: insert to defunct thread list */
        rt_thread_control(module->t_main, RT_THREAD_CTRL_CLOSE, RT_NULL);
    }
}

void dlmodule_exit(int value)
{
    struct rt_dlmodule *module;
    rt_thread_t tid;

    tid = rt_thread_self();
    module = tid->module;
    if(!module)
    {
        LOG_E("dlmodule_exit: module null\n");
        return;
    }

    module->ret_code = value;
    dlmodule_delete(module);

    if(tid != module->t_main)
    {
        rt_thread_suspend(tid);
        rt_schedule();
    }
}

rt_uint32_t dlmodule_symbol_find(const char *sym_str)
{
    /* find in kernel symbol table */
    struct rt_module_symtab *index;

    for (index = _rt_module_symtab_begin; index != _rt_module_symtab_end; index ++)
    {
        if (rt_strcmp(index->name, sym_str) == 0)
            return (rt_uint32_t)index->addr;
    }

    return 0;
}

int rt_system_dlmodule_init(void)
{
#if defined(__GNUC__) && !defined(__CC_ARM)
    extern int __rtmsymtab_start;
    extern int __rtmsymtab_end;

    _rt_module_symtab_begin = (struct rt_module_symtab *)&__rtmsymtab_start;
    _rt_module_symtab_end   = (struct rt_module_symtab *)&__rtmsymtab_end;
#elif defined (__CC_ARM)
    extern int RTMSymTab$$Base;
    extern int RTMSymTab$$Limit;

    _rt_module_symtab_begin = (struct rt_module_symtab *)&RTMSymTab$$Base;
    _rt_module_symtab_end   = (struct rt_module_symtab *)&RTMSymTab$$Limit;
#elif defined (__IAR_SYSTEMS_ICC__)
    _rt_module_symtab_begin = __section_begin("RTMSymTab");
    _rt_module_symtab_end   = __section_end("RTMSymTab");
#endif

    rt_memset(mids.map, RT_NULL, RT_MID_NUM_MAX * sizeof(struct rt_dlmodule*));
    mids.last = 0;

    return 0;
}
INIT_COMPONENT_EXPORT(rt_system_dlmodule_init);

/**
 * This function will find the specified module.
 *
 * @param name the name of module finding
 *
 * @return the module
 */
struct rt_dlmodule *dlmodule_find(const char *name)
{
    rt_object_t object;
    struct rt_dlmodule *ret = RT_NULL;

    object = rt_object_find(name, RT_Object_Class_Module);
    if (object)
    {
        ret = (struct rt_dlmodule*) object;
    }

    return ret;
}
RTM_EXPORT(dlmodule_find);

#ifdef RT_USING_FINSH

int list_symbols(void)
{
    extern int __rtmsymtab_start;
    extern int __rtmsymtab_end;

    /* find in kernel symbol table */
    struct rt_module_symtab *index;

    for (index = _rt_module_symtab_begin;
         index != _rt_module_symtab_end;
         index ++)
    {
        rt_kprintf("%s => 0x%08x\n", index->name, index->addr);
    }

    return 0;
}
MSH_CMD_EXPORT(list_symbols, list symbols information);

/* copy from components/finsh/cmd.c */
static void object_split(int len)
{
    while (len--)
    {
        rt_kprintf("-");
    }
}

static void print_thread_info(struct rt_thread* thread, int maxlen)
{
    rt_uint8_t *ptr;
    rt_uint8_t stat;

#ifdef RT_USING_SMP
    if (thread->oncpu != RT_CPU_DETACHED)
        rt_kprintf("%-*.*s %3d %3d ", maxlen, RT_NAME_MAX, thread->parent.name, thread->oncpu, thread->current_priority);
    else
        rt_kprintf("%-*.*s N/A %3d ", maxlen, RT_NAME_MAX, thread->parent.name, thread->current_priority);
#elif RT_VER_NUM <= 0x50000
    rt_kprintf("%-*.*s %3d ", maxlen, RT_NAME_MAX, thread->name, thread->current_priority);
#else
    rt_kprintf("%-*.*s %3d ", maxlen, RT_NAME_MAX, thread->parent.name, thread->current_priority);
#endif /*RT_USING_SMP*/

    stat = (thread->stat & RT_THREAD_STAT_MASK);
    if (stat == RT_THREAD_READY)        rt_kprintf(" ready  ");
#if RT_VER_NUM <= 0x50000
    else if (stat == RT_THREAD_SUSPEND) rt_kprintf(" suspend");
#else
    else if ((stat & RT_THREAD_SUSPEND_MASK) == RT_THREAD_SUSPEND_MASK) rt_kprintf(" suspend");
#endif
    else if (stat == RT_THREAD_INIT)    rt_kprintf(" init   ");
    else if (stat == RT_THREAD_CLOSE)   rt_kprintf(" close  ");
    else if (stat == RT_THREAD_RUNNING) rt_kprintf(" running");

#if defined(ARCH_CPU_STACK_GROWS_UPWARD)
    ptr = (rt_uint8_t *)thread->stack_addr + thread->stack_size;
    while (*ptr == '#')ptr--;

    rt_kprintf(" 0x%08x 0x%08x    %02d%%   0x%08x %03d\n",
            ((rt_uint32_t)thread->sp - (rt_uint32_t)thread->stack_addr),
            thread->stack_size,
            ((rt_uint32_t)ptr - (rt_uint32_t)thread->stack_addr) * 100 / thread->stack_size,
            thread->remaining_tick,
            thread->error);
#else
    ptr = (rt_uint8_t *)thread->stack_addr;
    while (*ptr == '#')ptr++;

    rt_kprintf(" 0x%08x 0x%08x    %02d%%   0x%08x %03d\n",
            (thread->stack_size + (rt_uint32_t)(rt_size_t)thread->stack_addr - (rt_uint32_t)(rt_size_t)thread->sp),
            thread->stack_size,
            (thread->stack_size + (rt_uint32_t)(rt_size_t)thread->stack_addr - (rt_uint32_t)(rt_size_t)ptr) * 100
            / thread->stack_size,
            thread->remaining_tick,
            thread->error);
#endif
}

int list_module(void)
{
    struct rt_dlmodule *module;
    struct rt_list_node *list, *node;
    struct rt_object_information *info;
    const char *item_title = "thread";
    int maxlen;

    info = rt_object_get_information(RT_Object_Class_Module);
    list = &info->object_list;
    maxlen = RT_NAME_MAX;

    rt_kprintf("%-*.s %-*.s %-*.s pri  status      sp     stack size max used left tick  error\n", 4, "ID", maxlen, "module", maxlen, item_title);
        object_split(4);rt_kprintf(" ");object_split(maxlen);rt_kprintf(" ");object_split(maxlen);rt_kprintf(" ");
        rt_kprintf(                  "---  ------- ---------- ----------  ------  ---------- ---\n");
    for (node = list->next; node != list; node = node->next)
    {
        struct rt_thread    *thread = RT_NULL;
        struct rt_list_node *n = RT_NULL;

        module = (struct rt_dlmodule *)(rt_list_entry(node, struct rt_object, list));

        for (n = module->t_grp.next; n != &module->t_grp; n = n->next)
        {
            thread = rt_list_entry(n, struct rt_thread, sibling);

            rt_kprintf("%4d %-*.*s ", 0 , maxlen, RT_NAME_MAX, module->parent.name);
            print_thread_info(thread, maxlen);
        }
    }

    return 0;
}
MSH_CMD_EXPORT(list_module, list modules in system);

int kill(int argc, char **argv)
{
    struct rt_dlmodule *module = RT_NULL;

    module = dlmodule_find(argv[1]);
    if(module != RT_NULL)
    {
        dlmodule_delete(module);
    }
    else
    {
        rt_kprintf("Can't find module %s\n", argv[1]);
    }

    return 0;
}
MSH_CMD_EXPORT(kill, delete modules in system);

#endif
