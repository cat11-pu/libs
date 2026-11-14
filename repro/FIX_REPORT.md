# palrle.h 修复说明与前后比对

## 1. 缺陷现象

- 长同色行的 RLE 块边界处解码错位，行整体平移，最后一行丢失。
- 越界 / 截断 / 损坏的输入会触发越界读写，实测旧解码器直接段错误。

## 2. 根因（两个独立问题，互相放大）

### 2.1 编码器缓冲区按错误上界分配（堆溢出）

RLE 行流由若干块组成：`[empty][count][color ...]`

- `count > 0`：同色 run，块开销 3 字节起。
- `count < 0`：逐像素 uniques，`|count|` 个颜色字节。

最坏情况下**每个像素都自成一个 uniques 块**（相邻像素颜色均不同，或 `width == 1`），
单像素成本为 `empty(1) + count(1) + color(1) = 3 字节/像素`。
带 mask 时空 span 还会额外产生 `[empty][0]` 标记，最坏到 5 字节/像素。

旧代码两个编码器都只预留：

```c
hpitch * vpitch * 2          // 仅 2 字节/像素
```

该上界在数学上不成立。最小复现 `width=1, height=64`（见 `repro/ab/compare.c`）：

| 项目 | 旧实现 | 实际需要 |
|---|---|---|
| RLE 区预留 | 412 B（含结构/调色板/偏移表 + 2n） | 475 B |
| 结果 | **越界写 63 字节**，破坏相邻堆块 | — |

越界写破坏堆元数据后，普通构建出现 `malloc(): corrupted top size`，ASan 构建报
`heap-buffer-overflow`。被破坏的内存随后被解码器线性读取，表现为行错位 / 末行丢失。

### 2.2 解码器完全没有边界检查

旧 `palrle_decode` / `palrle_blit` 只用一个自增指针线性走流：

- 不校验 blob `size`，流被截断时继续向后越界读；
- 不校验 `count` 与剩余行宽，损坏的 count 会把指针推出缓冲区（实测 SEGV）；
- 不校验像素坐标是否落在目标位图内；
- 每行不从偏移表独立寻址，单行出错会级联污染后续所有行；
- 用实现相关的 `char` 承载有符号 count（在 `char` 默认为无符号的平台上直接出错）。

## 3. 修复内容（均在 `palrle.h`）

1. **正确的容量上界**：无 mask 预留 `3n`、带 mask 预留 `5n` 字节 RLE 空间，
   并在编码结束处保留容量断言，违反即释放并返回 `NULL`。
2. **几何/溢出入参校验**：新增 `palrle_validate_geometry`，拒绝 `width/height <= 0`、
   超过 U16、调色板数非法及 `size_t` 溢出；`malloc` 失败返回 `NULL`。
3. **全边界检查解码器**：新增静态 `palrle_walk`，每个 RLE 字节读都对照 blob `size`，
   每个像素写都对照目标宽高；截断返回 `PALRLE_ERR_TRUNCATED`，结构损坏返回
   `PALRLE_ERR_CORRUPT`，空指针/非法尺寸返回对应错误码。
4. **按行独立寻址**：通过每行的 row offset 表定位行起点，并校验偏移单调且落在 RLE 区内，
   单行错误不再级联，末行不再丢失。
5. **可移植的有符号计数与对齐访问**：count 用显式 `signed char`（新增 `PALRLE_S8`）；
   偏移表 U32 经 `memcpy` 读写（`palrle_get_u32/palrle_put_u32`），消除严格对齐平台上的
   非对齐访问 UB。
6. **blit 裁剪修正**：原条件 `xp > 0 && yp > 0` 会漏掉坐标 0，改为 `>= 0` 的正确裁剪，
   并复用同一套边界检查。
7. **补齐头文件**：实现段补充 `#include <string.h>`（旧代码在严格 C11 下 `memset/memcpy`
   隐式声明报错）。
8. **API 兼容**：新增返回状态码的 `palrle_decode_safe` / `palrle_blit_safe` 与
   `PALRLE_OK / PALRLE_ERR_*` 宏；原 `void palrle_decode/palrle_blit` 保留为兼容包装。

## 4. 前后行为比对

| 输入场景 | 旧实现 | 新实现 |
|---|---|---|
| `width=1` 全 uniques | 堆越界写（63 B），`malloc` 报错/ASan 命中 | 容量充足，`PALRLE_OK`，逐像素一致 |
| 任意宽高正常图（含 127/128/255/256/257 边界） | 依赖堆布局，可能错位/丢行 | 全部逐像素一致 |
| 截断的 blob | 越界读，输出垃圾/崩溃 | `PALRLE_ERR_TRUNCATED`，无越界 |
| count 越界的损坏 blob | **SEGV（退出码 139）** | `PALRLE_ERR_CORRUPT`，无越界 |
| 超大 hpitch / 非法 row offset | 越界写 | `PALRLE_ERR_CORRUPT` |
| `width<=0`、负 palette、NULL | 未定义行为 | 返回 `NULL` / `PALRLE_ERR_*` |
| blit 到负坐标/边缘 | 漏画坐标 0 的像素 | 正确裁剪 |

## 5. 验证

- `repro/suite.c`：13,465 项断言，覆盖边界宽高、结构化图案、mask/稀疏 mask、
  截断与损坏 blob、非法入参、blit 裁剪、旧 void 包装；ASan+UBSan 下 0 失败，C 与 C++ 双编译通过。
- `repro/megafuzz.c`：20 万+ 随机 mask/非 mask 往返（ASan+UBSan），0 失败。
- `repro/fuzz.c`：30 万随机结构化图案往返，0 失败。
- `repro/ab/`：新旧实现符号重命名后同进程链接，直接对比编码账目与损坏输入处理。
- 头文件在 `gcc -std=c11` 与 `g++ -std=c++17` 的 `-Wall -Wextra -Wpedantic -Wshadow` 下零警告。

复现全部证据：`bash repro/run_all.sh`；可视化对比：`repro/palrle_repro.png`。
