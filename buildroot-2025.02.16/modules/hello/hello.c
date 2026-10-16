#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Estudante");
MODULE_DESCRIPTION("KMOD Exemplo");
MODULE_VERSION("0.0.1");

static int hello_init(void)
{
    printk(KERN_EMERG "KERN_EMERG: mensagens de emergência, normalmente antes de uma falha completa do sistema.\n");
    printk(KERN_ALERT "KERN_ALERT: uma situação que requer ação imediata.\n");
    printk(KERN_CRIT "KERN_CRIT: condições críticas, geralmente relacionadas a falhas de software ou hardware.\n");
    printk(KERN_ERR "KERN_ERR: condições de erro; drivers usam para reportar problemas no hardware.\n");
    printk(KERN_WARNING "KERN_WARNING: avisos sobre situações problemáticas que não criam, em si, problemas sérios.\n");
    printk(KERN_NOTICE "KERN_NOTICE: situações normais, porém dignas de nota, como condições de segurança.\n");
    printk(KERN_INFO "KERN_INFO: mensagens informativas, como as de inicialização de um driver.\n");
    printk(KERN_DEBUG "KERN_DEBUG: mensagens de depuração.\n");

    return 0;
}

static void hello_exit(void)
{
    pr_alert("Goodbye, cruel world\n");
}

module_init(hello_init);
module_exit(hello_exit);

