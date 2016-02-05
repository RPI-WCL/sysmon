
System Monitor
--------------

* Reads the following statistics information from the linux file system:
  - CPU:    /proc/stat
  - Disk:   /sys/block/sda/stat
  - Net:    /proc/net/dev/
  - Memory: /proc/meminfo  

* Captured data entries are configurable through the *print_flag* variable in *sysmon.c* (See *include/print_flags.h* for flag definitions).

* Monitoring interval can be configured with the *INTERVAL_SEC* macro in *sysmon.c*.

* Some files/functions are adapted from:
  - *sysstat* (https://github.com/sysstat/sysstat)
  - *top* (http://sourceforge.net/projects/unixtop/)

* Usage
~~~
 $ make
 $ ./sysmon 1 log.txt // real-time logging enabled & save monitored data in "log.txt"
~~~
