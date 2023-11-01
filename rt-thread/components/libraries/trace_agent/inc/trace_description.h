/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#ifndef TRACE_DESCRIPTION_H__
#define TRACE_DESCRIPTION_H__

/*
 * json/bson description for a C structure
 *
 * "struct_name" : {
 *   "length" : 100,
 *   "fields" : [
 *      {
 *          "name" : "a",
 *          "type" : "number/string/object, etc",
 *          "offset" : 100,
 *          "size" : 4
 *      },
 *      {
 *          "name" : "b",
 *          "type" : "number/string/object, etc",
 *          "offset" : 100,
 *          "size" : 4
 *      }
 *   ]
 * }
 */

#define DESC_TYPE_INTEGER    0x01    /* integer number with size 8/16/32/64 */
#define DESC_TYPE_NUMBER     0x02    /* float number with size 32/64 */
#define DESC_TYPE_BOOL       0x03    /* integer with 0/1 for False/True */
#define DESC_TYPE_CHAR       0x04    /* characters (size) */
#define DESC_TYPE_STR        0x05    /* string, char pointer with null-terminated string */
#define DESC_TYPE_OBJ        0x06    /* structure object */
#define DESC_TYPE_OBJ_PTR    0x07    /* structure with pointer */

struct data_desc
{
    const char* name;
    uint16_t type;
    uint16_t offset;

    union {
        uint32_t size;
        const struct data_desc *object;
    };
};

#define DATA_DESC_ITEM(obj, type, field)      {#field, type, (unsigned long)(&((obj *)0)->field), {.size = sizeof(((obj *)0)->field)}}
#define DATA_DESC_OBJ(obj, field, obj_desc)   {#field, DESC_TYPE_OBJ, (unsigned long)(&((obj *)0)->field), {.object = &obj_desc[0]}}
#define DATA_DESC_NULL()                      {0}

int desc_to_json(char *name, int hashkey, const struct data_desc* desc, char *json, size_t size);

#endif
