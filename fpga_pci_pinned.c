/**
   ユーザー空間とのインターフェース: ioctlやread/writeなどを使ってユーザー空間からデバイスとやりとりを行います。
   割り込みハンドリング: FPGAの割り込みを処理するために割り込みハンドラを登録します (request_irqを使用)。
*/

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

/**
   デバイス情報と初期化関数の定義
   PCIeデバイスを認識し、リソースを割り当てるための情報を保持します。
*/
#define DRIVER_NAME "fpga_pci"

static struct pci_device_id fpga_pci_ids[] = {
    { PCI_DEVICE(0x1234, 0x5678) }, // ベンダーIDとデバイスID
    { 0 }
};
MODULE_DEVICE_TABLE(pci, fpga_pci_ids);

static irqreturn_t fpga_pci_irq_handler(int irq, void *dev_id)
{
    pr_info("FPGA PCI interrupt handled\n");
    // 割り込み処理の実装をここに追加します
    return IRQ_HANDLED;
}

/**
   PCIデバイスのプローブ関数
   デバイスが認識された際に呼ばれる関数です。この中でメモリのマッピングやリソースの設定を行います。
*/
static int fpga_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int err;
    int irq;
    void __iomem *hw_addr;
    void __iomem *wc_mem;
    void *dma_buffer;
    dma_addr_t dma_handle;
    size_t dma_size = 4096; // 例として4KBのDMAバッファサイズ

    // デバイスを有効化
    err = pci_enable_device(pdev);
    if (err) {
        pr_err("Failed to enable PCI device\n");
        return err;
    }

    // メモリ領域のマッピング
    err = pci_request_regions(pdev, DRIVER_NAME);
    if (err) {
        pr_err("Failed to request PCI regions\n");
        pci_disable_device(pdev);
        return err;
    }

    // メモリマップドI/Oの取得
    hw_addr = pci_iomap(pdev, 0, pci_resource_len(pdev, 0));
    if (!hw_addr) {
        pr_err("Failed to map PCI I/O memory\n");
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    // WriteCombinedメモリのマッピング
    wc_mem = ioremap_wc(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
    if (!wc_mem) {
        pr_err("Failed to map WriteCombined memory\n");
        pci_iounmap(pdev, hw_addr);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    // Pinnedメモリのマッピング (DMAメモリの割り当て)
    dma_buffer = dma_alloc_coherent(&pdev->dev, dma_size, &dma_handle, GFP_KERNEL);
    if (!dma_buffer) {
        pr_err("Failed to allocate pinned DMA memory\n");
        iounmap(wc_mem);
        pci_iounmap(pdev, hw_addr);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    // FPGAに対するレジスタ操作などを行うためのアドレスが`hw_addr`に割り当てられます
    pci_set_drvdata(pdev, hw_addr);

    // 割り込みの設定
    irq = pdev->irq;
    err = request_irq(irq, fpga_pci_irq_handler, IRQF_SHARED, DRIVER_NAME, pdev);
    if (err) {
        pr_err("Failed to request IRQ %d\n", irq);
        dma_free_coherent(&pdev->dev, dma_size, dma_buffer, dma_handle);
        iounmap(wc_mem);
        pci_iounmap(pdev, hw_addr);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        return err;
    }

    pr_info("FPGA PCI device initialized successfully\n");
    return 0;
}

/**
   デバイス削除時のクリーンアップ関数
*/
static void fpga_pci_remove(struct pci_dev *pdev)
{
    void __iomem *hw_addr = pci_get_drvdata(pdev);
    void __iomem *wc_mem = ioremap_wc(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
    int irq = pdev->irq;
    size_t dma_size = 4096; // 例として4KBのDMAバッファサイズ
    dma_addr_t dma_handle;
    void *dma_buffer = dma_alloc_coherent(&pdev->dev, dma_size, &dma_handle, GFP_KERNEL);

    free_irq(irq, pdev);
    dma_free_coherent(&pdev->dev, dma_size, dma_buffer, dma_handle);
    iounmap(wc_mem);
    pci_iounmap(pdev, hw_addr);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    pr_info("FPGA PCI device removed\n");
}

/**
  PCIドライバ構造体の定義
  プローブ関数と削除関数をPCIドライバに登録します。
*/
static struct pci_driver fpga_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = fpga_pci_ids,
    .probe = fpga_pci_probe,
    .remove = fpga_pci_remove,
};

/**
   モジュールの初期化とクリーンアップ
*/
static int __init fpga_pci_init(void)
{
    return pci_register_driver(&fpga_pci_driver);
}

static void __exit fpga_pci_exit(void)
{
    pci_unregister_driver(&fpga_pci_driver);
}

module_init(fpga_pci_init);
module_exit(fpga_pci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsuyoshi Hamada");
MODULE_DESCRIPTION("PCIe FPGA Board Driver");
