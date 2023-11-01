# 示例程序

MCF 软件包提供一个简单的示例程序，可实现两个不同的设备进行数据通讯。

**示例文件**

| 示例程序路径          | 说明             |
| --------------------- | ---------------- |
| samples/mcf_samples.c | MCF 数据通讯示例 |

## 准备工作

### 获取软件包

- menuconfig 配置开启软件包示例代码

  打开 RT-Thread 提供的 ENV 工具，使用 **menuconfig** 配置软件包。

  启用 MCF  软件包，并配置使能测试例程（Enable samples），如下所示：

```shell
RT-Thread online packages
  IoT - internet of things  --->
    [*] MCF: Multi-machine Communication Framework
    (1024) The maxinum packet size
    [ ]   Enable ACK feature
    [ ]   Enable auto request execute feature
    [ ]   Enable data security feature
    [*]   Enable uart link support
    [ ]     Link uart crc16 feature
    [ ]   Enable debug log
    [*]   Enable samples
          Version (latest)  --->
```
- 使用 `pkgs --update` 命令下载软件包
- 编译下载

## 启动例程 

MCF 主要功能是实现多设备之间数据通讯，该例程功能需要两个设备配合完成，两个设备分别为请求方设备和响应放设备。

例程导出三个 FinSH 命令用于初始化和数据收发：

- mcf_init 命令：注册链路层通讯设备，添加链路层 ARP 列表，初始化和启动 MCF；
- mcf_send 命令：发送请求数据，并等待接收响应数据；
- mcf_recv 命令：等待接收请求数据，并发送响应数据；

### 接收方设备示例

示例流程：

- 注册链路层层通讯设备
- 添加链路层 ARP 列表
- 初始化和启动 MCF
- 创建 D2D 数据会话
- 创建 PKT 包对象
- 阻塞等待接收请求数据
- 接收到请求数据，赋值响应数据给 PKT 包
- 指定时间内发送响应数据给请求方
- 关闭当前会话

请求方设备，先通过执行 `mcf_init`  命令初始化和启动 MCF 软件包，然后执行 `mcf_recv` 命令等待接收请求数据，接收到请求数据后会拼接响应数据并返回给请求方，发送数据完成后关闭会话。

示例日志如下所示：

```shell
msh >mcf_init uart2 2 1
[23083] I/mcf.link: MCF link port(1) register successfully.
[23090] I/mcf.trans: MCF(V1.0.0) initialized successfully.
[23096] I/mcf.trans: MCF protocol started successfully.

msh >mcf_recv
waiting for data receive... 
receive request data: MCF send packet data!
send response data: MCF receive and response data!
```

###   发送方设备示例

示例流程：

- 注册链路层层通讯设备
- 添加链路层 ARP 列表
- 初始化和启动 MCF
- 创建 D2D 数据会话
- 创建 PKT 包对象并赋值发送数据
- 指定超时时间内发送 PKT 包
- 阻塞等待接收响应的数据
- 关闭当前会话

请求方设备，先通过执行 `mcf_init`  命令初始化和启动 MCF 软件包，然后执行 `mcf_send` 命令发送请求数据，并阻塞等待响应数据，接收到响应数据后关闭当前会话。

示例日志如下所示：

```shell
msh >mcf_init uart2 1 2
[23083] I/mcf.link: MCF link port(1) register successfully.
[23090] I/mcf.trans: MCF(V1.0.0) initialized successfully.
[23096] I/mcf.trans: MCF protocol started successfully.

msh >mcf_send
send request data: MCF send packet data!
receive response data: MCF receive and response data!
```
