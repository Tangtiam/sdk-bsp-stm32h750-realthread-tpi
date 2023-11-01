/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-03     tyustli       the first version
 */

#include <urpc.h>
#include <fal.h>

#define CRC32_READ_SIZE   64

extern rt_uint32_t _udbd_crc32_calculate(rt_uint32_t crc, rt_uint8_t *buf, size_t size);

int fal_crc32_calculate(const struct fal_partition *part, rt_uint32_t addr, size_t size)
{
    int crc32 = 0xFFFFFFFF;
    rt_uint8_t *r_buf = RT_NULL;
    size_t r_size = 0;
    int result = 0;
    rt_uint32_t offset_addr = addr;

    r_buf = rt_malloc(CRC32_READ_SIZE);
    if (r_buf == RT_NULL)
    {
        return -1;
    }

    while (size)
    {
        if (r_size < size)
        {
            r_size = CRC32_READ_SIZE;
        }
        else
        {
            r_size = size;
        }
        result = fal_partition_read(part, offset_addr, r_buf, r_size);
        if (result < 0)
        {
            return -1;
        }
        crc32 = _udbd_crc32_calculate(crc32, r_buf, r_size);
        offset_addr += r_size;
        size -= r_size;
    }

    return crc32;
}

static const char *flash_table_find(const char* name)
{
    const struct fal_partition *flash_table = RT_NULL;
    flash_table = fal_partition_find(name);
    if(flash_table != RT_NULL)
    {
        return flash_table->name;
    }
    return RT_NULL;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(flash_table_find, fal_partition_find)

static int flash_table_erase(const char *name, uint32_t addr, size_t size)
{
    int ret = -1;
    const struct fal_partition *flash_table = RT_NULL;
    flash_table = fal_partition_find(name);
    if(flash_table)
    {
        ret = fal_partition_erase(flash_table,addr,size);
        return ret;
    }
    return ret;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(flash_table_erase, fal_partition_erase)

static int flash_table_write(const char *name, uint32_t addr, const uint8_t *buf, size_t size)
{
    int ret = -1;
    const struct fal_partition *flash_table = RT_NULL;
    flash_table = fal_partition_find(name); 
    if(flash_table)
    {
        ret = fal_partition_write(flash_table,addr,buf,size);
        return ret;
    }
    return ret;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(flash_table_write, fal_partition_write)

static int flash_table_read(const char *name, uint32_t addr, uint8_t *buf, size_t size)
{
    int ret = -1;
    const struct fal_partition *flash_table = RT_NULL;
    flash_table = fal_partition_find(name);
    if(flash_table)
    {
        ret = fal_partition_read(flash_table,addr,buf,size);
        return ret;
    }
    return ret;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(flash_table_read, fal_partition_read)

static int flash_crc32_caculate(const char *name, rt_uint32_t addr, size_t size)
{
    int crc32 = -1;
    const struct fal_partition *flash_table = RT_NULL;
    flash_table = fal_partition_find(name);
    if(flash_table)
    {
        crc32 = fal_crc32_calculate(flash_table,addr,size);
        return crc32;
    }
    return crc32;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(flash_crc32_caculate, fal_crc32_calculate)

/******************** end of file *********************/
