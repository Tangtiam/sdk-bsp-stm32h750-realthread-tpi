#include "ubj.h"
#include <stdlib.h>
#include <string.h>

static ubj_err_t dub_dump_value(ubjsonr_t *ubj, const ubj_value_t *v);
static ubj_err_t ubj_dump_container(ubjsonr_t *ubj, const ubj_value_t *v)
{
    ubj_type_t type = v->type;

    if (v->error != UBJ_OK || v->container.act != UBJ_CONTAINER_ENTER_FLAG)
        return -UBJ_ERROR;

    if (type == UBJ_TYPE_OBJECT)
        printf("%c", '{');
    else
        printf("%c", '[');

    v = ubj_read_next(ubj); // read first (skip enter flag)
    if (v->error == UBJ_OK && v->container.act != UBJ_CONTAINER_EXIT_FLAG)
    {
        /* dump key */
        if (type == UBJ_TYPE_OBJECT)
            printf("\"%s\":", v->container.key);
        /* dump value */
        dub_dump_value(ubj, v);
        /* get next value */
        v = ubj_read_next(ubj);
        while (v->error == UBJ_OK && v->container.act != UBJ_CONTAINER_EXIT_FLAG)
        {
            /* dump key */
            if (type == UBJ_TYPE_OBJECT)
                printf(",\"%s\":", v->container.key);
            else
                printf("%c", ',');
            dub_dump_value(ubj, v);
            v = ubj_read_next(ubj);
        }
    }
    if (type == UBJ_TYPE_OBJECT)
        printf("%c", '}');
    else
        printf("%c", ']');
    return v->error;
}

static ubj_err_t dub_dump_value(ubjsonr_t *ubj, const ubj_value_t *v)
{
    ubj_err_t error = v->error;;
    switch (v->type)
    {
    case UBJ_TYPE_NULL:
        printf("%s", "null");
        break;
    case UBJ_TYPE_NOOP:
        printf("%s", "noop");
        break;
    case UBJ_TYPE_TRUE:
        printf("%s", "true");
        break;
    case UBJ_TYPE_FALSE:
        printf("%s", "false");
        break;
    case UBJ_TYPE_CHAR:
        printf("\"%c\"", v->value.vchar);
        break;
    case UBJ_TYPE_STRING:
    case UBJ_TYPE_HIGH_PRECISION:
        printf("\"%s\"", v->value.vstring.string);
        break;
    case UBJ_TYPE_INT8:
    case UBJ_TYPE_UINT8:
    case UBJ_TYPE_INT16:
    case UBJ_TYPE_INT32:
        printf("%d", v->value.vint32);
        break;
    case UBJ_TYPE_INT64:
        printf("%lld", v->value.vint64);
        break;
    case UBJ_TYPE_FLOAT32:
        /* TODO: 最大长度未测量 */
        printf("%lld", v->value.vfloat);
        break;
    case UBJ_TYPE_FLOAT64:
        /* TODO: 最大长度未测量 */
        printf("%lld", v->value.vdouble);
        break;
    case UBJ_TYPE_ARRAY:
    case UBJ_TYPE_OBJECT:
        /* TODO: 未返回错误值 */
        error = ubj_dump_container(ubj, v);
        break;
    default:
        break;
    }
    return error;
}

ubj_err_t ubj_encode_example(void)
{
    const ubj_value_t *v;
    static const char static_ubjson[] = "{#i\x3i\x2k1i\x05i\x2k2Si\x05helloi\x2k3i\x0a";
    ubjsonr_t *ubj;

    ubj = ubj_read_memory(static_ubjson, sizeof(static_ubjson) - 1);
    v = ubj_read_next(ubj);
    if (v && v->error == UBJ_OK)
    {
        dub_dump_value(ubj, v);
    }
    ubj_read_end(ubj);
    return UBJ_OK;
}
