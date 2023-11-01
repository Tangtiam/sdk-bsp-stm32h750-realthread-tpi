/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include "ubj.h"
#include "ubj_internal.h"
#include "string.h"

#define UBJ_MAKES(_v, _l) \
    do {    \
        memcpy(&(((uint8_t *)buf)[total]), _v, _l);   \
        total += _l;  \
    } while (0)

#define UBJ_MAKE(_t)   \
    do {    \
        ((uint8_t *)buf)[total] = (uint8_t)(_t); \
        total += 1;  \
    } while (0)

static const char *ubj_ctab = UBJ_TYPE_TAB;

uint8_t *ubj_make_null(uint8_t *buf)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_NULL]);
    }
    return buf;
}

uint8_t *ubj_make_noop(uint8_t *buf)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_NOOP]);
    }
    return buf;
}

uint8_t *ubj_make_true(uint8_t *buf)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_TRUE]);
    }
    return buf;
}

uint8_t *ubj_make_false(uint8_t *buf)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_FALSE]);
    }
    return buf;
}

uint8_t *ubj_make_bool(uint8_t *buf, int v)
{
    if (v)
        return ubj_make_true(buf);
    else
        return ubj_make_false(buf);
}

uint8_t *ubj_make_char(uint8_t *buf, char v)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_CHAR]);
        /* write char */
        UBJ_MAKES(&v, 1);
    }
    return buf;
}

uint8_t *ubj_make_uint8(uint8_t *buf, uint8_t v)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_UINT8]);
        /* write uint8 */
        UBJ_MAKES(&v, 1);
    }
    return buf;
}

uint8_t *ubj_make_int8(uint8_t *buf, int8_t v)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT8]);
        /* write int8 */
        UBJ_MAKES(&v, 1);
    }
    return buf;
}

uint8_t *ubj_make_int16(uint8_t *buf, int16_t v)
{
    size_t total = 0;
    int16_t out = 0;

    if (buf)
    {
        UBJ_BIGENDIAN16(&v, &out);
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT16]);
        /* write int16 */
        UBJ_MAKES(&out, 2);
    }
    return buf;
}

uint8_t *ubj_make_int32(uint8_t *buf, int32_t v)
{
    size_t total = 0;
    int32_t out = 0;

    if (buf)
    {
        UBJ_BIGENDIAN32(&v, &out);
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT32]);
        /* write int16 */
        UBJ_MAKES(&out, 4);
    }
    return buf;
}

uint8_t *ubj_make_float(uint8_t *buf, float v)
{
    size_t total = 0;
    float out = 0.0;

    if (buf)
    {
        UBJ_BIGENDIAN32(&v, &out);
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_FLOAT32]);
        /* write int16 */
        UBJ_MAKES(&out, 4);
    }
    return buf;
}

uint8_t *ubj_make_double(uint8_t *buf, double v)
{
    size_t total = 0;
    double out = 0.0;

    if (buf)
    {
        UBJ_BIGENDIAN64(&v, &out);
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_FLOAT64]);
        /* write int16 */
        UBJ_MAKES(&out, 8);
    }
    return buf;
}

uint8_t *ubj_make_int64(uint8_t *buf, int64_t v)
{
    size_t total = 0;
    int64_t out = 0;

    if (buf)
    {
        UBJ_BIGENDIAN64(&v, &out);
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT64]);
        /* write int16 */
        UBJ_MAKES(&out, 8);
    }
    return buf;
}

uint8_t *ubj_make_integer(uint8_t *buf, int64_t v)
{
    if (v >= INT8_MIN && v <= INT8_MAX)
    {
        return ubj_make_int8(buf, (int8_t)v);
    }
    else if (v >= INT16_MIN && v <= INT16_MAX)
    {
        return ubj_make_int16(buf, (int16_t)v);
    }
    else if (v >= INT32_MIN && v <= INT32_MAX)
    {
        return ubj_make_int32(buf, (int32_t)v);
    }
    else
    {
        return ubj_make_int64(buf, (int64_t)v);
    }
}

uint8_t *ubj_make_string(uint8_t *buf, const char *s, size_t len)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_STRING]);
        /* write string len */
        if (len <= INT8_MAX)
        {
            int8_t v = (int8_t)len;
            /* write tag */
            UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT8]);
            /* write int8 */
            UBJ_MAKES(&v, 1);
        }
        else if (len <= INT16_MAX)
        {
            int16_t out = 0, v = (int8_t)len;
            UBJ_BIGENDIAN16(&v, &out);
            /* write tag */
            UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT16]);
            /* write int16 */
            UBJ_MAKES(&out, 2);
        }
        else
        {
            int32_t out = 0, v = (int32_t)len;
            UBJ_BIGENDIAN32(&v, &out);
            /* write tag */
            UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT32]);
            /* write int16 */
            UBJ_MAKES(&out, 4);
        }
        /* write string */
        UBJ_MAKES(s, len);
    }
    return buf;
}

uint8_t *ubj_make_buffer(uint8_t *buf, uint8_t *data, size_t len)
{
    size_t total = 0;

    if (buf)
    {
        /* write tag */
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_ARRAY]);
        /* write type */
        UBJ_MAKE('$');
        UBJ_MAKE(ubj_ctab[UBJ_TYPE_UINT8]);
        /* write len */
        UBJ_MAKE('#');
        if (len <= INT8_MAX)
        {
            int8_t v = (int8_t)len;
            /* write tag */
            UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT8]);
            /* write int8 */
            UBJ_MAKES(&v, 1);
        }
        else if (len <= INT16_MAX)
        {
            int16_t out = 0, v = (int8_t)len;
            UBJ_BIGENDIAN16(&v, &out);
            /* write tag */
            UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT16]);
            /* write int16 */
            UBJ_MAKES(&out, 2);
        }
        else
        {
            int32_t out = 0, v = (int32_t)len;
            UBJ_BIGENDIAN32(&v, &out);
            /* write tag */
            UBJ_MAKE(ubj_ctab[UBJ_TYPE_INT32]);
            /* write int16 */
            UBJ_MAKES(&out, 4);
        }
        UBJ_MAKES(data, len);
    }
    return buf;
}

#ifdef TYPE_EQ
#undef TYPE_EQ
#endif
#define TYPE_EQ(_type, _utype)   (((char)(_type)) == ubj_ctab[_utype])

int ubjson_to_null(const uint8_t *ubjson, size_t len, int def)
{
    if (ubjson && len == UBJ_STATIC_MEM_NULL && TYPE_EQ(ubjson[0], UBJ_TYPE_NULL))
    {
        def = ubj_ctab[UBJ_TYPE_NULL];
    }
    return def;
}

int ubjson_to_noop(const uint8_t *ubjson, size_t len, int def)
{
    if (ubjson && len == UBJ_STATIC_MEM_NOOP && TYPE_EQ(ubjson[0], UBJ_TYPE_NOOP))
    {
        def = ubj_ctab[UBJ_TYPE_NOOP];
    }
    return def;
}

char ubjson_to_char(const uint8_t *ubjson, size_t len, char def)
{
    if (ubjson && len == UBJ_STATIC_MEM_CHAR && TYPE_EQ(ubjson[0], UBJ_TYPE_CHAR))
    {
        def = ((char *)(&ubjson[1]))[0];
    }
    return def;
}

int ubjson_to_true(const uint8_t *ubjson, size_t len, int def)
{
    if (ubjson && len == UBJ_STATIC_MEM_TRUE && TYPE_EQ(ubjson[0], UBJ_TYPE_TRUE))
    {
        def = 1;
    }
    return def;
}

int ubjson_to_false(const uint8_t *ubjson, size_t len, int def)
{
    if (ubjson && len == UBJ_STATIC_MEM_FALSE && TYPE_EQ(ubjson[0], UBJ_TYPE_FALSE))
    {
        def = 0;
    }
    return def;
}

int ubjson_to_bool(const uint8_t *ubjson, size_t len, int def)
{
    if (ubjson && len == UBJ_STATIC_MEM_BOOL)
    {
        if (TYPE_EQ(ubjson[0], UBJ_TYPE_TRUE))
        {
            return ubjson_to_true(ubjson, len, def);
        }
        else if (TYPE_EQ(ubjson[0], UBJ_TYPE_FALSE))
        {
            return ubjson_to_false(ubjson, len, def);
        }
    }
    return def;
}

uint8_t ubjson_to_uint8(const uint8_t *ubjson, size_t len, uint8_t def)
{
    if (ubjson && len == UBJ_STATIC_MEM_UINT8 && TYPE_EQ(ubjson[0], UBJ_TYPE_UINT8))
    {
        def = ((uint8_t *)(&ubjson[1]))[0];
    }
    return def;
}

int8_t ubjson_to_int8(const uint8_t *ubjson, size_t len, int8_t def)
{
    if (ubjson && len == UBJ_STATIC_MEM_INT8 && TYPE_EQ(ubjson[0], UBJ_TYPE_INT8))
    {
        def = ((int8_t *)(&ubjson[1]))[0];
    }
    return def;
}

int16_t ubjson_to_int16(const uint8_t *ubjson, size_t len, int16_t def)
{
    uint8_t *buf;

    if (ubjson && len == UBJ_STATIC_MEM_INT16 && TYPE_EQ(ubjson[0], UBJ_TYPE_INT16))
    {
        buf = (uint8_t *)(&ubjson[1]);
        UBJ_BIGENDIAN16(buf, &def);
    }
    return def;
}

int32_t ubjson_to_int32(const uint8_t *ubjson, size_t len, int32_t def)
{
    uint8_t *buf;

    if (ubjson && len == UBJ_STATIC_MEM_INT32 && TYPE_EQ(ubjson[0], UBJ_TYPE_INT32))
    {
        buf = (uint8_t *)(&ubjson[1]);
        UBJ_BIGENDIAN32(buf, &def);
    }
    return def;
}

float ubjson_to_float(const uint8_t *ubjson, size_t len, float def)
{
    uint8_t *buf;

    if (ubjson && len == UBJ_STATIC_MEM_FLOAT32 && TYPE_EQ(ubjson[0], UBJ_TYPE_FLOAT32))
    {
        buf = (uint8_t *)(&ubjson[1]);
        UBJ_BIGENDIAN32(buf, &def);
    }
    return def;
}

double ubjson_to_double(const uint8_t *ubjson, size_t len, double def)
{
    uint8_t *buf;

    if (ubjson && len == UBJ_STATIC_MEM_FLOAT64 && TYPE_EQ(ubjson[0], UBJ_TYPE_FLOAT64))
    {
        buf = (uint8_t *)(&ubjson[1]);
        UBJ_BIGENDIAN64(buf, &def);
    }
    return def;
}

int64_t ubjson_to_int64(const uint8_t *ubjson, size_t len, int64_t def)
{
    uint8_t *buf;

    if (ubjson && len == UBJ_STATIC_MEM_INT64 && TYPE_EQ(ubjson[0], UBJ_TYPE_INT64))
    {
        buf = (uint8_t *)(&ubjson[1]);
        UBJ_BIGENDIAN64(buf, &def);
    }
    return def;
}

int64_t ubjson_to_integer(const uint8_t *ubjson, size_t len, int64_t def)
{
    if (ubjson)
    {
        if (TYPE_EQ(ubjson[0], UBJ_TYPE_UINT8))
        {
            def = ubjson_to_uint8(ubjson, len, (uint8_t)def);
        }
        else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT8))
        {
            def = ubjson_to_int8(ubjson, len, (int8_t)def);
        }
        else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT16))
        {
            def = ubjson_to_int16(ubjson, len, (int16_t)def);
        }
        else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT32))
        {
            def = ubjson_to_int32(ubjson, len, (int32_t)def);
        }
        else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT64))
        {
            def = ubjson_to_int64(ubjson, len, (int64_t)def);
        }
    }
    return def;
}

static int64_t _ubj_len(const uint8_t *ubjson, size_t len, const uint8_t **start)
{
    int64_t slen = -1;

    if (TYPE_EQ(ubjson[0], UBJ_TYPE_UINT8))
    {
        if (len >= UBJ_STATIC_MEM_UINT8)
        {
            slen = ((uint8_t *)(&ubjson[1]))[0];
            *start = &ubjson[UBJ_STATIC_MEM_UINT8];
        }
    }
    else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT8))
    {
        if (len >= UBJ_STATIC_MEM_INT8)
        {
            slen = ((int8_t *)(&ubjson[1]))[0];
            *start = &ubjson[UBJ_STATIC_MEM_INT8];
        }
    }
    else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT16))
    {
        if (len >= UBJ_STATIC_MEM_INT16)
        {
            uint8_t *buf = (uint8_t *)(&ubjson[1]);
            int16_t tmp = -1;
            UBJ_BIGENDIAN16(buf, &tmp);
            slen = tmp;
            *start = &ubjson[UBJ_STATIC_MEM_INT16];
        }
    }
    else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT32))
    {
        if (len >= UBJ_STATIC_MEM_INT32)
        {
            uint8_t *buf = (uint8_t *)(&ubjson[1]);
            int32_t tmp = -1;
            UBJ_BIGENDIAN32(buf, &tmp);
            slen = tmp;
            *start = &ubjson[UBJ_STATIC_MEM_INT32];
        }
    }
    else if (TYPE_EQ(ubjson[0], UBJ_TYPE_INT64))
    {
        if (len >= UBJ_STATIC_MEM_INT64)
        {
            uint8_t *buf = (uint8_t *)(&ubjson[1]);
            int64_t tmp = -1;
            UBJ_BIGENDIAN64(buf, &tmp);
            slen = tmp;
            *start = &ubjson[UBJ_STATIC_MEM_INT64];
        }
    }
    return slen;
}

char *ubjson_to_string(const uint8_t *ubjson, size_t len, char *out_buf, size_t *out_len)
{
    const uint8_t *begin = NULL;
    int64_t slen;
    char *str_buf = NULL;

    if (ubjson && len > 2 && out_len && TYPE_EQ(ubjson[0], UBJ_TYPE_STRING))
    {
        slen = _ubj_len(&ubjson[1], len - 1, &begin);
        if (slen >= 0 && len == UBJ_STATIC_MEM_STRING(slen))
        {
            if (out_buf && *out_len >= slen)
            {
                memcpy(out_buf, begin, (size_t)slen);
                str_buf = out_buf;
            }
            *out_len = (size_t)slen;
        }
    }
    return str_buf;
}

uint8_t *ubjson_to_buffer(const uint8_t *ubjson, size_t len, uint8_t *out_buf, size_t *out_len)
{
    const uint8_t *begin = NULL;
    int64_t slen;
    uint8_t *buf = NULL;

    if (ubjson && len > 5 && out_len && TYPE_EQ(ubjson[0], UBJ_TYPE_ARRAY) && 
        ubjson[1] == '$' && TYPE_EQ(ubjson[2], UBJ_TYPE_UINT8) && ubjson[3] == '#')
    {
        slen = _ubj_len(&ubjson[4], len - 4, &begin);
        if (slen >= 0 && len == UBJ_STATIC_MEM_BUFFER(slen))
        {
            if (out_buf && *out_len >= slen)
            {
                memcpy(out_buf, begin, (size_t)slen);
                buf = out_buf;
            }
            *out_len = (size_t)slen;
        }
    }
    return buf;
}

char *ubjson_to_json(const uint8_t *ubjson, size_t len, char *out_buf, size_t *out_len)
{
    char *json = NULL;
    size_t json_size = 0;
    char *str_buf = NULL;

    if (ubjson && len && out_len)
    {
        ubjsonr_t *ubjr = ubj_read_static_memory(ubjson, len);
        if (ubj_to_json(ubjr, &json, &json_size) == UBJ_OK)
        {
            if (out_buf && *out_len >= json_size)
            {
                memcpy(out_buf, json, json_size);
                str_buf = out_buf;
            }
            *out_len = json_size;
            ubj_free(json);
        }
    }
    return str_buf;
}
