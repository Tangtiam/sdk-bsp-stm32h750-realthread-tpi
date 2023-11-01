/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include "ubj.h"
#include "ubj_internal.h"
#include <string.h>

#define UBJ_PRINT ubj_printf
#define UBJ_ERROR_PRINT ubj_printf

#define UBJ_INCREMENT_SIZE  1024

static const char *ubj_ctab = UBJ_TYPE_TAB;

ubjsonr_t *ubj_read_init(ubjsonr_t* ubj, 
    size_t (*read)(ubjsonr_t *, void *, size_t, void *),
    void (*close)(ubjsonr_t *, void *),
    void *userdata)
{
    struct ubjson_container *container;

    /* check function */
    if (read == NULL)
        return NULL;

    if (ubj != NULL)
    {
        /* clean mem */
        memset(ubj, 0, sizeof(*ubj));
        /* init opts */
        ubj->read = read;
        ubj->close = close;
        ubj->userdata = userdata;
        /* init param */
        ubj->stack_depth = UBJ_CONTAINER_STACK_DEPTH;
        ubj->stack_point = 0;
        ubj->total = 0;
        /* init container */
        container = &ubj->stack[ubj->stack_point];
        container->flags = 0;
        container->remaining = -1;
        container->type = UBJ_TYPE_MIXED;
    }
    return ubj;
}

size_t ubj_read_end(ubjsonr_t* ubj)
{
    size_t total;
    int i;

    for (i = 0; i < sizeof(ubj->mem.buf) / sizeof(ubj->mem.buf[0]); i++)
    {
        if (ubj->mem.buf[i].ptr)
        {
            ubj_free(ubj->mem.buf[i].ptr);
            ubj->mem.buf[i].ptr = NULL;
            ubj->mem.buf[i].count = 0;
        }
    }
    total = ubj->total;
    if (ubj->close)
        ubj->close(ubj, ubj->userdata);
    return total;
}

ubj_err_t ubj_skip_subobject(ubjsonr_t* ubj)
{
    ubj->iterator.flags |= UBJ_ITERATOR_SKIP_SUBOBJECT;
    return UBJ_OK;
}

static void ubj_value_clean(ubjsonr_t *ubj)
{
    /* init value */
    memset(&ubj->mem.value, 0, sizeof(ubj->mem.value));
}

static ubj_err_t ubj_read_raw(ubjsonr_t *ubj, uint8_t *buf, size_t len, uint8_t picked)
{
    if (ubj->iterator.flags & UBJ_ITERATOR_PICK_VALID)
    {
        /* Pick valid, read directly */
        buf[0] = ubj->iterator.head_value;
        len -= 1;
        buf += 1;
    }
    if (picked == 0)
    {
        ubj->iterator.flags &= ~UBJ_ITERATOR_PICK_VALID;
    }
    if (len)
    {
        if (ubj->read(ubj, buf, len, ubj->userdata) != len)
        {
            UBJ_ERROR_PRINT("read rwa err.\n");
            return (ubj_err_t)(-UBJ_WRITE_ERROR);
        }
        ubj->total+=len;
        if (picked)
        {
            ubj->iterator.head_value = buf[len -1];
            ubj->iterator.flags |= UBJ_ITERATOR_PICK_VALID;
        }
    }
    return UBJ_OK;
}

static ubj_err_t read_integer(ubjsonr_t *ubj, int64_t *v)
{
    uint8_t buf[8];
    int64_t _v;
    ubj_err_t err;

    if ((err = ubj_read_raw(ubj, buf, 1, 0)) != UBJ_OK)
        return err;
    /* Read integer value */
    switch (buf[0])
    {
    case 'i':
    {
        /* UBJ_TYPE_INT8 */
        err = ubj_read_raw(ubj, (uint8_t*)buf, 1, 0);
        _v = *((int8_t*)buf);
        break;
    }
    case 'U':
    {
        /* UBJ_TYPE_UINT8 */
        err = ubj_read_raw(ubj, (uint8_t*)buf, 1, 0);
        _v = *((uint8_t*)buf);
        break;
    }
    case 'I':
    {
        int16_t tmp;
        /* UBJ_TYPE_INT16 */
        err = ubj_read_raw(ubj, (uint8_t*)buf, 2, 0);
        UBJ_BIGENDIAN16(buf, &tmp);
        _v = tmp;
        break;
    }
    case 'l':
    {
        int32_t tmp;
        /* UBJ_TYPE_INT32 */
        err = ubj_read_raw(ubj, (uint8_t*)buf, 4, 0);
        UBJ_BIGENDIAN32(buf, &tmp);
        _v = tmp;
        break;
    }
    case 'L':
    {
        int64_t tmp;
        /* UBJ_TYPE_INT64 */
        err = ubj_read_raw(ubj, (uint8_t*)buf, 8, 0);
        UBJ_BIGENDIAN64(buf, &tmp);
        _v = tmp;
        break;
    }
    default:
        /* Unknown type, printing error message */
        UBJ_ERROR_PRINT("Integer type exception. %c:%02x\n", buf[0], buf[0]);
        err = (ubj_err_t)(-UBJ_ERROR);
        break;
    }
    if (err == UBJ_OK)
        *v = _v;
    return err;
}

static ubj_err_t ubj_read_key_len(ubjsonr_t *ubj, int64_t *len)
{
    ubj_err_t err;
    int64_t length = -1;

    if ((err = read_integer(ubj, &length)) != UBJ_OK)
    {
        return err;
    }
    if (length < 0)
    {
        UBJ_ERROR_PRINT("Keyword length is less than 0.%d\n", length);
        return (ubj_err_t)(-UBJ_ERROR);
    }
    *len = length;
    return UBJ_OK;
}

static ubj_err_t read_raw_string(ubjsonr_t *ubj, char *buf, size_t len)
{
    ubj_err_t err;
    if ((err = ubj_read_raw(ubj, (uint8_t *)buf, len, 0)) != UBJ_OK)
    {
        return err;
    }
    return UBJ_OK;
}

static ubj_type_t ubj_get_type(char t)
{
    int i;
    for (i = 1; i < UBJ_NUM_TYPES; i++)
    {
        if (ubj_ctab[i] == t)
            return (ubj_type_t)i;
    }
    return UBJ_NUM_TYPES;
}

static ubj_err_t ubj_begin_container(ubjsonr_t *ubj, ubj_type_t ctype)
{
    int64_t length = -1;
    struct ubjson_container *container;
    struct ubjson_iterator *iterator;
    uint8_t c = 0;
    uint8_t cflags;
    ubj_type_t type = UBJ_TYPE_MIXED;
    ubj_err_t error = UBJ_OK;

    iterator = &ubj->iterator;
    cflags = ctype == UBJ_TYPE_ARRAY ? UBJ_CONTAINER_IS_ARRAY : UBJ_CONTAINER_IS_OBJECT;
    error = ubj_read_raw(ubj, &c, 1, 1);
    if (error != UBJ_OK)
        return error;
    /* Determine whether it is a strongly typed array */
    if (c == '$')
    {
        /* Clear cache pick value */
        iterator->flags &= ~UBJ_ITERATOR_PICK_VALID;
        error = ubj_read_raw(ubj, &c, 1, 0);
        if (error != UBJ_OK)
            return error;
        type = ubj_get_type(c);
        if (type == UBJ_NUM_TYPES)
        {
            error = (ubj_err_t)(-UBJ_ERROR);
            return error;
        }
        iterator->flags &= ~UBJ_ITERATOR_PICK_VALID;
        error = ubj_read_raw(ubj, &c, 1, 1);
        if (error != UBJ_OK)
            return error;
        cflags |= UBJ_CONTAINER_IS_TYPED;
    }
    else
    {
        type = UBJ_TYPE_MIXED;
    }
    if (c == '#')
    {
        /* Clear cache pick value */
        iterator->flags &= ~UBJ_ITERATOR_PICK_VALID;
        /* Try to read length */
        error = read_integer(ubj, &length);
        if (error != UBJ_OK || length < 0 || length > INT16_MAX)
            return error;
        cflags |= UBJ_CONTAINER_IS_SIZED;
    }
    container = UBJ_CONTAINER_WALK();
    if (container == NULL)
    {
        UBJ_ERROR_PRINT("Maximum stack depth reached.%d\n", UBJ_CONTAINER_STACK_DEPTH);
        return -UBJ_NOMEM_ERROR;
    }
    container->type = type;
    container->remaining = (int16_t)length;
    container->flags = cflags;
    return error;
}

static uint8_t ubj_read_tag(ubjsonr_t* ubj)
{
    struct ubjson_container *container;
    uint8_t tag = '\0';

    container = UBJ_CONTAINER_PICK();
    /* The current container type is known */
    if (container->flags & (UBJ_CONTAINER_IS_TYPED | UBJ_CONTAINER_IS_SIZED))
    {
        if (container->remaining < 0)
        {
            UBJ_ERROR_PRINT("Fixed length type, illegal length value.%d\n", container->remaining);
            return '\0';
        }
        else if (container->remaining > 0)
        {
            container->remaining --;
            if (container->flags & UBJ_CONTAINER_IS_TYPED)
            {
                /* Return to fixed type */
                tag = ubj_ctab[container->type];
            }
            else
            {
                /* read tag */
                if (ubj_read_raw(ubj, (uint8_t *)&tag, 1, 0) != UBJ_OK)
                    tag = '\0';
            }
            return tag;
        }
        else
        {
            if (container->flags & UBJ_CONTAINER_IS_ARRAY)
                tag = ']';
            else if (container->flags & UBJ_CONTAINER_IS_OBJECT)
                tag = '}';
            return tag;
        }
    }
    /* read tag */
    if (ubj_read_raw(ubj, (uint8_t *)&tag, 1, 0) != UBJ_OK)
        tag = '\0';
    return tag;
}

static void full_container(ubjsonr_t *ubj)
{
    struct ubjson_container *container = UBJ_CONTAINER_PICK();
    ubj_value_t *value = &ubj->mem.value;
    int64_t length = -1;
    uint8_t tag = '\0';
    void *bufptr;
    size_t key_len;

    value->flags = container->flags;
    value->error = UBJ_OK;
    if (container->flags & UBJ_CONTAINER_IS_OBJECT)
    {
        value->container.act = UBJ_CONTAINER_ACCESS_FLAG;
        value->container.count = container->remaining;
        value->container.type = (ubj_type_t)container->type;
        value->container.level = ubj->stack_point;

        /* The current container type is known */
        if (container->flags & UBJ_CONTAINER_IS_SIZED)
        {
            if (container->remaining == 0)
                return;
        }
        else
        {
            value->error = ubj_read_raw(ubj, &tag, 1, 1);
            if (value->error != UBJ_OK ||
                tag == '}' ||
                tag == ']' ||
                tag == '{' ||
                tag == '}' ||
                tag == '#' ||
                tag == '$')
            {
                return;
            }
        }
        /* read key length */
        value->error = ubj_read_key_len(ubj, &length);
        if (value->error != UBJ_OK || length < 0 || length > INT32_MAX)
        {
            value->error = (ubj_err_t)(-UBJ_ERROR);
            return;
        }
        value->container.keylen = (int32_t)length;
        key_len = (size_t)length;
        if (ubj->mem.buf[_UBJSONR_MEM_KEY].count >= (key_len + 1))
        {
            bufptr = ubj->mem.buf[_UBJSONR_MEM_KEY].ptr;
        }
        else
        {
            bufptr = ubj_realloc(ubj->mem.buf[_UBJSONR_MEM_KEY].ptr, key_len + 1);
            if (bufptr)
            {
                ubj->mem.buf[_UBJSONR_MEM_KEY].ptr = bufptr;
                ubj->mem.buf[_UBJSONR_MEM_KEY].count = key_len + 1;
            }
        }
        value = &ubj->mem.value;
        value->container.key = (char *)bufptr;
        if (value->container.key == NULL)
        {
            value->error = (ubj_err_t)(-UBJ_NOMEM_ERROR);
            return;
        }
        value->error = read_raw_string(ubj, value->container.key, key_len);
        if (value->error != UBJ_OK)
        {
            return;
        }
        value->container.key[key_len] = '\0';
    }
    else if (container->flags & UBJ_CONTAINER_IS_ARRAY)
    {
        value->container.act = UBJ_CONTAINER_ACCESS_FLAG;
        value->container.count = container->remaining;
        value->container.type = (ubj_type_t)container->type;
        value->container.level = ubj->stack_point;
    }
}

static const ubj_value_t *ubj_read_noop(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    value->type = UBJ_TYPE_NOOP;
    return value;
}

static const ubj_value_t *ubj_read_null(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    value->type = UBJ_TYPE_NULL;
    return value;
}

static const ubj_value_t *ubj_read_true(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    value->type = UBJ_TYPE_TRUE;
    value->value.vbool = 1;
    return value;
}

static const ubj_value_t *ubj_read_false(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    value->type = UBJ_TYPE_FALSE;
    value->value.vbool = 0;
    return value;
}

static const ubj_value_t *ubj_read_char(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    value->type = UBJ_TYPE_CHAR;
    if (value->error != UBJ_OK)
        return value;
    /* Read raw data */
    value->error = ubj_read_raw(ubj, (uint8_t *)(&value->value.vchar), 1, 0);
    return value;
}

static const ubj_value_t *ubj_read_string(ubjsonr_t *ubj, ubj_type_t type)
{
    ubj_value_t *value = &ubj->mem.value;
    int64_t length = -1;
    void *bufptr;
    size_t key_len;

    value->type = type;
    if (value->error != UBJ_OK)
        return value;
    /* Read string length */
    value->error = ubj_read_key_len(ubj, &length);
    if (value->error != UBJ_OK)
        return value;

    key_len = (size_t)length;
    if (ubj->mem.buf[_UBJSONR_MEM_STRING].count >= (key_len + 1))
    {
        bufptr = ubj->mem.buf[_UBJSONR_MEM_STRING].ptr;
    }
    else
    {
        bufptr = ubj_realloc(ubj->mem.buf[_UBJSONR_MEM_STRING].ptr, key_len + 1);
        if (bufptr)
        {
            ubj->mem.buf[_UBJSONR_MEM_STRING].ptr = bufptr;
            ubj->mem.buf[_UBJSONR_MEM_STRING].count = key_len + 1;
        }
    }
    value = &ubj->mem.value;
    value->value.vstring.string = (char *)bufptr;
    if (value->value.vstring.string == NULL)
    {
        value->error = (ubj_err_t)(-UBJ_NOMEM_ERROR);
        return value;
    }
    value->error = read_raw_string(ubj, value->value.vstring.string, key_len);
    if (value->error != UBJ_OK)
        return value;
    value->value.vstring.string[key_len] = '\0';
    value->value.vstring.size = strlen(value->value.vstring.string);
    return value;
}

static const ubj_value_t *ubj_read_uint8(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    uint8_t v;

    value->type = UBJ_TYPE_UINT8;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, (uint8_t *)(&v), 1, 0)) != UBJ_OK)
        return value;
    value->value.vuint8 = v;
    return value;
}

static const ubj_value_t *ubj_read_int8(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    int8_t v;

    value->type = UBJ_TYPE_INT8;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, (uint8_t*)(&v), 1, 0)) != UBJ_OK)
        return value;
    value->value.vint8 = v;
    return value;
}

static const ubj_value_t *ubj_read_int16(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    uint8_t buf[2];
    int16_t v;

    value->type = UBJ_TYPE_INT16;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, buf, 2, 0)) != UBJ_OK)
        return value;
    UBJ_BIGENDIAN16(buf, &v);
    value->value.vint16 = v;
    return value;
}

static const ubj_value_t *ubj_read_int32(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    uint8_t buf[4];
    int32_t v;

    value->type = UBJ_TYPE_INT32;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, buf, 4, 0)) != UBJ_OK)
        return value;
    UBJ_BIGENDIAN32(buf, &v);
    value->value.vint32 = v;
    return value;
}

static const ubj_value_t *ubj_read_int64(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    uint8_t buf[8];
    int64_t v;

    value->type = UBJ_TYPE_INT64;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, buf, 8, 0)) != UBJ_OK)
        return value;
    UBJ_BIGENDIAN64(buf, &v);
    value->value.vint64 = v;
    return value;
}

static const ubj_value_t *ubj_read_float32(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    uint8_t buf[4];

    value->type = UBJ_TYPE_FLOAT32;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, buf, 4, 0)) != UBJ_OK)
        return value;
    UBJ_BIGENDIAN32(buf, &value->value.vfloat);
    return value;
}

static const ubj_value_t *ubj_read_float64(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    uint8_t buf[8];

    value->type = UBJ_TYPE_FLOAT64;
    if (value->error != UBJ_OK)
        return value;
    /* read raw data */
    if ((value->error = ubj_read_raw(ubj, buf, 8, 0)) != UBJ_OK)
        return value;
    UBJ_BIGENDIAN64(buf, &value->value.vdouble);
    return value;
}

static const ubj_value_t *ubj_read_object(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;

    value->type = UBJ_TYPE_OBJECT;
    value->error = ubj_begin_container(ubj, UBJ_TYPE_OBJECT);
    value->container.act = UBJ_CONTAINER_ENTER_FLAG;
    value->container.count = UBJ_CONTAINER_PICK()->remaining;
    value->container.type = (ubj_type_t)UBJ_CONTAINER_PICK()->type;
    value->container.level = ubj->stack_point;
    return value;
}

static const ubj_value_t *ubj_read_array(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;

    value->type = UBJ_TYPE_ARRAY;
    value->error = ubj_begin_container(ubj, UBJ_TYPE_ARRAY);
    value->container.act = UBJ_CONTAINER_ENTER_FLAG;
    value->container.count = UBJ_CONTAINER_PICK()->remaining;
    value->container.type = (ubj_type_t)UBJ_CONTAINER_PICK()->type;
    value->container.level = ubj->stack_point;
    return value;
}

static const ubj_value_t *ubj_read_object_end(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;

    value->flags = UBJ_CONTAINER_PICK()->flags;
    value->type = UBJ_TYPE_MIXED;
    value->container.act = UBJ_CONTAINER_EXIT_FLAG;
    value->container.count = 0;
    value->container.type = (ubj_type_t)UBJ_CONTAINER_PICK()->type;
    value->container.level = ubj->stack_point;
    UBJ_CONTAINER_POP();
    return value;
}

static const ubj_value_t *ubj_read_array_end(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;

    value->flags = UBJ_CONTAINER_PICK()->flags;
    value->type = UBJ_TYPE_ARRAY;
    value->container.act = UBJ_CONTAINER_EXIT_FLAG;
    value->container.count = 0;
    value->container.type = (ubj_type_t)UBJ_CONTAINER_PICK()->type;
    value->container.level = ubj->stack_point;
    UBJ_CONTAINER_POP();
    return value;
}

static const ubj_value_t *ubj_read_error_handle(ubjsonr_t *ubj)
{
    ubj_value_t *value = &ubj->mem.value;
    value->error = (ubj_err_t)(-UBJ_ERROR);
    return value;
}

const ubj_value_t *ubj_read_next(ubjsonr_t *ubj)
{
    struct ubjson_iterator *iterator = &ubj->iterator;
    const ubj_value_t *value;
    char tag;

    if (iterator->flags & UBJ_ITERATOR_READ_END)
        return NULL;
    /* Start parsing data */
    while (1)
    {
        /* clean mem */
        ubj_value_clean(ubj);
        /* read key */
        full_container(ubj);
        /* read tag */
        tag = ubj_read_tag(ubj);
        /* parsing Tags */
        switch (tag)
        {
        case 'Z':
            /* UBJ_TYPE_NULL */
            value = ubj_read_null(ubj);
            break;
        case 'N':
            /* UBJ_TYPE_NOOP */
            value = ubj_read_noop(ubj);
            break;
        case 'T':
            /* UBJ_TYPE_TRUE */
            value = ubj_read_true(ubj);
            break;
        case 'F':
            /* UBJ_TYPE_FALSE */
            value = ubj_read_false(ubj);
            break;
        case 'C':
            /* UBJ_TYPE_CHAR */
            value = ubj_read_char(ubj);
            break;
        case 'S':
            /* UBJ_TYPE_STRING */
            value = ubj_read_string(ubj, UBJ_TYPE_STRING);
            break;
        case 'H':
            /* UBJ_TYPE_HIGH_PRECISION */
            value = ubj_read_string(ubj, UBJ_TYPE_HIGH_PRECISION);
            break;
        case 'i':
            /* UBJ_TYPE_INT8 */
            value = ubj_read_int8(ubj);
            break;
        case 'U':
            /* UBJ_TYPE_UINT8 */
            value = ubj_read_uint8(ubj);
            break;
        case 'I':
            /* UBJ_TYPE_INT16 */
            value = ubj_read_int16(ubj);
            break;
        case 'l':
            /* UBJ_TYPE_INT32 */
            value = ubj_read_int32(ubj);
            break;
        case 'L':
            /* UBJ_TYPE_INT64 */
            value = ubj_read_int64(ubj);
            break;
        case 'd':
            /* UBJ_TYPE_FLOAT32 */
            value = ubj_read_float32(ubj);
            break;
        case 'D':
            /* UBJ_TYPE_FLOAT64 */
            value = ubj_read_float64(ubj);
            break;
        case '[':
            /* UBJ_TYPE_ARRAY */
            value = ubj_read_array(ubj);
            break;
        case '{':
            /* UBJ_TYPE_OBJECT */
            value = ubj_read_object(ubj);
            break;
        case ']':
            value = ubj_read_array_end(ubj);
            break;
        case '}':
            value = ubj_read_object_end(ubj);
            break;
        default:
            UBJ_ERROR_PRINT("unknown type. %c:%02x\n", tag, tag);
            value = ubj_read_error_handle(ubj);
            break;
        }
        /* Parsing error occurred */
        if (value->error != UBJ_OK)
        {
            /* break while(1) */
            break;
        }
        /* Check if it's over */
        if (ubj->stack_point == 0)
        {
            /* end */
            iterator->flags |= UBJ_ITERATOR_READ_END;
            break;
        }
        /* Check if child elements are skipped */
        if (!(iterator->flags & UBJ_ITERATOR_SKIP_SUBOBJECT))
        {
            /* break while(1) */
            break;
        }
    }
    return value;
}

static ubj_err_t _check_value(ubjsonr_t *ubjr, const ubj_value_t *v)
{
    /* check error */
    if (v->error != UBJ_OK)
    {
        return v->error;
    }
    switch (v->type)
    {
    case UBJ_TYPE_NULL:
    case UBJ_TYPE_NOOP:
    case UBJ_TYPE_TRUE:
    case UBJ_TYPE_FALSE:
    case UBJ_TYPE_CHAR:
    case UBJ_TYPE_INT8:
    case UBJ_TYPE_UINT8:
    case UBJ_TYPE_INT16:
    case UBJ_TYPE_INT32:
    case UBJ_TYPE_INT64:
    case UBJ_TYPE_FLOAT32:
    case UBJ_TYPE_FLOAT64:
    case UBJ_TYPE_STRING:
    case UBJ_TYPE_HIGH_PRECISION:
        return UBJ_OK;
    case UBJ_TYPE_ARRAY:
    case UBJ_TYPE_OBJECT:
    {
        ubj_err_t err;
        if (v->container.act != UBJ_CONTAINER_ENTER_FLAG)
            return -UBJ_ERROR;
        /* get next value */
        v = ubj_read_next(ubjr);
        while (v->error == UBJ_OK && v->container.act != UBJ_CONTAINER_EXIT_FLAG)
        {
            /* check value */
            err = _check_value(ubjr, v);
            if (err != UBJ_OK)
            {
                return err;
            }
            /* read Next */
            v = ubj_read_next(ubjr);
        }
        return v->error;
    }
    default:
        return -UBJ_ERROR;
    }
    return -UBJ_ERROR;
}

int ubj_isvalid(ubjsonr_t* ubj)
{
    const ubj_value_t *v;
    ubj_err_t res;

    v = ubj_read_next(ubj);
    if (v == NULL)
    {
        return 0;
    }
    if (v->error != UBJ_OK)
    {
        return 0;
    }
    res = _check_value(ubj, v);
    return res == UBJ_OK;
}
