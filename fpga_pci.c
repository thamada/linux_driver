#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DRIVER_NAME "fpga_pci"

static struct pci_device_id fpga_pci_ids[] = {
    { PCI_DEVICE(0x1234, 0x5678) }, // ベンダーIDとデバイスID
    { 0 }
};
MODULE_DEVICE_TABLE(pci, fpga_pci_ids);

static int fpga_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int err;

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
    void __iomem *hw_addr = pci_iomap(pdev, 0, pci_resource_len(pdev, 0));
    if (!hw_addr) {
        pr_err("Failed to map PCI I/O memory\n");
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    // FPGAに対するレジスタ操作などを行うためのアドレスが`hw_addr`に割り当てられます

    pci_set_drvdata(pdev, hw_addr);
    pr_info("FPGA PCI device initialized successfully\n");
    return 0;
}

static void fpga_pci_remove(struct pci_dev *pdev)
{
    void __iomem *hw_addr = pci_get_drvdata(pdev);
    pci_iounmap(pdev, hw_addr);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    pr_info("FPGA PCI device removed\n");
}

static struct pci_driver fpga_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = fpga_pci_ids,
    .probe = fpga_pci_probe,
    .remove = fpga_pci_remove,
};

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
