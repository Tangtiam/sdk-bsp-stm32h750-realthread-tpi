# MCF 多机通讯协议

## 1、介绍

MCF （Multi-machine Communication Framework），即多机通讯协议框架，是 RT-Thread 自主研发的，支持通过指定数据格式在多个 MCU 之间进行数据传输的通讯协议。

MCF 框架目前实现的功能如下：

- 面向无连接的设备通讯方式，支持正常数据请求和响应；
- 支持异步请求处理方式，多线程数据收发功能；
- 支持请求数据 ACK 应答机制，方便状态同步；
- 支持链路帧 CRC 校验功能；
- 支持数据错误重传功能；
- 支持数据广播发送功能；
- 支持数据加密压缩功能；

### 1.1 目录结构

MCF 软件包目录结构如下所示：

```
MCF
├───docs 
│   └───figures                     // 文档使用图片
├───inc                             // 头文件目录
├───src                             // 源文件目录
│   ├───link                        // 数据链路层相关目录
│   ├───trans                       // 数据传输层相关目录
│   └───utils                       // 通用工具接口目录
│   LICENSE                         // 软件包许可证
│   README.md                       // 软件包使用说明
└───SConscript                      // 工程构建脚本
```

### 1.2 许可证

MCF 软件包遵循 Apache-2.0 许可，详见 LICENSE 文件。

###  1.3 依赖

- RT-Thread 3.0+

## 2、获取软件包

使用 MCF 软件包需要在 RT-Thread 的包管理中选中它，具体路径如下：

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

配置完成后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到工程中。

### 3、使用 MCF 软件包

在使用 MCF 软件包之前，请查看软件包详细介绍，帮助你了解更多 MCF 相关功能，并查看 API 手册和示例文档，了解 MCF 软件包具体使用方式。

- 软件包详细介绍，请参考 [软件包介绍](docs/introduction.md)

- 软件包使用说明，请参考 [软件包使用说明](docs/user-guide.md)

- 软件包完整的 API 文档，请参考 [API 手册](docs/api.md)

- 详细的示例介绍，请参考 [示例文档](docs/samples.md)

## 注意事项

- 合理配置数据传输最大长度限制；

  数据长度限制设置越大，软件包资源占用越大，默认为 1024 bytes，可根据实际需求配置。

- 如开启请求自动响应功能，可根据需求合理配置请求线程池数量和线程池线程最大栈大小；

- 串口链路层数据 CRC16 校验功能为可配置，关闭可提高多机通讯中数据传输速率；


## 5、联系方式

- 维护：chenyong@rt-thread.com
- 主页：https://github.com/RT-Thread-packages/mcf
