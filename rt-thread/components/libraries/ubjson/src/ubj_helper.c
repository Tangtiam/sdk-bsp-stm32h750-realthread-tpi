/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include "ubj.h"
#include "string.h"

#define UBJ_ERROR_PRINT ubj_printf

struct _ubj_helper_res
{
    ubjh_t *item;
    ubj_err_t error;
};

static struct _ubj_helper_res _dump_value(ubjsonr_t *ubjr, const ubj_value_t *v);
static ubj_err_t _dump_container(ubjsonr_t *ubjr, const ubj_value_t *v, ubjh_t *item)
{
    ubj_type_t type = v->type;
    struct _ubj_helper_res res;
    char *s;
    ubjh_t *last = NULL;

    if (v->error != UBJ_OK || v->container.act != UBJ_CONTAINER_ENTER_FLAG)
        return -UBJ_ERROR;

    if (type == UBJ_TYPE_OBJECT)
        item->flags = UBJ_CONTAINER_IS_OBJECT;
    else
        item->flags = UBJ_CONTAINER_IS_ARRAY;

    item->value.vchild = NULL;

    /* get next value */
    v = ubj_read_next(ubjr);
    while (v->error == UBJ_OK && v->container.act != UBJ_CONTAINER_EXIT_FLAG)
    {
        /* dump key */
        if (type == UBJ_TYPE_OBJECT)
        {
            s = ubj_malloc(v->container.keylen + 1);
            if (s)
            {
                memcpy(s, v->container.key, v->container.keylen + 1);
            }
            else
            {
                // free child
                ubj_parse_free(item->value.vchild);
                item->value.vchild = NULL;
                return -UBJ_NOMEM_ERROR;
            }
        }
        else
        {
            s = NULL;
        }
        /* dump value */
        res =  _dump_value(ubjr, v);
        if (res.error != UBJ_OK)
        {
            // free child
            ubj_parse_free(item->value.vchild);
            // free key
            if (s)
            {
                ubj_free(s);
            }
            item->value.vchild = NULL;
            return res.error;
        }
        /* set key */
        if (type == UBJ_TYPE_OBJECT)
        {
            res.item->flags |= UBJ_CONTAINER_EXPECTS_KEY;
            res.item->auxiliary.key = s;
        }
        else if (type == UBJ_TYPE_ARRAY)
        {
            //res.item->auxiliary.index = idx++;
        }
        /* attch child */
        if (item->value.vchild == NULL)
        {
            item->value.vchild = res.item;
            last = res.item;
        }
        else
        {
            last->next = res.item;
            last = res.item;
        }
        /* read Next */
        v = ubj_read_next(ubjr);
    }
    if (v->error != UBJ_OK)
    {
        // free child
        ubj_parse_free(item->value.vchild);
        item->value.vchild = NULL;
    }
    return v->error;
}

static int _type_len(ubj_type_t type)
{
    switch (type)
    {
    case UBJ_TYPE_NULL:
    case UBJ_TYPE_NOOP:
    case UBJ_TYPE_TRUE:
    case UBJ_TYPE_FALSE:
        return 0;
    case UBJ_TYPE_INT8:
    case UBJ_TYPE_UINT8:
    case UBJ_TYPE_CHAR:
        return 1;
    case UBJ_TYPE_INT16:
        return 2;
    case UBJ_TYPE_INT32:
    case UBJ_TYPE_FLOAT32:
        return 4;
    case UBJ_TYPE_INT64:
    case UBJ_TYPE_FLOAT64:
        return 8;
    case UBJ_TYPE_STRING:
    case UBJ_TYPE_HIGH_PRECISION:
        return 4;
        break;
    default:
        return -1;
    }
}

static ubj_err_t _wirte_ubjh(const ubj_value_t *v, ubjh_t *item, int pos)
{
    char *s;
    uint8_t *begin;
    int tlen = _type_len(v->type);

    if (tlen < 0)
    {
        return -UBJ_ERROR;
    }
    if (tlen == 0)
    {
        return UBJ_OK;
    }
    // copy string
    if (v->type == UBJ_TYPE_STRING || v->type == UBJ_TYPE_HIGH_PRECISION)
    {
        s = ubj_malloc(v->value.vstring.size + 1);
        if (s)
        {
            memcpy(s, v->value.vstring.string, v->value.vstring.size + 1);
        }
        else
        {
            return -UBJ_NOMEM_ERROR;
        }
        if (pos >= 0)
        {
            char **tmp;
            tmp = (char **)item->value.vbuf.ptr + pos;
            *tmp = s;
        }
        else
        {
            item->value.vstring.ptr = s;
            item->value.vstring.size = v->value.vstring.size;
        }
    }
    else
    {
        if (pos >= 0)
        {
            begin = (uint8_t *)item->value.vbuf.ptr + tlen * pos;
            memcpy(begin, &v->value.vuint8, tlen);
        }
        else
        {
            memcpy(&item->value.vuint8, &v->value.vuint8, tlen);
        }
    }
    return UBJ_OK;
}

static ubj_err_t _dump_array(ubjsonr_t *ubjr, const ubj_value_t *v, ubjh_t *item)
{
    // ubj_type_t type = v->type;
    ubj_err_t err;
    int32_t i, count;
    int tlen = _type_len(v->container.type);

    err = v->error;

    if (v->error != UBJ_OK)
    {
        return v->error;
    }
    else if (v->container.act != UBJ_CONTAINER_ENTER_FLAG)
    {
        return -UBJ_PARSE_ERROR;
    }

    if (v->container.count >= 0 && tlen >= 0)
    {
        if (v->container.count >= INT16_MAX)
        {
            UBJ_ERROR_PRINT("Wrong data count. c:%ld\n", (long)v->container.count);
            return -UBJ_PARSE_ERROR;
        }
        item->type = v->container.type;
        item->flags = UBJ_CONTAINER_IS_SIZED | \
                    UBJ_CONTAINER_IS_TYPED | \
                    UBJ_CONTAINER_IS_ARRAY | \
                    UBJ_CONTAINER_BUFFER_FLAG;
        if (tlen == 0)
        {
            item->value.vbuf.count = v->container.count;
        }
        else
        {
            //alloc mem
            item->value.vbuf.count = v->container.count;
            if (v->container.count * tlen > 0)
            {
                item->value.vbuf.ptr = ubj_malloc(v->container.count * tlen);
                if (item->value.vbuf.ptr == NULL)
                {
                    return -UBJ_NOMEM_ERROR;
                }
            }
        }
        count = v->container.count;
        // buffer
        for (i = 0; i < count; i++)
        {
            v = ubj_read_next(ubjr);
            if (v->error != UBJ_OK)
            {
                ubj_free(item->value.vbuf.ptr);
                item->value.vbuf.ptr = NULL;
                return v->error;
            }
            else if (v->container.act != UBJ_CONTAINER_ACCESS_FLAG)
            {
                ubj_free(item->value.vbuf.ptr);
                item->value.vbuf.ptr = NULL;
                return -UBJ_PARSE_ERROR;
            }
            else
            {
                err = _wirte_ubjh(v, item, i);
                if (err != UBJ_OK)
                {
                    ubj_free(item->value.vbuf.ptr);
                    item->value.vbuf.ptr = NULL;
                    return err;
                }
            }
        }
        // skip end flag
        v = ubj_read_next(ubjr);
        if (v->error != UBJ_OK || v->container.act != UBJ_CONTAINER_EXIT_FLAG)
        {
            ubj_free(item->value.vbuf.ptr);
            item->value.vbuf.ptr = NULL;
            return -UBJ_PARSE_ERROR;
        }
        else
        {
            return UBJ_OK;
        }
    }
    else
    {
        // container
        return _dump_container(ubjr, v, item);
    }
}

static struct _ubj_helper_res _dump_value(ubjsonr_t *ubjr, const ubj_value_t *v)
{
    struct _ubj_helper_res res;

    /* check error */
    res.error = v->error;
    res.item = NULL;
    if (res.error != UBJ_OK)
    {
        return res;
    }
    /* new item */
    res.item = ubj_malloc(sizeof(ubjh_t));
    if (res.item == NULL)
    {
        res.error = -UBJ_NOMEM_ERROR;
        return res;
    }
    memset(res.item, 0, sizeof(ubjh_t));
    res.item->type = v->type;

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
        res.error = _wirte_ubjh(v, res.item, -1);
        break;
    case UBJ_TYPE_ARRAY:
        res.error = _dump_array(ubjr, v, res.item);
        break;
    case UBJ_TYPE_OBJECT:
        res.error = _dump_container(ubjr, v, res.item);
        break;
    default:
        res.error = -UBJ_ERROR;
        break;
    }
    if (res.error != UBJ_OK)
    {
        // free mem
        ubj_free(res.item);
        res.item = NULL;
    }
    return res;
}

ubjh_t *ubj_parse(const void *buff, size_t len)
{
    const ubj_value_t *v;
    ubjsonr_t *ubjr = NULL;
    struct _ubj_helper_res res;

    ubjr = ubj_read_static_memory(buff, len);
    v = ubj_read_next(ubjr);
    res.item = NULL;
    if (v && v->error == UBJ_OK)
    {
        res = _dump_value(ubjr, v);
    }
    ubj_read_end(ubjr);
    return res.item;
}

void ubj_parse_free(ubjh_t *ubj)
{
    const ubjh_t *n;
    int i;

    if (ubj == NULL)
    {
        return;
    }
    // free child
    n = ubj_child_item(ubj);
    if (n)
    {
        ubj_parse_free((ubjh_t *)n);
    }
    // free next
    n = ubj_next_item(ubj);
    if (n)
    {
        ubj_parse_free((ubjh_t *)n);
    }
    // free buff
    if (ubj_is_buffer(ubj))
    {
        // free string buf
        if (ubj->type == UBJ_TYPE_STRING || ubj->type == UBJ_TYPE_HIGH_PRECISION)
        {
            char **s = ubj->value.vbuf.ptr;
            int count = ubj->value.vbuf.count;
            for (i = 0; i < count; i++)
            {
                ubj_free(s[i]);
                s[i] = NULL;
            }
        }
        ubj_free(ubj->value.vbuf.ptr);
        ubj->value.vbuf.ptr = NULL;
    }
    else if (ubj->type == UBJ_TYPE_STRING || ubj->type == UBJ_TYPE_HIGH_PRECISION)
    {
        ubj_free(ubj->value.vstring.ptr);
    }
    // free key
    if (ubj->flags & UBJ_CONTAINER_EXPECTS_KEY && ubj->auxiliary.key)
    {
        ubj_free(ubj->auxiliary.key);
        ubj->auxiliary.key = NULL;
    }
    // free self
    ubj_free(ubj);
}

int ubj_array_length(const ubjh_t *ubj)
{
    int cnt = 0;
    struct ubjsonr_helper_object *n;

    if (ubj_is_array(ubj))
    {
        n = ubj->value.vchild;
        while (n)
        {
            n = n->next;
            cnt++;
        }
        return cnt;
    }
    else
    {
        return -1;
    }
}

ubjh_t *ubj_array_item(const ubjh_t *ubj, int index)
{
    int cnt = 0;
    struct ubjsonr_helper_object *n;

    if (ubj_is_array(ubj))
    {
        n = ubj->value.vchild;
        while (n)
        {
            if (cnt == index)
            {
                return n;
            }
            else
            {
                n = n->next;
                cnt++;
            }
        }
        return NULL;
    }
    else
    {
        return NULL;
    }
}

ubjh_t *ubj_object_item(const ubjh_t *ubj, const char *key)
{
    int cnt = 0;
    ubjh_t *n;

    if (!ubj_is_object(ubj))
    {
        return NULL;
    }
    n = ubj_child_item(ubj);
    while (n)
    {
        if (strcmp(n->auxiliary.key, key) == 0)
        {
            return n;
        }
        else
        {
            n = ubj_next_item(n);
            cnt++;
        }
    }
    return NULL;
}

ubjh_t *ubj_next_item(const ubjh_t *ubj)
{
    return ubj->next;
}

ubjh_t *ubj_child_item(const ubjh_t *ubj)
{
    if (ubj_is_object(ubj) || ubj_is_array(ubj))
    {
        return ubj->value.vchild;
    }
    else
    {
        return NULL;
    }
}

ubj_type_t ubj_item_type(const ubjh_t *ubj)
{
    return ubj->type;
}

int ubj_buffer_length(const ubjh_t *ubj)
{
    int len;

    if (ubj_is_buffer(ubj))
    {
        len = _type_len(ubj->type);
        if (len == 0)
        {
            return ubj->value.vbuf.count;
        }
        else
        {
            return ubj->value.vbuf.count;
        }
    }
    else
    {
        return -1;
    }
}

/* Check item type and return its value */
char ubj_get_char(const ubjh_t *ubj)
{
    return ubj->value.vchar;
}

uint8_t ubj_get_bool(const ubjh_t *ubj)
{
    return ubj_is_true(ubj) ? 1 : 0;
}

uint8_t ubj_get_uint8(const ubjh_t *ubj)
{
    return ubj->value.vuint8;
}

int8_t ubj_get_int8(const ubjh_t *ubj)
{
    return ubj->value.vint8;
}

int16_t ubj_get_int16(const ubjh_t *ubj)
{
    return ubj->value.vint16;
}

int32_t ubj_get_int32(const ubjh_t *ubj)
{
    return ubj->value.vint32;
}

float ubj_get_float(const ubjh_t *ubj)
{
    return ubj->value.vfloat;
}

double ubj_get_double(const ubjh_t *ubj)
{
    return ubj->value.vdouble;
}

int64_t ubj_get_int64(const ubjh_t *ubj)
{
    return ubj->value.vint64;
}

int64_t ubj_get_integer(const ubjh_t *ubj)
{
    if (ubj_is_int32(ubj))
    {
        return ubj_get_int32(ubj);
    }
    else if (ubj_is_int8(ubj))
    {
        return ubj_get_int8(ubj);
    }
    else if (ubj_is_int16(ubj))
    {
        return ubj_get_int16(ubj);
    }
    else if (ubj_is_uint8(ubj))
    {
        return ubj_get_uint8(ubj);
    }
    else if (ubj_is_int64(ubj))
    {
        return ubj_get_int64(ubj);
    }
    else
    {
        return ubj->value.vint64;
    }
}

ubj_str_t ubj_get_string(const ubjh_t *ubj)
{
    return ubj->value.vstring;
}

ubj_str_t ubj_get_hpstring(const ubjh_t *ubj)
{
    return ubj->value.vhp;
}

ubj_buf_t ubj_get_buffer(const ubjh_t *ubj)
{
    ubj_buf_t buf;

    int c = ubj_buffer_length(ubj);

    if (c < 0)
    {
        buf.ptr = NULL;
        buf.count = UBJ_SIZE_INVALID;
        return buf;
    }
    return ubj->value.vbuf;
}

/* These functions check the type of an item */
int ubj_is_char(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_CHAR && !ubj_is_buffer(ubj);
}

int ubj_is_bool(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj_is_true(ubj) || ubj_is_false(ubj);
}

int ubj_is_true(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_TRUE && !ubj_is_buffer(ubj);
}

int ubj_is_false(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_FALSE && !ubj_is_buffer(ubj);
}

int ubj_is_uint8(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_UINT8 && !ubj_is_buffer(ubj);
}

int ubj_is_int8(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_INT8 && !ubj_is_buffer(ubj);
}

int ubj_is_int16(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_INT16 && !ubj_is_buffer(ubj);
}

int ubj_is_int32(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_INT32 && !ubj_is_buffer(ubj);
}

int ubj_is_float(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_FLOAT32 && !ubj_is_buffer(ubj);
}

int ubj_is_double(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_FLOAT64 && !ubj_is_buffer(ubj);
}

int ubj_is_int64(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_INT64 && !ubj_is_buffer(ubj);
}

int ubj_is_integer(const ubjh_t *ubj)
{
    if (ubj_is_int32(ubj) ||
        ubj_is_int64(ubj) ||
        ubj_is_int16(ubj) ||
        ubj_is_int8(ubj) ||
        ubj_is_uint8(ubj))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int ubj_is_string(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_STRING && !ubj_is_buffer(ubj);
}

int ubj_is_hpstring(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_HIGH_PRECISION && !ubj_is_buffer(ubj);
}

int ubj_is_null(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_NULL && !ubj_is_buffer(ubj);
}

int ubj_is_noop(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->type == UBJ_TYPE_NOOP && !ubj_is_buffer(ubj);
}

int ubj_is_array(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->flags & UBJ_CONTAINER_IS_ARRAY && !ubj_is_buffer(ubj);
}

int ubj_is_object(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return ubj->flags & UBJ_CONTAINER_IS_OBJECT && !ubj_is_buffer(ubj);
}

int ubj_is_buffer(const ubjh_t *ubj)
{
    if (ubj == NULL)
        return 0;
    else
        return (ubj->flags & UBJ_CONTAINER_IS_BUFFER) == UBJ_CONTAINER_IS_BUFFER;
}

ubj_err_t ubj_dumpb(const ubjh_t *ubj, ubjsonw_t *ubj_write)
{
    ubj_err_t err;
    int len;

    if (ubj == NULL || ubj_write == NULL)
    {
        return -UBJ_PARAM_ERROR;
    }
    // write buff
    if ((len = ubj_buffer_length(ubj)) >= 0)
    {
        ubj_buf_t buf = ubj_get_buffer(ubj);
        return ubj_write_buffer(ubj_write, buf.ptr, ubj_item_type(ubj), len);
    }
    // write data
    switch (ubj_item_type(ubj))
    {
    case UBJ_TYPE_NULL:                  // NULL
        err = ubj_write_null(ubj_write);
        break;
    case UBJ_TYPE_NOOP:                  // NOOP
        err = ubj_write_noop(ubj_write);
        break;
    case UBJ_TYPE_TRUE:                  // TRUE
        err = ubj_write_true(ubj_write);
        break;
    case UBJ_TYPE_FALSE:                 // FALSE
        err = ubj_write_false(ubj_write);
        break;
    case UBJ_TYPE_CHAR:                  // CHAR
        err = ubj_write_char(ubj_write, ubj_get_char(ubj));
        break;
    case UBJ_TYPE_STRING:                // STRING
    {
        ubj_str_t s = ubj_get_string(ubj);
        err = ubj_write_string(ubj_write, s.ptr);
        break;
    }
    case UBJ_TYPE_HIGH_PRECISION:        // PRECISION(STRING)
    {
        ubj_str_t s = ubj_get_hpstring(ubj);
        err = ubj_write_high_precision(ubj_write, s.ptr);
        break;
    }
    case UBJ_TYPE_INT8:                  // INTEGER
        err = ubj_write_int8(ubj_write, ubj_get_int8(ubj));
        break;
    case UBJ_TYPE_UINT8:                 // UINT8
        err = ubj_write_uint8(ubj_write, ubj_get_uint8(ubj));
        break;
    case UBJ_TYPE_INT16:                 // UINT16
        err = ubj_write_int16(ubj_write, ubj_get_int16(ubj));
        break;
    case UBJ_TYPE_INT32:                 // UINT32
        err = ubj_write_int32(ubj_write, ubj_get_int32(ubj));
        break;
    case UBJ_TYPE_INT64:                 // UINT64
        err = ubj_write_int64(ubj_write, ubj_get_int64(ubj));
        break;
    case UBJ_TYPE_FLOAT32:               // FLOAT
        err = ubj_write_float32(ubj_write, ubj_get_float(ubj));
        break;
    case UBJ_TYPE_FLOAT64:               // DOUBLE
        err = ubj_write_float64(ubj_write, ubj_get_double(ubj));
        break;
    case UBJ_TYPE_ARRAY:                 // ARRAY
    {
        const ubjh_t *n = ubj_child_item(ubj);
        err = ubj_begin_array(ubj_write);
        if (err != UBJ_OK)
            break;
        while (n)
        {
            err = ubj_dumpb(n, ubj_write);
            if (err != UBJ_OK)
                return err;
            n = ubj_next_item(n);
        }
        err = ubj_end_array(ubj_write);
        break;
    }
    case UBJ_TYPE_OBJECT:                // OBJECT
    {
        const ubjh_t *n = ubj_child_item(ubj);
        if ((err = ubj_begin_object(ubj_write)) != UBJ_OK)
            break;
        while (n)
        {
            if ((err = ubj_write_key(ubj_write, n->auxiliary.key)) != UBJ_OK)
                return err;
            if ((err = ubj_dumpb(n, ubj_write)) != UBJ_OK)
                return err;
            n = ubj_next_item(n);
        }
        err = ubj_end_object(ubj_write);
        break;
    }
    default:
        err = -UBJ_PARAM_ERROR;
        break;
    }
    return err;
}

ubjh_t *ubj_create_item(ubj_type_t type, const void *v)
{
    ubjh_t *item;
    int _tlen;
    ubj_str_t s;

    _tlen = _type_len(type);
    if (_tlen > 0 && v == NULL)
    {
        return NULL;
    }
    item = ubj_malloc(sizeof(ubjh_t));
    if (item)
    {
        // alloc mem
        memset(item, 0, sizeof(ubjh_t));
        item->type = type;
        if (type == UBJ_TYPE_STRING || type == UBJ_TYPE_HIGH_PRECISION)
        {
            s.size = strlen(v) + 1;
            s.ptr = ubj_malloc(s.size);
            if (s.ptr == NULL)
            {
                ubj_free(item);
                return NULL;
            }
            memcpy(s.ptr, v, s.size);
            v = &s;
        }
        else if (type == UBJ_TYPE_ARRAY)
        {
            _tlen = 0;
            item->flags = UBJ_CONTAINER_IS_ARRAY;
        }
        else if (type == UBJ_TYPE_OBJECT)
        {
            _tlen = 0;
            item->flags = UBJ_CONTAINER_IS_OBJECT;
        }
        if (_tlen > 0)
        {
            memcpy(&item->value.vuint8, v, _tlen);
        }
    }
    return item;
}

ubjh_t *ubj_create_string(const char *v)
{
    return ubj_create_item(UBJ_TYPE_STRING, v);
}

ubjh_t *ubj_create_char(char v)
{
    return ubj_create_item(UBJ_TYPE_CHAR, &v);
}

ubjh_t *ubj_create_uint8(uint8_t v)
{
    return ubj_create_item(UBJ_TYPE_UINT8, &v);
}

ubjh_t *ubj_create_int8(int8_t v)
{
    return ubj_create_item(UBJ_TYPE_INT8, &v);
}

ubjh_t *ubj_create_int16(int16_t v)
{
    return ubj_create_item(UBJ_TYPE_INT16, &v);
}

ubjh_t *ubj_create_int32(int32_t v)
{
    return ubj_create_item(UBJ_TYPE_INT32, &v);
}

ubjh_t *ubj_create_int64(int64_t v)
{
    return ubj_create_item(UBJ_TYPE_INT64, &v);
}

ubjh_t *ubj_create_high_precision(const char *hp)
{
    return ubj_create_item(UBJ_TYPE_HIGH_PRECISION, hp);
}

ubjh_t *ubj_create_integer(int64_t v)
{
    if (v >= 0 && v <= UINT8_MAX)
    {
        return ubj_create_uint8((uint8_t)v);
    }
    else if (v >= INT8_MIN && v <= INT8_MAX)
    {
        return ubj_create_int8((int8_t)v);
    }
    else if (v >= INT16_MIN && v <= INT16_MAX)
    {
        return ubj_create_int16((int16_t)v);
    }
    else if (v >= INT32_MIN && v <= INT32_MAX)
    {
        return ubj_create_int32((int32_t)v);
    }
    else
    {
        return ubj_create_int64((int64_t)v);
    }
}

ubjh_t *ubj_create_float32(float v)
{
    return ubj_create_item(UBJ_TYPE_FLOAT32, &v);
}

ubjh_t *ubj_create_float64(double v)
{
    return ubj_create_item(UBJ_TYPE_FLOAT64, &v);
}

ubjh_t *ubj_create_noop(void)
{
    return ubj_create_item(UBJ_TYPE_NOOP, NULL);
}

ubjh_t *ubj_create_null(void)
{
    return ubj_create_item(UBJ_TYPE_NULL, NULL);
}

ubjh_t *ubj_create_true(void)
{
    return ubj_create_item(UBJ_TYPE_TRUE, NULL);
}

ubjh_t *ubj_create_false(void)
{
    return ubj_create_item(UBJ_TYPE_FALSE, NULL);
}

ubjh_t *ubj_create_array(void)
{
    ubjh_t *item = ubj_malloc(sizeof(ubjh_t));

    if (item)
    {
        memset(item, 0, sizeof(ubjh_t));
        item->type = UBJ_TYPE_ARRAY;
        item->flags = UBJ_CONTAINER_IS_ARRAY;
        item->value.vchild = NULL;
    }
    return item;
}

ubjh_t *ubj_create_object(void)
{
    ubjh_t *item = ubj_malloc(sizeof(ubjh_t));

    if (item)
    {
        memset(item, 0, sizeof(ubjh_t));
        item->type = UBJ_TYPE_OBJECT;
        item->flags = UBJ_CONTAINER_IS_OBJECT;
        item->value.vchild = NULL;
    }
    return item;
}

ubjh_t *ubj_create_buffer(const uint8_t *data, ubj_type_t type, size_t count)
{
    size_t i;
    ubjh_t *item;
    char **ubj_str_ptr;
    int tlen = _type_len(type);

    if (tlen < 0 || count >= INT16_MAX)
    {
        return NULL;
    }
    // alloc mem
    item = ubj_malloc(sizeof(ubjh_t));
    if (item == NULL)
    {
        return NULL;
    }
    // init item
    memset(item, 0, sizeof(ubjh_t));
    item->type = type;
    item->flags = UBJ_CONTAINER_IS_SIZED | \
                UBJ_CONTAINER_IS_TYPED | \
                UBJ_CONTAINER_IS_ARRAY | \
                UBJ_CONTAINER_BUFFER_FLAG;
    if (tlen == 0)
    {
        item->value.vbuf.count = count;
        ubj_str_ptr = NULL;
    }
    else
    {
        //alloc mem
        item->value.vbuf.count = count;
        if (count * tlen > 0)
        {
            item->value.vbuf.ptr = ubj_malloc(count * tlen);
            if (item->value.vbuf.ptr == NULL)
            {
                ubj_free(item);
                return NULL;
            }
        }
        ubj_str_ptr = (char **)item->value.vbuf.ptr;
    }
    // write buffer
    if (type == UBJ_TYPE_STRING || type == UBJ_TYPE_HIGH_PRECISION)
    {
        const char **s_ptr = (const char **)data;
        long slen;
        for (i = 0; i < count; i++)
        {
            slen = strlen(s_ptr[i]) + 1;
            ubj_str_ptr[i] = ubj_malloc(slen);
            if (ubj_str_ptr[i] == NULL)
            {
                // free all string
                for (count = i, i = 0; i < count; i++)
                {
                    ubj_free(ubj_str_ptr[i]);
                }
                // free item
                ubj_free(item);
                return NULL;
            }
            else
            {
                // string
                memcpy(ubj_str_ptr[i], s_ptr[i], slen);
            }
        }
        return item;
    }
    else
    {
        memcpy(item->value.vbuf.ptr, data, tlen * count);
        return item;
    }
}

ubjh_t *ubj_add_item_to_array(ubjh_t *ubj_array, ubjh_t *item, long index)
{
    long cnt = 0;

    if (ubj_is_array(ubj_array) == 0 || item->next != NULL)
    {
        return NULL;
    }
    /* attch child */
    if (ubj_array->value.vchild == NULL)
    {
        ubj_array->value.vchild = item;
    }
    else
    {
        ubjh_t *next_ubj = ubj_array->value.vchild;
        while (index != cnt && next_ubj->next != NULL)
        {
            next_ubj = next_ubj->next;
        }
        item->next = next_ubj->next;
        next_ubj->next = item;
    }
    return ubj_array;
}

ubjh_t *ubj_add_item_to_object(ubjh_t *ubj_object, const char *key, ubjh_t *item)
{
    size_t len;
    char *k;

    if (ubj_is_object(ubj_object) == 0 || item->next != NULL)
    {
        return NULL;
    }
    len = strlen(key) + 1;
    k = ubj_malloc((unsigned int)len);
    if (k == NULL)
    {
        return NULL;
    }
    memcpy(k, key, len);
    item->auxiliary.key = k;
    /* attch child */
    if (ubj_object->value.vchild == NULL)
    {
        ubj_object->value.vchild = item;
    }
    else
    {
        ubjh_t *next_ubj = ubj_object->value.vchild;
        item->next = next_ubj->next;
        next_ubj->next = item;
    }
    return ubj_object;
}

ubjh_t *ubj_deepcopy(const ubjh_t *ubj)
{
    ubj_type_t type;

    if (ubj == NULL)
    {
        return NULL;
    }
    // write buff
    if (ubj_buffer_length(ubj) > 0)
    {
        ubj_buf_t buf = ubj_get_buffer(ubj);
        return ubj_create_buffer(buf.ptr, ubj_item_type(ubj), buf.count);
    }
    type = ubj_item_type(ubj);
    // write data
    switch (type)
    {
    case UBJ_TYPE_NULL:                  // NULL
        return ubj_create_null();
    case UBJ_TYPE_NOOP:                  // NOOP
        return ubj_create_noop();
    case UBJ_TYPE_TRUE:                  // TRUE
        return ubj_create_true();
    case UBJ_TYPE_FALSE:                 // FALSE
        return ubj_create_false();
    case UBJ_TYPE_CHAR:                  // CHAR
        return ubj_create_char(ubj_get_char(ubj));
    case UBJ_TYPE_STRING:                // STRING
        return ubj_create_string(ubj_get_string(ubj).ptr);
    case UBJ_TYPE_HIGH_PRECISION:        // PRECISION(STRING)
        return ubj_create_high_precision(ubj_get_hpstring(ubj).ptr);
    case UBJ_TYPE_INT8:                  // INTEGER
        return ubj_create_int8(ubj_get_int8(ubj));
    case UBJ_TYPE_UINT8:                 // UINT8
        return ubj_create_uint8(ubj_get_uint8(ubj));
    case UBJ_TYPE_INT16:                 // UINT16
        return ubj_create_int16(ubj_get_int16(ubj));
    case UBJ_TYPE_INT32:                 // UINT32
        return ubj_create_int32(ubj_get_int32(ubj));
    case UBJ_TYPE_INT64:                 // UINT64
        return ubj_create_int64(ubj_get_int64(ubj));
    case UBJ_TYPE_FLOAT32:               // FLOAT
        return ubj_create_float32(ubj_get_float(ubj));
    case UBJ_TYPE_FLOAT64:               // DOUBLE
        return ubj_create_float64(ubj_get_double(ubj));
    case UBJ_TYPE_ARRAY:                 // ARRAY
    case UBJ_TYPE_OBJECT:                // OBJECT
    {
        const ubjh_t *n;
        ubjh_t *container_ubj;
        if (type == UBJ_TYPE_ARRAY)
            container_ubj = ubj_create_array();
        else
            container_ubj = ubj_create_object();
        if (container_ubj == NULL)
            return NULL;
        n = ubj_child_item(ubj);
        while (n)
        {
            ubjh_t *item = ubj_deepcopy(n);
            if (item == NULL)
            {
                ubj_parse_free(container_ubj);
                return NULL;
            }
            else
            {
                if (type == UBJ_TYPE_ARRAY)
                    ubj_add_item_to_array(container_ubj, item, -1);
                else
                    ubj_add_item_to_object(container_ubj, n->auxiliary.key, item);
            }
            n = ubj_next_item(n);
        }
        return container_ubj;
    }
    default:
        return NULL;
    }
    return NULL;
}
