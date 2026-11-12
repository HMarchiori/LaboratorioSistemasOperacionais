EXT4-fs (sda): mounted filesystem 471fe4ce-a1eb-4d49-bbc9-73ca6b11df8f ro without journal. Quota mode: disabled.
VFS: Mounted root (ext2 filesystem) readonly on device 8:0.
devtmpfs: mounted
Freeing unused kernel image (initmem) memory: 724K
Write protecting kernel text and read-only data: 10448k
Run /sbin/init as init process
EXT4-fs (sda): re-mounted 471fe4ce-a1eb-4d49-bbc9-73ca6b11df8f r/w. Quota mode: disabled.
Saving 256 bits of non-creditable seed for next boot
Starting syslogd: OK
Starting klogd: OK
Running sysctl: OK
Starting network: OK
Configuring host communication.e1000: eth0 NIC Link is Up 1000 Mbps Full Duplex, Flow Control: RX
OK
Hello World!
Starting crond: OK
Starting dropbear sshd: OK
Starting httpd: OK

Welcome to Buildroot
buildroot login: root
Password: 
# dmesg -c > /dev/null
# mem-test-write | tail -3

mem-test-write invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
CPU: 0 UID: 0 PID: 110 Comm: mem-test-write Not tainted 6.12.27 #4
Hardware name: QEMU Ubuntu 24.04 PC v2 (i440FX + PIIX, arch_caps fix, 1996), BIOS 1.16.3-debian-1.16.3-2 04/01/2014
Call Trace:
 dump_stack_lvl+0x56/0x64
 dump_stack+0x12/0x18
 dump_header+0x51/0x3e8
 ? ___ratelimit+0x9f/0x110
 oom_kill_process+0x330/0x4f8
 ? try_to_free_pages+0x17d/0x5cc
 ? oom_badness+0x176/0x1e4
 out_of_memory+0xb3/0x45c
 __alloc_pages_noprof+0xb38/0xda8
 __folio_alloc_noprof+0x19/0x28
 __filemap_get_folio+0xd2/0x260
 filemap_fault+0x180/0xa1c
 __do_fault+0x26/0x88
 handle_mm_fault+0x6e0/0xc38
 exc_page_fault+0x1c4/0x4f4
 ? pvclock_clocksource_read_nowd+0x100/0x100
 handle_exception+0x133/0x133
EIP: 0xb7f04930
Code: Unable to access opcode bytes at 0xb7f04906.
EAX: 00000000 EBX: b7faee34 ECX: 00061000 EDX: b14cbf90
ESI: bf95543c EDI: b7ff7b60 EBP: bf955368 ESP: bf955338
DS: 007b ES: 007b FS: 0000 GS: 0033 SS: 007b EFLAGS: 00000206
 ? pvclock_clocksource_read_nowd+0x100/0x100
Mem-Info:
active_anon:18 inactive_anon:27075 isolated_anon:0
 active_file:3 inactive_file:12 isolated_file:0
 unevictable:0 dirty:0 writeback:0
 slab_reclaimable:125 slab_unreclaimable:964
 mapped:2 shmem:15 pagetables:54
 sec_pagetables:0 bounce:0
 kernel_misc_reclaimable:0
 free:435 free_pcp:13 free_cma:0
Node 0 active_anon:72kB inactive_anon:108300kB active_file:12kB inactive_file:48kB unevictable:0kB isolated(anon):0kB isolated(file):0kB mapped:8kB dirty:0kB writeback:0kB shmem:6s
DMA free:564kB boost:0kB min:180kB low:224kB high:268kB reserved_highatomic:0KB active_anon:0kB inactive_anon:14760kB active_file:8kB inactive_file:0kB unevictable:0kB writependinB
lowmem_reserve[]: 0 98 98 98
Normal free:1176kB boost:0kB min:1176kB low:1468kB high:1760kB reserved_highatomic:0KB active_anon:72kB inactive_anon:93528kB active_file:12kB inactive_file:44kB unevictable:0kB wB
lowmem_reserve[]: 0 0 0 0
DMA: 1*4kB (U) 1*8kB (U) 1*16kB (U) 1*32kB (U) 8*64kB (U) 0*128kB 0*256kB 0*512kB 0*1024kB 0*2048kB 0*4096kB = 572kB
Normal: 4*4kB (U) 3*8kB (UM) 1*16kB (U) 1*32kB (M) 1*64kB (M) 0*128kB 0*256kB 0*512kB 1*1024kB (M) 0*2048kB 0*4096kB = 1176kB
Node 0 hugepages_total=0 hugepages_free=0 hugepages_surp=0 hugepages_size=4096kB
33 total pagecache pages
0 pages in swap cache
Free swap  = 0kB
Total swap = 0kB
32638 pages RAM
0 pages HighMem/MovableOnly
3496 pages reserved
Tasks state (memory values in pages):
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883       24        0       24         0    12288        0             0 syslogd
[     68]     0    68      883       15        0       15         0    12288        0             0 klogd
[     95]     0    95      882        8        0        8         0    12288        0             0 crond
[    101]     0   101      855       24        0       24         0    12288        0             0 dropbear
[    106]     0   106      882       30        0       30         0    12288        0             0 sh
[    107]     0   107      883       33        0       33         0    12288        0             0 getty
[    110]     0   110    27668    26901    26880       21         0   118784        0             0 mem-test-write
[    111]     0   111      883       16        0       16         0    12288        0             0 tail
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=mem-test-write,pid=110,uid=0
Out of memory: Killed process 110 (mem-test-write) total-vm:110672kB, anon-rss:107520kB, file-rss:84kB, shmem-rss:0kB, UID:0 pgtables:116kB oom_score_adj:0
Killed
# dmesg | grep -E "invoked oom-killer|Out of memory"
mem-test-write invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
Out of memory: Killed process 110 (mem-test-write) total-vm:110672kB, anon-rss:107520kB, file-rss:84kB, shmem-rss:0kB, UID:0 pgtables:116kB oom_score_adj:0
# 