# VOFA+ 通用可配置协议引擎

这是一个面向 VOFA+ 的 Qt 5.14.2 / MSVC2017 协议引擎。DLL 在运行时读取
`configurable_engine.json`，无需为每次字段偏移、字节序或缩放变化重新编译。

## 已支持

- 固定帧头与可选固定帧尾；
- 固定帧长、长度字段或帧尾定帧；
- `int8/16/32/64`、`uint8/16/32/64`、`float32/64`；
- 每字段独立设置大端/小端、比例和偏移；
- `sum8`、`xor8`、`crc16_modbus` 校验；
- 噪声跳过、粘包、半包保留和配置热重载；
- JSON 配置合法性和字段越界检查。

当前版本使用 JSON。Qt Core 原生支持 JSON；YAML 需要增加第三方依赖，建议先将 YAML
转换为 JSON，避免给 VOFA+ 插件部署额外 DLL。

## 编译

构建脚本不包含开发者本机的绝对路径。通过 `-QtRoot` 指定对应架构的 Qt Kit
目录，或者设置 `QT_ROOT` 环境变量；如果 `qmake.exe` 已在 `PATH` 中，也可以省略。
脚本优先查找 MSVC v141，找不到时回退到 ABI 兼容的 v142。

在 PowerShell 中运行：

```powershell
.\build.ps1 -Architecture x64 -QtRoot "<Qt安装目录>\5.14.2\msvc2017_64" -RunTests
```

也可以先设置环境变量：

```powershell
$env:QT_ROOT = "<Qt安装目录>\5.14.2\msvc2017_64"
.\build.ps1 -Architecture x64 -RunTests
```

产物位于 `dist\ConfigurableEngine.dll`。VOFA+ 是 32 位时改用
`-Architecture x86`，DLL 位数必须与 VOFA+ 一致。

仓库中附带的预编译 DLL 是 **x64 Release**，使用 Qt 5.14.2
`msvc2017_64` Kit 和 MSVC 14.29（VS2019/v142）构建。MSVC 2017 与 2019
二进制兼容，但如果目标电脑没有
`VCRUNTIME140_1.dll`，请安装 Microsoft VC++ 2015–2022 x64 运行库，或者补全 v141
编译器后重新构建。

## 安装

关闭 VOFA+，然后执行：

```powershell
.\install.ps1 -VofaDirectory "<VOFA+安装目录>"
```

也可以手动把下面三个文件复制到 `VOFA+\plugins\dataengines`：

- `ConfigurableEngine.dll`
- `configurable_engine.json`
- `ConfigurableEngine.json`（VOFA+ 协议提示的多语言说明文件）

注意两个 JSON 用途不同：`configurable_engine.json` 是引擎读取的帧格式配置；
`ConfigurableEngine.json` 是 VOFA+ 界面读取的协议说明。缺少后者会提示
“找不到对应语言的协议文档”，但不影响实际数据解析。

重启后选择 `ConfigurableEngine` 协议引擎。插件每秒检查一次 JSON 的修改时间，修改配置后
通常不需要重新编译；若 VOFA+ 已缓存较长的半帧，建议重新连接数据源。

也可以通过环境变量指定配置文件的绝对路径：

```powershell
$env:VOFA_PROTOCOL_CONFIG = "<协议文件目录>\sensor.json"
```

## JSON 格式

`framing` 支持三种模式，优先级依次为：

1. `fixed_length`：总帧长，包括帧头、数据、校验和帧尾；
2. `length_field`：`总帧长 = 长度字段值 + adjust`；
3. `tail`：从帧头开始搜索第一个帧尾。

十六进制帧头/帧尾可写为 `"AA 55"`、`"0xAA,0x55"` 或 `"AA55"`。
所有 `offset` 都从帧头第一个字节开始，以 0 为起点。

字段定义：

```json
{
  "name": "temperature",
  "offset": 2,
  "type": "int16",
  "endian": "little",
  "scale": 0.01,
  "bias": 0.0
}
```

输出通道顺序就是 `fields` 数组顺序，`name` 只用于配置检查和诊断。

长度字段示例：

```json
"length_field": {
  "offset": 2,
  "size": 2,
  "endian": "little",
  "adjust": 4
}
```

例如长度字段值只表示“负载长度”，而帧头、长度字段和 CRC 共占 6 字节，则应将
`adjust` 设为 6。

校验范围采用左闭右开区间 `[range_start, range_end)`。省略 `range_end` 时默认校验到
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

## 完整示例一：以 `0xA0` 开头、以 `0xA1` 结尾

假设设备发送的完整帧为：

```text
A0 2E FB CD 8B 01 00 A1
│  └─温度─┘ └──压力───┘ │
帧头                    帧尾
```

字段含义如下：

| 偏移 | 长度 | 内容 | 解释 |
|---:|---:|---|---|
| 0 | 1 | `A0` | 帧头 |
| 1 | 2 | `2E FB` | 小端 `int16`，原始值 -1234，乘 0.01 后为 -12.34 °C |
| 3 | 4 | `CD 8B 01 00` | 小端 `uint32`，值为 101325 Pa |
| 7 | 1 | `A1` | 帧尾 |

将 `configurable_engine.json` 修改为：

```json
{
  "name": "A0A1Sensor",
  "framing": {
    "header": "A0",
    "tail": "A1",
    "minimum_length": 8,
    "maximum_length": 8
  },
  "fields": [
    {
      "name": "temperature_c",
      "offset": 1,
      "type": "int16",
      "endian": "little",
      "scale": 0.01,
      "bias": 0.0
    },
    {
      "name": "pressure_pa",
      "offset": 3,
      "type": "uint32",
      "endian": "little"
    }
  ],
  "checksum": {
    "type": "none"
  }
}
```

注意：这里必须删除原示例中的 `fixed_length: 14`。定帧方式有优先级；只要
`fixed_length` 仍然存在，引擎就会先按固定长度读取，而不会搜索 `A1`。

工程中已经提供可直接复制的
[`examples/a0_a1_tail.json`](examples/a0_a1_tail.json)。复制到 VOFA+ 后应改名为
`configurable_engine.json`。

### 如果负载中可能出现 `0xA1`

“搜索第一个 `A1`”只适用于负载保证不会包含 `A1` 的协议。二进制整数或浮点数的任意一个
字节都可能恰好等于 `A1`，这会造成提前截帧。

如果本例始终是 8 字节，更稳妥的写法是增加：

```json
"fixed_length": 8
```

此时引擎固定读取 8 字节，再检查最后一个字节是否为 `A1`，负载中间出现 `A1` 不受影响。
完整配置见 [`examples/a0_a1_fixed.json`](examples/a0_a1_fixed.json)。

如果帧长可变，而且负载可能出现 `A1`，协议本身必须再提供以下机制之一：

- 长度字段；
- 转义规则，例如把负载中的 `A1` 编码成其他字节序列；
- COBS、SLIP 等明确的封装规则。

当前引擎支持长度字段，但尚未支持字节转义、COBS 和 SLIP。

## 完整示例二：只有一个 `float32` 数据

帧定义：`A0 + 小端 float32 + A1`，总长固定为 6 字节。例如 `12.5f` 的帧为：

```text
A0 00 00 48 41 A1
```

对应配置：

```json
{
  "name": "SingleFloat",
  "framing": {
    "header": "A0",
    "tail": "A1",
    "fixed_length": 6,
    "minimum_length": 6,
    "maximum_length": 6
  },
  "fields": [
    {
      "name": "value",
      "offset": 1,
      "type": "float32",
      "endian": "little"
    }
  ],
  "checksum": {"type": "none"}
}
```

VOFA+ 中会得到一个通道，数值为 `12.5`。

## 完整示例三：多个 16 位通道并带 Sum8

帧格式定义为：

```text
A0 CH0_L CH0_H CH1_L CH1_H SUM8 A1
```

其中 `SUM8` 是从帧头开始、到校验字节之前所有字节相加后保留低 8 位。配置为：

```json
{
  "name": "TwoChannelsWithSum8",
  "framing": {
    "header": "A0",
    "tail": "A1",
    "fixed_length": 7,
    "minimum_length": 7,
    "maximum_length": 7
  },
  "fields": [
    {"name": "channel_0", "offset": 1, "type": "int16", "endian": "little"},
    {"name": "channel_1", "offset": 3, "type": "int16", "endian": "little"}
  ],
  "checksum": {
    "type": "sum8",
    "offset": 5,
    "range_start": 0,
    "range_end": 5
  }
}
```

例如通道值分别为 1 和 2 时，帧为：

```text
A0 01 00 02 00 A3 A1
```

因为 `(0xA0 + 0x01 + 0x00 + 0x02 + 0x00) & 0xFF = 0xA3`。

## 如何根据自己的协议填写字段

1. 把完整帧的每个字节从 0 开始编号，帧头也计入偏移。
2. 确定一种可靠的定帧方式：优先使用固定长度或长度字段。
3. 为每个要显示的通道填写 `offset`、`type` 和 `endian`。
4. 如果实际值需要换算，使用 `实际值 = 原始值 × scale + bias`。
5. 有校验时明确校验字段偏移，以及参与计算的左闭右开区间。
6. 用一帧已知数据手算结果，再与 VOFA+ 显示值对比。

常见字段类型及字节数：

| `type` | 字节数 | 含义 |
|---|---:|---|
| `int8` / `uint8` | 1 | 8 位有符号/无符号整数 |
| `int16` / `uint16` | 2 | 16 位整数 |
| `int32` / `uint32` | 4 | 32 位整数 |
| `float32` | 4 | IEEE-754 单精度浮点数 |
| `int64` / `uint64` | 8 | 64 位整数，输出到 VOFA+ 时可能损失精度 |
| `float64` | 8 | IEEE-754 双精度，输出到 VOFA+ 时转为单精度 |

`little` 表示低字节在前，例如数值 `0x1234` 发送为 `34 12`；`big` 表示高字节在前，
发送为 `12 34`。

## 用 CSV 验证原始 `configurable_engine.json`

原始配置会输出三个通道，顺序严格等于 `fields` 数组顺序：

1. `temperature_c`：`int16` 小端，缩放系数 0.01；
2. `pressure_pa`：`uint32` 小端；
3. `voltage_v`：`float32` 小端。

提供的 [`examples/original_config_vofa_source.csv`](examples/original_config_vofa_source.csv)
包含 400 组数据，无表头，每行都是：

```text
temperature_c,pressure_pa,voltage_v
```

例如第一行：

```text
25.00,101714,3.600000
```

在 VOFA+ 的 Demo/CSV 数据源中载入该文件时，应使用 FireWater/CSV 方式，并把采样间隔
设为 20 ms。它会直接产生三个通道，可用于检查波形控件、通道顺序和预期数值。

需要注意：VOFA+ 载入通道 CSV 时，CSV 数值已经是“解析结果”，不会先转换成
`AA 55 ... 0D 0A` 二进制帧，因此这种载入方式**不能证明自定义 DLL 完成了拆帧和字段解析**。

要验证 DLL 本身，请让 VOFA+ 建立 TCP Server 数据接口、选择 `ConfigurableEngine`，然后运行：

```powershell
python .\tools\replay_csv_tcp.py .\examples\original_config_vofa_source.csv `
  --host 127.0.0.1 --port 1347 --interval-ms 20
```

脚本会把每一行编码为：

```text
AA 55 + int16温度原始值 + uint32压力 + float32电压 + 0D 0A
```

可先执行离线检查，不连接 VOFA+：

```powershell
python .\tools\replay_csv_tcp.py --dry-run
```

CSV 路径现在是可选参数；不填写时，脚本自动使用工程自带的
`examples\original_config_vofa_source.csv`。因此，在 VOFA+ TCP Server 已启动且端口为
1347 时，也可以直接运行 `python .\tools\replay_csv_tcp.py`。

[`examples/original_config_frame_audit.csv`](examples/original_config_frame_audit.csv) 带表头，
额外列出了每行对应的温度原始整数和完整帧十六进制，供人工核对；不要把这份审计表作为
VOFA+ CSV 数据源。`examples/original_config_frames.bin` 则是全部 400 帧首尾相接的原始字节流。

### 数据栏有 I0/I1/I2，但波形图没有曲线

这表示协议引擎已经解析成功，只是波形控件尚未绑定通道：

1. 在中央画布添加一个波形图控件；
2. 在波形图内部右键，进入 Y 轴/绑定数据菜单；
3. 勾选 `I0`、`I1`、`I2`；
4. 将采样缓冲区的绿色位置条拖到最右端，显示最新数据；
5. 点击波形图的 `Auto`，自动调整 Y 轴范围；
6. 建议将 Δt 设置为 20 ms，与发送脚本的 `--interval-ms 20` 一致。

压力约为 101000 Pa，而温度约为 25、电压约为 3.3，三者量级差异很大。如果同时绑定在
同一根 Y 轴上，温度和电压曲线会被压力曲线压缩得接近直线。可分别放到三个波形图，或在
数据栏为压力设置比例系数 `0.001`，以 kPa 显示。

## 限制

- 当前不解析图片通道；
- 当前不实现转义、COBS/SLIP、加密、压缩和跨帧状态机；
- `QVector<float>` 是 VOFA+ 接口限制，64 位整数和 `float64` 最终会转为单精度；
- 同一个 DLL 同时只加载一份配置。需要并行使用多套协议时，应生成不同插件类/IID，或扩展
  配置以按帧头分派多个子协议。

## 接口来源

`src/dataengineinterface.h` 按 VOFA+ 官方开源 Vodka 插件接口整理：
<https://github.com/je00/Vodka/tree/master/dataengines/shared>。上游仓库采用 MIT License。
