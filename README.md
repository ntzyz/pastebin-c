# PasteBin

尝试实现一个最小的 PasteBin。

<del>放假在家闲的（</del>

---

Tested on Ubuntu 18.04.5 with nginx. Compile:

```bash
% make SOCKET_PATH=/tmp/paste-bin.sock STORE_PATH=/var/www/html BASE_URL="http://localhost"
```

Nginx config:

```
server {
    listen 80;
    server_name localhost;

    autoindex on;
    sub_filter_once off;
    add_header Access-Control-Allow-Origin "*";
    client_max_body_size 5m;
    default_type text/plain;

    location / {
        if ($request_method = POST) {
            proxy_pass http://unix:/tmp/paste-bin.sock:;
        }
    }

    root /var/www/html;
}
```

Run and post:

```bash
# Start the service.
% sudo ./pastebin &

# Paste file use cURL
% curl --data-binary @/proc/cpuinfo http://localhost/
http://localhost/0c331c21-0666-451b-9c44-9eed9c82d56f

# Access the pasted file
% curl http://localhost/0c331c21-0666-451b-9c44-9eed9c82d56f
processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 62
model name      : Intel(R) Xeon(R) CPU E5-2637 v2 @ 3.50GHz
stepping        : 4
microcode       : 0x42e
cpu MHz         : 3491.586
cache size      : 15360 KB
physical id     : 0
siblings        : 1
core id         : 0
cpu cores       : 1
apicid          : 0
initial apicid  : 0
fpu             : yes
fpu_exception   : yes
cpuid level     : 13
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ss syscall nx rdtscp lm constant_tsc arch_perfmon nopl xtopology tsc_reliable nonstop_tsc cpuid pni pclmulqdq ssse3 cx16 pcid sse4_1 sse4_2 x2apic popcnt tsc_deadline_timer aes xsave avx f16c rdrand hypervisor lahf_lm cpuid_fault pti ssbd ibrs ibpb stibp fsgsbase tsc_adjust smep arat flush_l1d arch_capabilities
bugs            : cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds swapgs itlb_multihit
bogomips        : 6983.17
clflush size    : 64
cache_alignment : 64
address sizes   : 43 bits physical, 48 bits virtual
power management:
```
