# Claude Code 嵌入式工程师助手 - 快速开始

> 🤖 **我现在是你的 STM32F407VET6 专业嵌入式工程师助手**
> 
> 我拥有完整的硬件知识库、代码审查规范、以及自动化开发工具链。

## ⚡ 3 分钟快速上手

### 第一步：我已经准备好的
- ✅ 嵌入式开发最佳实践库
- ✅ HAL 库使用规范
- ✅ 中断安全检查清单
- ✅ 电机驱动完整指南
- ✅ 原理图和 PCB 阅读指南
- ✅ 自动编译验证 hooks
- ✅ 代码安全审查工具

### 第二步：上传你的硬件文件（可选但推荐）

放在 `.claude/docs/` 目录下：

```bash
.claude/docs/
├── datasheets/
│   ├── STM32F407VET6.pdf    # MCU参考手册
│   ├── motor-model.pdf       # 你的电机型号
│   └── driver-ic.pdf         # 驱动芯片
├── schematics/
│   ├── main-schematic.pdf
│   ├── motor-driver-board.pdf
│   └── power-supply.pdf
└── pcb/
    └── layout.pdf
```

**我会自动**：
- 📖 读取并理解你的硬件配置
- 🔍 从 PDF 中提取规格参数
- 💡 基于实际硬件给出代码建议
- ✅ 在代码审查时检查硬件兼容性

### 第三步：开始开发

```bash
# 编辑代码
vim Core/Src/main.c

# 我会自动：
# ✓ 编译验证
# ✓ 检查内存安全
# ✓ 验证硬件参数
```

## 📚 完整的知识库

### 快速参考文档

| 文档 | 内容 | 何时使用 |
|------|------|--------|
| [CLAUDE.md](CLAUDE.md) | 项目技术指南 | 第一次学习项目 |
| [.claude/rules/embedded-best-practices.md](.claude/rules/embedded-best-practices.md) | 嵌入式开发规范 | 写代码时参考 |
| [.claude/rules/motor-control-guide.md](.claude/rules/motor-control-guide.md) | 电机控制完整指南 | 实现电机控制 |
| [.claude/rules/schematic-reading-guide.md](.claude/rules/schematic-reading-guide.md) | 原理图解读 | 理解硬件电路 |
| [.claude/rules/code-review-checklist.md](.claude/rules/code-review-checklist.md) | 代码审查清单 | 提交代码前 |
| [.claude/rules/hardware-integration.md](.claude/rules/hardware-integration.md) | 硬件集成清单 | 集成新硬件 |
| [.claude/docs/README.md](.claude/docs/README.md) | 文档索引 | 查找具体资料 |

### 目录结构

```
项目/
├── Core/                    # 你的应用代码
├── Drivers/                 # STM32 HAL 和 CMSIS
├── cmake/                   # 构建配置
├── CLAUDE.md                # 项目技术指南（必读）
├── .claude/
│   ├── settings.json        # 权限和 hooks 配置
│   ├── HARDWARE_SETUP.md    # 硬件接入指南
│   ├── docs/                # 硬件文档库（放你的 PDF）
│   │   ├── README.md
│   │   ├── datasheets/
│   │   ├── schematics/
│   │   └── pcb/
│   └── rules/               # 开发规范库
│       ├── embedded-best-practices.md
│       ├── motor-control-guide.md
│       ├── schematic-reading-guide.md
│       ├── code-review-checklist.md
│       └── hardware-integration.md
└── CMakeLists.txt
```

## 💡 我能帮你做什么

### 🔧 硬件问题
```
你: "电机转不了，怎么排查？"

我会:
1. 查看诊断树指导排查步骤
2. 查阅你的硬件文档（如有）
3. 建议具体的测试命令
4. 推荐修复方案
```

### 📝 代码审查
```
你: "帮我审查这段电机驱动代码"

我会:
✓ 检查内存安全（缓冲区、栈溢出）
✓ 验证中断规范（无阻塞、原子操作）
✓ 确认 HAL 使用正确
✓ 基于硬件规格的参数建议
✓ 标记安全问题（🔴CRITICAL 到 ✅APPROVED）
```

### 🏗️ 硬件集成
```
你: "我要集成 TT-6V 电机和 L298N 驱动板"

我会:
1. 按 hardware-integration.md 清单指导
2. 检查电源、GPIO、PWM 配置
3. 验证信号接线和驱动能力
4. 提供测试清单
```

### 🚀 性能优化
```
你: "电机控制怎样才能更平稳？"

我会:
1. 建议 PWM 参数（频率、分辨率、死区时间）
2. 推荐软启动和过载保护代码
3. 根据电机型号优化控制算法
```

## 🎯 快速命令参考

### 构建项目
```bash
cmake --preset Debug              # 配置调试版本
cmake --build build/Debug         # 编译
```

### 调试
```bash
# OpenOCD 会自动启动（VS Code 中 F5）
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
```

### 查询文档
```
问我任何关于以下的问题：
• GPIO 配置
• PWM 频率设置
• 中断处理
• 电机驱动
• 电源管理
• ADC 转换
```

## 📋 典型工作流

### Day 1: 项目学习
1. 阅读 CLAUDE.md 了解项目结构
2. 查看 embedded-best-practices.md 了解规范
3. 如有硬件，上传到 .claude/docs/

### Day 2: 开始开发
1. 编辑 Core/Src/main.c 或创建新模块
2. 我会自动编译验证
3. 遇到问题查阅对应规范文档

### Day 3+: 迭代开发
1. 实现功能
2. 我自动审查代码安全
3. 基于硬件规格给建议
4. 硬件调试时参考诊断树

## ❓ 常见问题

**Q: 我没有硬件文档怎么办？**
A: 没关系，我会用基础知识回答。但有文档会让建议更精准。

**Q: 怎样上传硬件文件？**
A: 直接放在 `.claude/docs/datasheets/`, `.claude/docs/schematics/` 等目录下的 PDF 文件即可。我会自动识别。

**Q: 编译失败了怎么办？**
A: 
1. 查看错误信息
2. 检查 CMakeLists.txt 中是否包含新文件
3. 确认 Core/ 中的源文件有对应头文件

**Q: 怎样添加新的外设（如 ADC、SPI）？**
A: 
1. 在 STM32CubeMX 中配置（更新 .ioc 文件）
2. CubeMX 重新生成代码
3. 我会自动识别新文件
4. 查看 CLAUDE.md 中的 STM32CubeMX 工作流

**Q: 可以改动生成的代码吗？**
A: 只能在 `/* USER CODE BEGIN/END */` 标记内修改。其他地方的改动会在 CubeMX 重新生成时丢失。

## 🚀 现在就开始

### 选项 A: 立即开发（无硬件文档）
```bash
# 我已经准备好帮你了
# 编辑代码，我会自动验证
vim Core/Src/main.c
```

### 选项 B: 完整配置（有硬件文档）
1. 收集你的硬件 PDF 文件
2. 放在 `.claude/docs/` 目录下
3. 告诉我你的硬件配置
4. 开始开发！

### 选项 C: 学习模式（理解最佳实践）
1. 阅读 [.claude/rules/embedded-best-practices.md](.claude/rules/embedded-best-practices.md)
2. 查看 [.claude/rules/motor-control-guide.md](.claude/rules/motor-control-guide.md)
3. 问我任何关于嵌入式开发的问题

---

## 📞 需要帮助？

问我以下任何问题，我都能精准回答：

- **代码问题**：内存泄漏、中断、HAL 库使用
- **硬件问题**：GPIO 配置、电机控制、原理图理解
- **集成问题**：添加新外设、调试技巧
- **性能问题**：功耗优化、速度提升

## ✨ 下一步

选择一个开始：

1. 📖 **深入学习**：阅读 [CLAUDE.md](CLAUDE.md)
2. 📁 **上传文件**：将硬件 PDF 放入 `.claude/docs/`
3. 💻 **立即编码**：编辑 Core/Src/main.c，我来帮你验证
4. 🔍 **参考文档**：查看 `.claude/rules/` 中的具体指南

---

**祝你开发愉快！有问题随时问我。** 🚀
