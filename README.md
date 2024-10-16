# linux_driver
linux_driver


## デバイスドライバのビルド

```
make
```

## デバイスドライバのロード

ビルドが成功したら、insmodコマンドでカーネルモジュールをロードします。

```
sudo insmod fpga_pci.ko
```

## 確認

カーネルログを確認して、デバイスが認識されているか確認します。

```
dmesg | grep fpga_pci
```

