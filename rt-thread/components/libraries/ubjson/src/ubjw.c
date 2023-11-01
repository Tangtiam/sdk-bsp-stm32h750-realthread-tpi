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

#define UBJ_PRINT(...)
#define UBJ_ERROR_PRINT ubj_printf

#define UBJ_WRITE(v, l) \
    do {    \
        if (l && ubj->write(ubj, v, l, ubj->userdata) != l) \
        {   \
            UBJ_ERROR_PRINT("WRITE error\n"); \
            return -UBJ_WRITE_ERROR;    \
        }   \
        ubj->total+=l;  \
    } while (0)

#define UBJ_APPEND(t)   \
    do {    \
        uint8_t tag = (uint8_t)(t);  \
        if (ubj->write(ubj, &tag, 1, ubj->userdata) != 1)   \
        {   \
            UBJ_ERROR_PRINT("APPEND error\n");    \
            return -UBJ_WRITE_ERROR;    \
        }   \
        ubj->total++;   \
    } while (0)

#define UBJ_WRITE_TAG(t)    \
    do {    \
        ubj_err_t _err; \
        if ((_err = ubj_write_tag(ubj, t)) != UBJ_OK)   \
        {   \
            return _err;    \
        }   \
    } while (0)

static const char *ubj_ctab = UBJ_TYPE_TAB;

static ubj_err_t ubj_write_tag(ubjsonw_t* ubj, ubj_type_t type)
{
    struct ubjson_container *container = UBJ_CONTAINER_PICK();

    if (!(container->flags & UBJ_CONTAINER_IGNORE_FLAG))
    {
        /* press in data check */
        if ((container->flags & UBJ_CONTAINER_IS_OBJECT &&
            ((container->flags & UBJ_CONTAINER_EXPECTS_KEY) == 0)))
        {
            UBJ_ERROR_PRINT("No key written, write value directly\n");
            return -UBJ_ERROR;
        }
        if (((container->flags & UBJ_CONTAINER_IS_TYPED) &&
            container->type != type))
        {
            UBJ_ERROR_PRINT("The data type does not match the container\n");
            return -UBJ_ERROR;
        }
        if (container->flags & UBJ_CONTAINER_IS_SIZED)
        {
            if (container->remaining <= 0)
            {
                UBJ_ERROR_PRINT("write tag remainint <= 0.\n");
                return -UBJ_ERROR;
            }
            else
                container->remaining--;
        }
        container->flags ^= UBJ_CONTAINER_EXPECTS_KEY;
        /* Identify the container type, no need to write repeatedly */
        if (container->flags & UBJ_CONTAINER_IS_TYPED)
        {
            return UBJ_OK;
        }
    }
    /* write tag */
    if (type > UBJ_TYPE_MIXED && type < UBJ_NUM_TYPES)
    {
        UBJ_APPEND(ubj_ctab[type]);
    }
    return UBJ_OK;
}

ubjsonw_t *ubj_write_init(ubjsonw_t* ubj,
    size_t (*write)(ubjsonw_t *, const void *, size_t, void *),
    void (*close)(ubjsonw_t *, void *),
    void *userdata)
{
    struct ubjson_container *container;

    /* check opt function */
    if (write == NULL)
        return NULL;

    if (ubj != NULL)
    {
        /* clean mem */
        memset(ubj, 0, sizeof(*ubj));
        /* init opts */
        ubj->write = write;
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

size_t ubj_write_end(ubjsonw_t* ubj)
{
    size_t total = ubj->total;

    while (ubj->stack_point > 0)
    {
        UBJ_PRINT("stack point %d > 0.\n", ubj->stack_point);
        if (ubj->stack[ubj->stack_point].flags & UBJ_CONTAINER_IS_ARRAY)
        {
            if (ubj_end_array(ubj) != UBJ_OK)
            {
                break;
            }
        }
        else if (ubj->stack[ubj->stack_point].flags & UBJ_CONTAINER_IS_OBJECT)
        {
            if (ubj_end_object(ubj) != UBJ_OK)
            {
                break;
            }
        }
        else
        {
            UBJ_CONTAINER_POP();
        }
    }
    if (ubj->close)
        ubj->close(ubj, ubj->userdata);
    UBJ_PRINT("write end. total:%d\n", total);
    return total;
}

static ubj_err_t ubj_begin_container(ubjsonw_t *ubj, ubj_type_t ctype, ubj_type_t type, long count)
{
    struct ubjson_container *container;
    uint8_t flags = ctype == UBJ_TYPE_OBJECT ? UBJ_CONTAINER_IS_OBJECT : UBJ_CONTAINER_IS_ARRAY;

    if ((int)type >= UBJ_NUM_TYPES ||
        (int)type < UBJ_TYPE_MIXED ||
        count >= INT16_MAX ||
        (type != UBJ_TYPE_MIXED && count < 0))
    {
        UBJ_ERROR_PRINT("Wrong data type or count.t:%02x c:%ld\n", type, count);
        /* Container with type, length must be greater than 0 */
        return -UBJ_PARAM_ERROR;
    }
    /* write tag begin */
    UBJ_WRITE_TAG(ctype);
    /* write container type */
    if ((0x1 << type) & 0x1FFFE)
    {
        flags |= UBJ_CONTAINER_IS_TYPED;
        UBJ_APPEND('$');
        UBJ_APPEND(ubj_ctab[type]);
    }
    /* write array count */
    if (count >= 0)
    {
        flags |= UBJ_CONTAINER_IS_SIZED;
        UBJ_APPEND('#');
        UBJ_CONTAINER_PICK()->flags |= UBJ_CONTAINER_IGNORE_FLAG;
        ubj_write_integer(ubj, count);
        UBJ_CONTAINER_PICK()->flags ^= UBJ_CONTAINER_IGNORE_FLAG;
    }
    /* Get a container, record context */
    container = UBJ_CONTAINER_WALK();
    if (container == NULL)
    {
        /* TODO:Failed to get a valid container, exit. Mark cannot recover error */
        UBJ_ERROR_PRINT("Maximum stack depth reached.%d\n", UBJ_CONTAINER_STACK_DEPTH);
        return -UBJ_NOMEM_ERROR;
    }
    /* Initialize container */
    container->flags = flags;
    container->type = type;
    container->remaining = (int16_t)count;
    UBJ_PRINT("init container. type:%d remaining:%d\n", container->type, container->remaining);
    return UBJ_OK;
}

static ubj_err_t ubj_end_container(ubjsonw_t *ubj, ubj_type_t ctype)
{
    struct ubjson_container *container = UBJ_CONTAINER_PICK();
    char c = ctype == UBJ_TYPE_OBJECT ? '}' : ']';

    /* Fixed length inspection */
    if (container->flags & UBJ_CONTAINER_IS_SIZED)
    {
        if (container->remaining > 0)
        {
            UBJ_ERROR_PRINT("container remaining > 0. %d\n", container->remaining);
            return -UBJ_ERROR;
        }
    }
    else
    {
        UBJ_APPEND(c);
    }
    UBJ_CONTAINER_POP();
    UBJ_PRINT("container end.\n");
    return UBJ_OK;
}

ubj_err_t ubj_begin_array(ubjsonw_t *ubj)
{
    return ubj_begin_container(ubj, UBJ_TYPE_ARRAY, UBJ_TYPE_MIXED, -1);
}

ubj_err_t ubj_begin_fixed_array(ubjsonw_t *ubj, ubj_type_t type, long count)
{
    return ubj_begin_container(ubj, UBJ_TYPE_ARRAY, type, count);
}

ubj_err_t ubj_end_array(ubjsonw_t *ubj)
{
    struct ubjson_container *container = UBJ_CONTAINER_PICK();

    if (!(container->flags & UBJ_CONTAINER_IS_ARRAY))
    {
        UBJ_ERROR_PRINT("It's not an array\n");
        return -UBJ_PARAM_ERROR;
    }
    return ubj_end_container(ubj, UBJ_TYPE_ARRAY);
}

ubj_err_t ubj_begin_object(ubjsonw_t *ubj)
{
    return ubj_begin_container(ubj, UBJ_TYPE_OBJECT, UBJ_TYPE_MIXED, -1);
}

ubj_err_t ubj_begin_fixed_object(ubjsonw_t *ubj, ubj_type_t type, long count)
{
    return ubj_begin_container(ubj, UBJ_TYPE_OBJECT, type, count);
}

ubj_err_t ubj_end_object(ubjsonw_t *ubj)
{
    struct ubjson_container *container = UBJ_CONTAINER_PICK();

    if (!(container->flags & UBJ_CONTAINER_IS_OBJECT))
    {
        UBJ_ERROR_PRINT("It's not an object\n");
        return -UBJ_PARAM_ERROR;
    }
    return ubj_end_container(ubj, UBJ_TYPE_OBJECT);
}

static ubj_err_t ubj_write_raw_string(ubjsonw_t* ubj, const char* v, int len)//TODO: possibly use a safe string
{
    struct ubjson_container* container = UBJ_CONTAINER_PICK();
    ubj_err_t err;

    container->flags |= UBJ_CONTAINER_IGNORE_FLAG;
    err = ubj_write_integer(ubj, len);
    container->flags ^= UBJ_CONTAINER_IGNORE_FLAG;
    if (err != UBJ_OK)
        return err;
    UBJ_WRITE(v, len);
    return UBJ_OK;
}

ubj_err_t ubj_write_key(ubjsonw_t *ubj, const char *key)
{
    struct ubjson_container *container = UBJ_CONTAINER_PICK();

    if (((container->flags & UBJ_CONTAINER_IS_OBJECT) == 0) ||
        (container->flags & UBJ_CONTAINER_EXPECTS_KEY))
    {
        return -UBJ_ERROR;
    }
    container->flags |= UBJ_CONTAINER_EXPECTS_KEY;
    return ubj_write_raw_string(ubj, key, strlen(key));
}

ubj_err_t ubj_write_fixed_string(ubjsonw_t *ubj, const char *v, int len)
{
    if (len >= 0)
    {
        UBJ_WRITE_TAG(UBJ_TYPE_STRING);
        return ubj_write_raw_string(ubj, v, len);
    }
    else
    {
        return -UBJ_PARAM_ERROR;
    }
}

ubj_err_t ubj_write_string(ubjsonw_t *ubj, const char *v)
{
    UBJ_WRITE_TAG(UBJ_TYPE_STRING);
    return ubj_write_raw_string(ubj, v, strlen(v));
}

ubj_err_t ubj_write_char(ubjsonw_t *ubj, char v)
{
    UBJ_WRITE_TAG(UBJ_TYPE_CHAR);
    UBJ_APPEND(v);
    return UBJ_OK;
}

ubj_err_t ubj_write_uint8(ubjsonw_t *ubj, uint8_t v)
{
    UBJ_WRITE_TAG(UBJ_TYPE_UINT8);
    UBJ_APPEND(v);
    return UBJ_OK;
}

ubj_err_t ubj_write_int8(ubjsonw_t *ubj, int8_t v)
{
    UBJ_WRITE_TAG(UBJ_TYPE_INT8);
    UBJ_APPEND(v);
    return UBJ_OK;
}

ubj_err_t ubj_write_int16(ubjsonw_t *ubj, int16_t v)
{
    uint8_t buff[2];
    UBJ_BIGENDIAN16(&v, buff);
    UBJ_WRITE_TAG(UBJ_TYPE_INT16);
    UBJ_WRITE(buff, sizeof(int16_t));
    return UBJ_OK;
}

ubj_err_t ubj_write_int32(ubjsonw_t *ubj, int32_t v)
{
    uint8_t buff[4];
    UBJ_BIGENDIAN32(&v, buff);
    UBJ_WRITE_TAG(UBJ_TYPE_INT32);
    UBJ_WRITE(buff, sizeof(int32_t));
    return UBJ_OK;
}

ubj_err_t ubj_write_int64(ubjsonw_t *ubj, int64_t v)
{
    uint8_t buff[8];
    UBJ_BIGENDIAN64(&v, buff);
    UBJ_WRITE_TAG(UBJ_TYPE_INT64);
    UBJ_WRITE(buff, sizeof(int64_t));
    return UBJ_OK;
}

ubj_err_t ubj_write_high_precision(ubjsonw_t *ubj, const char *hp)
{
    UBJ_WRITE_TAG(UBJ_TYPE_HIGH_PRECISION);
    return ubj_write_raw_string(ubj, hp, strlen(hp));
}

ubj_err_t ubj_write_integer(ubjsonw_t *ubj, int64_t v)
{
    if (v >= 0 && v <= UINT8_MAX)
    {
        return ubj_write_uint8(ubj, (uint8_t)v);
    }
    else if (v >= INT8_MIN && v <= INT8_MAX)
    {
        return ubj_write_int8(ubj, (int8_t)v);
    }
    else if (v >= INT16_MIN && v <= INT16_MAX)
    {
        return ubj_write_int16(ubj, (int16_t)v);
    }
    else if (v >= INT32_MIN && v <= INT32_MAX)
    {
        return ubj_write_int32(ubj, (int32_t)v);
    }
    else
    {
        return ubj_write_int64(ubj, (int64_t)v);
    }
}

ubj_err_t ubj_write_float32(ubjsonw_t *ubj, float v)
{
    uint8_t buff[4];
    UBJ_BIGENDIAN32(&v, buff);
    UBJ_WRITE_TAG(UBJ_TYPE_FLOAT32);
    UBJ_WRITE(buff, sizeof(float));
    return UBJ_OK;
}

ubj_err_t ubj_write_float64(ubjsonw_t *ubj, double v)
{
    uint8_t buff[8];
    UBJ_BIGENDIAN64(&v, buff);
    UBJ_WRITE_TAG(UBJ_TYPE_FLOAT64);
    UBJ_WRITE(buff, sizeof(double));
    return UBJ_OK;
}

ubj_err_t ubj_write_noop(ubjsonw_t *ubj)
{
    UBJ_WRITE_TAG(UBJ_TYPE_NOOP);
    return UBJ_OK;
}

ubj_err_t ubj_write_null(ubjsonw_t *ubj)
{
    UBJ_WRITE_TAG(UBJ_TYPE_NULL);
    return UBJ_OK;
}

ubj_err_t ubj_write_true(ubjsonw_t *ubj)
{
    UBJ_WRITE_TAG(UBJ_TYPE_TRUE);
    return UBJ_OK;
}

ubj_err_t ubj_write_false(ubjsonw_t *ubj)
{
    UBJ_WRITE_TAG(UBJ_TYPE_FALSE);
    return UBJ_OK;
}

ubj_err_t ubj_object_write_string(ubjsonw_t *ubj, const char *key, const char *v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_string(ubj, v);
}

ubj_err_t ubj_object_write_fixed_string(ubjsonw_t *ubj, const char *key, const char *v, int len)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_fixed_string(ubj, v, len);
}


ubj_err_t ubj_object_write_char(ubjsonw_t *ubj, const char *key, char v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_char(ubj, v);
}

ubj_err_t ubj_object_write_uint8(ubjsonw_t *ubj, const char *key, uint8_t v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_uint8(ubj, v);
}

ubj_err_t ubj_object_write_int8(ubjsonw_t *ubj, const char *key, int8_t v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_int8(ubj, v);
}

ubj_err_t ubj_object_write_int16(ubjsonw_t *ubj, const char *key, int16_t v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_int16(ubj, v);
}

ubj_err_t ubj_object_write_int32(ubjsonw_t *ubj, const char *key, int32_t v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_int32(ubj, v);
}

ubj_err_t ubj_object_write_int64(ubjsonw_t *ubj, const char *key, int64_t v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_int64(ubj, v);
}

ubj_err_t ubj_object_write_high_precision(ubjsonw_t *ubj, const char *key, const char *hp)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_high_precision(ubj, hp);
}

ubj_err_t ubj_object_write_integer(ubjsonw_t *ubj, const char *key, int64_t v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_integer(ubj, v);
}

ubj_err_t ubj_object_write_float32(ubjsonw_t *ubj, const char *key, float v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_float32(ubj, v);
}

ubj_err_t ubj_object_write_float64(ubjsonw_t *ubj, const char *key, double v)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_float64(ubj, v);
}

ubj_err_t ubj_object_write_null(ubjsonw_t *ubj, const char *key)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_null(ubj);
}

ubj_err_t ubj_object_write_true(ubjsonw_t *ubj, const char *key)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_true(ubj);
}

ubj_err_t ubj_object_write_false(ubjsonw_t *ubj, const char *key)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_false(ubj);
}

ubj_err_t ubj_object_write_object(ubjsonw_t *ubj, const char *key)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_begin_object(ubj);
}

ubj_err_t ubj_object_write_array(ubjsonw_t *ubj, const char *key)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_begin_array(ubj);
}

ubj_err_t ubj_object_write_fixed_array(ubjsonw_t *ubj, const char *key, ubj_type_t type, long count)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_begin_fixed_array(ubj, type, count);
}

ubj_err_t ubj_object_write_fixed_object(ubjsonw_t *ubj, const char *key, ubj_type_t type, long count)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_begin_fixed_object(ubj, type, count);
}

ubj_err_t ubj_write_buffer(ubjsonw_t *ubj, const uint8_t *data, ubj_type_t type, size_t count)
{
    ubj_err_t err = -UBJ_PARAM_ERROR;

    /* Used only as a fast write byte buffer */
    if (type == UBJ_TYPE_INT8 || type == UBJ_TYPE_UINT8 || type == UBJ_TYPE_CHAR)
    {
        if ((err = ubj_begin_fixed_array(ubj, type, count)) != UBJ_OK)
        {
            return err;
        }
        /* write data */
        UBJ_WRITE(data, count);
        UBJ_CONTAINER_PICK()->remaining -= (int16_t)count;
        /* write end */
        return ubj_end_array(ubj);
    }
    /* check parameter */
    else if (type == UBJ_TYPE_NULL || type == UBJ_TYPE_NOOP || type == UBJ_TYPE_TRUE ||
             type == UBJ_TYPE_FALSE || type == UBJ_TYPE_INT16 || type == UBJ_TYPE_INT32 ||
             type == UBJ_TYPE_FLOAT32 || type == UBJ_TYPE_INT64 || type == UBJ_TYPE_FLOAT64 ||
             type == UBJ_TYPE_STRING || type == UBJ_TYPE_HIGH_PRECISION)
    {
        // write array
        err = ubj_begin_fixed_array(ubj, type, count);
        // write data
        while (err == UBJ_OK && count-- > 0)
        {
            switch (type)
            {
            case UBJ_TYPE_NULL:
                err = ubj_write_null(ubj);
                break;
            case UBJ_TYPE_NOOP:
                err = ubj_write_noop(ubj);
                break;
            case UBJ_TYPE_TRUE:
                err = ubj_write_true(ubj);
                break;
            case UBJ_TYPE_FALSE:
                err = ubj_write_false(ubj);
                break;
            case UBJ_TYPE_INT16:
                err = ubj_write_int16(ubj, *((int16_t *)data));
                data = data + sizeof(int16_t);
                break;
            case UBJ_TYPE_INT32:
                err = ubj_write_int32(ubj, *((int32_t *)data));
                data = data + sizeof(int32_t);
                break;
            case UBJ_TYPE_FLOAT32:
                err = ubj_write_float32(ubj, *((float *)data));
                data = data + sizeof(float);
                break;
            case UBJ_TYPE_INT64:
                err = ubj_write_int64(ubj, *((int64_t *)data));
                data = data + sizeof(int64_t);
                break;
            case UBJ_TYPE_FLOAT64:
                err = ubj_write_float64(ubj, *((double *)data));
                data = data + sizeof(double);
                break;
            case UBJ_TYPE_STRING:
                err = ubj_write_string(ubj, *((const char **)data));
                data = data + sizeof(const char *);
                break;
            case UBJ_TYPE_HIGH_PRECISION:
                err = ubj_write_high_precision(ubj, *((const char **)data));
                data = data + sizeof(const char *);
                break;
            default:
                return err;
            }
        }
        if (err == UBJ_OK)
        {
            /* write end */
            return ubj_end_array(ubj);
        }
        else
        {
            return err;
        }
    }
    return err;
}

ubj_err_t ubj_write_arraybuffer(ubjsonw_t* ubj, const uint8_t* data, size_t count)
{
    return ubj_write_buffer(ubj, data, UBJ_TYPE_UINT8, count);
}

ubj_err_t ubj_object_write_buffer(ubjsonw_t *ubj, const char *key, const uint8_t *data, ubj_type_t type, size_t count)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_buffer(ubj, data, type, count);
}

ubj_err_t ubj_object_write_arraybuffer(ubjsonw_t* ubj, const char* key, const uint8_t* data, size_t count)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_buffer(ubj, data, UBJ_TYPE_UINT8, count);
}

ubj_err_t ubj_write_ubjson(ubjsonw_t *ubj, const uint8_t *ubjson, size_t len)
{
    UBJ_WRITE_TAG(UBJ_TYPE_MIXED);
    UBJ_WRITE(ubjson, len);
    return UBJ_OK;
}

ubj_err_t ubj_object_write_ubjson(ubjsonw_t *ubj, const char *key, const uint8_t *ubjson, size_t len)
{
    ubj_err_t err;
    if ((err = ubj_write_key(ubj, key)) != UBJ_OK)
        return err;
    return ubj_write_ubjson(ubj, ubjson, len);
}
