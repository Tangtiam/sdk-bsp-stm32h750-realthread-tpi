/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include "ubj.h"
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

struct _json_buf
{
    char *buf;
    int size;
    int total;
};

#define UBJ_ALIGN(size, align)           (((size) + (align) - 1) & ~((align) - 1))

static ubj_err_t dump_value(ubjsonr_t *ubj, const ubj_value_t *v, struct _json_buf *jbuff);

static char *mem_ensure(struct _json_buf *jbuff, size_t size)
{
    int alloc_size;
    char *tmpbuf, *outbuf = NULL;

    alloc_size = size - (jbuff->total - jbuff->size);
    if (alloc_size > 0)
    {
        alloc_size = UBJ_ALIGN(alloc_size, 64);
        tmpbuf = ubj_realloc(jbuff->buf, jbuff->total + alloc_size);
        if (tmpbuf)
        {
            jbuff->buf = tmpbuf;
            jbuff->total += alloc_size;
            outbuf = &jbuff->buf[jbuff->size];
        }
        else
        {
            /* TODO: Unhandled memory allocation failure */
        }
    }
    else
    {
        outbuf = &jbuff->buf[jbuff->size];
    }
    return outbuf;
}

static int _compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

static ubj_err_t dump_number(struct _json_buf *jbuff, double d)
{
    int length = 0;
    char number_buffer[26] = {0};
    double test = 0.0;
    char *output_pointer;

    /* This checks for NaN and Infinity */
    if (isnan(d) || isinf(d))
    {
        length = sprintf(number_buffer, "null");
    }
    else
    {
        /* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
        length = sprintf((char*)number_buffer, "%1.15g", d);

        /* Check whether the original double can be recovered */
        if ((sscanf((char*)number_buffer, "%lg", &test) != 1) || !_compare_double((double)test, d))
        {
            /* If not, print with 17 decimal places of precision */
            length = sprintf((char*)number_buffer, "%1.17g", d);
        }
    }

    /* sprintf failed or buffer overrun occurred */
    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return -UBJ_ERROR;
    }

    /* reserve appropriate space in the output */
    output_pointer = mem_ensure(jbuff, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return -UBJ_NOMEM_ERROR;
    }
    /* copy the printed number to the output buffer */
    memcpy(output_pointer, number_buffer, length);
    output_pointer[length] = '\0';
    jbuff->size += (int)length;

    return UBJ_OK;
}

static ubj_err_t dump_string(struct _json_buf *jbuff, const char * const input)
{
    const char *input_pointer = NULL;
    char *output_pointer = NULL;
    size_t output_length = 0;
    /* numbers of additional characters needed for escaping */
    size_t escape_characters = 0;

    /* empty string */
    if (input == NULL)
    {
        output_pointer = mem_ensure(jbuff, sizeof("\"\""));
        if (output_pointer == NULL)
        {
            return -UBJ_NOMEM_ERROR;
        }
        strcpy(output_pointer, "\"\"");
        jbuff->size = jbuff->size + 2;
        return UBJ_OK;
    }

    /* set "flag" to 1 if something needs to be escaped */
    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                /* one character escape sequence */
                escape_characters++;
                break;
            default:
                if (*input_pointer < 32)
                {
                    /* UTF-16 escape sequence uXXXX */
                    escape_characters += 5;
                }
                break;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output_pointer = mem_ensure(jbuff, output_length + sizeof("\"\""));
    if (output_pointer == NULL)
    {
        return -UBJ_NOMEM_ERROR;
    }
    jbuff->size = jbuff->size + output_length + 2;

    /* no characters have to be escaped */
    if (escape_characters == 0)
    {
        output_pointer[0] = '\"';
        memcpy(output_pointer + 1, input, output_length);
        output_pointer[output_length + 1] = '\"';
        output_pointer[output_length + 2] = '\0';
        return UBJ_OK;
    }

    output_pointer[0] = '\"';
    output_pointer = output_pointer + 1;
    /* copy the string */
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            /* normal character, copy */
            *output_pointer = *input_pointer;
        }
        else
        {
            /* character needs to be escaped */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* escape and print as unicode codepoint */
                    sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    *output_pointer++ = '\"';
    *output_pointer++ = '\0';

    return UBJ_OK;
}

static ubj_err_t dump_tag(struct _json_buf *jbuff, char tag)
{
    char *buf;
    if ((buf = mem_ensure(jbuff, 2)) != NULL)
    {
        buf[0] = tag;
        buf[1] = '\0';
        jbuff->size = jbuff->size + 1;
        return UBJ_OK;
    }
    else
    {
        return -UBJ_NOMEM_ERROR;
    }
}

static ubj_err_t container_begin(struct _json_buf *jbuff, ubj_type_t type, uint8_t act)
{
    ubj_err_t error = -UBJ_ERROR;

    if (act != UBJ_CONTAINER_ENTER_FLAG)
    {
        return -UBJ_ERROR;
    }
    // dump container
    if (type == UBJ_TYPE_OBJECT)
    {
        error = dump_tag(jbuff, '{');
    }
    else if (type == UBJ_TYPE_ARRAY)
    {
        error = dump_tag(jbuff, '[');
    }
    return error;
}

static ubj_err_t container_end(struct _json_buf *jbuff, ubj_type_t type, uint8_t act)
{
    ubj_err_t error = -UBJ_ERROR;

    if (act != UBJ_CONTAINER_EXIT_FLAG)
    {
        return -UBJ_ERROR;
    }
    // dump container
    if (type == UBJ_TYPE_OBJECT)
    {
        error = dump_tag(jbuff, '}');
    }
    else if (type == UBJ_TYPE_ARRAY)
    {
        error = dump_tag(jbuff, ']');
    }
    return error;
}

static ubj_err_t container_object_key(struct _json_buf *jbuff, const ubj_value_t *v, int first)
{
    ubj_err_t error = -UBJ_ERROR;

    if (first)
    {
        // read first (skip enter flag)
        error = dump_string(jbuff, v->container.key);
        if (error != UBJ_OK)
        {
            return error;
        }
        error = dump_tag(jbuff, ':');
    }
    else
    {
        error = dump_tag(jbuff, ',');
        if (error != UBJ_OK)
        {
            return error;
        }
        error = dump_string(jbuff, v->container.key);
        if (error != UBJ_OK)
        {
            return error;
        }
        error = dump_tag(jbuff, ':');
    }
    return error;
}

static ubj_err_t container_array_separation(struct _json_buf *jbuff, int first)
{
    if (first)
    {
        return UBJ_OK;
    }
    else
    {
        return dump_tag(jbuff, ',');
    }
}

static ubj_err_t container_key(struct _json_buf *jbuff, const ubj_value_t *v, ubj_type_t type, int first)
{
    ubj_err_t error = -UBJ_ERROR;

    /* dump key */
    if (type == UBJ_TYPE_OBJECT)
    {
        return container_object_key(jbuff, v, first);
    }
    else if (type == UBJ_TYPE_ARRAY)
    {
        return container_array_separation(jbuff, first);
    }
    return error;
}

static ubj_err_t container_value(struct _json_buf *jbuff, ubjsonr_t *ubj, const ubj_value_t *v, ubj_type_t type, int first)
{
    ubj_err_t error;

    if (v->error != UBJ_OK)
    {
        return v->error;
    }
    if (v->container.act == UBJ_CONTAINER_EXIT_FLAG)
    {
        return UBJ_OK;
    }
    // dump key
    error = container_key(jbuff, v, type, first);
    if (error == UBJ_OK)
    {
        // dump value
        error = dump_value(ubj, v, jbuff);
    }
    return error;
}

static ubj_err_t ubj_dump_container(ubjsonr_t *ubj, const ubj_value_t *v, struct _json_buf *jbuff)
{
    ubj_type_t type = v->type;
    ubj_err_t error = -UBJ_NOMEM_ERROR;
    int cnt = 0;
    uint8_t act;

    // begin
    error = container_begin(jbuff, type, v->container.act);
    if (error != UBJ_OK)
    {
        return error;
    }
    // dump container
    do 
    {
        // try read next
        v = ubj_read_next(ubj);
        act = v->container.act;
        // value
        error = container_value(jbuff, ubj, v, type, !cnt);
        if (error != UBJ_OK)
        {
            return error;
        }
        cnt ++;
    } while (act != UBJ_CONTAINER_EXIT_FLAG);
    // end
    error = container_end(jbuff, type, v->container.act);
    return error;
}

static ubj_err_t dump_value(ubjsonr_t *ubj, const ubj_value_t *v, struct _json_buf *jbuff)
{
    ubj_err_t error = -UBJ_NOMEM_ERROR;
    char *buf;

    if (v->error != UBJ_OK)
    {
        return v->error;
    }
    switch (v->type)
    {
    case UBJ_TYPE_NULL:
        if ((buf = mem_ensure(jbuff, 5)) != NULL)
        {
            strcpy(buf, "null");
            jbuff->size += 4;
            error = UBJ_OK;
        }
        break;
    case UBJ_TYPE_NOOP:
        if ((buf = mem_ensure(jbuff, 5)) != NULL)
        {
            strcpy(buf, "noop");
            jbuff->size += 4;
            error = UBJ_OK;
        }
        break;
    case UBJ_TYPE_TRUE:
        if ((buf = mem_ensure(jbuff, 5)) != NULL)
        {
            strcpy(buf, "true");
            jbuff->size += 4;
            error = UBJ_OK;
        }
        break;
    case UBJ_TYPE_FALSE:
        if ((buf = mem_ensure(jbuff, 6)) != NULL)
        {
            strcpy(buf, "false");
            jbuff->size += 5;
            error = UBJ_OK;
        }
        break;
    case UBJ_TYPE_CHAR:
        if ((buf = mem_ensure(jbuff, 4)) != NULL)
        {
            buf[0] = '\"';
            buf[1] = v->value.vchar;
            buf[2] = '\"';
            buf[3] = '\0';
            jbuff->size += 3;
            error = UBJ_OK;
        }
        break;
    case UBJ_TYPE_STRING:
    case UBJ_TYPE_HIGH_PRECISION:
        error = dump_string(jbuff, v->value.vstring.string);
        break;
    case UBJ_TYPE_INT8:
        error = dump_number(jbuff, v->value.vint8);
        break;
    case UBJ_TYPE_UINT8:
        error = dump_number(jbuff, v->value.vuint8);
        break;
    case UBJ_TYPE_INT16:
        error = dump_number(jbuff, v->value.vint16);
        break;
    case UBJ_TYPE_INT32:
        error = dump_number(jbuff, v->value.vint32);
        break;
    case UBJ_TYPE_INT64:
        //NOTE: possible loss of accuracy
        error = dump_number(jbuff, (double)v->value.vint64);
        break;
    case UBJ_TYPE_FLOAT32:
        error = dump_number(jbuff, v->value.vfloat);
        break;
    case UBJ_TYPE_FLOAT64:
        error = dump_number(jbuff, v->value.vdouble);
        break;
    case UBJ_TYPE_ARRAY:
    case UBJ_TYPE_OBJECT:
        /* TODO: No error value returned */
        error = ubj_dump_container(ubj, v, jbuff);
        break;
    default:
        break;
    }
    return error;
}

ubj_err_t ubj_to_json(ubjsonr_t *ubj, char **buff, size_t *len)
{
    struct _json_buf jbuf;
    const ubj_value_t *v;
    ubj_err_t err;

    jbuf.total = 0;
    jbuf.size = 0;
    jbuf.buf = NULL;
    v = ubj_read_next(ubj);
    err = v->error;
    if (v && err == UBJ_OK)
    {
        err = dump_value(ubj, v, &jbuf);
        if (err != UBJ_OK)
        {
            ubj_free(jbuf.buf);
            jbuf.size = 0;
            jbuf.buf = NULL;
        }
        else
        {
            if (buff)
            {
                *buff = jbuf.buf;
            }
            else
            {
                ubj_free(jbuf.buf);
                jbuf.buf = NULL;
            }
            if (len)
            {
                *len = jbuf.size;
            }
        }
    }
    return err;
}

#include "cJSON.h"
static ubj_err_t dump_json(ubjsonw_t *ubj, cJSON *item)
{
    ubj_err_t res = UBJ_OK;

    switch (item->type & 0xFF)
    {
    case cJSON_False:
        ubj_write_false(ubj);
        break;
    case cJSON_True:
        ubj_write_true(ubj);
        break;
    case cJSON_NULL:
        ubj_write_null(ubj);
        break;
    case cJSON_Number:
    {
        double double_value = cJSON_GetNumberValue(item);
        int64_t int64_value = (int64_t)double_value;
        if (double_value - int64_value == 0)
        {
            if (int64_value > INT32_MAX || int64_value < INT32_MIN)
            {
                ubj_write_int64(ubj, int64_value);
            }
            else
            {
                ubj_write_int32(ubj, (int32_t)int64_value);
            }
        }
        else
        {
            ubj_write_float64(ubj, double_value);
        }
        break;
    }
    case cJSON_String:
        ubj_write_string(ubj, cJSON_GetStringValue(item));
        break;
    case cJSON_Raw:
        ubj_write_string(ubj, item->valuestring);
        break;
    case cJSON_Array:
    {
        cJSON *element = NULL;
        ubj_begin_array(ubj);
        cJSON_ArrayForEach(element, item)
        {
            dump_json(ubj, element);
        }
        ubj_end_array(ubj);
        break;
    }
    case cJSON_Object:
    {
        cJSON *element = NULL;
        ubj_begin_object(ubj);
        cJSON_ArrayForEach(element, item)
        {
            ubj_write_key(ubj, element->string);
            dump_json(ubj, element);
        }
        ubj_end_object(ubj);
        break;
    }
    }
    return res;
}

ubj_err_t json_to_ubj(ubjsonw_t *ubj, const char *json, size_t len)
{
    cJSON *root;
    ubj_err_t err;

    if (json == NULL)
    {
        return -UBJ_PARAM_ERROR;
    }
    root = cJSON_ParseWithLengthOpts(json, len, NULL, 0);
    if (root == NULL)
    {
        return -UBJ_PARSE_ERROR;
    }
    err = dump_json(ubj, root);
    cJSON_Delete(root);
    return err;
}
