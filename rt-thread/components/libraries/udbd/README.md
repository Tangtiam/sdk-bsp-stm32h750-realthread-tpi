# udb 设备端服务

## 1、介绍

UDB 是一款轻量级嵌入式调试桥工具，其设计思虑来源与 Android 的 ADB，但 ADB 对嵌入式设备的有一定资源消耗，不支持串口链路，所以开发此工具
通过 UDB 可以将嵌入式设备与 PC 端打通，无需额外的硬件及软件工具，即可实现对嵌入式设备的完全调试

udbd 是 UDB 调试工具的设备端服务软件，为设备提供了 udb 服务的支撑。

### 1.1 依赖

- RT-Thread 4.0+
- uPRC 软件包
- MCF 软件包

### 1.2 许可证

udbd 软件包遵循 Apache-2.0 许可，详见 LICENSE 文件。

## 3、使用方式

udbd 服务使用静态注册的方式，使用前需完成 urpc 及 mcf 软件包的初始化即可

## 4、注意事项

打库时需注意使用动态服务注册的模式，否则服务可能无法找到

## 5、联系方式

- 维护：armink@rt-thread.com
- 主页：https://github.com/RT-Thread-packages/udb