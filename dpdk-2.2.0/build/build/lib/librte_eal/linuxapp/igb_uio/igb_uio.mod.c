#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x96cec1da, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x780b2e4f, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0xa997e3f1, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0x13b81555, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0x932344a8, __VMLINUX_SYMBOL_STR(__register_chrdev) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x6bd49c13, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0xe67759ca, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0xcede301c, __VMLINUX_SYMBOL_STR(__uio_register_device) },
	{ 0x9850ffba, __VMLINUX_SYMBOL_STR(dev_notice) },
	{ 0xc0e42aca, __VMLINUX_SYMBOL_STR(pci_intx_mask_supported) },
	{ 0xf0a71e70, __VMLINUX_SYMBOL_STR(pci_release_selected_regions) },
	{ 0xd97c8829, __VMLINUX_SYMBOL_STR(pci_select_bars) },
	{ 0x1a09356e, __VMLINUX_SYMBOL_STR(register_netdev) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x3767c91f, __VMLINUX_SYMBOL_STR(alloc_etherdev_mqs) },
	{ 0xf7b6fbdd, __VMLINUX_SYMBOL_STR(sysfs_create_group) },
	{ 0xf7d65bb0, __VMLINUX_SYMBOL_STR(pci_enable_msix) },
	{ 0xa11b55b2, __VMLINUX_SYMBOL_STR(xen_start_info) },
	{ 0x731dba7a, __VMLINUX_SYMBOL_STR(xen_domain_type) },
	{ 0x6f820778, __VMLINUX_SYMBOL_STR(dma_supported) },
	{ 0x42c8de35, __VMLINUX_SYMBOL_STR(ioremap_nocache) },
	{ 0x3a9b5f2d, __VMLINUX_SYMBOL_STR(dma_set_mask) },
	{ 0x90ded659, __VMLINUX_SYMBOL_STR(pci_set_master) },
	{ 0xf7c2cca1, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x5998e623, __VMLINUX_SYMBOL_STR(pci_request_regions) },
	{ 0xa098b005, __VMLINUX_SYMBOL_STR(pci_enable_device) },
	{ 0xa4d5abf7, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x643c4c76, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x54283c6d, __VMLINUX_SYMBOL_STR(pci_check_and_mask_intx) },
	{ 0xafe1adf3, __VMLINUX_SYMBOL_STR(pci_intx) },
	{ 0x5c10a2f3, __VMLINUX_SYMBOL_STR(pci_cfg_access_unlock) },
	{ 0x7cade89a, __VMLINUX_SYMBOL_STR(pci_cfg_access_lock) },
	{ 0x6fef8a97, __VMLINUX_SYMBOL_STR(remap_pfn_range) },
	{ 0x5944d015, __VMLINUX_SYMBOL_STR(__cachemode2pte_tbl) },
	{ 0x4c4f1833, __VMLINUX_SYMBOL_STR(boot_cpu_data) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0xa8a29d8c, __VMLINUX_SYMBOL_STR(pci_bus_type) },
	{ 0x8144c064, __VMLINUX_SYMBOL_STR(pci_enable_sriov) },
	{ 0xca48eb66, __VMLINUX_SYMBOL_STR(pci_num_vf) },
	{ 0xe770be2a, __VMLINUX_SYMBOL_STR(pci_disable_sriov) },
	{ 0x3c80c06c, __VMLINUX_SYMBOL_STR(kstrtoull) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x7043a19c, __VMLINUX_SYMBOL_STR(pci_disable_msix) },
	{ 0xe480aa2, __VMLINUX_SYMBOL_STR(unregister_netdev) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x3637f396, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0x8fd674, __VMLINUX_SYMBOL_STR(pci_release_regions) },
	{ 0x59050e4, __VMLINUX_SYMBOL_STR(uio_unregister_device) },
	{ 0x5454966f, __VMLINUX_SYMBOL_STR(sysfs_remove_group) },
	{ 0x128cf3fb, __VMLINUX_SYMBOL_STR(free_netdev) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=uio";


MODULE_INFO(srcversion, "DAFE4DECB23238E824B3527");
