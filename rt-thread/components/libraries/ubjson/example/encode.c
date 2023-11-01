#include "ubj.h"
#include <stdlib.h>
#include <string.h>

ubj_err_t ubj_decode_example(void)
{
    ubjsonw_t *ubj;

    ubj = ubj_write_memory();
    /* 对象开始写入 */
    ubj_begin_object(ubj);
    /* 写入整型 */
    ubj_object_write_integer(ubj, "k1", 5);
    /* 写入字符串 */
    ubj_object_write_string(ubj, "k2", "hello");
    /* 写入整型 */
    ubj_object_write_integer(ubj, "k1", 10);
    /* 结束对象写入 */
    ubj_end_object(ubj);
    ubj_write_end(ubj);
    return UBJ_OK;
}
