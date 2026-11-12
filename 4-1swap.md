Hello World!
Starting crond: OK
Starting dropbear sshd: OK
Starting httpd: OK

Welcome to Buildroot
buildroot login: root
Password: 
# mkswap /dev/sdb
Setting up swapspace version 1, size = 33550336 bytes
# swapon /dev/sdb
Adding 32764k swap on /dev/sdb.  Priority:-2 extents:1 across:32764k 
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       102Mi        60Ki       5.0Mi       102Mi
Swap:           31Mi          0B        31Mi
# mem-test-malloc | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# 
# dmesg -c > /dev/null
# mem-test-write | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg | grep -E "invoked oom-killer|Out of memory"
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        20Mi        92Mi        60Ki       5.0Mi        92Mi
Swap:           31Mi          0B        31Mi
# dmesg -c > /dev/null
# mem-test-read | tail -3

random: crng init done
mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
CPU: 0 UID: 0 PID: 119 Comm: mem-test-read Not tainted 6.12.27 #4
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
EIP: 0xb7e41930
Code: Unable to access opcode bytes at 0xb7e41906.
EAX: 00000000 EBX: b7eebe34 ECX: 00036000 EDX: b0322f90
ESI: bf9498ec EDI: 00000bee EBP: bf953548 ESP: bf9498b8
DS: 007b ES: 007b FS: 0000 GS: 0033 SS: 007b EFLAGS: 00000206
 ? pvclock_clocksource_read_nowd+0x100/0x100
Mem-Info:
active_anon:470 inactive_anon:25866 isolated_anon:0
 active_file:1 inactive_file:10 isolated_file:0
 unevictable:0 dirty:0 writeback:0
 slab_reclaimable:136 slab_unreclaimable:966
 mapped:3 shmem:9 pagetables:793
 sec_pagetables:0 bounce:0
 kernel_misc_reclaimable:0
 free:434 free_pcp:13 free_cma:0
Node 0 active_anon:1880kB inactive_anon:103464kB active_file:4kB inactive_file:40kB unevictable:0kB isolated(anon):0kB isolated(file):0kB mapped:12kB dirty:0kB writeback:0ks
DMA free:564kB boost:0kB min:180kB low:224kB high:268kB reserved_highatomic:0KB active_anon:0kB inactive_anon:14792kB active_file:4kB inactive_file:0kB unevictable:0kB writB
lowmem_reserve[]: 0 98 98 98
Normal free:1172kB boost:0kB min:1176kB low:1468kB high:1760kB reserved_highatomic:0KB active_anon:1880kB inactive_anon:88680kB active_file:4kB inactive_file:40kB unevictabB
lowmem_reserve[]: 0 0 0 0
DMA: 2*4kB (U) 0*8kB 1*16kB (U) 1*32kB (U) 8*64kB (U) 0*128kB 0*256kB 0*512kB 0*1024kB 0*2048kB 0*4096kB = 568kB
Normal: 22*4kB (U) 9*8kB (U) 7*16kB (U) 2*32kB (UM) 1*64kB (M) 0*128kB 1*256kB (M) 1*512kB (M) 0*1024kB 0*2048kB 0*4096kB = 1168kB
Node 0 hugepages_total=0 hugepages_free=0 hugepages_surp=0 hugepages_size=4096kB
107 total pagecache pages
85 pages in swap cache
Free swap  = 0kB
Total swap = 32764kB
32638 pages RAM
0 pages HighMem/MovableOnly
3496 pages reserved
Tasks state (memory values in pages):
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883       27        0       27         0    12288        0             0 syslogd
[     68]     0    68      883       21        0       21         0    12288        0             0 klogd
[     95]     0    95      882       21        0       21         0    12288        0             0 crond
[     99]     0    99      855        8        0        8         0    12288        0             0 dropbear
[    104]     0   104      882       30        0       30         0    12288        0             0 sh
[    105]     0   105      883        7        0        7         0    12288        0             0 getty
[    119]     0   119   785561    26230    26208       22         0  3145728     8032             0 mem-test-read
[    120]     0   120      883       12        0       12         0    12288        0             0 tail
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=mem-test-read,pid=119,uid=0
Out of memory: Killed process 119 (mem-test-read) total-vm:3142244kB, anon-rss:104832kB, file-rss:88kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
Killed
# dmesg | grep -E "invoked oom-killer|Out of memory"
mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
Out of memory: Killed process 119 (mem-test-read) total-vm:3142244kB, anon-rss:104832kB, file-rss:88kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
# 



modo 1

# echo 1 > /proc/sys/vm/overcommit_memory
# cat /proc/sys/vm/overcommit_memory
1
# mem-test-malloc | tail -3
got 3053 MiB
got 3054 MiB
malloc failure after 3054 MiB
# dmesg -c > /dev/null
# mem-test-write | tail -3
got 3054 MiB
got 3055 MiB
malloc failure after 3055 MiB
# dmesg | grep -E "invoked oom-killer|Out of memory"
# dmesg -c > /dev/null
# mem-test-read | tail -3
mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
CPU: 0 UID: 0 PID: 132 Comm: mem-test-read Not tainted 6.12.27 #4
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
EIP: 0xb7e43930
Code: Unable to access opcode bytes at 0xb7e43906.
EAX: 00000000 EBX: b7eede34 ECX: 000ea000 EDX: b0270f90
ESI: bfd5501c EDI: 00000bee EBP: bfd5ec78 ESP: bfd54fe8
DS: 007b ES: 007b FS: 0000 GS: 0033 SS: 007b EFLAGS: 00000206
 ? pvclock_clocksource_read_nowd+0x100/0x100
Mem-Info:
active_anon:68 inactive_anon:26267 isolated_anon:0
 active_file:6 inactive_file:9 isolated_file:0
 unevictable:0 dirty:2 writeback:0
 slab_reclaimable:137 slab_unreclaimable:966
 mapped:3 shmem:3 pagetables:793
 sec_pagetables:0 bounce:0
 kernel_misc_reclaimable:0
 free:436 free_pcp:6 free_cma:0
Node 0 active_anon:272kB inactive_anon:105068kB active_file:24kB inactive_file:36kB unevictable:0kB isolated(anon):0kB isolated(file):0kB mapped:12kB dirty:8kB writeback:0ks
DMA free:572kB boost:0kB min:180kB low:224kB high:268kB reserved_highatomic:0KB active_anon:0kB inactive_anon:14780kB active_file:0kB inactive_file:0kB unevictable:0kB writB
lowmem_reserve[]: 0 98 98 98
Normal free:1172kB boost:0kB min:1176kB low:1468kB high:1760kB reserved_highatomic:0KB active_anon:272kB inactive_anon:90288kB active_file:24kB inactive_file:36kB unevictabB
lowmem_reserve[]: 0 0 0 0
DMA: 1*4kB (U) 0*8kB 0*16kB 0*32kB 9*64kB (U) 0*128kB 0*256kB 0*512kB 0*1024kB 0*2048kB 0*4096kB = 580kB
Normal: 15*4kB (U) 5*8kB (UM) 1*16kB (U) 1*32kB (M) 0*64kB 0*128kB 0*256kB 0*512kB 1*1024kB (M) 0*2048kB 0*4096kB = 1172kB
Node 0 hugepages_total=0 hugepages_free=0 hugepages_surp=0 hugepages_size=4096kB
280 total pagecache pages
262 pages in swap cache
Free swap  = 0kB
Total swap = 32764kB
32638 pages RAM
0 pages HighMem/MovableOnly
3496 pages reserved
Tasks state (memory values in pages):
[  pid  ]   uid  tgid total_vm      rss rss_anon rss_file rss_shmem pgtables_bytes swapents oom_score_adj name
[     64]     0    64      883        3        0        3         0    12288        0             0 syslogd
[     68]     0    68      883       25        0       25         0    12288        0             0 klogd
[     95]     0    95      882       28        0       28         0    12288        0             0 crond
[     99]     0    99      855        8        0        8         0    12288        0             0 dropbear
[    104]     0   104      882       24        0       24         0    12288        0             0 sh
[    105]     0   105      883        7        0        7         0    12288        0             0 getty
[    132]     0   132   785561    26079    26048       31         0  3145728     8000             0 mem-test-read
[    133]     0   133      883       17        0       17         0    12288        0             0 tail
oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),task=mem-test-read,pid=132,uid=0
Out of memory: Killed process 132 (mem-test-read) total-vm:3142244kB, anon-rss:104192kB, file-rss:124kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
Killed
# dmesg | grep -E "invoked oom-killer|Out of memory"
mem-test-read invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0
Out of memory: Killed process 132 (mem-test-read) total-vm:3142244kB, anon-rss:104192kB, file-rss:124kB, shmem-rss:0kB, UID:0 pgtables:3072kB oom_score_adj:0
# 
# 

modo 2

# 
# echo 2 > /proc/sys/vm/overcommit_memory
# cat /proc/sys/vm/overcommit_ratio
50
# grep -i commit /proc/meminfo
CommitLimit:       91048 kB
Committed_AS:       3024 kB
# free -h
               total        used        free      shared  buff/cache   available
Mem:           113Mi        11Mi       103Mi        16Ki       3.6Mi       102Mi
Swap:           31Mi       772Ki        31Mi
# 
# 
# mem-test-malloc | tail -3
__vm_enough_memory: pid: 139, comm: mem-test-malloc, bytes: 1052672 not enough memory for the allocation
__vm_enough_memory: pid: 139, comm: mem-test-malloc, bytes: 1048576 not enough memory for the allocation
__vm_enough_memory: pid: 139, comm: mem-test-malloc, bytes: 1183744 not enough memory for the allocation
got 81 MiB
got 82 MiB
malloc failure after 82 MiB
# mem-test-write | tail -3
__vm_enough_memory: pid: 141, comm: mem-test-write, bytes: 1052672 not enough memory for the allocation
__vm_enough_memory: pid: 141, comm: mem-test-write, bytes: 1048576 not enough memory for the allocation
__vm_enough_memory: pid: 141, comm: mem-test-write, bytes: 1183744 not enough memory for the allocation
got 81 MiB
got 82 MiB
malloc failure after 82 MiB
# mem-test-read | tail -3
__vm_enough_memory: pid: 143, comm: mem-test-read, bytes: 1052672 not enough memory for the allocation
__vm_enough_memory: pid: 143, comm: mem-test-read, bytes: 1044480 not enough memory for the allocation
__vm_enough_memory: pid: 143, comm: mem-test-read, bytes: 1183744 not enough memory for the allocation
80
81
82
# echo 0 > /proc/sys/vm/overcommit_memory
# 