# UBJSON 组件说明

UBJSON 是 JSON 二进制表现形式。相比 JSON 具有体积小，解析快等特点。适合在系统内交互数据使用。

## 构造 UBJSON

UBJSON 与 JSON 表现形式上是一一对应的。对应关系如下表：

| JSON  | UBJSON  |
| ----- | ------- |
| `{`     | ubj_begin_object() |
| `}`     | ubj_end_object() |
| `[`     | ubj_begin_array() |
| `]`     | ubj_end_array() |
| `字符串` | ubj_write_string(), ubj_object_write_string()|
| `数字`  | ubj_write_integer(), ubj_object_write_integer() |
| `对象`  | ubj_begin_object(), ubj_object_write_object() |
| `数组`  | ubj_begin_array(), ubj_object_write_array() |
| `布尔值` | ubj_write_true(), ubj_object_write_true() |

## API

### UBJSON 编码 API

#### 数组容器

`ubjsonw_t *ubj_write_init(ubjsonw_t* ubj, size_t (*write)(ubjsonw_t *, const void *, size_t, void *), void (*close)(ubjsonw_t *, void *), void *userdata)`

初始化 UBJSON 编码对象

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 内存空间 | |
| write  | 写入数据函数指针 | 必须是非空函数指针 |
| close  | 全部结束写入调用 |  |
| **返回** | **描述** |
| ubjsonw_t | 非空表示初始化成功 |

`size_t ubj_write_end(ubjsonw_t* ubj)`

结束 UBJSON 编码

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 | 结束写入 |
| **返回** | **描述** |
| size_t | 总共写入的数据量 |

`ubj_err_t ubj_begin_array(ubjsonw_t *ubj)`

标记后续数据，放在数组容器中。

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |

`ubj_err_t ubj_begin_fixed_array(ubjsonw_t *ubj, ubj_type_t type, long count)`

标记后续数据，放在数组容器中。

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| type | 容器的存放数据类型 | |
| count | 容器的大小 | |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |

容器类型取值范围

| 类型 | 说明 |
| ---- | ----|
| UBJ_TYPE_MIXED             |      混合类型                 |
| UBJ_TYPE_NULL              |      空类型                   |
| UBJ_TYPE_NOOP              |      占位类型                 |
| UBJ_TYPE_TRUE              |      bool 类型-真             |
| UBJ_TYPE_FALSE             |      bool 类型-假             |
| UBJ_TYPE_CHAR              |      字符类型                 |
| UBJ_TYPE_STRING            |      字符串类型               |
| UBJ_TYPE_HIGH_PRECISION    |      高精度数值类型（字符串） |
| UBJ_TYPE_INT8              |      整型                     |
| UBJ_TYPE_UINT8             |      无符号单字节整型         |
| UBJ_TYPE_INT16             |      有符号双字节整型         |
| UBJ_TYPE_INT32             |      有符号四字节整型         |
| UBJ_TYPE_INT64             |      有符号八字节整型         |
| UBJ_TYPE_FLOAT32           |      单精度浮点数             |
| UBJ_TYPE_FLOAT64           |      双精度浮点数             |
| UBJ_TYPE_ARRAY             |      数组容器                 |
| UBJ_TYPE_OBJECT            |      对象容器                 |


`ubj_err_t ubj_end_array(ubjsonw_t *ubj)`

数组写入结束标记。

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |

数组容器写入数据。函数定义基本相似，以表格进行展示

`ubj_err_t ubj_write_xxx(ubj, v)`

| 函数 | 描述 | 说明 |
| ---- | ----| ---- |
| `ubj_err_t ubj_write_string(ubjsonw_t *ubj, const char *v)` | 字符串 | 传入一个字符串指针 |
| `ubj_err_t ubj_write_char(ubjsonw_t *ubj, char v)` | 字符 | |
| `ubj_err_t ubj_write_uint8(ubjsonw_t *ubj, uint8_t v)` | UINT8 类型数据 | |
| `ubj_err_t ubj_write_high_precision(ubjsonw_t *ubj, const char *hp)` | 高精度字符串 | |
| `ubj_err_t ubj_write_integer(ubjsonw_t *ubj, int64_t v)` | 整型 | |
| `ubj_err_t ubj_write_float32(ubjsonw_t *ubj, float v)` | 单精度浮点 | |
| `ubj_err_t ubj_write_float64(ubjsonw_t *ubj, double v)` | 双进度浮点 | |
| `ubj_err_t ubj_write_null(ubjsonw_t *ubj)` | 空类型 | |
| `ubj_err_t ubj_write_true(ubjsonw_t *ubj)` | bool true | |
| `ubj_err_t ubj_write_false(ubjsonw_t *ubj)` | bool false | |
| `ubj_err_t ubj_begin_array(ubjsonw_t *ubj)` | 嵌套数组容器 | |
| `ubj_err_t ubj_begin_object(ubjsonw_t *ubj)` | 嵌套对象容器 | |

数容器写入示例

```json
[
    false,
    4782345193,
    [
        "hello"
    ]
]
```

```c
/* 标记容器内容为混合类型，忽略长度 */
ubj_begin_array(ubj);                                       /* [  */
ubj_write_false(ubj);                                       /*     false */                          
ubj_write_integer(ubj, 4782345193);                         /*     4782345193 */
ubj_begin_array(ubj);                                       /*     [  */
ubj_write_string(ubj, "hello")                              /*         "hello" */
ubj_end_array(ubj);                                         /*     ] */
ubj_end_array(ubj);                                         /* ] */
```

### 对象容器

`ubj_err_t ubj_begin_object(ubjsonw_t *ubj)`

标记后续数据，放在对象容器中

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |

`ubj_err_t ubj_begin_fixed_object(ubjsonw_t *ubj, ubj_type_t type, long count)`

标记后续数据，放在对象容器中

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| type | 容器的存放数据类型 | |
| count | 容器的大小 | |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |

`ubj_err_t ubj_end_object(ubjsonw_t *ubj)`

对象容器写入结束标记

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |

`ubj_err_t ubj_write_key(ubjsonw_t *ubj, const char *key)`

写入关键字，仅用于对象容器关键字写入。对象容器写入数据之前，必须先写关键字

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 对象 |  |
| key  | 关键字 | 对象容器使用 |
| **返回** | **描述** |
| ubj_err_t | 错误码，返回 UBJ_OK 表示没有错误 |


对象容器写入数据，函数定义类似，以表格进行展示

`ubj_err_t ubj_object_write_xxx(ubjsonw_t *ubj, const char *key, v)`

| 函数 | 描述 | 说明 |
| ---- | ----| ---- |
| `ubj_err_t ubj_object_write_string(ubjsonw_t *ubj, const char *key, const char *v)` | 字符串 | 传入一个字符串指针 |
| `ubj_err_t ubj_object_write_char(ubjsonw_t *ubj, const char *key, char v)` | 字符 | |
| `ubj_err_t ubj_object_write_uint8(ubjsonw_t *ubj, const char *key, uint8_t v)` | UINT8 类型数据 | |
| `ubj_err_t ubj_object_write_high_precision(ubjsonw_t *ubj, const char *key, const char *hp)` | 高精度字符串 | |
| `ubj_err_t ubj_object_write_integer(ubjsonw_t *ubj, const char *key, int64_t v)` | 整型 | |
| `ubj_err_t ubj_object_write_float32(ubjsonw_t *ubj, const char *key, float v)` | 单精度浮点 | |
| `ubj_err_t ubj_object_write_float64(ubjsonw_t *ubj, const char *key, double v)` | 双进度浮点 | |
| `ubj_err_t ubj_object_write_null(ubjsonw_t *ubj, const char *key)` | 空类型 | |
| `ubj_err_t ubj_object_write_true(ubjsonw_t *ubj, const char *key)` | bool true | |
| `ubj_err_t ubj_object_write_false(ubjsonw_t *ubj, const char *key)` | bool false | |
| `ubj_err_t ubj_object_write_object(ubjsonw_t *ubj, const char *key)` | 嵌套对象容器 | |
| `ubj_err_t ubj_object_write_array(ubjsonw_t *ubj, const char *key)` | 嵌套数组容器 | |

对象容器写入示例

```json
{
    "bool":false,
    "number":4782345193,
    "object":
    {
        "string":"hello"
    }
}
```

```c
/* 标记容器内容为混合类型，忽略长度 */
ubj_begin_object(ubj);                                      /* {  */
ubj_object_write_false(ubj, "bool");                        /*     "bool" : false */                          
ubj_object_write_integer(ubj, "number", 4782345193);        /*     "number" : 4782345193 */
ubj_object_write_object(ubj, "object");                     /*     "object" : { */
ubj_write_string(ubj, "string", "hello")                    /*          "string" : "hello" */
ubj_end_object(ubj);                                        /*      } */
ubj_end_object(ubj);                                        /* } */
```

### UBJSON 解码 API

`ubjsonr_t *ubj_read_init(ubjsonr_t* ubj,size_t (*read)(ubjsonr_t *, void *, size_t, void *),void (*close)(ubjsonr_t *, void *),void *userdata);`

初始化 UBJSON 解码对象

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 内存空间 | |
| read  | 读取数据函数指针 | 必须是非空函数指针 |
| close  | 全部结束写入调用 |  |
| **返回** | **描述** |
| ubjsonr_t | 非空表示初始化成功 |

`size_t ubj_read_end(ubjsonr_t* ubj)`

结束 UBJSON 解码

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj  | UBJSON 解码对象 | |
| **返回** | **描述** |
| size_t | 读取的总字节数 |

`const ubj_value_t *ubj_read_next(ubjsonr_t *ubj)`

解码迭代器。通过不断迭代，获取所有的数据。

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj | UBJSON 解码对象 | |
| **返回** | **描述** |
| const ubj_value_t * | 数据 |

`ubj_err_t ubj_skip_subobject(ubjsonr_t* ubj)`

跳过子容器。

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj | UBJSON 解码对象 | |
| **返回** | **描述** |
| ubj_err_t | 错误码，UBJ_OK 表示无错误 | 

### 工具 API

`ubj_err_t ubj_to_json(ubjsonr_t *ubj, char **buff, size_t *len)`

UBJSON 转 JSON 

| 参数 | 描述 | 说明 |
| ---- | ----| ---- |
| ubj | UBJSON 解码对象 | |
| buff | 接收 JSON buff 的指针 |  |
| len | 接收 buff 长度的指针 | |
| **返回** | **描述** |
| ubj_err_t | 错误码，UBJ_OK 表示无错误 | 

## 格式描述

### 数组格式

#### 常规数组

- `[` 符号开头
- `]` 符号结尾

示例:

JSON 格式 

```json
[
    null,
    true,
    false,
    4782345193,
    153.132,
    "ham"
]
```

UBJSON 格式

```
[[]
    [Z]
    [T]
    [F]
    [l][4782345193]
    [d][153.132]
    [S][i][3][ham]
[]]
```

#### 强类型数组

强类型数组，整个数组中仅包含一种类型的数据。

| 大小 | TAG | TAG TYPE | 示例 | 说明 |
| ---- | --- | -------- | ---- | ---- |
| 1B   | `$`   | 数值类型或容器类型 | `[$][S]` | string type | 

#### 定长数组

ubjson 是 JSON 的二进制形式，与传统字符串格式相比，性能上更有优势。

| 大小 | TAG | TAG TYPE | 示例 | 说明 |
| ---- | --- | -------- | ---- | ---- |
| 1B   | `#`   | 正整数类型 | ` [#][i][64]` | 数组长度64 | 

- [计数]计数必须大于等于0。
- [计数]计数可以自己指定。
- [计数]如果指定了计数，则容器不能指定结束标记。
- [计数]指定计数的容器必须包含指定数量的子元素。
- [类型]如果指定了类型，则必须在计数之前指定。
- [类型]如果指定了类型，则还必须指定计数。类型不能自己指定。
- [类型的标记]指定不包含任何附加值的类型。