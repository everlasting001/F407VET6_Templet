# 原理图阅读指南

> 原始文档: `.claude/rules/schematic-reading-guide.md`

## STM32F407VET6 原理图关键部分

### 电源树
- **VDD**: 3.3V 主供电 (多组独立引脚)
- **VDDA**: 模拟部分 3.3V
- **VREF+**: ADC 参考电压 (可接 VDDA)
- **VBAT**: RTC 备用电池 (1.65-3.6V)
- **VCAP_1/2**: 内核稳压器去耦电容

### 时钟源
- **HSE**: 8MHz 外部晶振 (或 25MHz)
- **LSE**: 32.768kHz RTC 晶振
- **HSI**: 16MHz 内部 RC
- **PLL**: 倍频至 168MHz

### 调试接口
- **SWD**: SWCLK (PA14), SWDIO (PA13)
- **NRST**: 复位引脚 (低电平复位)

## 常见检查点
- 去耦电容是否靠近 MCU 电源引脚
- 晶振负载电容是否匹配
- 启动模式引脚 (BOOT0/BOOT1) 配置
- 各外设引脚复用功能是否正确

> 完整原理图请参阅 `.claude/docs/schematics/FK407M3-VET6 原理图.pdf`
