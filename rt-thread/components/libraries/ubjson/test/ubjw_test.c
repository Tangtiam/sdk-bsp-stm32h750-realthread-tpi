
/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include "ubj.h"
#include <stdlib.h>
#include <string.h>
#include <rtthread.h>

#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
static void dump_hex(const void *ptr, rt_size_t buflen)
{
    unsigned char *buf = (unsigned char*)ptr;
    rt_size_t i, j;

    rt_kprintf("---- mem dump ----\n");
    for (i=0; i<buflen; i+=16)
    {
        rt_kprintf("%08X: ", i);

        for (j=0; j<16; j++)
            if (i+j < buflen)
                rt_kprintf("%02X ", buf[i+j]);
            else
                rt_kprintf("   ");
        rt_kprintf(" ");

        for (j=0; j<16; j++)
            if (i+j < buflen)
                rt_kprintf("%c", __is_print(buf[i+j]) ? buf[i+j] : '.');
        rt_kprintf("\n");
    }
}

void ubj_dump(ubjsonr_t *ubj)
{
    char *jsonptr;
    size_t length;

    length = 0;
    jsonptr = NULL;
    // 转换成 JSON
    ubj_to_json(ubj, &jsonptr, &length);
    if (jsonptr && length)
    {
        printf("%s\n", jsonptr);
        ubj_free(jsonptr);
    }
}

static void ubjson_general_string(ubjsonw_t *ubj)
{
    /* 字符串测试 */
    ubj_write_string(ubj, "this is string");
}

static void ubjson_general_char(ubjsonw_t *ubj)
{
    /* 字符测试 */
    ubj_write_char(ubj, '<');
}

static void ubjson_general_uint8(ubjsonw_t *ubj)
{
    /* UIN8 测试 */
    ubj_write_uint8(ubj, 249);
}

static void ubjson_general_int8(ubjsonw_t *ubj)
{
    /* INT8 测试 */
    ubj_write_int8(ubj, -98);
}

static void ubjson_general_int16(ubjsonw_t *ubj)
{
    /* INT16 测试 */
    ubj_write_int16(ubj, -10240);
}

static void ubjson_general_int32(ubjsonw_t *ubj)
{
    /* INT32 测试 */
    ubj_write_int32(ubj, -1147483648);
}

static void ubjson_general_int64(ubjsonw_t *ubj)
{
    /* INT64 测试 */
    ubj_write_int64(ubj, 8223372036854775808);
}

static void ubjson_general_float32(ubjsonw_t *ubj)
{
    /* 单精度浮点测试 */
    ubj_write_float32(ubj, 3.125f);
}

static void ubjson_general_float64(ubjsonw_t *ubj)
{
    /* 双进度浮点 */
    ubj_write_float64(ubj, 3.125125125125);
}

static void ubjson_general_null(ubjsonw_t *ubj)
{
    /* 空测试 */
    ubj_write_null(ubj);
}

static void ubjson_general_true(ubjsonw_t *ubj)
{
    /* bool 测试 */
    ubj_write_true(ubj);
}

static void ubjson_general_false(ubjsonw_t *ubj)
{
    /* bool 测试 */
    ubj_write_false(ubj);
}

static void ubjson_general_noop(ubjsonw_t *ubj)
{
    /* noop 测试 */
    ubj_write_noop(ubj);
}

static void ubjson_general_object(ubjsonw_t *ubj)
{
    /* 空 object 测试 */
    ubj_begin_object(ubj);
    ubj_end_object(ubj);
}

static void ubjson_general_array(ubjsonw_t *ubj)
{
    /* 空 array 测试 */
    ubj_begin_array(ubj);
    ubj_end_array(ubj);
}

static void ubjson_general_buffer(ubjsonw_t *ubj)
{
    /* buffer 写入测试 */
    ubj_write_buffer(ubj, (uint8_t *)"0123456789", UBJ_TYPE_CHAR, sizeof("0123456789") - 1);
}

static void ubjson_general_empty_buffer(ubjsonw_t *ubj)
{
    /* 空 buffer 写入测试 */
    ubj_write_buffer(ubj, NULL, UBJ_TYPE_UINT8, 0);
}

static void ubjson_object_one(ubjsonw_t *ubj)
{
    ubj_begin_object(ubj);
    /* 字符串测试 */
    ubj_object_write_string(ubj, "1", "this is string");
    /* 字符测试 */
    ubj_object_write_char(ubj, "2", '<');
    /* UIN8 测试 */
    ubj_object_write_uint8(ubj, "3", 249);
    /* INT8 测试 */
    ubj_object_write_int8(ubj, "4", -98);
    /* INT16 测试 */
    ubj_object_write_int16(ubj, "5", -10240);
    /* INT32 测试 */
    ubj_object_write_int32(ubj, "6", -1147483648);
    /* INT64 测试 */
    ubj_object_write_int64(ubj, "7", 8223372036854775808);
    /* 单精度浮点测试 */
    ubj_object_write_float32(ubj, "8", 3.125);
    /* 双进度浮点 */
    ubj_object_write_float64(ubj, "9", 3.125);
    /* 空测试 */
    ubj_object_write_null(ubj, "10");
    /* bool 测试 */
    ubj_object_write_true(ubj, "11");
    ubj_object_write_false(ubj, "12");
    ubj_end_object(ubj);
}

static void ubjson_object_two(ubjsonw_t *ubj)
{
    ubj_begin_object(ubj);
    /* 字符串测试 */
    ubj_object_write_string(ubj, "1", "this is string");
    /* 嵌套对象测试 */
    ubj_object_write_object(ubj, "2");
    /* UIN8 测试 */
    ubj_object_write_uint8(ubj, "2.1", 249);
    /* INT8 测试 */
    ubj_object_write_int8(ubj, "2.2", -98);
    /* 结束嵌套 <2> */
    ubj_end_object(ubj);

    /* 嵌套对象测试 */
    ubj_object_write_object(ubj, "3");
    /* INT16 测试 */
    ubj_object_write_int16(ubj, "3.1", -10240);
    /* INT32 测试 */
    ubj_object_write_int32(ubj, "3.2", -1147483648);
    /* 嵌套对象测试 */
    ubj_object_write_object(ubj, "3.3");
    /* 单精度浮点测试 */
    ubj_object_write_float32(ubj, "3.3.1", 3.125);
    /* 双进度浮点 */
    ubj_object_write_float64(ubj, "3.3.2", 3.125);
    /* 结束嵌套 <3.3> */
    ubj_end_object(ubj);
    /* 结束嵌套 <3> */
    ubj_end_object(ubj);

    ubj_end_object(ubj);
}

static void ubjson_object_three(ubjsonw_t *ubj)
{
    ubj_begin_object(ubj);
    /* 强类型+长度测试 */
    /* 字符串 */
    ubj_object_write_fixed_object(ubj, "1", UBJ_TYPE_STRING, 3);
    ubj_object_write_string(ubj, "1.1", "this is string 1");
    ubj_object_write_string(ubj, "1.2", "this is string 2");
    ubj_object_write_string(ubj, "1.3", "this is string 3");
    ubj_end_object(ubj);
    /* 字符测试 */
    ubj_object_write_fixed_object(ubj, "2", UBJ_TYPE_CHAR, 3);
    ubj_object_write_char(ubj, "2.1", '<');
    ubj_object_write_char(ubj, "2.2", '>');
    ubj_object_write_char(ubj, "2.3", '.');
    ubj_end_object(ubj);
    /* UIN8 测试 */
    ubj_object_write_fixed_object(ubj, "3", UBJ_TYPE_UINT8, 3);
    ubj_object_write_uint8(ubj, "3.1", 246);
    ubj_object_write_uint8(ubj, "3.2", 247);
    ubj_object_write_uint8(ubj, "3.3", 248);
    ubj_end_object(ubj);
    /* INT8 测试 */
    ubj_object_write_fixed_object(ubj, "4", UBJ_TYPE_INT8, 3);
    ubj_object_write_int8(ubj, "4.1", -97);
    ubj_object_write_int8(ubj, "4.2", -98);
    ubj_object_write_int8(ubj, "4.3", -99);
    ubj_end_object(ubj);
    /* INT16 测试 */
    ubj_object_write_fixed_object(ubj, "5", UBJ_TYPE_INT16, 3);
    ubj_object_write_int16(ubj, "5.1", -10238);
    ubj_object_write_int16(ubj, "5.2", -10239);
    ubj_object_write_int16(ubj, "5.3", -10240);
    ubj_end_object(ubj);
    /* INT32 测试 */
    ubj_object_write_fixed_object(ubj, "6", UBJ_TYPE_INT32, 3);
    ubj_object_write_int32(ubj, "6.1", -1147483647);
    ubj_object_write_int32(ubj, "6.2", -1147483648);
    ubj_object_write_int32(ubj, "6.3", -1147483649);
    ubj_end_object(ubj);
    /* INT64 测试 */
    ubj_object_write_fixed_object(ubj, "7", UBJ_TYPE_INT64, 3);
    ubj_object_write_int64(ubj, "7.1", 8223372036854775807);
    ubj_object_write_int64(ubj, "7.2", 8223372036854775808);
    ubj_object_write_int64(ubj, "7.3", 8223372036854775809);
    ubj_end_object(ubj);
    /* 单精度浮点测试 */
    ubj_object_write_fixed_object(ubj, "8", UBJ_TYPE_FLOAT32, 3);
    ubj_object_write_float32(ubj, "8.1", 3.120f);
    ubj_object_write_float32(ubj, "8.2", 3.125f);
    ubj_object_write_float32(ubj, "8.3", 3.130f);
    ubj_end_object(ubj);
    /* 双进度浮点 */
    ubj_object_write_fixed_object(ubj, "9", UBJ_TYPE_FLOAT64, 3);
    ubj_object_write_float64(ubj, "9.1", 3.141592653587);
    ubj_object_write_float64(ubj, "9.2", 3.141592653588);
    ubj_object_write_float64(ubj, "9.3", 3.125);
    ubj_end_object(ubj);
    /* 空测试 */
    ubj_object_write_fixed_object(ubj, "10", UBJ_TYPE_NULL, 3);
    ubj_object_write_null(ubj, "10.1");
    ubj_object_write_null(ubj, "10.2");
    ubj_object_write_null(ubj, "10.3");
    ubj_end_object(ubj);
    /* bool 测试 */
    ubj_object_write_fixed_object(ubj, "11", UBJ_TYPE_TRUE, 3);
    ubj_object_write_true(ubj, "11.1");
    ubj_object_write_true(ubj, "11.2");
    ubj_object_write_true(ubj, "11.3");
    ubj_end_object(ubj);
    ubj_object_write_fixed_object(ubj, "12", UBJ_TYPE_FALSE, 3);
    ubj_object_write_false(ubj, "12.1");
    ubj_object_write_false(ubj, "12.2");
    ubj_object_write_false(ubj, "12.3");
    ubj_end_object(ubj);

    ubj_end_object(ubj);
}

static void ubjson_object_four(ubjsonw_t *ubj)
{
    ubj_begin_object(ubj);
    /* 强类型+长度测试 */
    /* 字符串 */
    ubj_object_write_fixed_object(ubj, "1", UBJ_TYPE_MIXED, 3);
    ubj_object_write_string(ubj, "1.1", "this is string 1");
    ubj_object_write_string(ubj, "1.2", "this is string 2");
    ubj_object_write_string(ubj, "1.3", "this is string 3");
    ubj_end_object(ubj);
    /* 字符测试 */
    ubj_object_write_fixed_object(ubj, "2", UBJ_TYPE_MIXED, 3);
    ubj_object_write_char(ubj, "2.1", '<');
    ubj_object_write_char(ubj, "2.2", '>');
    ubj_object_write_char(ubj, "2.3", '.');
    ubj_end_object(ubj);
    /* UIN8 测试 */
    ubj_object_write_fixed_object(ubj, "3", UBJ_TYPE_MIXED, 3);
    ubj_object_write_uint8(ubj, "3.1", 246);
    ubj_object_write_uint8(ubj, "3.2", 247);
    ubj_object_write_uint8(ubj, "3.3", 248);
    ubj_end_object(ubj);
    /* INT8 测试 */
    ubj_object_write_fixed_object(ubj, "4", UBJ_TYPE_MIXED, 3);
    ubj_object_write_int8(ubj, "4.1", -97);
    ubj_object_write_int8(ubj, "4.2", -98);
    ubj_object_write_int8(ubj, "4.3", -99);
    ubj_end_object(ubj);
    /* INT16 测试 */
    ubj_object_write_fixed_object(ubj, "5", UBJ_TYPE_MIXED, 3);
    ubj_object_write_int16(ubj, "5.1", -10238);
    ubj_object_write_int16(ubj, "5.2", -10239);
    ubj_object_write_int16(ubj, "5.3", -10240);
    ubj_end_object(ubj);
    /* INT32 测试 */
    ubj_object_write_fixed_object(ubj, "6", UBJ_TYPE_MIXED, 3);
    ubj_object_write_int32(ubj, "6.1", -1147483647);
    ubj_object_write_int32(ubj, "6.2", -1147483648);
    ubj_object_write_int32(ubj, "6.3", -1147483649);
    ubj_end_object(ubj);
    /* INT64 测试 */
    ubj_object_write_fixed_object(ubj, "7", UBJ_TYPE_MIXED, 3);
    ubj_object_write_int64(ubj, "7.1", 8223372036854775807);
    ubj_object_write_int64(ubj, "7.2", 8223372036854775808);
    ubj_object_write_int64(ubj, "7.3", 8223372036854775809);
    ubj_end_object(ubj);
    /* 单精度浮点测试 */
    ubj_object_write_fixed_object(ubj, "8", UBJ_TYPE_MIXED, 3);
    ubj_object_write_float32(ubj, "8.1", 3.120f);
    ubj_object_write_float32(ubj, "8.2", 3.125f);
    ubj_object_write_float32(ubj, "8.3", 3.130f);
    ubj_end_object(ubj);
    /* 双进度浮点 */
    ubj_object_write_fixed_object(ubj, "9", UBJ_TYPE_MIXED, 3);
    ubj_object_write_float64(ubj, "9.1", 3.141592653587);
    ubj_object_write_float64(ubj, "9.2", 3.141592653588);
    ubj_object_write_float64(ubj, "9.3", 3.125);
    ubj_end_object(ubj);
    /* 空测试 */
    ubj_object_write_fixed_object(ubj, "10", UBJ_TYPE_MIXED, 3);
    ubj_object_write_null(ubj, "10.1");
    ubj_object_write_null(ubj, "10.2");
    ubj_object_write_null(ubj, "10.3");
    ubj_end_object(ubj);
    /* bool 测试 */
    ubj_object_write_fixed_object(ubj, "11", UBJ_TYPE_MIXED, 3);
    ubj_object_write_true(ubj, "11.1");
    ubj_object_write_true(ubj, "11.2");
    ubj_object_write_true(ubj, "11.3");
    ubj_end_object(ubj);
    ubj_object_write_fixed_object(ubj, "12", UBJ_TYPE_MIXED, 3);
    ubj_object_write_false(ubj, "12.1");
    ubj_object_write_false(ubj, "12.2");
    ubj_object_write_false(ubj, "12.3");
    ubj_end_object(ubj);

    ubj_end_object(ubj);
}

static void ubjson_array_three(ubjsonw_t *ubj)
{
    ubj_begin_array(ubj);
    /* 强类型+长度测试 */
    /* 字符串 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_STRING, 3);
    ubj_write_string(ubj, "this is string 1");
    ubj_write_string(ubj, "this is string 2");
    ubj_write_string(ubj, "this is string 3");
    ubj_end_array(ubj);
    /* 字符测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_CHAR, 3);
    ubj_write_char(ubj, '<');
    ubj_write_char(ubj, '>');
    ubj_write_char(ubj, '.');
    ubj_end_array(ubj);
    /* UIN8 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_UINT8, 3);
    ubj_write_uint8(ubj, 246);
    ubj_write_uint8(ubj, 247);
    ubj_write_uint8(ubj, 248);
    ubj_end_array(ubj);
    /* INT8 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_INT8, 3);
    ubj_write_int8(ubj, -97);
    ubj_write_int8(ubj, -98);
    ubj_write_int8(ubj, -99);
    ubj_end_array(ubj);
    /* INT16 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_INT16, 3);
    ubj_write_int16(ubj, -10238);
    ubj_write_int16(ubj, -10239);
    ubj_write_int16(ubj, -10240);
    ubj_end_array(ubj);
    /* INT32 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_INT32, 3);
    ubj_write_int32(ubj, -1147483647);
    ubj_write_int32(ubj, -1147483648);
    ubj_write_int32(ubj, -1147483649);
    ubj_end_array(ubj);
    /* INT64 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_INT64, 3);
    ubj_write_int64(ubj, 8223372036854775807);
    ubj_write_int64(ubj, 8223372036854775808);
    ubj_write_int64(ubj, 8223372036854775809);
    ubj_end_array(ubj);
    /* 单精度浮点测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_FLOAT32, 3);
    ubj_write_float32(ubj, 3.120f);
    ubj_write_float32(ubj, 3.125f);
    ubj_write_float32(ubj, 3.130f);
    ubj_end_array(ubj);
    /* 双进度浮点 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_FLOAT64, 3);
    ubj_write_float64(ubj, 3.141592653587);
    ubj_write_float64(ubj, 3.141592653588);
    ubj_write_float64(ubj, 3.125);
    ubj_end_array(ubj);
    /* 空测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_NULL, 3);
    ubj_write_null(ubj);
    ubj_write_null(ubj);
    ubj_write_null(ubj);
    ubj_end_array(ubj);
    /* bool 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_TRUE, 3);
    ubj_write_true(ubj);
    ubj_write_true(ubj);
    ubj_write_true(ubj);
    ubj_end_array(ubj);
    ubj_begin_fixed_array(ubj, UBJ_TYPE_FALSE, 3);
    ubj_write_false(ubj);
    ubj_write_false(ubj);
    ubj_write_false(ubj);
    ubj_end_array(ubj);

    ubj_end_array(ubj);
}

static void ubjson_array_four(ubjsonw_t *ubj)
{
    ubj_begin_array(ubj);
    /* 强类型+长度测试 */
    /* 字符串 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_string(ubj, "this is string 1");
    ubj_write_string(ubj, "this is string 2");
    ubj_write_string(ubj, "this is string 3");
    ubj_end_array(ubj);
    /* 字符测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_char(ubj, '<');
    ubj_write_char(ubj, '>');
    ubj_write_char(ubj, '.');
    ubj_end_array(ubj);
    /* UIN8 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_UINT8, 3);
    ubj_write_uint8(ubj, 246);
    ubj_write_uint8(ubj, 247);
    ubj_write_uint8(ubj, 248);
    ubj_end_array(ubj);
    /* INT8 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_int8(ubj, -97);
    ubj_write_int8(ubj, -98);
    ubj_write_int8(ubj, -99);
    ubj_end_array(ubj);
    /* INT16 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_int16(ubj, -10238);
    ubj_write_int16(ubj, -10239);
    ubj_write_int16(ubj, -10240);
    ubj_end_array(ubj);
    /* INT32 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_int32(ubj, -1147483647);
    ubj_write_int32(ubj, -1147483648);
    ubj_write_int32(ubj, -1147483649);
    ubj_end_array(ubj);
    /* INT64 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_int64(ubj, 8223372036854775807);
    ubj_write_int64(ubj, 8223372036854775808);
    ubj_write_int64(ubj, 8223372036854775809);
    ubj_end_array(ubj);
    /* 单精度浮点测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_float32(ubj, 3.120f);
    ubj_write_float32(ubj, 3.125f);
    ubj_write_float32(ubj, 3.130f);
    ubj_end_array(ubj);
    /* 双进度浮点 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_float64(ubj, 3.141592653587);
    ubj_write_float64(ubj, 3.141592653588);
    ubj_write_float64(ubj, 3.125);
    ubj_end_array(ubj);
    /* 空测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_null(ubj);
    ubj_write_null(ubj);
    ubj_write_null(ubj);
    ubj_end_array(ubj);
    /* bool 测试 */
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_true(ubj);
    ubj_write_true(ubj);
    ubj_write_true(ubj);
    ubj_end_array(ubj);
    ubj_begin_fixed_array(ubj, UBJ_TYPE_MIXED, 3);
    ubj_write_false(ubj);
    ubj_write_false(ubj);
    ubj_write_false(ubj);
    ubj_end_array(ubj);

    ubj_end_array(ubj);
}

static void ubjson_array_one(ubjsonw_t *ubj)
{
    ubj_begin_array(ubj);
    /* 字符串测试 */
    ubj_write_string(ubj, "this is string");
    /* 字符测试 */
    ubj_write_char(ubj, '<');
    /* UIN8 测试 */
    ubj_write_uint8(ubj, 249);
    /* INT8 测试 */
    ubj_write_int8(ubj, -98);
    /* INT16 测试 */
    ubj_write_int16(ubj, -10240);
    /* INT32 测试 */
    ubj_write_int32(ubj, -1147483648);
    /* INT64 测试 */
    ubj_write_int64(ubj, 8223372036854775808);
    /* 单精度浮点测试 */
    ubj_write_float32(ubj, 3.125);
    /* 双进度浮点 */
    ubj_write_float64(ubj, 3.125);
    /* 空测试 */
    ubj_write_null(ubj);
    /* bool 测试 */
    ubj_write_true(ubj);
    ubj_write_false(ubj);
    ubj_end_array(ubj);
}

static void ubjson_array_two(ubjsonw_t *ubj)
{
    ubj_begin_array(ubj);
    /* 字符串测试 */
    ubj_write_string(ubj, "this is string");
    /* 嵌套数组测试 */
    ubj_begin_array(ubj);
    /* UIN8 测试 */
    ubj_write_uint8(ubj, 249);
    /* INT8 测试 */
    ubj_write_int8(ubj, -98);
    /* 结束嵌套 <2> */
    ubj_end_array(ubj);

    /* 嵌套数组测试 */
    ubj_begin_array(ubj);
    /* INT16 测试 */
    ubj_write_int16(ubj, -10240);
    /* INT32 测试 */
    ubj_write_int32(ubj, -1147483648);
    /* 嵌套数组测试 */
    ubj_begin_array(ubj);
    /* 单精度浮点测试 */
    ubj_write_float32(ubj, 3.125);
    /* 双进度浮点 */
    ubj_write_float64(ubj, 3.125);
    /* 结束嵌套 <3.3> */
    ubj_end_array(ubj);
    /* 结束嵌套 <3> */
    ubj_end_array(ubj);

    ubj_end_array(ubj);
}

static void ubjson_object_array(ubjsonw_t *ubj)
{
    ubj_begin_object(ubj);
    /* 字符串测试 */
    ubj_object_write_string(ubj, "1", "this is string");
    /* 嵌套对象测试 */
    ubj_object_write_object(ubj, "2");
    /* UIN8 测试 */
    ubj_object_write_uint8(ubj, "2.1", 249);
    /* INT8 测试 */
    ubj_object_write_int8(ubj, "2.2", -98);
    /* 结束嵌套 <2> */
    ubj_end_object(ubj);

    /* 嵌套数组测试 */
    ubj_object_write_array(ubj, "3");
    /* INT8 测试 */
    ubj_write_int8(ubj, -97);
    /* 结束嵌套 <2> */
    ubj_end_array(ubj);

    /* 嵌套数组测试 */
    ubj_object_write_array(ubj, "4");
    /* INT8 测试 */
    ubj_write_int8(ubj, -98);
    /* 结束嵌套 <2> */
    ubj_end_array(ubj);

    /* 嵌套数组测试 */
    ubj_object_write_fixed_array(ubj, "5", UBJ_TYPE_INT8, 1);
    /* INT8 测试 */
    ubj_write_int8(ubj, -99);
    /* 结束嵌套 <2> */
    ubj_end_array(ubj);

    /* 嵌套对象测试 */
    ubj_object_write_array(ubj, "6");
    /* 嵌套数组测试 */
    ubj_begin_object(ubj);
    /* INT16 测试 */
    ubj_object_write_int16(ubj, "6.1.1", -10240);
    /* INT32 测试 */
    ubj_object_write_int32(ubj, "6.1.2", -1147483648);
    /* 嵌套对象测试 */
    ubj_object_write_object(ubj, "6.1.3");
    /* 单精度浮点测试 */
    ubj_object_write_float32(ubj, "6.1.3.1", 3.125);
    /* 双进度浮点 */
    ubj_object_write_float64(ubj, "6.1.3.2", 3.125);
    /* 结束嵌套 <6.1.3> */
    ubj_end_object(ubj);
    /* 结束嵌套 <6.1> */
    ubj_end_object(ubj);
    /* 结束嵌套 <3> */
    ubj_end_array(ubj);

    ubj_end_object(ubj);
}

static void ubjson_object_write_buffer(ubjsonw_t *ubj)
{
    ubj_begin_object(ubj);
    /* char buf 测试 */
    ubj_object_write_buffer(ubj, "char", (uint8_t *)"0123456789", UBJ_TYPE_CHAR, sizeof("0123456789") - 1);
    ubj_end_object(ubj);
}

static void ubjson_array_write_buffer(ubjsonw_t *ubj)
{
    ubj_begin_array(ubj);
    /* char buf 测试 */
    ubj_write_buffer(ubj, (uint8_t *)"0123456789", UBJ_TYPE_CHAR, sizeof("0123456789") - 1);
    ubj_end_array(ubj);
}

static void __ubj_dump_mem(ubjsonw_t *ubj)
{
    ubjsonr_t *rubj;

    dump_hex(ubj_get_memory(ubj), ubj->total);
    /* dump 测试 */
    rubj = ubj_read_memory(ubj_get_memory(ubj), ubj->total);
    ubj_dump(rubj);
    ubj_read_end(rubj);
}

static void ubjson_mem_test(void)
{
    ubjsonw_t *ubj;

    /* string 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_string(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* char 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_char(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* uint8_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_uint8(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* int8_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_int8(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* int16_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_int16(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* int32_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_int32(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* int64_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_int64(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* float32_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_float32(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* float64_t 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_float64(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* null 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_null(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* true 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_true(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* false 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_false(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* noop 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_noop(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 空 object 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_object(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 空 array 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_array(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* buffer 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_buffer(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 空 buffer 写入测试 */
    ubj = ubj_write_memory();
    ubjson_general_empty_buffer(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 对象写入测试 */
    ubj = ubj_write_memory();
    ubjson_object_one(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 嵌套对象测试 */
    ubj = ubj_write_memory();
    ubjson_object_two(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 数组测试 */
    ubj = ubj_write_memory();
    ubjson_array_one(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 嵌套对象测试 */
    ubj = ubj_write_memory();
    ubjson_array_two(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 对象强类型+长度测试 */
    ubj = ubj_write_memory();
    ubjson_object_three(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 数组强类型+长度测试 */
    ubj = ubj_write_memory();
    ubjson_array_three(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 对象长度测试 */
    ubj = ubj_write_memory();
    ubjson_object_four(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 数组长度测试 */
    ubj = ubj_write_memory();
    ubjson_array_four(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 数组对象混合嵌套 */
    ubj = ubj_write_memory();
    ubjson_object_array(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 对象 buffer 写入测试 */
    ubj = ubj_write_memory();
    ubjson_object_write_buffer(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);

    /* 数组 buffer 写入测试 */
    ubj = ubj_write_memory();
    ubjson_array_write_buffer(ubj);
    /* dump */
    __ubj_dump_mem(ubj);
    ubj_write_end(ubj);
}

static int integer_cmp(int64_t raw, const ubj_value_t *value)
{
    int res = 0;

    switch (value->type)
    {
    case UBJ_TYPE_UINT8:
        res = raw == value->value.vuint8;
        break;
    case UBJ_TYPE_INT8:
        res = raw == value->value.vint8;
        break;
    case UBJ_TYPE_INT16:
        res = raw == value->value.vint16;
        break;
    case UBJ_TYPE_INT32:
        res = raw == value->value.vint32;
        break;
    case UBJ_TYPE_INT64:
        res = raw == value->value.vint64;
        break;
    default:
        break;
    }
    return res;
}

static void ubj_integer_test(void)
{
    int64_t v_tab[] = {INT8_MIN, INT16_MIN, INT32_MIN, INT64_MIN, INT8_MAX, INT16_MAX,
        INT32_MAX, INT64_MAX, UINT8_MAX, UINT16_MAX, UINT32_MAX,UINT64_MAX};
    ubjsonw_t* ubjw;
    ubjsonr_t* ubjr;
    const ubj_value_t* value;
    ubj_err_t err;
    int i;

    ubjw = ubj_write_memory();
    err = ubj_begin_array(ubjw);
    for (i = 0; i < sizeof(v_tab) / sizeof(v_tab[0]); i++)
    {
        ubj_write_integer(ubjw, v_tab[i] - 1);
        ubj_write_integer(ubjw, v_tab[i]);
        ubj_write_integer(ubjw, v_tab[i] + 1);
    }
    err = ubj_end_array(ubjw);
    /* make ubj read object */
    ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total);
    value = ubj_read_next(ubjr);
    for (i = 0; i < sizeof(v_tab) / sizeof(v_tab[0]); i++)
    {
        value = ubj_read_next(ubjr);
        if (!integer_cmp(v_tab[i] - 1, value))
        {
            rt_kprintf("err!\n");
        }
        value = ubj_read_next(ubjr);
        if (!integer_cmp(v_tab[i], value))
        {
            rt_kprintf("err!\n");
        }
        value = ubj_read_next(ubjr);
        if (!integer_cmp(v_tab[i] + 1, value))
        {
            rt_kprintf("err!\n");
        }
    }
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
}



static void ubjson_helper_general_string(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建字符串 */
    ubjw = ubj_write_memory();
    ubj_write_string(ubjw, "this is string");
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_STRING;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 1;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= strcmp(ubj_get_string(ubjh).ptr, "this is string") == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_char(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建字符 */
    ubjw = ubj_write_memory();
    ubj_write_char(ubjw, '<');
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_CHAR;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 1;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_char(ubjh) == '<';
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_uint8(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 UINT8 */
    ubjw = ubj_write_memory();
    ubj_write_uint8(ubjw, 249);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_UINT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 1;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_uint8(ubjh) == 249;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_int8(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT8 */
    ubjw = ubj_write_memory();
    ubj_write_int8(ubjw, -98);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 1;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int8(ubjh) == -98;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_int16(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT16 */
    ubjw = ubj_write_memory();
    ubj_write_int16(ubjw, -10240);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT16;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 1;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int16(ubjh) == -10240;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_int32(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT32 */
    ubjw = ubj_write_memory();
    ubj_write_int32(ubjw, -1147483648);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT32;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 1;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int32(ubjh) == -1147483648;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_int64(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT64 */
    ubjw = ubj_write_memory();
    ubj_write_int64(ubjw, 8223372036854775808);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT64;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 1;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int64(ubjh) == 8223372036854775808;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_float32(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 单精度浮点 */
    ubjw = ubj_write_memory();
    ubj_write_float32(ubjw, 3.125f);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FLOAT32;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 1;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_float(ubjh) == 3.125f;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_float64(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 双进度浮点 */
    ubjw = ubj_write_memory();
    ubj_write_float64(ubjw, 3.125125);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FLOAT64;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 1;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_double(ubjh) == 3.125125;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_null(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 */
    ubjw = ubj_write_memory();
    ubj_write_null(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_NULL;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 1;
    ok &= ubj_is_noop(ubjh) == 0;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_true(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 真 */
    ubjw = ubj_write_memory();
    ubj_write_true(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_TRUE;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 1;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_bool(ubjh) != 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_false(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 假 */
    ubjw = ubj_write_memory();
    ubj_write_false(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FALSE;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 1;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_bool(ubjh) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_noop(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 */
    ubjw = ubj_write_memory();
    ubj_write_noop(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_NOOP;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 1;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_object(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 object */
    ubjw = ubj_write_memory();
    ubj_begin_object(ubjw);
    ubj_end_object(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_OBJECT;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 1;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_array(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 array */
    ubjw = ubj_write_memory();
    ubj_begin_array(ubjw);
    ubj_end_array(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_ARRAY;
    ok &= ubj_array_length(ubjh) == 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 1;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_empty_buffer(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 buffer */
    ubjw = ubj_write_memory();
    ubj_write_buffer(ubjw, NULL, UBJ_TYPE_UINT8, 0);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_UINT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) == 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 1;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_general_buffer(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 buffer */
    ubjw = ubj_write_memory();
    ubj_write_buffer(ubjw, (uint8_t *)"0123456789", UBJ_TYPE_CHAR, sizeof("0123456789") - 1);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_CHAR;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) == 10;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 1;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_buffer(ubjh).count == 10;
    ok &= memcmp(ubj_get_buffer(ubjh).ptr, "0123456789", 10) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_object(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 对象 */
    ubjw = ubj_write_memory();
    ubj_begin_object(ubjw);
    /* 字符串测试 */
    ubj_object_write_string(ubjw, "1", "this is string");
    /* 字符测试 */
    ubj_object_write_char(ubjw, "2", '<');
    /* UIN8 测试 */
    ubj_object_write_uint8(ubjw, "3", 249);
    /* INT8 测试 */
    ubj_object_write_int8(ubjw, "4", -98);
    /* INT16 测试 */
    ubj_object_write_int16(ubjw, "5", -10240);
    /* INT32 测试 */
    ubj_object_write_int32(ubjw, "6", -1147483648);
    /* INT64 测试 */
    ubj_object_write_int64(ubjw, "7", 8223372036854775808);
    /* 单精度浮点测试 */
    ubj_object_write_float32(ubjw, "8", 3.125f);
    /* 双进度浮点 */
    ubj_object_write_float64(ubjw, "9", 3.125);
    /* 空测试 */
    ubj_object_write_null(ubjw, "10");
    /* bool 测试 */
    ubj_object_write_true(ubjw, "11");
    ubj_object_write_false(ubjw, "12");
    /* buffer 测试 */
    ubj_object_write_buffer(ubjw, "13", "0123456789", UBJ_TYPE_CHAR, sizeof("0123456789") - 1);
    /* 空 array 测试 */
    ubj_object_write_array(ubjw, "14");
    ubj_end_array(ubjw);
    /* array 测试 */
    ubj_object_write_array(ubjw, "15");
    ubj_write_char(ubjw, '*');
    ubj_write_char(ubjw, '*');
    ubj_end_array(ubjw);
    /* 空 object 测试 */
    ubj_object_write_object(ubjw, "16");
    ubj_end_object(ubjw);
    /* object 测试 */
    ubj_object_write_object(ubjw, "17");
    ubj_object_write_uint8(ubjw, "1", 249);
    ubj_object_write_uint8(ubjw, "2", 250);
    ubj_end_object(ubjw);

    ubj_end_object(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);

    /* 验证 */
    ok &= ubj_is_object(ubjh);
    ok &= ubj_is_string(ubj_object_item(ubjh, "1"));
    ok &= strcmp(ubj_get_string(ubj_object_item(ubjh, "1")).ptr, "this is string") == 0;
    ok &= ubj_is_char(ubj_object_item(ubjh, "2"));
    ok &= ubj_get_char(ubj_object_item(ubjh, "2")) == '<';
    ok &= ubj_is_uint8(ubj_object_item(ubjh, "3"));
    ok &= ubj_get_uint8(ubj_object_item(ubjh, "3")) == 249;
    ok &= ubj_is_int8(ubj_object_item(ubjh, "4"));
    ok &= ubj_get_int8(ubj_object_item(ubjh, "4")) == -98;
    ok &= ubj_is_int16(ubj_object_item(ubjh, "5"));
    ok &= ubj_get_int16(ubj_object_item(ubjh, "5")) == -10240;
    ok &= ubj_is_int32(ubj_object_item(ubjh, "6"));
    ok &= ubj_get_int32(ubj_object_item(ubjh, "6")) == -1147483648;
    ok &= ubj_is_int64(ubj_object_item(ubjh, "7"));
    ok &= ubj_get_int64(ubj_object_item(ubjh, "7")) == 8223372036854775808;
    ok &= ubj_is_float(ubj_object_item(ubjh, "8"));
    ok &= ubj_get_float(ubj_object_item(ubjh, "8")) == 3.125f;
    ok &= ubj_is_double(ubj_object_item(ubjh, "9"));
    ok &= ubj_get_double(ubj_object_item(ubjh, "9")) == 3.125;
    ok &= ubj_is_null(ubj_object_item(ubjh, "10"));
    ok &= ubj_is_true(ubj_object_item(ubjh, "11"));
    ok &= ubj_is_false(ubj_object_item(ubjh, "12"));
    ok &= ubj_is_buffer(ubj_object_item(ubjh, "13"));
    ok &= ubj_get_buffer(ubj_object_item(ubjh, "13")).count == 10;
    ok &= memcmp(ubj_get_buffer(ubj_object_item(ubjh, "13")).ptr, "0123456789", 10) == 0;
    ok &= ubj_is_array(ubj_object_item(ubjh, "14"));
    ok &= ubj_array_length(ubj_object_item(ubjh, "14")) == 0;
    ok &= ubj_is_array(ubj_object_item(ubjh, "15"));
    ok &= ubj_array_length(ubj_object_item(ubjh, "15")) == 2;
    ok &= ubj_is_char(ubj_array_item(ubj_object_item(ubjh, "15"), 0));
    ok &= ubj_get_char(ubj_array_item(ubj_object_item(ubjh, "15"), 0)) == '*';
    ok &= ubj_is_char(ubj_array_item(ubj_object_item(ubjh, "15"), 1));
    ok &= ubj_get_char(ubj_array_item(ubj_object_item(ubjh, "15"), 1)) == '*';
    ok &= ubj_is_object(ubj_object_item(ubjh, "16"));
    ok &= ubj_child_item(ubj_object_item(ubjh, "16")) == NULL;
    ok &= ubj_is_object(ubj_object_item(ubjh, "17"));
    ok &= ubj_is_uint8(ubj_object_item(ubj_object_item(ubjh, "17"), "1"));
    ok &= ubj_get_uint8(ubj_object_item(ubj_object_item(ubjh, "17"), "1")) == 249;
    ok &= ubj_is_uint8(ubj_object_item(ubj_object_item(ubjh, "17"), "2"));
    ok &= ubj_get_uint8(ubj_object_item(ubj_object_item(ubjh, "17"), "2")) == 250;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_helper_array(void)
{
    ubjsonw_t *ubjw;
    const ubjh_t *ubjh;
    int ok = 1;

    /* 构建 对象 */
    ubjw = ubj_write_memory();
    ubj_begin_array(ubjw);
    /* 字符串测试 */
    ubj_write_string(ubjw, "this is string");
    /* 字符测试 */
    ubj_write_char(ubjw, '<');
    /* UIN8 测试 */
    ubj_write_uint8(ubjw, 249);
    /* INT8 测试 */
    ubj_write_int8(ubjw, -98);
    /* INT16 测试 */
    ubj_write_int16(ubjw, -10240);
    /* INT32 测试 */
    ubj_write_int32(ubjw, -1147483648);
    /* INT64 测试 */
    ubj_write_int64(ubjw, 8223372036854775808);
    /* 单精度浮点测试 */
    ubj_write_float32(ubjw, 3.125f);
    /* 双进度浮点 */
    ubj_write_float64(ubjw, 3.125);
    /* 空测试 */
    ubj_write_null(ubjw);
    /* bool 测试 */
    ubj_write_true(ubjw);
    ubj_write_false(ubjw);
    /* buffer 测试 */
    ubj_write_buffer(ubjw, "0123456789", UBJ_TYPE_CHAR, sizeof("0123456789") - 1);
    /* 空 object 测试 */
    ubj_begin_object(ubjw);
    ubj_end_object(ubjw);
    /* object 测试 */
    ubj_begin_object(ubjw);
    ubj_object_write_uint8(ubjw, "1", 249);
    ubj_object_write_uint8(ubjw, "2", 250);
    ubj_end_object(ubjw);
    /* 空 array 测试 */
    ubj_begin_array(ubjw);
    ubj_end_array(ubjw);
    /* array 测试 */
    ubj_begin_array(ubjw);
    ubj_write_char(ubjw, '*');
    ubj_write_char(ubjw, '*');
    ubj_end_array(ubjw);

    ubj_end_array(ubjw);
    /* 解析 */
    ubjh = ubj_parse(ubj_get_memory(ubjw), ubjw->total);
    ubj_write_end(ubjw);

    /* 验证 */
    ok &= ubj_is_array(ubjh);
    ok &= ubj_is_string(ubj_array_item(ubjh, 0));
    ok &= strcmp(ubj_get_string(ubj_array_item(ubjh, 0)).ptr, "this is string") == 0;
    ok &= ubj_is_char(ubj_array_item(ubjh, 1));
    ok &= ubj_get_char(ubj_array_item(ubjh, 1)) == '<';
    ok &= ubj_is_uint8(ubj_array_item(ubjh, 2));
    ok &= ubj_get_uint8(ubj_array_item(ubjh, 2)) == 249;
    ok &= ubj_is_int8(ubj_array_item(ubjh, 3));
    ok &= ubj_get_int8(ubj_array_item(ubjh, 3)) == -98;
    ok &= ubj_is_int16(ubj_array_item(ubjh, 4));
    ok &= ubj_get_int16(ubj_array_item(ubjh, 4)) == -10240;
    ok &= ubj_is_int32(ubj_array_item(ubjh, 5));
    ok &= ubj_get_int32(ubj_array_item(ubjh, 5)) == -1147483648;
    ok &= ubj_is_int64(ubj_array_item(ubjh, 6));
    ok &= ubj_get_int64(ubj_array_item(ubjh, 6)) == 8223372036854775808;
    ok &= ubj_is_float(ubj_array_item(ubjh, 7));
    ok &= ubj_get_float(ubj_array_item(ubjh, 7)) == 3.125f;
    ok &= ubj_is_double(ubj_array_item(ubjh, 8));
    ok &= ubj_get_double(ubj_array_item(ubjh, 8)) == 3.125;
    ok &= ubj_is_null(ubj_array_item(ubjh, 9));
    ok &= ubj_is_true(ubj_array_item(ubjh, 10));
    ok &= ubj_is_false(ubj_array_item(ubjh, 11));
    ok &= ubj_is_buffer(ubj_array_item(ubjh, 12));
    ok &= ubj_get_buffer(ubj_array_item(ubjh, 12)).count == 10;
    ok &= memcmp(ubj_get_buffer(ubj_array_item(ubjh, 12)).ptr, "0123456789", 10) == 0;
    ok &= ubj_is_object(ubj_array_item(ubjh, 13));
    ok &= ubj_child_item(ubj_array_item(ubjh, 13)) == NULL;
    ok &= ubj_is_object(ubj_array_item(ubjh, 14));
    ok &= ubj_is_uint8(ubj_object_item(ubj_array_item(ubjh, 14), "1"));
    ok &= ubj_get_uint8(ubj_object_item(ubj_array_item(ubjh, 14), "1")) == 249;
    ok &= ubj_is_uint8(ubj_object_item(ubj_array_item(ubjh, 14), "2"));
    ok &= ubj_get_uint8(ubj_object_item(ubj_array_item(ubjh, 14), "2")) == 250;
    ok &= ubj_is_array(ubj_array_item(ubjh, 15));
    ok &= ubj_array_length(ubj_array_item(ubjh, 15)) == 0;
    ok &= ubj_is_array(ubj_array_item(ubjh, 16));
    ok &= ubj_array_length(ubj_array_item(ubjh, 16)) == 2;
    ok &= ubj_is_char(ubj_array_item(ubj_array_item(ubjh, 16), 0));
    ok &= ubj_get_char(ubj_array_item(ubj_array_item(ubjh, 16), 0)) == '*';
    ok &= ubj_is_char(ubj_array_item(ubj_array_item(ubjh, 16), 1));
    ok &= ubj_get_char(ubj_array_item(ubj_array_item(ubjh, 16), 1)) == '*';

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_helper_test(void)
{
    ubjson_helper_general_string();
    ubjson_helper_general_char();
    ubjson_helper_general_uint8();
    ubjson_helper_general_int8();
    ubjson_helper_general_int16();
    ubjson_helper_general_int32();
    ubjson_helper_general_int64();
    ubjson_helper_general_float32();
    ubjson_helper_general_float64();
    ubjson_helper_general_null();
    ubjson_helper_general_true();
    ubjson_helper_general_false();
    ubjson_helper_general_noop();
    ubjson_helper_general_object();
    ubjson_helper_general_array();
    ubjson_helper_general_empty_buffer();
    ubjson_helper_general_buffer();
}

static void ubjson_helper_container_test(void)
{
    ubjson_helper_object();
    ubjson_helper_array();
}


/*
UBJ_STATIC_MEM_NULL        
UBJ_STATIC_MEM_NOOP        
UBJ_STATIC_MEM_TRUE        
UBJ_STATIC_MEM_FALSE       
UBJ_STATIC_MEM_CHAR        
UBJ_STATIC_MEM_INT8        
UBJ_STATIC_MEM_UINT8       
UBJ_STATIC_MEM_INT16       
UBJ_STATIC_MEM_INT32       
UBJ_STATIC_MEM_INT64       
UBJ_STATIC_MEM_INTEGER(_i) 
                           
UBJ_STATIC_MEM_FLOAT32     
UBJ_STATIC_MEM_FLOAT64     
UBJ_STATIC_MEM_STRING(_l)  
UBJ_STATIC_MEM_BUFFER(_l)  
*/

static void ubj_make_null_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_NULL];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 */
    ubj_make_null(buf);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_NULL;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 1;
    ok &= ubj_is_noop(ubjh) == 0;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_noop_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_NOOP];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 空 */
    ubj_make_noop(buf);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_NOOP;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 1;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_char_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_CHAR];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建字符 */
    ubj_make_char(buf, '<');
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_CHAR;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 1;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_char(ubjh) == '<';
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_true_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_BOOL];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 真 */
    ubj_make_true(buf);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_TRUE;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 1;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_bool(ubjh) != 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_false_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_BOOL];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 假 */
    ubj_make_false(buf);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FALSE;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 1;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_bool(ubjh) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_uint8_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_UINT8];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 UINT8 */
    ubj_make_uint8(buf, 249);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_UINT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 1;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_uint8(ubjh) == 249;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_int8_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT8];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT8 */
    ubj_make_int8(buf, -98);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 1;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int8(ubjh) == -98;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_int16_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT16];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT16 */
    ubj_make_int16(buf, -10240);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT16;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 1;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int16(ubjh) == -10240;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_int32_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT32];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT32 */
    ubj_make_int32(buf, -1147483648);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT32;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 1;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int32(ubjh) == -1147483648;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_float_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_FLOAT32];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 单精度浮点 */
    ubj_make_float(buf, 3.125f);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FLOAT32;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 1;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_float(ubjh) == 3.125f;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_double_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_FLOAT64];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 双进度浮点 */
    ubj_make_double(buf, 3.125125);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FLOAT64;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 1;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_double(ubjh) == 3.125125;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_int64_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT64];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 INT64 */
    ubj_make_int64(buf, 8223372036854775808);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT64;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 1;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int64(ubjh) == 8223372036854775808;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_string_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_STRING(sizeof("this is string") - 1)];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建字符串 */
    ubj_make_string(buf, "this is string", sizeof("this is string") - 1);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_STRING;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 1;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= strcmp(ubj_get_string(ubjh).ptr, "this is string") == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_make_buffer_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_BUFFER(sizeof("0123456789") - 1)];
    ubjh_t *ubjh;
    int ok = 1;

    /* 构建 buffer */
    ubj_make_buffer(buf, (uint8_t *)"0123456789", sizeof("0123456789") - 1);
    /* 解析 */
    ubjh = ubj_parse(buf, sizeof(buf));
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_UINT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) == 10;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 1;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_buffer(ubjh).count == 10;
    ok &= memcmp(ubj_get_buffer(ubjh).ptr, "0123456789", 10) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubjson_make_test(void)
{
    ubj_make_null_test();
    ubj_make_noop_test();
    ubj_make_char_test();
    ubj_make_true_test();
    ubj_make_false_test();
    ubj_make_uint8_test();
    ubj_make_int8_test();
    ubj_make_int16_test();
    ubj_make_int32_test();
    ubj_make_float_test();
    ubj_make_double_test();
    ubj_make_int64_test();
    ubj_make_string_test();
    ubj_make_buffer_test();
}

static ubj_write_buffer_01(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x5A, 0x23, 0x55, 0x02, 0x5B, 0x24, 0x4E, 0x23, 0x55, 0x02, 0x5B, 0x24, 0x54, \
                         0x23, 0x55, 0x02, 0x5B, 0x24, 0x46, 0x23, 0x55, 0x02, 0x5D};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* 空类型测试 */
    ubj_write_buffer(ubj, RT_NULL, UBJ_TYPE_NULL, 2);   // NULL
    ubj_write_buffer(ubj, RT_NULL, UBJ_TYPE_NOOP, 2);   // NOOP
    ubj_write_buffer(ubj, RT_NULL, UBJ_TYPE_TRUE, 2);   // TRUE
    ubj_write_buffer(ubj, RT_NULL, UBJ_TYPE_FALSE, 2);  // FALSE
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_02(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x43, 0x23, 0x55, 0x02, 0x31, 0x32, 0x5D};
    char buf[2] = {'1', '2'};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* 单字节类型测试 */
    ubj_write_buffer(ubj, buf, UBJ_TYPE_CHAR, 2);    // CHAR
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_03(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x69, 0x23, 0x55, 0x02, 0xFF, 0xFE, 0x5D};
    int8_t buf[2] = {-1, -2};
    //uint8_t buf[2] = {1, 2};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* 单字节类型测试 */
    ubj_write_buffer(ubj, buf, UBJ_TYPE_INT8, 2);   // INT8
    //ubj_write_buffer(ubj, RT_NULL, UBJ_TYPE_UINT8, 2);   // UINT8
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_04(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x55, 0x23, 0x55, 0x02, 0x01, 0x02, 0x5D};
    uint8_t buf[2] = {1, 2};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* 单字节类型测试 */
    ubj_write_buffer(ubj, buf, UBJ_TYPE_UINT8, 2);   // UINT8
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_05(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x53, 0x23, 0x55, 0x02, 0x55, 0x03, 0x31, 0x32, 0x33, 0x55, 0x03, 0x34, 0x35, \
                         0x36, 0x5B, 0x24, 0x48, 0x23, 0x55, 0x02, 0x55, 0x03, 0x31, 0x32, 0x33, 0x55, 0x03, 0x34, 0x35, \
                         0x36, 0x5D};
    char *buf[2] = {"123", "456"};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* 字符串类型测试 */
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_STRING, 2);   // STRING
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_HIGH_PRECISION, 2);   // PRECISION(STRING)
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_06(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x6C, 0x23, 0x55, 0x02, 0x80, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x5D};
    int32_t buf[2] = {INT32_MIN, INT32_MAX};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* int32类型测试 */
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_INT32, 2);   // INT32
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_07(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x4C, 0x23, 0x55, 0x02, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, \
                         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5D};
    int64_t buf[2] = {INT64_MIN, INT64_MAX};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* int64类型测试 */
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_INT64, 2);   // INT64
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_08(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x49, 0x23, 0x55, 0x02, 0x80, 0x00, 0x7F, 0xFF, 0x5D};
    int16_t buf[2] = {INT16_MIN, INT16_MAX};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* int16类型测试 */
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_INT16, 2);   // INT16
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_09(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x64, 0x23, 0x55, 0x02, 0x40, 0x48, 0x00, 0x00, 0x40, 0x48, 0x00, 0x00, 0x5D};
    float buf[2] = {3.125, 3.125};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* float类型测试 */
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_FLOAT32, 2);   // FLOAT
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

static ubj_write_buffer_10(void)
{
    uint8_t cmp_buf[] = {0x5B, 0x5B, 0x24, 0x44, 0x23, 0x55, 0x02, 0x40, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, \
                         0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D};
    double buf[2] = {3.125, 3.125};
    int ok = 1;

    ubjsonw_t *ubj = ubj_write_memory();
    ubj_begin_array(ubj);
    /* double 类型测试 */
    ubj_write_buffer(ubj, (const uint8_t *)buf, UBJ_TYPE_FLOAT64, 2);   // DOUBLE
    ubj_end_array(ubj);
    // 打印
    //__ubj_dump_mem(ubj);
    // 校验
    ok &= ubj->total == sizeof(cmp_buf);
    ok &= memcmp(ubj_get_memory(ubj), cmp_buf, ubj->total) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    // 释放内存
    ubj_write_end(ubj);
}

void ubj_write_buffer_test(void)
{
    ubj_write_buffer_01();
    ubj_write_buffer_02();
    ubj_write_buffer_03();
    ubj_write_buffer_04();
    ubj_write_buffer_05();
    ubj_write_buffer_06();
    ubj_write_buffer_07();
    ubj_write_buffer_08();
    ubj_write_buffer_09();
    ubj_write_buffer_10();
}

void ubj_create_test_00(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_null();
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_NULL;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 1;
    ok &= ubj_is_noop(ubjh) == 0;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_01(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_noop();
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_NOOP;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 1;

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_02(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_char('<');
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_CHAR;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 1;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_char(ubjh) == '<';
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_03(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_true();
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_TRUE;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 1;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_bool(ubjh) != 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_04(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_false();
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FALSE;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 1;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_bool(ubjh) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_05(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_uint8(249);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_UINT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 1;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_uint8(ubjh) == 249;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_06(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_int8(-98);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 1;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int8(ubjh) == -98;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_07(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_int16(-10240);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT16;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 1;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int16(ubjh) == -10240;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_08(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_int32(-1147483648);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT32;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 1;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int32(ubjh) == -1147483648;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_09(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_float32(3.125f);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FLOAT32;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 1;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_float(ubjh) == 3.125f;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_10(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_float64(3.125125);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_FLOAT64;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 1;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_double(ubjh) == 3.125125;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_11(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_int64(8223372036854775808);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_INT64;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 1;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_int64(ubjh) == 8223372036854775808;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_12(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_string("this is string");
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_STRING;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) < 0;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 1;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 0;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= strcmp(ubj_get_string(ubjh).ptr, "this is string") == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_13(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_buffer((uint8_t *)"0123456789", UBJ_TYPE_UINT8, sizeof("0123456789") - 1);
    /* 测试 */
    ok &= ubj_item_type(ubjh) == UBJ_TYPE_UINT8;
    ok &= ubj_array_length(ubjh) < 0;
    ok &= ubj_buffer_length(ubjh) == 10;
    ok &= ubj_array_item(ubjh, 0) == NULL;
    ok &= ubj_object_item(ubjh, "") == NULL;
    ok &= ubj_next_item(ubjh) == NULL;
    ok &= ubj_child_item(ubjh) == NULL;
    ok &= ubj_is_char(ubjh) == 0;
    ok &= ubj_is_bool(ubjh) == 0;
    ok &= ubj_is_uint8(ubjh) == 0;
    ok &= ubj_is_int8(ubjh) == 0;
    ok &= ubj_is_int16(ubjh) == 0;
    ok &= ubj_is_int32(ubjh) == 0;
    ok &= ubj_is_float(ubjh) == 0;
    ok &= ubj_is_double(ubjh) == 0;
    ok &= ubj_is_int64(ubjh) == 0;
    ok &= ubj_is_string(ubjh) == 0;
    ok &= ubj_is_hpstring(ubjh) == 0;
    ok &= ubj_is_buffer(ubjh) == 1;
    ok &= ubj_is_object(ubjh) == 0;
    ok &= ubj_is_array(ubjh) == 0;
    ok &= ubj_is_null(ubjh) == 0;
    ok &= ubj_is_noop(ubjh) == 0;
    /* 值测试 */
    ok &= ubj_get_buffer(ubjh).count == 10;
    ok &= memcmp(ubj_get_buffer(ubjh).ptr, "0123456789", 10) == 0;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_14(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_array();
    ubj_add_item_to_array(ubjh, ubj_create_null(), 0);
    ubj_add_item_to_array(ubjh, ubj_create_noop(), 1);
    ubj_add_item_to_array(ubjh, ubj_create_char('<'), 2);
    ubj_add_item_to_array(ubjh, ubj_create_true(), 3);
    ubj_add_item_to_array(ubjh, ubj_create_false(), 4);
    ubj_add_item_to_array(ubjh, ubj_create_uint8(249), 5);
    ubj_add_item_to_array(ubjh, ubj_create_int8(-98), 6);
    ubj_add_item_to_array(ubjh, ubj_create_int16(-10240), 7);
    ubj_add_item_to_array(ubjh, ubj_create_int32(-1147483648), 8);
    ubj_add_item_to_array(ubjh, ubj_create_float32(3.125f), 9);
    ubj_add_item_to_array(ubjh, ubj_create_float64(3.125125), 10);
    ubj_add_item_to_array(ubjh, ubj_create_int64(8223372036854775808), 11);
    ubj_add_item_to_array(ubjh, ubj_create_string("this is string"), -1);

    ok &= ubj_is_null(ubj_array_item(ubjh, 0));
    ok &= ubj_is_noop(ubj_array_item(ubjh, 1));
    ok &= ubj_is_char(ubj_array_item(ubjh, 2));
    ok &= ubj_get_char(ubj_array_item(ubjh, 2)) == '<';
    ok &= ubj_is_true(ubj_array_item(ubjh, 3));
    ok &= ubj_is_false(ubj_array_item(ubjh, 4));
    ok &= ubj_is_uint8(ubj_array_item(ubjh, 5));
    ok &= ubj_get_uint8(ubj_array_item(ubjh, 5)) == 249;
    ok &= ubj_is_int8(ubj_array_item(ubjh, 6));
    ok &= ubj_get_int8(ubj_array_item(ubjh, 6)) == -98;
    ok &= ubj_is_int16(ubj_array_item(ubjh, 7));
    ok &= ubj_get_int16(ubj_array_item(ubjh, 7)) == -10240;
    ok &= ubj_is_int32(ubj_array_item(ubjh, 8));
    ok &= ubj_get_int32(ubj_array_item(ubjh, 8)) == -1147483648;
    ok &= ubj_is_float(ubj_array_item(ubjh, 9));
    ok &= ubj_get_float(ubj_array_item(ubjh, 9)) == 3.125f;
    ok &= ubj_is_double(ubj_array_item(ubjh, 10));
    ok &= ubj_get_double(ubj_array_item(ubjh, 10)) == 3.125125;
    ok &= ubj_is_int64(ubj_array_item(ubjh, 11));
    ok &= ubj_get_int64(ubj_array_item(ubjh, 11)) == 8223372036854775808;
    ok &= ubj_is_string(ubj_array_item(ubjh, 12));

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_15(void)
{
    ubjh_t *ubjh;
    int ok = 1;

    /* 解析 */
    ubjh = ubj_create_object();
    ubj_add_item_to_object(ubjh, "0", ubj_create_null());
    ubj_add_item_to_object(ubjh, "1", ubj_create_noop());
    ubj_add_item_to_object(ubjh, "2", ubj_create_char('<'));
    ubj_add_item_to_object(ubjh, "3", ubj_create_true());
    ubj_add_item_to_object(ubjh, "4", ubj_create_false());
    ubj_add_item_to_object(ubjh, "5", ubj_create_uint8(249));
    ubj_add_item_to_object(ubjh, "6", ubj_create_int8(-98));
    ubj_add_item_to_object(ubjh, "7", ubj_create_int16(-10240));
    ubj_add_item_to_object(ubjh, "8", ubj_create_int32(-1147483648));
    ubj_add_item_to_object(ubjh, "9", ubj_create_float32(3.125f));
    ubj_add_item_to_object(ubjh, "10", ubj_create_float64(3.125125));
    ubj_add_item_to_object(ubjh, "11", ubj_create_int64(8223372036854775808));
    ubj_add_item_to_object(ubjh, "-1", ubj_create_string("this is string"));

    ok &= ubj_is_null(ubj_object_item(ubjh, "0"));
    ok &= ubj_is_noop(ubj_object_item(ubjh, "1"));
    ok &= ubj_is_char(ubj_object_item(ubjh, "2"));
    ok &= ubj_get_char(ubj_object_item(ubjh, "2")) == '<';
    ok &= ubj_is_true(ubj_object_item(ubjh, "3"));
    ok &= ubj_is_false(ubj_object_item(ubjh, "4"));
    ok &= ubj_is_uint8(ubj_object_item(ubjh, "5"));
    ok &= ubj_get_uint8(ubj_object_item(ubjh, "5")) == 249;
    ok &= ubj_is_int8(ubj_object_item(ubjh, "6"));
    ok &= ubj_get_int8(ubj_object_item(ubjh, "6")) == -98;
    ok &= ubj_is_int16(ubj_object_item(ubjh, "7"));
    ok &= ubj_get_int16(ubj_object_item(ubjh, "7")) == -10240;
    ok &= ubj_is_int32(ubj_object_item(ubjh, "8"));
    ok &= ubj_get_int32(ubj_object_item(ubjh, "8")) == -1147483648;
    ok &= ubj_is_float(ubj_object_item(ubjh, "9"));
    ok &= ubj_get_float(ubj_object_item(ubjh, "9")) == 3.125f;
    ok &= ubj_is_double(ubj_object_item(ubjh, "10"));
    ok &= ubj_get_double(ubj_object_item(ubjh, "10")) == 3.125125;
    ok &= ubj_is_int64(ubj_object_item(ubjh, "11"));
    ok &= ubj_get_int64(ubj_object_item(ubjh, "11")) == 8223372036854775808;
    ok &= ubj_is_string(ubj_object_item(ubjh, "-1"));

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubjh);
}

static void ubj_create_test_16(void)
{
    ubjh_t *ubj_array, *ubj_object;
    int ok = 1;

    /* 解析 */
    ubj_array = ubj_create_array();
    ubj_add_item_to_array(ubj_array, ubj_create_null(), -1);
    ubj_add_item_to_array(ubj_array, ubj_create_char('<'), -1);
    ubj_add_item_to_array(ubj_array, ubj_deepcopy(ubj_array), -1);

    ubj_object = ubj_create_object();
    ubj_add_item_to_object(ubj_object, "0", ubj_create_null());
    ubj_add_item_to_object(ubj_object, "-1", ubj_create_char('<'));
    ubj_add_item_to_object(ubj_object, "-2", ubj_deepcopy(ubj_object));

    ubj_add_item_to_array(ubj_array, ubj_deepcopy(ubj_object), -1);
    ubj_add_item_to_object(ubj_object, "-3", ubj_deepcopy(ubj_array));

    ok &= ubj_is_null(ubj_array_item(ubj_array, 0));
    ok &= ubj_is_char(ubj_array_item(ubj_array, 1));
    ok &= ubj_get_char(ubj_array_item(ubj_array, 1)) == '<';
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 2);
        ok &= ubj_is_array(tmp);
        ok &= ubj_array_length(tmp) == 2;
        ok &= ubj_is_null(ubj_array_item(tmp, 0));
        ok &= ubj_is_char(ubj_array_item(tmp, 1));
        ok &= ubj_get_char(ubj_array_item(tmp, 1)) == '<';
    }
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 3);
        ok &= ubj_is_object(tmp);
        ok &= ubj_is_null(ubj_object_item(tmp, "0"));
        ok &= ubj_is_char(ubj_object_item(tmp, "-1"));
        ok &= ubj_get_char(ubj_object_item(tmp, "-1")) == '<';
        ok &= ubj_is_object(ubj_object_item(tmp, "-2"));
        {
            ubjh_t *tmp_ = ubj_object_item(tmp, "-2");
            ok &= ubj_is_null(ubj_object_item(tmp_, "0"));
            ok &= ubj_is_char(ubj_object_item(tmp_, "-1"));
        }
    }
    ok &= ubj_is_null(ubj_object_item(ubj_object, "0"));
    ok &= ubj_is_char(ubj_object_item(ubj_object, "-1"));
    ok &= ubj_get_char(ubj_object_item(ubj_object, "-1")) == '<';
    ok &= ubj_is_object(ubj_object_item(ubj_object, "-2"));
    {
        ubjh_t *tmp = ubj_object_item(ubj_object, "-2");
        ok &= ubj_is_null(ubj_object_item(tmp, "0"));
        ok &= ubj_is_char(ubj_object_item(tmp, "-1"));
        ok &= ubj_get_char(ubj_object_item(tmp, "-1")) == '<';
    }
    ok &= ubj_is_array(ubj_object_item(ubj_object, "-3"));
    {
        ubjh_t *tmp = ubj_object_item(ubj_object, "-3");
        ok &= ubj_is_null(ubj_array_item(tmp, 0));
        ok &= ubj_is_char(ubj_array_item(tmp, 1));
        ok &= ubj_get_char(ubj_array_item(tmp, 1)) == '<';
        {
            ubjh_t *tmp_ = ubj_array_item(tmp, 2);
            ok &= ubj_is_array(tmp_);
            ok &= ubj_array_length(tmp_) == 2;
            ok &= ubj_is_null(ubj_array_item(tmp_, 0));
            ok &= ubj_is_char(ubj_array_item(tmp_, 1));
            ok &= ubj_get_char(ubj_array_item(tmp_, 1)) == '<';
        }
        {
            ubjh_t *tmp__ = ubj_array_item(tmp, 3);
            ok &= ubj_is_object(tmp__);
            ok &= ubj_is_null(ubj_object_item(tmp__, "0"));
            ok &= ubj_is_char(ubj_object_item(tmp__, "-1"));
            ok &= ubj_get_char(ubj_object_item(tmp__, "-1")) == '<';
            ok &= ubj_is_object(ubj_object_item(tmp__, "-2"));
            {
                ubjh_t *tmp___ = ubj_object_item(tmp__, "-2");
                ok &= ubj_is_null(ubj_object_item(tmp___, "0"));
                ok &= ubj_is_char(ubj_object_item(tmp___, "-1"));
            }
        }
    }
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubj_array);
    ubj_parse_free((ubjh_t *)ubj_object);
}

void ubj_create_test(void)
{
    ubj_create_test_00();
    ubj_create_test_01();
    ubj_create_test_02();
    ubj_create_test_03();
    ubj_create_test_04();
    ubj_create_test_05();
    ubj_create_test_06();
    ubj_create_test_07();
    ubj_create_test_08();
    ubj_create_test_09();
    ubj_create_test_10();
    ubj_create_test_11();
    ubj_create_test_12();
    ubj_create_test_13();
    ubj_create_test_14();
    ubj_create_test_15();
    ubj_create_test_16();
}

static void ubj_other_test_00(void)
{
    ubjh_t *ubj_array, *ubj_object;
    int ok = 1;
    ubjsonw_t *ubj_array_write, *ubj_object_write;

    /* 解析 */
    ubj_array = ubj_create_array();
    ubj_add_item_to_array(ubj_array, ubj_create_null(), -1);
    ubj_add_item_to_array(ubj_array, ubj_create_char('<'), -1);
    ubj_add_item_to_array(ubj_array, ubj_deepcopy(ubj_array), -1);

    ubj_object = ubj_create_object();
    ubj_add_item_to_object(ubj_object, "0", ubj_create_null());
    ubj_add_item_to_object(ubj_object, "-1", ubj_create_char('<'));
    ubj_add_item_to_object(ubj_object, "-2", ubj_deepcopy(ubj_object));

    ubj_add_item_to_array(ubj_array, ubj_deepcopy(ubj_object), -1);
    ubj_add_item_to_object(ubj_object, "-3", ubj_deepcopy(ubj_array));

    /* 序列化 */
    ubj_array_write = ubj_write_memory();
    ubj_dumpb(ubj_array, ubj_array_write);
    ubj_parse_free((ubjh_t *)ubj_array);

    /* 转换成对象并测试 */
    ubj_array = ubj_parse(ubj_get_memory(ubj_array_write), ubj_array_write->total);
    ubj_write_end(ubj_array_write);
    ok &= ubj_is_null(ubj_array_item(ubj_array, 0));
    ok &= ubj_is_char(ubj_array_item(ubj_array, 1));
    ok &= ubj_get_char(ubj_array_item(ubj_array, 1)) == '<';
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 2);
        ok &= ubj_is_array(tmp);
        ok &= ubj_array_length(tmp) == 2;
        ok &= ubj_is_null(ubj_array_item(tmp, 0));
        ok &= ubj_is_char(ubj_array_item(tmp, 1));
        ok &= ubj_get_char(ubj_array_item(tmp, 1)) == '<';
    }
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 3);
        ok &= ubj_is_object(tmp);
        ok &= ubj_is_null(ubj_object_item(tmp, "0"));
        ok &= ubj_is_char(ubj_object_item(tmp, "-1"));
        ok &= ubj_get_char(ubj_object_item(tmp, "-1")) == '<';
        ok &= ubj_is_object(ubj_object_item(tmp, "-2"));
        {
            ubjh_t *tmp_ = ubj_object_item(tmp, "-2");
            ok &= ubj_is_null(ubj_object_item(tmp_, "0"));
            ok &= ubj_is_char(ubj_object_item(tmp_, "-1"));
        }
    }

    /* 序列化 */
    ubj_object_write = ubj_write_memory();
    ubj_dumpb(ubj_object, ubj_object_write);
    ubj_parse_free((ubjh_t *)ubj_object);
    /* 转换成对象并测试 */
    ubj_object = ubj_parse(ubj_get_memory(ubj_object_write), ubj_object_write->total);
    ubj_write_end(ubj_object_write);
    ok &= ubj_is_null(ubj_object_item(ubj_object, "0"));
    ok &= ubj_is_char(ubj_object_item(ubj_object, "-1"));
    ok &= ubj_get_char(ubj_object_item(ubj_object, "-1")) == '<';
    ok &= ubj_is_object(ubj_object_item(ubj_object, "-2"));
    {
        ubjh_t *tmp = ubj_object_item(ubj_object, "-2");
        ok &= ubj_is_null(ubj_object_item(tmp, "0"));
        ok &= ubj_is_char(ubj_object_item(tmp, "-1"));
        ok &= ubj_get_char(ubj_object_item(tmp, "-1")) == '<';
    }
    ok &= ubj_is_array(ubj_object_item(ubj_object, "-3"));
    {
        ubjh_t *tmp = ubj_object_item(ubj_object, "-3");
        ok &= ubj_is_null(ubj_array_item(tmp, 0));
        ok &= ubj_is_char(ubj_array_item(tmp, 1));
        ok &= ubj_get_char(ubj_array_item(tmp, 1)) == '<';
        {
            ubjh_t *tmp_ = ubj_array_item(tmp, 2);
            ok &= ubj_is_array(tmp_);
            ok &= ubj_array_length(tmp_) == 2;
            ok &= ubj_is_null(ubj_array_item(tmp_, 0));
            ok &= ubj_is_char(ubj_array_item(tmp_, 1));
            ok &= ubj_get_char(ubj_array_item(tmp_, 1)) == '<';
        }
        {
            ubjh_t *tmp__ = ubj_array_item(tmp, 3);
            ok &= ubj_is_object(tmp__);
            ok &= ubj_is_null(ubj_object_item(tmp__, "0"));
            ok &= ubj_is_char(ubj_object_item(tmp__, "-1"));
            ok &= ubj_get_char(ubj_object_item(tmp__, "-1")) == '<';
            ok &= ubj_is_object(ubj_object_item(tmp__, "-2"));
            {
                ubjh_t *tmp___ = ubj_object_item(tmp__, "-2");
                ok &= ubj_is_null(ubj_object_item(tmp___, "0"));
                ok &= ubj_is_char(ubj_object_item(tmp___, "-1"));
            }
        }
    }

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }

    /* 释放内存 */
    ubj_parse_free((ubjh_t *)ubj_array);
    ubj_parse_free((ubjh_t *)ubj_object);
}

static void ubj_other_test(void)
{
    ubj_other_test_00();
}

static void ubj_write_ubjson_test_00(void)
{
    ubjh_t *ubj_array, *ubj_object;
    int ok = 1;
    ubjsonw_t *ubj_array_write, *ubj_object_write;

    /* 解析 */
    ubj_array = ubj_create_array();
    ubj_add_item_to_array(ubj_array, ubj_create_null(), -1);
    ubj_add_item_to_array(ubj_array, ubj_create_char('<'), -1);
    ubj_add_item_to_array(ubj_array, ubj_deepcopy(ubj_array), -1);

    ubj_object = ubj_create_object();
    ubj_add_item_to_object(ubj_object, "0", ubj_create_null());
    ubj_add_item_to_object(ubj_object, "-1", ubj_create_char('<'));
    ubj_add_item_to_object(ubj_object, "-2", ubj_deepcopy(ubj_object));

    ubj_add_item_to_array(ubj_array, ubj_object, -1);

    // 序列化
    ubj_array_write = ubj_write_memory();
    ubj_dumpb(ubj_array, ubj_array_write);
    ubj_parse_free((ubjh_t *)ubj_array);

    // 写入 ubjson
    ubj_object_write = ubj_write_memory();
    ubj_begin_object(ubj_object_write);
    ubj_object_write_ubjson(ubj_object_write, "ubjson", ubj_get_memory(ubj_array_write), ubj_array_write->total);
    ubj_object_write_array(ubj_object_write, "array");
    ubj_write_ubjson(ubj_object_write, ubj_get_memory(ubj_array_write), ubj_array_write->total);
    ubj_end_array(ubj_object_write);
    ubj_end_object(ubj_object_write);

    // 打印
    //__ubj_dump_mem(ubj_object_write);

    /* 转换成对象并测试 */
    ubj_object = ubj_parse(ubj_get_memory(ubj_object_write), ubj_object_write->total);
    ubj_write_end(ubj_array_write);
    ubj_write_end(ubj_object_write);

    ubj_array = ubj_object_item(ubj_object, "ubjson");
    ok &= ubj_is_null(ubj_array_item(ubj_array, 0));
    ok &= ubj_is_char(ubj_array_item(ubj_array, 1));
    ok &= ubj_get_char(ubj_array_item(ubj_array, 1)) == '<';
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 2);
        ok &= ubj_is_array(tmp);
        ok &= ubj_array_length(tmp) == 2;
        ok &= ubj_is_null(ubj_array_item(tmp, 0));
        ok &= ubj_is_char(ubj_array_item(tmp, 1));
        ok &= ubj_get_char(ubj_array_item(tmp, 1)) == '<';
    }
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 3);
        ok &= ubj_is_object(tmp);
        ok &= ubj_is_null(ubj_object_item(tmp, "0"));
        ok &= ubj_is_char(ubj_object_item(tmp, "-1"));
        ok &= ubj_get_char(ubj_object_item(tmp, "-1")) == '<';
        ok &= ubj_is_object(ubj_object_item(tmp, "-2"));
        {
            ubjh_t *tmp_ = ubj_object_item(tmp, "-2");
            ok &= ubj_is_null(ubj_object_item(tmp_, "0"));
            ok &= ubj_is_char(ubj_object_item(tmp_, "-1"));
        }
    }

    ubj_array = ubj_array_item(ubj_object_item(ubj_object, "array"), 0);
    ok &= ubj_is_null(ubj_array_item(ubj_array, 0));
    ok &= ubj_is_char(ubj_array_item(ubj_array, 1));
    ok &= ubj_get_char(ubj_array_item(ubj_array, 1)) == '<';
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 2);
        ok &= ubj_is_array(tmp);
        ok &= ubj_array_length(tmp) == 2;
        ok &= ubj_is_null(ubj_array_item(tmp, 0));
        ok &= ubj_is_char(ubj_array_item(tmp, 1));
        ok &= ubj_get_char(ubj_array_item(tmp, 1)) == '<';
    }
    {
        ubjh_t *tmp = ubj_array_item(ubj_array, 3);
        ok &= ubj_is_object(tmp);
        ok &= ubj_is_null(ubj_object_item(tmp, "0"));
        ok &= ubj_is_char(ubj_object_item(tmp, "-1"));
        ok &= ubj_get_char(ubj_object_item(tmp, "-1")) == '<';
        ok &= ubj_is_object(ubj_object_item(tmp, "-2"));
        {
            ubjh_t *tmp_ = ubj_object_item(tmp, "-2");
            ok &= ubj_is_null(ubj_object_item(tmp_, "0"));
            ok &= ubj_is_char(ubj_object_item(tmp_, "-1"));
        }
    }

    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }

    ubj_parse_free((ubjh_t *)ubj_object);
}

static void ubj_write_ubjson_test(void)
{
    ubj_write_ubjson_test_00();
}



static void ubj_isvalid_test_00(void)
{
    uint8_t cmp_ok_buf0[] = {0x5B, 0x5B, 0x24, 0x5A, 0x23, 0x55, 0x02, 0x5B, 0x24, 0x4E, 0x23, 0x55, 0x02, 0x5B, 0x24, 0x54, \
                             0x23, 0x55, 0x02, 0x5B, 0x24, 0x46, 0x23, 0x55, 0x02, 0x5D};
    uint8_t cmp_ok_buf1[] = {0x5B, 0x5B, 0x24, 0x43, 0x23, 0x55, 0x02, 0x31, 0x32, 0x5D};
    uint8_t cmp_ok_buf2[] = {0x5B, 0x5B, 0x24, 0x69, 0x23, 0x55, 0x02, 0xFF, 0xFE, 0x5D};
    uint8_t cmp_ok_buf3[] = {0x5B, 0x5B, 0x24, 0x55, 0x23, 0x55, 0x02, 0x01, 0x02, 0x5D};
    uint8_t cmp_ok_buf4[] = {0x5B, 0x5B, 0x24, 0x53, 0x23, 0x55, 0x02, 0x55, 0x03, 0x31, 0x32, 0x33, 0x55, 0x03, 0x34, 0x35, \
                             0x36, 0x5B, 0x24, 0x48, 0x23, 0x55, 0x02, 0x55, 0x03, 0x31, 0x32, 0x33, 0x55, 0x03, 0x34, 0x35, \
                             0x36, 0x5D};
    uint8_t cmp_ok_buf5[] = {0x5B, 0x5B, 0x24, 0x6C, 0x23, 0x55, 0x02, 0x80, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x5D};
    uint8_t cmp_ok_buf6[] = {0x5B, 0x5B, 0x24, 0x4C, 0x23, 0x55, 0x02, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, \
                             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5D};
    uint8_t cmp_ok_buf7[] = {0x5B, 0x5B, 0x24, 0x49, 0x23, 0x55, 0x02, 0x80, 0x00, 0x7F, 0xFF, 0x5D};
    uint8_t cmp_ok_buf8[] = {0x5B, 0x5B, 0x24, 0x64, 0x23, 0x55, 0x02, 0x40, 0x48, 0x00, 0x00, 0x40, 0x48, 0x00, 0x00, 0x5D};
    uint8_t cmp_ok_buf9[] = {0x5B, 0x5B, 0x24, 0x44, 0x23, 0x55, 0x02, 0x40, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, \
                             0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D};
    int ok = 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf0, sizeof(cmp_ok_buf0))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf1, sizeof(cmp_ok_buf1))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf2, sizeof(cmp_ok_buf2))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf3, sizeof(cmp_ok_buf3))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf4, sizeof(cmp_ok_buf4))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf5, sizeof(cmp_ok_buf5))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf6, sizeof(cmp_ok_buf6))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf7, sizeof(cmp_ok_buf7))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf8, sizeof(cmp_ok_buf8))) == 1;
    ok &= ubj_isvalid(ubj_read_memory(cmp_ok_buf9, sizeof(cmp_ok_buf9))) == 1;
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubj_isvalid_test(void)
{
    ubj_isvalid_test_00();
}

static void json_to_ubj_test_01(void)
{
    const char *json = "1.79769313486231e+308";  //double max - 0.5
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_02(void)
{
    const char *json = "2.2250738585072014e-308";   //double min
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_03(void)
{
    const char *json = "-2147483648";   //int32 min
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_04(void)
{
    const char *json = "2147483647";   //int32 max
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_05(void)
{
    const char *json = "\"this is string[]{},.?:'/*-+\\n \\\"\\b\\f\\n\\r\\t\"";   //string
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_06(void)
{
    const char *json = "null";   //null
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_07(void)
{
    const char *json = "true";   //true
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_08(void)
{
    const char *json = "false";   //false
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_09(void)
{
    const char *json = "[1,true,\"true\",null]";   //array
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_10(void)
{
    const char *json = "{\"Number\":1,\"Bool\":false,\"String\":\"this is string\",\"Null\":null}";   //object
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_11(void)
{
    const char *json = "{\"Number\":1,\"Bool\":false,\"String\":\"this is string\",\"Null\":null,\"Array\":[1,true,\"true\",null]}";   //object and array
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_12(void)
{
    const char *json = "[1,true,\"true\",null,{\"Number\":1,\"Bool\":false,\"String\":\"this is string\",\"Null\":null}]";   // array and object
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test_13(void)
{
    const char *json = "[{\"Number\":1},{\"Bool\":false},{\"String\":\"this is string\"},{\"Null\":null},{\"Array\":[1,true,\"true\",null]}]";   // array and object
    int ok = 1;
    char *buf = NULL;
    int len = 0;
    ubjsonr_t *ubjr;
    ubjsonw_t *ubjw;

    ok &= (ubjw = ubj_write_memory()) != NULL;
    ok &= json_to_ubj(ubjw, json, strlen(json)) == UBJ_OK;
    ok &= (ubjr = ubj_read_memory(ubj_get_memory(ubjw), ubjw->total)) != NULL;
    ok &= ubj_to_json(ubjr, &buf, &len) == UBJ_OK;
    ubj_write_end(ubjw);
    ubj_read_end(ubjr);
    ok &= strlen(json) == len;
    if (buf)
    {
        ok &= strcmp(buf, json) == 0;
    }
    if (ok)
    {
        rt_kprintf("%s\n", buf);
        ubj_free(buf);
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void json_to_ubj_test(void)
{
    json_to_ubj_test_01();
    json_to_ubj_test_02();
    json_to_ubj_test_03();
    json_to_ubj_test_04();
    json_to_ubj_test_05();
    json_to_ubj_test_06();
    json_to_ubj_test_07();
    json_to_ubj_test_08();
    json_to_ubj_test_09();
    json_to_ubj_test_10();
    json_to_ubj_test_11();
    json_to_ubj_test_12();
    json_to_ubj_test_13();
}

static void ubjson_to_null_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_NULL];

    /* 构建 空 */
    ubj_make_null(buf);
    /* 解析 */
    if (ubjson_to_null(buf, sizeof(buf), -1) == 'Z')
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_noop_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_NOOP];

    /* 构建 NOOP */
    ubj_make_noop(buf);
    /* 解析 */
    if (ubjson_to_noop(buf, sizeof(buf), -1) == 'N')
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_true_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_TRUE];

    /* 构建 真 */
    ubj_make_true(buf);
    /* 解析 */
    if (ubjson_to_true(buf, sizeof(buf), -1) == 1)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_false_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_FALSE];

    /* 构建 假 */
    ubj_make_false(buf);
    /* 解析 */
    if (ubjson_to_false(buf, sizeof(buf), -1) == 0)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_bool_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_BOOL];
    int ok = 1;

    /* 构建 真 */
    ubj_make_bool(buf, 1);
    /* 解析 */
    ok = ok & (ubjson_to_bool(buf, sizeof(buf), -1) == 1);
    /* 构建 假 */
    ubj_make_bool(buf, 0);
    /* 解析 */
    ok = ok & (ubjson_to_bool(buf, sizeof(buf), -1) == 0);
    if (ok)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_char_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_CHAR];

    ubj_make_char(buf, '\\');
    /* 解析 */
    if (ubjson_to_char(buf, sizeof(buf), '/') == '\\')
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_uint8_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_UINT8];

    ubj_make_uint8(buf, 128);
    /* 解析 */
    if (ubjson_to_uint8(buf, sizeof(buf), 0) == 128)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_int8_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT8];

    ubj_make_int8(buf, -126);
    /* 解析 */
    if (ubjson_to_int8(buf, sizeof(buf), 0) == -126)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_int16_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT16];

    ubj_make_int16(buf, -512);
    /* 解析 */
    if (ubjson_to_int16(buf, sizeof(buf), 0) == -512)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_int32_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT32];

    ubj_make_int32(buf, INT_MAX);
    /* 解析 */
    if (ubjson_to_int32(buf, sizeof(buf), 0) == INT_MAX)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_int64_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_INT64];

    ubj_make_int64(buf, LLONG_MAX);
    /* 解析 */
    if (ubjson_to_int64(buf, sizeof(buf), 0) == LLONG_MAX)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_float_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_FLOAT32];

    ubj_make_float(buf, 0.5);
    /* 解析 */
    if (ubjson_to_float(buf, sizeof(buf), 0) == 0.5)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_double_test(void)
{
    uint8_t buf[UBJ_STATIC_MEM_FLOAT64];

    ubj_make_double(buf, 0.5);
    /* 解析 */
    if (ubjson_to_double(buf, sizeof(buf), 0) == 0.5)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_string_test(void)
{
#define UBJSON_TO_STRING_TEST_VAULE "1234"
    uint8_t buf[UBJ_STATIC_MEM_STRING(sizeof(UBJSON_TO_STRING_TEST_VAULE) - 1)];
    char str[sizeof(UBJSON_TO_STRING_TEST_VAULE) - 1];
    size_t out_len = sizeof(str);
    ubj_make_string(buf, UBJSON_TO_STRING_TEST_VAULE, sizeof(UBJSON_TO_STRING_TEST_VAULE) - 1);
    /* 解析 */
    if (ubjson_to_string(buf, sizeof(buf), str, &out_len) != NULL &&
        out_len == sizeof(str) &&
        memcmp(UBJSON_TO_STRING_TEST_VAULE, str, sizeof(str)) == 0)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_buffer_test(void)
{
#define UBJSON_TO_BUFFER_TEST_VALUE "0123456789"
    uint8_t buf[UBJ_STATIC_MEM_BUFFER(sizeof(UBJSON_TO_BUFFER_TEST_VALUE) - 1)];
    uint8_t out_buf[(sizeof(UBJSON_TO_BUFFER_TEST_VALUE) - 1)];
    size_t out_len = sizeof(out_buf);
    int ok = 1;

    /* 构建 buffer */
    ubj_make_buffer(buf, (uint8_t *)"0123456789", sizeof("0123456789") - 1);
    /* 解析 */
    if (ubjson_to_buffer(buf, sizeof(buf), out_buf, &out_len) != NULL &&
        out_len == sizeof(out_buf) &&
        memcmp(UBJSON_TO_BUFFER_TEST_VALUE, out_buf, sizeof(out_buf)) == 0)
    {
        rt_kprintf("F:%s test ok\n", __FUNCTION__);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_json_test(void)
{
#define UBJSON_TO_STRING_TEST_VAULE "1234"
    uint8_t buf[UBJ_STATIC_MEM_STRING(sizeof(UBJSON_TO_STRING_TEST_VAULE) - 1)];
    char str[16] = { 0 };
    size_t out_len = sizeof(str);
    ubj_make_string(buf, UBJSON_TO_STRING_TEST_VAULE, sizeof(UBJSON_TO_STRING_TEST_VAULE) - 1);
    /* 解析 */
    if (ubjson_to_json(buf, sizeof(buf), str, &out_len) != NULL)
    {
        rt_kprintf("F:%s test ok. json:%s\n", __FUNCTION__, str);
    }
    else
    {
        rt_kprintf("F:%s test failed\n", __FUNCTION__);
    }
}

static void ubjson_to_xxx_test(void)
{
    ubjson_to_null_test();
    ubjson_to_noop_test();
    ubjson_to_true_test();
    ubjson_to_false_test();
    ubjson_to_bool_test();
    ubjson_to_uint8_test();
    ubjson_to_int8_test();
    ubjson_to_int16_test();
    ubjson_to_int32_test();
    ubjson_to_int64_test();
    ubjson_to_float_test();
    ubjson_to_double_test();
    ubjson_to_string_test();
    ubjson_to_buffer_test();
    ubjson_to_json_test();
}

int ubjson_msh(int argc, char** argv)
{
    ubjson_mem_test();
    ubj_integer_test();
    ubjson_helper_test();
    ubjson_helper_container_test();
    ubjson_make_test();

    ubj_write_buffer_test();
    ubj_create_test();
    ubj_other_test();
    ubj_write_ubjson_test();
    ubj_isvalid_test();
    json_to_ubj_test();
    ubjson_to_xxx_test();
    // ubj_encode_example();
    // ubj_decode_example();
    return 0;
}
MSH_CMD_EXPORT(ubjson_msh, ubjsonw cmd);
