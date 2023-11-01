# 使用指南

本章节主要介绍 MCF 软件包的基本使用流程，并针对使用过程中经常涉及的结构体和重要 API 进行简要说明。

## 准备工作

首先需要下载 MCF 软件包，并将软件包加入工程中。在工程目录中使用 menuconfig 命令打开 ENV 配置界面，配置参数如下介绍：

```shell
RT-Thread online packages
  IoT - internet of things  --->
    [*] MCF: Multi-machine Communication Framework
    (1024) The maxinum packet size
    [ ]   Enable ACK feature
    (500)   ACK timout (unit tick)
    [ ]   Enable auto request execute feature
    [ ]     The maximum number of thread pools requested
    [ ]     The maximum stack size of thread pools requested
    [ ]   Enable data security feature
    [ ]     Enable AES256 encrypt feature
    [ ]     Enable fastlz compress feature
    [*]   Enable uart link support
    [ ]     Link uart crc check feature
    [ ]   Enable debug log
    [ ]   Enable samples
          Version (latest)  --->
```

- **The maxinum packet size**：设置传输数据最大长度限制（范围 512 - 4096）；
- **Enable ACK feature**：配置开启数据传输 ACK 响应功能；
  - **ACK timout (unit tick)**：配置 ACK 超时时间，单位 tick
- **Enable auto request execute feature**：配置开启请求数据自动执行响应操作功能；
    - **The maximum number of thread pools requested**：配置最大请求线程池数量；
    - **The maximum stack size of thread pools requested**：配置最大请求线程池线程栈大小；
- **Enable data security feature**：配置开启数据安全处理（解密、压缩）功能；
  - **Enable AES256 encrypt feature**：配置开启 AES256 数据加密方式 支持；
  - **Enable fastlz compress feature**：配置开启 FastLZ 数据压缩方式支持；
- **Enable uart link support**：配置开启链路层 UART 串口数据传输功能；
    - **Link uart crc check feature**：配置开启链路层 UART 串口数据传输 CRC 校验功能；
- **Enable debug log**：配置开启调试日志功能，可用于显示传输层数据包信息；
- **Enable samples**：配置添加示例代码；
- **Version**：配置软件包版本。

选择合适的配置项后，使用 `pkgs --update` 命令下载软件包并更新用户配置。 

## 使用流程

使用 MCF 软件包在多设备之间进行数据发送，需要完成如下操作：

**1、注册链路层设备，添加 ARP 列表**

在 MCF 初始化之前需要先对底层链路层设备进行注册和 ARP 列表进行处理，一般为自动初始化操作。

```c
#define MCF_PORT_ID    1    /* 当前注册的设备对应的端口索引值 */
#define MCF_DST_ID     2    /* 目标设备 ID */

int mcf_test_register(void)
{
	/* MCF 链路层注册和初始化串口 UART2 设备，对应端口号为 MCF_PORT_ID  */
    mcf_link_uart_register(MCF_PORT_ID, "uart2", RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX, RT_NULL);
     /* 添加 ARP 列表，指明当前端口号（MCF_PORT_ID）连接的设备 ID（MCF_DST_ID）*/
    mcf_link_arp_add(MCF_PORT_ID, MCF_DST_ID);
    return 0；
}
INIT_APP_EXPORT(mcf_register_sample);
```

**2、  初始化和启动 MCF 软件包**

底层设备注册完成之后，可以对 MCF 软件包进行初始化和启动。

```c
#define MCF_DEV_ID    1 /* 当前设备 ID */

int mcf_test_init(void)
{
    /* 通过当前设备 ID 初始化 MCF */
    mcf_init(MCF_DEV_ID);
    /* 启动 MCF 功能，开启进行数据接收和处理 */
    mcf_start();
    return 0;
}
```

**3、创建 D2D 数据会话**

在发送或者接收 D2D 格式数据之间，需要创建 D2D 数据会话，创建完成返回当前会话 ID，即发送数据包唯一 ID。

```c
int mcf_test_comm(void)
{
    int sid = 0;
    
    /* 通过目标设备 ID 创建会话 */
    sid = mcf_d2d_open(MCF_DST_ID);
    ...
}
```

**4、通过会话发送数据或接收数据**

创建会话完成之后，可以通过会话进行数据发送和接收。发送和接收数据之前需要完成 mcf_pkt 格式数据包创建和赋值等操作。下面是数据发送流程：

```c
struct mcf_pkt
{
    struct mcf_pbuf pbuf;              /* 包数据缓冲区  */
    struct mcf_pkt_info info;          /* 包信息 */
};

int mcf_test_comm(void)
{
    int sid = 0;
    struct mcf_pkt *pkt = RT_NULL;
    
    /* 通过目标设备 ID 创建会话 */
    sid = mcf_d2d_open(MCF_DST_ID);
    ...
    /* 动态创建并赋值 mcf_pkt 格式包数据 */
    pkt = mcf_pkt_alloc(sid, 128);
    ...
    /* 设置发送的包信息 */
    mcf_pkt_info_set(pkt, MCF_PKT_TYPE_REQ, MCF_PKT_FLAG_NEED_ACK | MCF_PKT_FLAG_NEED_RSP);
    /* 指定时间（5秒）内发送包数据 */
    mcf_d2d_send(sid, pkt, 5 * RT_TICK_PER_SECOND);
    ...
}
```

**5、关闭并释放当前会话**

发送数据和接收响应完成之后，需要关闭当前会话。如果需要再次发送数据，需要重新打开新的会话并发送。

```c
int mcf_test_comm(void)
{
    ....
    if (sid > 0)
    {
        mcf_d2d_close(sid);
    }
}
```

## 使用方式

MCF 软件包对于数据传输功能，提供如下几种数据传输方式，用于不同情况。

- **数据发送模式（无响应、无ACK）**

该模式下只发送数据，不需要接收 ACK 和响应数据。

```c
int mcf_test_send(void)
{
#define MCF_DST_ID    2

    int sid = 0, result = 0;
    struct mcf_pkt *pkt = RT_NULL;
    char *send_data = "hello_world!";

    /* 创建会话 ID  */
    sid = mcf_d2d_open(MCF_DST_ID);
	
    /* 动态分配并赋值 mcf_pkt 包格式 */
    pkt = mcf_pkt_alloc(sid, 128);
    rt_memcpy(pkt->pbuf.buf, send_data, rt_strlen(send_data));
    pkt->pbuf.len = rt_strlen(send_data);

    /* 设置包信息并在指定时间内发送数据 */
    mcf_pkt_info_set(pkt, MCF_PKT_TYPE_REQ, 0);
    result = mcf_d2d_send(sid, pkt, 5 * RT_TICK_PER_SECOND);

    /* 发送完成，关闭会话，释放包对象 */
    mcf_d2d_close(sid);
    mcf_pkt_free(pkt);
    return 0;
}
```

- **数据发送模式（有响应，有ACK）**

该模式下发送数据，并且可以等待并处理接收响应数据。

```c
int mcf_test_send(void)
{
#define MCF_DST_ID    2

    int sid = 0, result = 0;
    struct mcf_pkt *pkt = RT_NULL;
    char *send_data = "hello_world!";

    /* 创建会话 ID  */
    sid = mcf_d2d_open(MCF_DST_ID);
	...

    /* 动态分配并赋值 mcf_pkt 包格式 */
    pkt = mcf_pkt_alloc(sid, 128);
    rt_memcpy(pkt->pbuf.buf, send_data, rt_strlen(send_data));
    pkt->pbuf.len = rt_strlen(send_data);

    /* 设置包信息（需要响应和需要 ACK），并在指定时间内发送数据 */
    mcf_pkt_info_set(pkt, MCF_PKT_TYPE_REQ, MCF_PKT_FLAG_NEED_ACK | MCF_PKT_FLAG_NEED_RSP);
    result = mcf_d2d_send(sid, pkt, 5 * RT_TICK_PER_SECOND);
	...
    /* 阻塞等待并接收响应数据 */
    result = mcf_d2d_recv(sid, pkt, RT_WAITING_FOREVER);
    /* 处理接收的响应数据 */
    ...
        
    /* 发送完成，关闭会话，释放包对象 */
    mcf_d2d_close(sid);
    mcf_pkt_free(pkt);
    return 0;
}
```

- **数据接收模式**

正常 MCF 数据发送模式下 mcf_d2d_recv 只能接收当前会话 ID 的数据，在数据接收模式下 mcf_d2d_recv 函数可以接收所有会话 ID 的数据，并对接收的数据进行处理，如下所示：

```c
int mcf_recv_test(void)
{
#define MCF_RECV_ID    254

    int sid = 0, result = 0;
    struct mcf_pkt *pkt = RT_NULL;

    /* 创建会话 ID  */
    sid = mcf_d2d_open(MCF_DST_ID);
	...

    /* 动态分配 mcf_pkt 包格式, 用于数据接收 */
    pkt = mcf_pkt_alloc(sid, 1024);
	...

    while (1)
    {
        rt_memset(pkt->pbuf.buf, 0, pkt->pbuf.size);
        pkt->pbuf.len = 0;

        /* 循环接收发往该设备的所有会话的数据  */
        result = mcf_d2d_recv(sid, pkt, RT_WAITING_FOREVER);
        if (result != 0)
        {
            goto _exit;
        }

        /* 处理接收到的数据 */
        ...
    }
        
    /* 发送完成，关闭会话，释放包对象 */
    mcf_d2d_close(sid);
    mcf_pkt_free(pkt);
    return 0;
}
```

- **请求自动响应模式**

软件包配置选项中如果开启  `Enable auto request execute feature` 功能，则开启请求自动响应模式，该模式下支持多个命令同时发送和处理（线程池方式实现）。

该模式下需要先注册请求自动响应回调函数，当前设备在接收到连接设备发送的请求类型数据时，会执行注册的回调函数，得到响应结果，再将响应结果发送会请求的设备端，完成整个请求自动处理和响应的过程。

```c
static int mcf_request_execute_cb(struct mcf_pkt *in_pkt, struct mcf_pkt *out_pkt)
{
    char *recv_data = "mcf recv and response data!"

    rt_memcpy(out_pkt->pbuf.buf, recv_data, rt_strlen(recv_data));
    out_pkt->pbuf.len = rt_strlen(recv_data);
    return 0;
}

int mcf_test_request_exec(void)
{
#define MCF_DST_ID    2

    int sid = 0, result = 0;
    struct mcf_pkt *pkt = RT_NULL;
    char *send_data = "hello_world!";

    /* 注册请求响应回调函数 */
    mcf_req_exec_fn_register((mcf_req_exec_fn) mcf_request_execute_cb);

    /* 创建会话 ID  */
    sid = mcf_d2d_open(MCF_DST_ID);
	...

    /* 动态分配并赋值 mcf_pkt 包格式 */
    pkt = mcf_pkt_alloc(sid, 128);
    rt_memcpy(pkt->pbuf.buf, send_data, rt_strlen(send_data));
    pkt->pbuf.len = rt_strlen(send_data);

    /* 设置包信息（需要响应和需要 ACK），并在指定时间内发送数据 */
    mcf_pkt_info_set(pkt, MCF_PKT_TYPE_REQ, MCF_PKT_FLAG_NEED_ACK | MCF_PKT_FLAG_NEED_RSP);
    result = mcf_d2d_send(sid, pkt, 5 * RT_TICK_PER_SECOND);
	...
    /* 阻塞等待并接收自动响应的数据 */
    result = mcf_d2d_recv(sid, pkt, RT_WAITING_FOREVER);
    /* 处理接收的响应数据 */
    ...
        
    /* 发送完成，关闭会话，释放包对象 */
    mcf_d2d_close(sid);
    mcf_pkt_free(pkt);
    return 0;
}
```
## 常见问题

**发送数据过长断言**

```c
(size && size <= MCF_PKT_MAX_SIZE) assertion failed at function:mcf_pkt_alloc, line number:59 
```

该问题是创建和发送 mcf_pkt 包对象时，传入的最大长度操作配置的最大包长度支持（默认 1024 bytes），需要修改传入最大长度或者配置修改最大长度支持。

**请求自动响应功能不支持**

```c
[E/mcf.tans] not enable MCF request execute feature.
```

该问题 ENV 中没有开启配置请求自动响应配置选项支持，需先开启该功能支持。
