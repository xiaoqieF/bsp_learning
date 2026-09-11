# Lab 08 Solution

可构建的完整参考实现位于上级 `src/bsp_edu.c`。它覆盖 PCI ID 匹配、BAR0 映射、MMIO 和 IRQ；中断 handler 按 QEMU EDU 规格向 acknowledge 寄存器清除中断。
