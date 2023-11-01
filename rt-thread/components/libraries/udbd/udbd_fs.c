/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-11     tyustli      the first version
 */

#include <udbd.h>
#if RT_VER_NUM <= 0x40003
#include <dfs_posix.h>
#else
#include <dfs_file.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define DBG_TAG                       "udbd.fs"
#define DBG_LVL                        DBG_LOG
#include <rtdbg.h>

#define UDBD_FS_INVALID_DATA    (~0x0UL)

static struct rt_mutex udbd_fs_mutex;

static rt_base_t udbd_fs_object_lock(void)
{
    rt_mutex_take(&udbd_fs_mutex, RT_WAITING_FOREVER);
    return (rt_base_t)&udbd_fs_mutex;
}

static void udbd_fs_object_unlock(rt_base_t ctx)
{
    rt_mutex_release((struct rt_mutex *)ctx);
}

static rt_list_t udbd_fs_object_list = RT_LIST_OBJECT_INIT(udbd_fs_object_list);

struct udbd_fs_object;
typedef void (*udbd_fs_object_free_fun)(struct udbd_fs_object *);

struct udbd_fs_object
{
    int size;               // object memory space size
    rt_list_t node;         // object linked list node
    udbd_fs_object_free_fun object_free;    // object destructor
};

static void __object_free(struct udbd_fs_object *object)
{
    rt_base_t level;

    level = udbd_fs_object_lock();
    rt_list_remove(&object->node);
    object->object_free = (udbd_fs_object_free_fun)UDBD_FS_INVALID_DATA;
    object->size = UDBD_FS_INVALID_DATA;
    udbd_fs_object_unlock(level);
    rt_free(object);
}

static struct udbd_fs_object *udbd_fs_object_new(int size)
{
    struct udbd_fs_object *object;
    rt_base_t level;

    object = rt_malloc(size);
    if (object)
    {
        rt_memset(object, UDBD_FS_INVALID_DATA, size);
        rt_list_init(&object->node);
        object->size = size;
        object->object_free = __object_free;
        level = udbd_fs_object_lock();
        rt_list_insert_before(&udbd_fs_object_list, &object->node);
        udbd_fs_object_unlock(level);
    }
    return object;
}

static void udbd_fs_object_free(struct udbd_fs_object *object)
{
    if (object)
    {
        object->object_free(object);
    }
}

static rt_bool_t udbd_fs_object_isempty(void)
{
    return rt_list_isempty(&udbd_fs_object_list);
}

static void udbd_fs_object_free_redirect(struct udbd_fs_object *object,
    udbd_fs_object_free_fun object_free)
{
    if (object)
    {
        object->object_free = object_free;
    }
}

static struct rt_timer cache_time;

struct udbd_fs_cache_object;
typedef int (*udbd_fs_cache_timeout_fun)(struct udbd_fs_cache_object *object);

struct udbd_fs_cache_object
{
    struct udbd_fs_object parent;
    rt_tick_t tick;     /* Recent access timestamp */
    rt_tick_t timeout;
    udbd_fs_cache_timeout_fun timeout_fun;
};

static void __cache_object_free(struct udbd_fs_cache_object *object)
{
    object->timeout_fun = (udbd_fs_cache_timeout_fun)UDBD_FS_INVALID_DATA;
    object->timeout = UDBD_FS_INVALID_DATA;
    object->tick = UDBD_FS_INVALID_DATA;
    // free parent
    __object_free(&object->parent);
}

static struct udbd_fs_cache_object *udbd_fs_cache_object_new(int size, udbd_fs_cache_timeout_fun timeout_fun)
{
    struct udbd_fs_cache_object *cache_object = RT_NULL;
    rt_base_t level;

    // When the list is empty, start the timer
    level = udbd_fs_object_lock();
    if (udbd_fs_object_isempty())
    {
        rt_timer_start(&cache_time);
    }
    udbd_fs_object_unlock(level);
    // init parent
    cache_object = (struct udbd_fs_cache_object *)udbd_fs_object_new(size);
    if (cache_object)
    {
        // init myself
        level = udbd_fs_object_lock();
        cache_object->timeout = UDBD_FS_INVALID_DATA;
        cache_object->tick = rt_tick_get();
        cache_object->timeout_fun = timeout_fun;
        udbd_fs_object_free_redirect((struct udbd_fs_object *)cache_object,
                (udbd_fs_object_free_fun)__cache_object_free);
        udbd_fs_object_unlock(level);
    }
    return cache_object;
}

static void udbd_fs_cache_timeout_check(void *user)
{
    rt_base_t level;
    rt_list_t *l, *tmp;
    struct udbd_fs_cache_object *fd_cache_object = RT_NULL;
    rt_tick_t tick;

    tick = rt_tick_get();
    /* find timeout node */
    level = udbd_fs_object_lock();
    rt_list_for_each_safe(l, tmp, &udbd_fs_object_list)
    {
        fd_cache_object = (struct udbd_fs_cache_object *)rt_list_entry(l, struct udbd_fs_object, node);
        if ((fd_cache_object->timeout != UDBD_FS_INVALID_DATA &&
            fd_cache_object->timeout < RT_TICK_MAX / 2 &&
            tick - fd_cache_object->tick > fd_cache_object->timeout))
        {
            /* handle timeout node */
            fd_cache_object->timeout_fun(fd_cache_object);
        }
    }
    udbd_fs_object_unlock(level);
    /* When the list is empty, stop the timer */
    level = udbd_fs_object_lock();
    if (udbd_fs_object_isempty())
    {
        rt_timer_stop(&cache_time);
    }
    udbd_fs_object_unlock(level);
}

static void udbd_fs_cache_keeplive(struct udbd_fs_cache_object *cache_object)
{
    cache_object->tick = rt_tick_get(); /* update tick */
}

static void udbd_fs_cache_timeout_set(struct udbd_fs_cache_object *cache_object, rt_tick_t timeout)
{
    cache_object->timeout = timeout; /* set timeout */
}

/**
 * This function will create dir accroding to the incoming path.
 * 
 * @param path : current directory.
 * @param mode : create dir mode.
 * 
 * @return -1: dir exist; -2: create fail; others: success
 */
static int udbd_fs_mkdir(const char *path, mode_t mode)
{
    rt_err_t result = RT_EOK;

    if (mkdir(path, mode) != 0)
    {
        result = rt_get_errno();
        if (result == -EEXIST)
        {
            return -1;
        }
        else if (result < 0)
        {
            return -2;
        }
    }
    
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_mkdir, mkdir_r)

/**
 * This function will judge the incoming pathname is file or dir
 * 
 * @param pathname : current directory.
 * 
 * @return 0: fail; 1: DIR; 2: FILE 
 */
static int udbd_fs_isdir(char *pathname)
{
    char *path;
    struct stat buf;
    int ret;
    int result = 0;
    
    if (pathname == NULL)
    {
#ifdef DFS_USING_WORKDIR
        extern char working_directory[];
        /* open current working directory */
        path = rt_strdup(working_directory);
#else
        path = rt_strdup("/");
#endif
        if (path == NULL)
        {
            return 0; /* out of memory */
        }
    }
    else
    {
        path = pathname;
    }

    ret = stat(path, &buf);
    if (ret == 0)
    {
        if (S_ISDIR(buf.st_mode))
        {
            result = 1;
        }
        else
        {
            result =  2;
            goto _exit;
        }
    }
    else
    {
        result = 0;
        LOG_W("%s not fonud", path);
    }

_exit:
    if (pathname == NULL)
    {
        rt_free(path);
    }
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_isdir, is_dir)

static char *path_cat(char *path_list, const char *path, int *path_list_len)
{
    int i = 0;

    for (; path[i] != '\0' && *path_list_len > 1; i++)
    {
        path_list[i] = path[i];
        *path_list_len = *path_list_len - 1;
    } 
    if (*path_list_len > 0)
    {
        path_list[i] = '\0';
    }
    return &path_list[i];
}

/**
 * This function will list the files and dirs according to the incoming pathname
 * 
 * @param pathname : current directory.
 * @param path_list: files and dirs name in current directory, such as: main.c:FIL&test:DIR.
 * @param path_len : path_list length.
 * 
 * @return the number of files and folders in the current directory : success
 *         others: faile
 */
static int udbd_fs_lsdir(char *pathname, char *path_list, int path_len)
{
    int result = 0;
    char *fullpath, *path;
    struct stat stat_buf;
    DIR *dir = RT_NULL;
    struct dirent *ent = RT_NULL;
    int left_len = path_len;

    if(path_len <= 0)
    {
        LOG_E("fs_lsdir: path_list space len(%d) <= 0", path_len);
        return -1;
    }
    memset(path_list, 0, path_len);

    if (pathname == NULL)
    {
#ifdef DFS_USING_WORKDIR
        extern char working_directory[];
        /* open current working directory */
        path = rt_strdup(working_directory);
#else
        path = rt_strdup("/");
#endif
        if (path == NULL)
        {
            return -1; /* out of memory */
        }
    }
    else
    {
        path = pathname;
    }
    
    /* list directory */
    if ((dir = opendir(path)) != RT_NULL)
    {
        char *ptr = path_list;
        while ((ent = readdir(dir)) != RT_NULL)
        {
            memset(&stat_buf, 0, sizeof(struct stat));

            /* build full path for each file */
            fullpath = dfs_normalize_path(path, ent->d_name);
            if (fullpath == NULL)
            {
                break;
            }
            if (stat(fullpath, &stat_buf) == 0)
            {
                ptr = path_cat(ptr, ent->d_name, &left_len);
                if (S_ISDIR(stat_buf.st_mode))
                {
                    ptr = path_cat(ptr, ":DIR&", &left_len);
                    result++;
                }
                else
                {
                    ptr = path_cat(ptr, ":FIL&", &left_len);
                    result++;
                }
                if(left_len < 0)
                {
                    LOG_E("fs_lsdir: path_list not enough space!");
                    rt_free(fullpath);
                    result = -1;
                    break;
                }
            }
            else
            {
                LOG_E("BAD file: %s", ent->d_name);
            }
            rt_free(fullpath);
        }

        closedir(dir);
    }
    else
    {
        result = -1;
        LOG_W("No such directory");
        goto _exit;
    } 
    
_exit:
    if (pathname == NULL)
    {
        rt_free(path);
    }
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_lsdir, lsdir_r)

struct urpc_blob* udbd_fs_lsdir_svc(struct urpc_blob* input)
{
    int result = 0;
    struct stat stat_buf;
    DIR *dir = RT_NULL;
    struct dirent *ent = RT_NULL;
    char *pathname = input->buf;
    char *fullpath = RT_NULL;
    char *rootpath = RT_NULL;

    char *output_str = RT_NULL;
    char* output_str_dup = RT_NULL;
    struct urpc_blob* output_blob = RT_NULL;

    cJSON* json_root = cJSON_CreateObject();
    cJSON* json_list = cJSON_CreateArray();
    cJSON_AddItemToObject(json_root, "array", json_list);

    if (pathname == NULL)
    {
#ifdef DFS_USING_WORKDIR
        extern char working_directory[];
        /* open current working directory */
        rootpath = rt_strdup(working_directory);
#else
        rootpath = rt_strdup("/");
#endif
    }
    else
    {
        rootpath = pathname;
    }

    /* list directory */
    if ((dir = opendir(rootpath)) != RT_NULL)
    {
        while ((ent = readdir(dir)) != RT_NULL)
        {
            memset(&stat_buf, 0, sizeof(struct stat));

            /* build full path for each file */
            fullpath = dfs_normalize_path(rootpath, ent->d_name);
            if (fullpath == NULL)
            {
                break;
            }
            if (stat(fullpath, &stat_buf) == 0)
            {
                cJSON* json_item = cJSON_CreateObject();
                if (S_ISDIR(stat_buf.st_mode))
                {
                    cJSON_AddStringToObject(json_item, ent->d_name, "DIR");
                }
                else
                {
                    cJSON_AddStringToObject(json_item, ent->d_name, "FIL");
                }
                cJSON_AddItemToArray(json_list, json_item);
                result++;
            }
            else
            {
                LOG_E("BAD file: %s", ent->d_name);
            }
            rt_free(fullpath);
        }
        closedir(dir);
    }
    else
    {
        result = -1;
        LOG_W("No such directory: %s", rootpath);
    }
    cJSON_AddNumberToObject(json_root, "count", result);

    output_str = cJSON_PrintUnformatted(json_root);
    RT_ASSERT(output_str);
    /* create a copy */
    output_str_dup = rt_strdup(output_str);
    RT_ASSERT(output_str_dup);
    /* clean the json object */
    cJSON_free(output_str);
    cJSON_Delete(json_root);
    /* create the result, the output blob will be free when service execute finished */
    output_blob = (struct urpc_blob*)rt_malloc(sizeof(struct urpc_blob));
    urpc_blob_make(output_blob, output_str_dup, rt_strlen(output_str_dup), rt_strlen(output_str_dup));
    RT_ASSERT(output_blob);

    return output_blob;
}
URPC_SVC_STATIC_REGISTER_ALIAS(udbd_fs_lsdir_svc, lsdir_svc);

/**
 * This function will sync file according to the incoming pathname.
 *
 * @param pathname: file path.
 * @param crc     : point to the file crc32 value.
 *
 * @return 0: success; -5: file open fail; -10: malloc fail
 */
static int calc_file_crc32(char *pathname, uint8_t crc[4])
{
    #define FILE_READ_SIZE    4096
    extern rt_uint32_t _udbd_crc32_calculate(rt_uint32_t crc, rt_uint8_t *buf, rt_size_t size);

    int fd = -1;
    int crc32 = 0xFFFFFFFF;
    rt_uint8_t *buffer = RT_NULL;
    uint32_t length;
    
    if ((fd = open(pathname, O_RDONLY)) < 0)
    {
        LOG_D("remote file: %s not exist", pathname);
        return -RT_EINVAL;
    }
    
    buffer = rt_malloc(FILE_READ_SIZE);
    if (buffer == RT_NULL)
    {
        LOG_E("low memory");
        return -RT_ENOMEM;
    }
    
    do
    {
        memset(buffer, 0, FILE_READ_SIZE);
        length = read(fd, buffer, FILE_READ_SIZE);
        if (length > 0)
        {
            crc32 = _udbd_crc32_calculate(crc32, buffer, length);
        } 
    } 
    while (length > 0);
    
    rt_free(buffer);
    close(fd);
    
    crc[0] = (crc32 >> 24) & 0xFF;
    crc[1] = (crc32 >> 16) & 0xFF;
    crc[2] = (crc32 >> 8)  & 0xFF;
    crc[3] = crc32 & 0xFF;
    
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(calc_file_crc32, calc_file_crc32)

/**
 * This function will remove file according to the incoming pathname.
 * 
 * @param pathname: file or dir path.
 * 
 * @return 0: remove success; others: fail
 */
static int udbd_fs_remove(char *pathname)
{
    struct stat s;
    char f = 0, v = 0;

    if (stat(pathname, &s) == 0)
    {
        if (s.st_mode & S_IFDIR)
        {
            return rmdir(pathname);
        }
        else if (s.st_mode & S_IFREG)
        {
            if (unlink(pathname) != 0)
            {
                if (f == 0)
                {
                    LOG_E("cannot remove '%s'", pathname);
                    return -1;
                }
            }
            else if (v)
            {
                LOG_D("removed '%s'", pathname);
            }
        }
    }
    else if (f == 0)
    {
        LOG_W("cannot remove '%s': No such file or directory", pathname);
        return -1;
    }
    
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_remove, remove)

struct udbd_fs_file_object
{
    struct udbd_fs_cache_object parent;
    int fd;
};

#define FD_CACHE_TIMEOUT (20 * 1000)

static int udbd_fs_file_timeout(struct udbd_fs_cache_object *object)
{
    struct udbd_fs_file_object *file_object = (struct udbd_fs_file_object *)object;

    if (file_object->fd == UDBD_FS_INVALID_DATA)
    {
        return RT_FALSE;
    }
    else
    {
        udbd_fs_object_free((struct udbd_fs_object *)file_object);
        return RT_TRUE;
    }
}

static void __file_object_free(struct udbd_fs_file_object *object)
{
    int fd;
    int res;

    fd = object->fd;
    object->fd = UDBD_FS_INVALID_DATA;
    if (fd != UDBD_FS_INVALID_DATA && fd >= 0)
    {
        res = close(fd);
        if (res != 0)
        {
            LOG_E("close fd (%d) error:%d", fd, rt_get_errno());
        }
        else
        {
            LOG_D("close fd:%d", fd);
        }
    }
    // free parent
    __cache_object_free(&object->parent);
}

static struct udbd_fs_file_object* udbd_fs_file_object_new(int size, const char *file, int flags)
{
    int fd;
    struct udbd_fs_file_object *file_object;
    rt_base_t level;

    // init parent
    file_object = (struct udbd_fs_file_object *)udbd_fs_cache_object_new(size, udbd_fs_file_timeout);
    if (file_object)
    {
        fd = open(file, flags);
        if (fd < 0)
        {
            LOG_W("open file:%s failed. err:%d", file, rt_get_errno());
            // free parent
            udbd_fs_object_free((struct udbd_fs_object *)file_object);
            return RT_NULL;
        }
        LOG_D("open fd:%d", fd);
        level = udbd_fs_object_lock();
        file_object->fd = fd;
        udbd_fs_object_free_redirect((struct udbd_fs_object *)file_object,
            (udbd_fs_object_free_fun)__file_object_free);
        udbd_fs_object_unlock(level);
    }
    return file_object;
}

static void udbd_fs_file_timeout_set(struct udbd_fs_file_object* object, int timeout)
{
    if (object)
    {
        udbd_fs_cache_timeout_set(&object->parent, timeout);
    }
}

static int udbd_fs_file_vfd_get(struct udbd_fs_file_object* object)
{
    if (object)
    {
        // TODO: Return virtual file
        return object->fd;
    }
    else
    {
        return -1;
    }
}

static struct udbd_fs_file_object* udbd_fs_file_object_get(int vfd)
{
    rt_base_t level;
    rt_list_t *l, *tmp;
    struct udbd_fs_file_object *file_object = RT_NULL;

    if (vfd >= 0)
    {
        /* Update file handle usage time */
        level = udbd_fs_object_lock();
        rt_list_for_each_safe(l, tmp, &udbd_fs_object_list)
        {
            file_object = (struct udbd_fs_file_object *)rt_list_entry(l, struct udbd_fs_object, node);
            if (file_object->fd == vfd)
            {
                udbd_fs_cache_keeplive(&file_object->parent);
                break;
            }
        }
        udbd_fs_object_unlock(level);
    }
    return file_object;
}

static int udbd_fs_open(const char *file, int flags)
{
    struct udbd_fs_file_object *file_object;
    int f = 0;

#define UDBD_FSO_RDONLY 0000
#define UDBD_FSO_WRONLY 0001
#define UDBD_FSO_RDWR   0002
#define UDBD_FSO_CREAT  0100
#define UDBD_FSO_APPEND 0200
#define UDBD_FSO_TRUNC  01000

    if (flags & UDBD_FSO_RDONLY)
        f |= O_RDONLY;
    if (flags & UDBD_FSO_WRONLY)
        f |= O_WRONLY;
    if (flags & UDBD_FSO_RDWR)
        f |= O_RDWR;
    if (flags & UDBD_FSO_CREAT)
        f |= O_CREAT;
    if (flags & UDBD_FSO_APPEND)
        f |= O_APPEND;
    if (flags & UDBD_FSO_TRUNC)
        f |= O_TRUNC;
    file_object = udbd_fs_file_object_new(sizeof(struct udbd_fs_file_object), file, f);
    udbd_fs_file_timeout_set(file_object, FD_CACHE_TIMEOUT);
    return udbd_fs_file_vfd_get(file_object);
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_open, open)

/* NOTICE: using `int length` instead of `off_t length` for fix 64 bit platform */
static int udbd_fs_ftruncate(int vfd, int length)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    return ftruncate(file_object->fd, (off_t)length);
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_ftruncate, ftruncate)

static int udbd_fs_write(int vfd, const void *buf, size_t len)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    return write(file_object->fd, buf, len);
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_write, write)

static int udbd_fs_close(int vfd)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    udbd_fs_object_free((struct udbd_fs_object *)file_object);
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_close, close)

/* NOTICE: using `int length` instead of `off_t length` for fix 64 bit platform */
static int udbd_fs_lseek(int vfd, int offset, int whence)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    return lseek(file_object->fd, (off_t)offset, whence);
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_lseek, lseek)

static int udbd_fs_read(int vfd, void *buf, size_t len)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    return read(file_object->fd, buf, len);
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_read, read)

static int read_by_offset(int vfd, off_t offset, void *buf, size_t len)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    int result = lseek(file_object->fd, offset, SEEK_SET);
    if (result >= 0)
    {
        return read(file_object->fd, buf, len);
    }
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(read_by_offset, read_by_offset)

static int write_by_offset(int vfd, off_t offset, void *buf, size_t len)
{
    struct udbd_fs_file_object *file_object;

    file_object = udbd_fs_file_object_get(vfd);
    if (file_object == RT_NULL)
    {
        return -1;
    }
    int result = udbd_fs_lseek(file_object->fd, offset, SEEK_SET);
    if (result >= 0)
    {
        return udbd_fs_write(file_object->fd, buf, len);
    }
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(write_by_offset, write_by_offset)

static int udbd_fs_statfs(const char *path, size_t *f_bsize, size_t *f_blocks, size_t *f_bfree)
{
    struct statfs buf = { 0 };
    int res;

    res = statfs(path, &buf);
    if (res >= 0)
    {
        if (f_bsize != RT_NULL)
            *f_bsize = buf.f_bsize;   /* block size */
        if (f_blocks != RT_NULL)
            *f_blocks = buf.f_blocks;  /* total data blocks in file system */
        if (f_bfree != RT_NULL)
            *f_bfree = buf.f_bfree;   /* free blocks in file system */
    }
    return res;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_fs_statfs, statfs)

#ifdef UDBD_USING_FILE_COMPRESS 
#if MCF_REQ_THREAD_POOL_SIZE < 2
#error "MCF_REQ_THREAD_POOL_SIZE min number is 2"
#endif
#include "zlib.h"

struct udbd_fs_file_compress_object
{
    struct udbd_fs_file_object parent;
    z_stream strm;
    void *zlib_out;
    int chunk;
};

static void __file_compress_object_free(struct udbd_fs_file_compress_object *object)
{
    struct udbd_fs_file_compress_object *compress_object = (struct udbd_fs_file_compress_object *)object;
    (void)inflateEnd(&compress_object->strm);
    rt_free(object->zlib_out);
    object->zlib_out = RT_NULL;
    // free parent
    __file_object_free((struct udbd_fs_file_object *)object);
}

static struct udbd_fs_file_compress_object *udbd_fs_file_compress_object_new(
    int size, const char *file, int flags)
{
    #define CHUNK 2048
    struct udbd_fs_file_compress_object *object;

    // init parent
    object = (struct udbd_fs_file_compress_object *)udbd_fs_file_object_new(size, file, flags);
    if (object)
    {
        object->zlib_out = rt_malloc(CHUNK);
        if (object->zlib_out == RT_NULL)
        {
            udbd_fs_object_free((struct udbd_fs_object *)object);
        }
        object->chunk = CHUNK;
        // init compress object
        rt_memset(&object->strm, 0, sizeof(object->strm));
        if (inflateInit(&object->strm) != Z_OK)
        {
            LOG_E("zlib inflateinit failed");
            rt_free(object->zlib_out);
            udbd_fs_object_free((struct udbd_fs_object *)object);
            return RT_NULL;
        }
        udbd_fs_object_free_redirect((struct udbd_fs_object *)object,
            (udbd_fs_object_free_fun)__file_compress_object_free);
    }
    return object;
}

/**
 * This function will initialize an incremental inflate context with the specified.
 * 
 * @param file  : the file name.
 * @param flags : the file open flags.
 * @param buf   : the file zlib stream struct address.
 * 
 * @return the non-negative integer on successful open, others for failed
 */
static int decompress_open(const char *file, int flags, uint8_t *buf)
{
    struct udbd_fs_file_compress_object *object;

    object = udbd_fs_file_compress_object_new(sizeof(struct udbd_fs_file_compress_object), file, flags);
    udbd_fs_file_timeout_set(&object->parent, FD_CACHE_TIMEOUT);
    return udbd_fs_file_vfd_get(&object->parent);
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(decompress_open, dec_open)

/**
 * This function will decompress buf and write decompress date to file.
 * 
 * @param fd      : the file descriptor.
 * @param buf     : the data buffer to be decompressed.
 * @param len     : the data buffer length.
 * @param stream  : the file zlib stream struct address.
 * 
 * @return 0 on successful, others on failed.
 */
static int decompress_write(int vfd, const void *buf, size_t len, uint32_t *stream)
{
    struct udbd_fs_file_compress_object *object;
    rt_uint8_t *zlib_out = RT_NULL;
    int result = 0, have = 0, chunk, totle = 0;
    z_stream *strm;

    RT_ASSERT(len != 0);

    object = (struct udbd_fs_file_compress_object *)udbd_fs_file_object_get(vfd);
    if (object == RT_NULL)
    {
        return -1;
    }

    /* get current compress file zlib stream */
    zlib_out = object->zlib_out;
    chunk = object->chunk;
    strm = &object->strm;
    strm->avail_in = len;
    strm->next_in = (unsigned char *)buf;

    /* exit when the compressed data is decompressed */
    while (1)
    {
        strm->avail_out = chunk;
        strm->next_out = zlib_out;

        result = inflate(strm, Z_NO_FLUSH);
        if ((result == Z_NEED_DICT) || (result == Z_DATA_ERROR) || (result == Z_MEM_ERROR) || (result == Z_STREAM_ERROR))
        {
            LOG_E("zlib inflate failed, error code: %d", result);
            result = -RT_ERROR;
            goto __exit;
        }
        have = chunk - strm->avail_out;
        if (have > 0)
        {
            /* write the decompressed data to file */
            if (udbd_fs_write(vfd, zlib_out, have) != have)
            {
                LOG_E("zlib write to decompress file failed");
                result = -RT_ERROR;
                goto __exit;
            }
            totle = totle + have;
        }
        /* decompress a package complete */
        if (strm->avail_out != 0)
        {
            result = totle;
            break;
        }
    }

__exit:
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(decompress_write, dec_write)

static int decompress_write_by_offset(int vfd, off_t offset, const void *buf, size_t len, uint32_t *stream)
{
    int result = udbd_fs_lseek(vfd, offset, SEEK_SET);
    if (result >= 0)
    {
        return decompress_write(vfd, buf, len, stream);
    }
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(decompress_write_by_offset, dec_write_by_offset)

/**
 * This function will close the decompress file.
 * 
 * @param fd      : the file descriptor.
 * @param stream  : the file zlib stream struct address.
 * 
 * @return 0 on successful, others on failed.
 */
static int decompress_close(int vfd, uint32_t *stream)
{
    struct udbd_fs_file_compress_object *object;

    object = (struct udbd_fs_file_compress_object *)udbd_fs_file_object_get(vfd);
    if (object == RT_NULL)
    {
        return -1;
    }
    udbd_fs_object_free((struct udbd_fs_object *)object);
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(decompress_close, dec_close)
#endif /* UDBD_USING_FILE_COMPRESS */

static int _udbd_fs_init(void)
{
    rt_mutex_init(&udbd_fs_mutex, "udbd_fs", RT_IPC_FLAG_PRIO);
    rt_timer_init(&cache_time, "udbd_fs", udbd_fs_cache_timeout_check, RT_NULL,
        rt_tick_from_millisecond(500), RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    return 0;
}

static int _udbd_fs_deinit(void)
{
    rt_mutex_detach(&udbd_fs_mutex);
    rt_timer_detach(&cache_time);
    return 0;
}
UDBD_MODULE_EXPORT(udbd_fs_module, _udbd_fs_init, _udbd_fs_deinit, 5);

/********************** end of file ********************/
