# VOFA+ Configurable Engine

一个适用于 VOFA+ 的通用二进制协议引擎。通过 JSON 描述帧头、帧长、字段类型、
字节序、缩放和校验规则，无需为每种设备协议重新编译 DLL。

> 本项目是社区实现，与 VOFA+ 官方无隶属关系。使用前请确认插件架构与 VOFA+ 位数一致。

## 功能

- 固定帧头和可选固定帧尾；
- 固定帧长、长度字段或帧尾定帧；
- `int8/16/32/64`、`uint8/16/32/64`、`float32/64` 字段；
- 每个字段独立配置大小端、比例系数和偏移量；
- `sum8`、`xor8`、`crc16_modbus` 校验；
- 支持噪声跳过、粘包、半包保留和配置热重载；
- 配置合法性、字段越界和帧长度检查。

当前配置格式为 JSON。Qt Core 可以直接解析 JSON；支持 YAML 会引入额外运行时依赖，
因此暂未内置。

## 兼容性

| 项目 | 当前版本 |
|---|---|
| 操作系统 | Windows |
| Qt | 5.14.2 |
| 编译器 ABI | MSVC2017 兼容 |
| 仓库预编译 DLL | x64 Release |
| 已验证的 VOFA+ 版本 | 1.3.10 |

预编译 DLL 使用 Qt 5.14.2 `msvc2017_64` Kit 和 MSVC 14.29 构建。如果系统缺少
`VCRUNTIME140_1.dll`，请安装 Microsoft Visual C++ 2015–2022 x64 运行库，或使用本机
工具链重新构建。

## 快速开始

### 1. 安装插件

关闭 VOFA+，把以下文件复制到 `VOFA+\plugins\dataengines`：

- `dist\ConfigurableEngine.dll`
- `configurable_engine.json`
- `ConfigurableEngine.json`

也可以使用安装脚本：

```powershell
.\install.ps1 -VofaDirectory "<VOFA+安装目录>"
```

两个 JSON 文件用途不同：

- `configurable_engine.json`：描述设备的二进制帧格式；
- `ConfigurableEngine.json`：供 VOFA+ 显示协议引擎说明。

### 2. 配置协议

仓库默认配置解析一个 14 字节固定长度帧：

```text
AA 55 | int16 temperature | uint32 pressure | float32 voltage | 0D 0A
```

对应配置位于 [`configurable_engine.json`](configurable_engine.json)。修改该文件即可适配
自己的协议。插件每秒检查一次文件修改时间，通常无需重新编译；修改后如果仍残留旧半帧，
请重新连接 VOFA+ 数据源。

也可以通过环境变量加载其他位置的配置：

```powershell
$env:VOFA_PROTOCOL_CONFIG = "<协议文件目录>\sensor.json"
```

### 3. 在 VOFA+ 中使用

重启 VOFA+，为数据源选择 `ConfigurableEngine` 协议引擎。引擎按照 `fields` 数组的顺序
向 VOFA+ 输出通道数据。

## 配置参考

### 定帧规则

`framing` 支持三种定帧方式，优先级如下：

1. `fixed_length`：完整帧的固定字节数；
2. `length_field`：`完整帧长 = 长度字段值 + adjust`；
3. `tail`：从帧头开始查找帧尾。

优先使用固定帧长或长度字段。仅依靠帧尾定帧时，负载中出现与帧尾相同的字节可能导致
提前截帧。

帧头和帧尾支持以下十六进制写法：

```json
"header": "AA 55"
```

```json
"header": "0xAA,0x55"
```

```json
"header": "AA55"
```

长度字段示例：

```json
"length_field": {
  "offset": 2,
  "size": 2,
  "endian": "little",
  "adjust": 6
}
```

如果长度字段只表示负载长度，而帧头、长度字段、校验等额外占用 6 字节，则将
`adjust` 设置为 6。

### 字段规则

所有 `offset` 都从帧头第一个字节开始，以 0 为起点：

```json
{
  "name": "temperature_c",
  "offset": 2,
  "type": "int16",
  "endian": "little",
  "scale": 0.01,
  "bias": 0.0
}
```

换算公式为：

```text
输出值 = 原始值 × scale + bias
```

| `type` | 字节数 | 说明 |
|---|---:|---|
| `int8` / `uint8` | 1 | 8 位整数 |
| `int16` / `uint16` | 2 | 16 位整数 |
| `int32` / `uint32` | 4 | 32 位整数 |
| `float32` | 4 | IEEE-754 单精度浮点数 |
| `int64` / `uint64` | 8 | 64 位整数，输出时可能损失精度 |
| `float64` | 8 | 双精度输入，输出到 VOFA+ 时转为单精度 |

`little` 表示低字节在前，`big` 表示高字节在前。字段输出顺序就是 `fields` 数组顺序；
`name` 用于配置检查和诊断。

### 校验规则

支持的 `checksum.type`：

- `none`
- `sum8`
- `xor8`
- `crc16_modbus`

校验范围采用左闭右开区间 `[range_start, range_end)`。省略 `range_end` 时，默认校验到
校验字段之前：

```json
"checksum": {
  "type": "crc16_modbus",
  "offset": 12,
  "range_start": 0,
  "range_end": 12,
  "endian": "little"
}
```

更多配置可参考 [`examples`](examples) 目录。

## 构建

### 环境要求

- Qt 5.14.2，对应目标架构的 `msvc2017` Kit；
- Visual Studio 2017 或 ABI 兼容的 Visual Studio 2019 C++ 工具链；
- PowerShell 5.1 或更高版本。

通过 `-QtRoot` 指定 Qt Kit 目录：

```powershell
.\build.ps1 -Architecture x64 -QtRoot "<Qt安装目录>\5.14.2\msvc2017_64" -RunTests
```

也可以设置环境变量，或者将 `qmake.exe` 加入 `PATH`：

```powershell
$env:QT_ROOT = "<Qt安装目录>\5.14.2\msvc2017_64"
.\build.ps1 -Architecture x64 -RunTests
```

编译结果位于 `dist\ConfigurableEngine.dll`。如果 VOFA+ 是 32 位，请使用
`-Architecture x86`；插件位数必须与宿主程序一致。

## 验证

仓库提供 400 组示例数据和 TCP 回放工具：

- `examples/original_config_vofa_source.csv`：三个解析后通道，无表头；
- `examples/original_config_frame_audit.csv`：带原始值和帧十六进制的审计表；
- `examples/original_config_frames.bin`：连续的原始二进制帧。

离线验证 CSV 编码：

```powershell
python .\tools\replay_csv_tcp.py --dry-run
```

验证 DLL 时，在 VOFA+ 中建立 TCP Server 数据接口并选择 `ConfigurableEngine`，然后运行：

```powershell
python .\tools\replay_csv_tcp.py .\examples\original_config_vofa_source.csv `
  --host 127.0.0.1 --port 1347 --interval-ms 20
```

直接把通道 CSV 载入 VOFA+ 只能验证通道数据和界面，不会经过自定义 DLL；TCP 回放会把
每一行重新编码为二进制帧，可用于验证拆帧和字段解析。

## 项目结构

```text
├─ src/                         插件和协议解析源码
├─ tests/                       解析器测试与插件冒烟测试
├─ tools/                       CSV 到 TCP 二进制帧回放工具
├─ examples/                    协议配置与测试数据
├─ dist/ConfigurableEngine.dll  预编译 x64 插件
├─ configurable_engine.json     默认帧格式配置
├─ ConfigurableEngine.json      VOFA+ 协议说明
├─ build.ps1                    构建脚本
└─ install.ps1                  本地安装脚本
```

## 已知限制

- 不解析图片通道；
- 不支持转义、COBS、SLIP、加密、压缩和跨帧状态机；
- VOFA+ 接口输出类型为 `QVector<float>`，64 位整数和 `float64` 可能损失精度；
- 单个 DLL 实例只加载一份配置。并行使用多套协议需要按帧头分派子协议，或构建具有不同
  插件类和 IID 的 DLL。

## 参与贡献

欢迎提交 Issue 或 Pull Request。提交代码前建议：

1. 使用最小 JSON 和十六进制帧描述问题；
2. 为新增定帧、字段或校验行为补充测试；
3. 运行 `build.ps1 -RunTests`；
4. 不要提交本机绝对路径、账号、令牌或设备隐私数据。

## 许可证与致谢

本项目采用 [MIT License](LICENSE)。

`src/dataengineinterface.h` 根据 VOFA+ 开源 Vodka 插件接口整理：
[je00/Vodka](https://github.com/je00/Vodka/tree/master/dataengines/shared)。上游代码采用
MIT License。
