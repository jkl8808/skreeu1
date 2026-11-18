# sysroot

交叉编译时链接用的目标机库（`libfreetype.so.6` / `libpng16.so.16` / `libcurl.so.4`）。

这些文件来自打印机固件，有各自的版权，**不放进仓库**。部署前在自己的打印机上取一份：

```bash
scripts/fetch_sysroot.sh root@<printer-ip>
```

生成的 `sysroot/lib/*.so*` 已经被 `.gitignore` 忽略。
