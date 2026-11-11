# Qt NI-VISA VXI-11 客户端

这是一个 Qt Widgets 客户端，通过 NI-VISA 的 `TCPIP...::INSTR` 资源连接 VXI-11 设备。支持连接、断开、写入、读取以及写入后读取的查询。VISA 调用在工作线程中执行，超时期间窗口仍可响应。

## 准备

1. 安装 [NI-VISA（Full）](https://www.ni.com/en/support/downloads/drivers/download.ni-visa.html)，并确认下载项的 Application Bitness 包含程序所需的位数。32 位程序需要 32 位 VISA 组件，64 位程序需要 64 位组件。`NI System Configuration` 只提供系统配置相关组件，安装它不会提供本工程所需的 VISA 库。本项目运行时加载 `visa32.dll` 或 `visa64.dll`，编译时不需要 `visa.h` 和导入库。如果同时安装了多家 VISA 实现，请确保 NI-VISA 是系统的主 VISA 实现。
2. 在 Qt Creator 打开 `qt-ni-visa.pro`，选择 Windows Desktop Qt 5.15 或 Qt 6 套件并构建。
3. 确认服务器上的 `rpcbind` 正在运行，VXI-11 服务已注册，并允许客户端访问其 RPC 端口。服务端与客户端应能互相访问。

## 连接 python-vxi11-server

在资源名中填写 `TCPIP0::<服务器IP>::<设备名>::INSTR`。例如 README 中的时钟示例调用 `add_device_handler(TimeDevice, 'inst1')`，所以可填写：

```text
TCPIP0::192.168.1.50::inst1::INSTR
```

将 IP 换为实际服务器地址。连接后直接点 **读取**，时钟示例的 `device_read` 会返回当前时间，不需要先写入命令。其他设备按服务端实现输入命令，使用 **写入** 或 **查询（写入后读取）**。如设备要求命令以 LF 结束，勾选“写入时追加换行符”。

读取按 1000 字节分块，直到 VXI-11 消息结束；最多显示 1 MiB，超过时会提示响应可能还有剩余数据。文本响应按 UTF-8 显示，二进制数据以十六进制显示。

## 常见问题

- **无法加载 VISA 库**：检查 NI-VISA 是否安装，以及程序与运行库位数是否一致。64 位 Windows 上的 32 位程序应能找到 `C:\Windows\SysWOW64\visa32.dll`。
- **viOpen 找不到资源或超时**：检查 IP、设备名、服务端 `rpcbind` 与防火墙。设备名必须与服务端 `add_device_handler` 注册的名称一致；这里使用 VXI-11 `INSTR`，不是原始 TCP `SOCKET`。
- **读取超时**：确认服务端的 `device_read` 会返回数据；按需增加界面中的超时时间。

NI 文档：[VISA 资源与 LAN 仪器](https://knowledge.ni.com/KnowledgeArticleDetails?id=kA03q000000x3gXCAQ)、[viRead](https://www.ni.com/docs/en-US/bundle/ni-visa-api-ref/page/ni-visa-api-ref/viread.html)。[python-vxi11-server 示例](https://github.com/coburnw/python-vxi11-server)提供了 `inst1` 时钟设备。NI 记录了 2026.x 的 VXI-11 大块读取问题，因此这里采用不超过 1000 字节的读取块：[NI 说明](https://knowledge.ni.com/KnowledgeArticleDetails?id=kA0VU000000F5CH0A0&l=en-US)。
