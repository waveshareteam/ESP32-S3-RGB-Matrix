# ESP32-S3 RGB Matrix BSP 说明

IDF版本: v5.5.2
使用的组件版本: lvgl v9.3.0 
              esp-hub75 v0.3.5
              esp_lvgl_port v2.0.0
              esp_codec_dev v1.3.1
              waveshare/pcf85063a v*
              waveshare/qmi8658 v1.0.1
              waveshare/shtc3 v1.4.0
              waveshare/button v 4.1.3

本组件是工程里连接 `应用层` 与 `HUB75 驱动` 的板级适配层（BSP）。  
核心目标是把显示、I2C、音频、存储、WiFi 等底层能力统一为稳定 API，并让上层示例按统一流程调用。

---

## 1. 分层架构

工程整体建议按三层理解：

1. **BSP 层（Board Support Package）**
   - 路径：`components/bsp/esp32_s3_matrix`
   - 职责：封装硬件相关初始化与底层资源管理。
   - 对外接口：`bsp_display_*`、`bsp_i2c_*`、`bsp_audio_*`、`bsp_sdcard_*`、`bsp_wifi_*`。

2. **Middleware 层**
   - 路径：`components/Middleware/*`
   - 职责：把业务功能封装为“显式 init + 纯读写接口”的中间层服务。
   - 模块：`middle_sensor`、`middle_rtc`、`middle_sdcard`、`middle_wifi`、`middle_audio`。

3. **Example/UI 层**
   - 路径：`main/examples/*`
   - 职责：页面布局、状态刷新、示例演示，不直接操作硬件寄存器。

### 1.1 典型调用链

```text
Example/UI
  -> Middleware (可选)
    -> BSP
      -> hub75_bridge
        -> esphome__esp-hub75 (Hub75Driver)
          -> LCD_CAM + GDMA (ESP32-S3)
            -> HUB75 面板
```

---

## 2. HUB75 显示链路

本工程不是手搓 GPIO bit-bang，而是通过 `esphome__esp-hub75` 走硬件并行外设，降低 CPU 抖动并提升刷新稳定性。

### 2.1 参数映射（menuconfig -> 驱动）

`hub75_bridge.cpp` 负责把 `sdkconfig` 宏转换成 `Hub75Config`，关键映射包括：

- 面板尺寸：`CONFIG_HUB75_PANEL_WIDTH/HEIGHT`
- 扫描线型：`CONFIG_HUB75_WIRING_*`
- 驱动芯片：`CONFIG_HUB75_DRIVER_*`
- 布局与旋转：`CONFIG_HUB75_LAYOUT_*`、`CONFIG_HUB75_ROTATE_*`
- 引脚定义：`CONFIG_HUB75_PIN_*`
- 时钟/刷新：`CONFIG_HUB75_CLK_*`、`CONFIG_HUB75_MIN_REFRESH_RATE`
- 亮度与功能：`CONFIG_HUB75_BRIGHTNESS`、`CONFIG_HUB75_DOUBLE_BUFFER`、`CONFIG_HUB75_CLK_PHASE_INVERTED`

### 2.2 刷新数据路径

1. LVGL 触发 `flush_cb()`
2. `flush_cb()` 调 `hub75_bridge_draw()`
3. `Hub75Driver` 将像素转为底层时序缓冲
4. LCD_CAM + GDMA 按并行时序持续输出到 HUB75

---

### 2.3 UI排版机制

`main/examples` 下各示例基本遵循同一套 UI 组织规律，便于复用与横向对比：

1. **公共骨架优先**
   - 先调用 `matrix_example_ui()` 与 `matrix_example_ui_init_screen()` 生成基础页面对象。
   - 基础对象统一提供 `line1~line4` 文本缓冲和对应 label。

2. **顶部状态 + 内容分区**
   - `line1` 常作为标题/状态（模块名与颜色状态）。
   - `line2~line4` 常用于主数据项（温湿度、姿态、时间、网络状态等）。
   - 超出四行时再创建额外 label（例如 RTC 的 `line5`、QMI 的 gyro 三行）。

3. **布局模式统一**
   - 单值类页面常用 `LV_ALIGN_OUT_BOTTOM_MID` 纵向居中堆叠。
   - 数据列表类页面常用左对齐 + 固定左边距（便于数字列对齐）。
   - 通过 `lv_obj_clear_flag(..., LV_OBJ_FLAG_HIDDEN)` 控制基础行是否可见。

4. **状态与渲染分离**
   - 示例普遍使用 `state` 结构体存 `init/read` 结果与业务数据。
   - `*_ui_apply()` 只做“状态 -> 文本/颜色”映射，不直接做硬件访问。
   - 定时回调（`matrix_example_ui_install_timer(...)`）负责采样并刷新状态。

5. **刷新节奏建议**
   - 传感器/网络信息通常用 `100~1000ms` 刷新周期。
   - 不建议用过小周期去“硬刷 UI”，避免无意义占用 CPU 与 I2C 总线。

## 3. HUB75 接口要点

### 3.1 信号分组

- **颜色数据线**
  - `R1/G1/B1`：上半区数据
  - `R2/G2/B2`：下半区数据
- **行地址线**
  - `A/B/C/D/E`：选择当前扫描行（或行组）
- **控制线**
  - `CLK`：移位时钟，逐 bit 推入
  - `LAT`：锁存，把移位寄存器内容切到输出寄存器
  - `OE`：输出使能（常见为低有效），决定该行点亮窗口

### 3.2 一行显示的基本节拍

```text
选择行地址(A~E)
  -> 输出 RGB bit 流并打 CLK
    -> LAT 锁存
      -> OE 打开一段时间
        -> OE 关闭，切下一行
```

### 3.3 扫描与亮度的关系

- 面板一般是行扫描（如 1/16、1/32）。
- 每帧会轮询多行，视觉上由人眼暂留合成整屏。
- 亮度由 `OE` 有效时间与位平面权重共同决定（驱动内部调度）。

---

## 4. 显示映射模式（多面板）

`esp32_s3_matrix.c` 中默认映射模式与 `CONFIG_HUB75_LAYOUT_COLS` 相关：

- `BSP_DISPLAY_MAP_EXTEND`
  - 逻辑宽度按 `panel_width * layout_cols` 扩展
  - 适合拼接成一整块大画布
- `BSP_DISPLAY_MAP_MIRROR`
  - 逻辑宽度按单屏宽度
  - 同一画面镜像/复制到多列面板

可在显示启动前通过 `bsp_display_set_map_mode()` 修改。

---

## 5. 与中间层配合建议

- 示例层不要直接访问底层设备句柄，优先调用 `middle_*`。
- 中间层使用显式初始化，失败状态保持可观测，避免隐式重试引发行为不确定。
- UI 定时器用于刷新显示，不作为硬件时间基准。

---

## 6. 常见问题速查

### Q1: 为什么刷新频率很高但时间显示还是有抖动？

- UI 回调频率只影响“多久刷新一次文字”，不改变 RTC 晶振精度。
- 实际走时基准来自 RTC 外设。

### Q2: 为什么多屏拼接时内容重复？

- 默认可能处于 `MIRROR` 模式，请检查 `display_map_mode` 或在启动前切换为 `EXTEND`。

### Q3: 为什么某些 HUB75 面板颜色或行序异常？

- 优先检查 `CONFIG_HUB75_WIRING_*` 与 `CONFIG_HUB75_DRIVER_*` 是否匹配面板。
- 其次检查引脚与时钟配置。

---

## 7. 关键文件索引

- BSP 主实现：`esp32_s3_matrix.c`
- HUB75 配置桥接：`hub75_bridge.cpp`
- BSP 公开接口：`include/bsp/esp32_s3_matrix.h`
- 中间层：`components/Middleware/*`
- 示例入口：`main/examples/*`
