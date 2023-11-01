/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-28     armink       the first version
 */

#include <rtthread.h>
#include <urpc.h>

#define DBG_TAG    "urpc.ffi"
#define DBG_LVL    DBG_INFO//DBG_LOG
#include <rtdbg.h>

#define ARRAY             (0x80)
#define EDITABLE          (0x40)
#define TYPE_LEN          (1)
#define ARRAY_TYPE_LEN    (TYPE_LEN + 2)

typedef rt_base_t ffi_arg;

static rt_uint8_t u8_sum(rt_uint8_t a, rt_uint8_t b)
{
    return a + b;
}
URPC_FFI_FUNC_STATIC_REGISTER(u8_sum)

static rt_uint32_t u32_sum(rt_uint32_t a, rt_uint32_t b)
{
    return a + b;
}
URPC_FFI_FUNC_STATIC_REGISTER(u32_sum)

static rt_size_t write_test(rt_off_t off, rt_uint8_t *buf, rt_size_t len)
{
    rt_size_t ret = off;

    for (rt_size_t i = 0; i < len; i++)
    {
        ret += buf[i];
    }

    return ret;
}
URPC_FFI_FUNC_STATIC_REGISTER(write_test)

static rt_size_t read_test(rt_off_t off, rt_uint8_t *buf, rt_size_t len)
{
    rt_size_t ret = off;

    for (rt_size_t i = 0; i < len; i++)
    {
        buf[i] = (rt_uint8_t)(off + i);
        ret += buf[i];
    }

    return ret;
}
URPC_FFI_FUNC_STATIC_REGISTER(read_test)

static ffi_arg get_arg(rt_uint8_t *args, rt_uint8_t *arg_type, rt_uint16_t *value_len, rt_uint16_t *arg_len)
{
    ffi_arg ret = 0;
    *arg_type = args[0];

    if ((*arg_type & ARRAY) == ARRAY)
    {
        rt_memcpy(value_len, args + 1, sizeof(rt_uint16_t));

        if ((*arg_type & EDITABLE) == EDITABLE)
        {
            *arg_len = ARRAY_TYPE_LEN;
            ret = (ffi_arg)rt_malloc(*value_len);
            if (ret == 0) {
                LOG_W("low memory");
            }
        }
        else
        {

            *arg_len = ARRAY_TYPE_LEN + *value_len;
            ret = (ffi_arg) (args + ARRAY_TYPE_LEN);
        }
    }
    else
    {
        /* only the lower 4 bits contains its value size */
        *value_len = (*arg_type) & 0x0f;
        if ((*arg_type & EDITABLE) == EDITABLE)
        {
            *arg_len = TYPE_LEN;
            ret = (ffi_arg)rt_malloc(*value_len);
            if (ret == 0)
            {
                LOG_W("low memory");
            }
        }
        else
        {
            *arg_len = TYPE_LEN + *value_len;
            if (*value_len == 1) {
                ret = *((rt_uint8_t *)(args + TYPE_LEN));
            } else if (*value_len == 2) {
                rt_uint8_t buf[2];
                buf[0] = *(args + TYPE_LEN + 0);
                buf[1] = *(args + TYPE_LEN + 1);
                rt_memcpy(&ret, buf, sizeof(buf));
            } else if (*value_len == 4) {
                rt_uint8_t buf[4];
                /* avoid the unalign access */
                buf[0] = *(args + TYPE_LEN + 0);
                buf[1] = *(args + TYPE_LEN + 1);
                buf[2] = *(args + TYPE_LEN + 2);
                buf[3] = *(args + TYPE_LEN + 3);
                rt_memcpy(&ret, buf, sizeof(buf));
            } else if (*value_len == 8) {
                rt_uint8_t buf[8];
                /* avoid the unalign access */
                buf[0] = *(args + TYPE_LEN + 0);
                buf[1] = *(args + TYPE_LEN + 1);
                buf[2] = *(args + TYPE_LEN + 2);
                buf[3] = *(args + TYPE_LEN + 3);
                buf[4] = *(args + TYPE_LEN + 4);
                buf[5] = *(args + TYPE_LEN + 5);
                buf[6] = *(args + TYPE_LEN + 6);
                buf[7] = *(args + TYPE_LEN + 7);
                rt_memcpy(&ret, buf, sizeof(buf));
            }
        }
    }

    return ret;
}

static rt_ubase_t ffi_func_find(const char *func_name)
{
    char svc_name[64];
    struct urpc_svc *service = NULL;
    extern struct urpc_svc *_urpc_svc_find(const char *name);

    rt_snprintf(svc_name, sizeof(svc_name), "ffi.%s", func_name);
    service = _urpc_svc_find(svc_name);

    if (service)
    {
        return (rt_ubase_t)service->fn;
    }

    return 0;
}

static struct urpc_blob *ffi_svc(struct urpc_blob *input)
{
#define MAX_ARGS   10
    extern rt_uint32_t _urpc_ffi_func_find(const char *func_name);

    typedef ffi_arg(*f10_t)(ffi_arg, ffi_arg, ffi_arg, ffi_arg, ffi_arg, ffi_arg, ffi_arg, ffi_arg, ffi_arg, ffi_arg);
    ffi_arg args[MAX_ARGS] = { 0 }, ret_val = 0;
    rt_uint8_t *raw = input->buf, char_bak, arg_type[MAX_ARGS] = { 0 };
    rt_size_t i = 0, blob_len = 0;
    const char *func_name;
    rt_size_t offset = 0;
    rt_uint16_t value_len[MAX_ARGS] = { 0 }, arg_len, sym_len;
    struct urpc_blob* ret_blob = NULL;
    /* get function name */
    func_name = (const char *)get_arg(raw, &arg_type[0], &sym_len, &arg_len);
    char_bak = raw[arg_len];
    raw[arg_len] = '\0';
    /* get function address */
    void *func = (void *)ffi_func_find(func_name);

    LOG_D("exec ffi service %s", func_name);

    if (func)
    {
        raw[arg_len] = char_bak;
        raw += arg_len;
        for (i = 0; i < sizeof(args) / sizeof(args[0]) && offset + ARRAY_TYPE_LEN + sym_len < input->len; i++)
        {
            args[i] = get_arg(raw + offset, &arg_type[i], &value_len[i], &arg_len);
            offset += arg_len;
        }
        ret_val = ((f10_t) (func))(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]);

        /* calc the editable parameter total length */
        for (i = 0; i < MAX_ARGS; i ++)
        {
            if ((arg_type[i] & EDITABLE) == EDITABLE)
            {
                if ((arg_type[i] & ARRAY) == ARRAY)
                    blob_len += ARRAY_TYPE_LEN + value_len[i];
                else
                    blob_len += TYPE_LEN + value_len[i];
            }
        }
        /* alloc the return value and editable parameter blob as response */
        ret_blob = urpc_blob_alloc(1 + sizeof(ffi_arg) + blob_len);
        /* generate the return result to rt_uint32_t type */
        if (sizeof(void *) == 4)
            *((rt_uint8_t *)ret_blob->buf + 0) = 0x04;
        else if (sizeof(void *) == 8)
            *((rt_uint8_t *)ret_blob->buf + 0) = 0x08;
        rt_memcpy((rt_uint8_t *)ret_blob->buf + 1, &ret_val, sizeof(ffi_arg));
        raw = (rt_uint8_t *)ret_blob->buf + 1 + sizeof(ffi_arg);

        for (i = 0; i < MAX_ARGS; i++)
        {
            if ((arg_type[i] & EDITABLE) == EDITABLE)
            {
                rt_memcpy(raw, &arg_type[i], 1);
                raw ++;
                if ((arg_type[i] & ARRAY) == ARRAY)
                {
                    rt_memcpy(raw, &value_len[i], 2);
                    raw += 2;
                    if (args[i]) {
                        rt_memcpy(raw, (rt_uint8_t *)args[i], value_len[i]);
                        raw += value_len[i];
                        rt_free((rt_uint8_t *)args[i]);
                    }
                }
                else
                {
                    if (args[i]) {
                        rt_memcpy(raw, (rt_uint8_t *)args[i], value_len[i]);
                        raw += value_len[i];
                        rt_free((rt_uint8_t *)args[i]);
                    }
                }
            }
        }

        ret_blob->len = ret_blob->size;

        return ret_blob;
    }

    return RT_NULL;
}

int urpc_ffi_func_register(const char *func_name, void *func_addr)
{
    char svc_name[64];
    rt_snprintf(svc_name, sizeof(svc_name), "ffi.%s", func_name);

    return urpc_svc_register(svc_name, (urpc_svc_fn)func_addr);
}

int _urpc_ffi_init(void)
{
    urpc_svc_register("ffi", ffi_svc);

    return 0;
}
