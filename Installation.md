# Installation #

How to install row cache for innodb.
The patch is for mysql 5.1.48 and innodb plugin 1.0.9


## Installation Steps ##

  1. Download mysql 5.1.48 source code if you don't have. Link is http://downloads.mysql.com/archives/mysql-5.1/mysql-5.1.48.tar.gz
  1. Decompress the mysql source code.
    * `tar zvxf mysql-5.1.48.tar.gz`
  1. Apply the patch in the source root directory .
    * `patch -p0 < ./row_cache_for_mysql.5.1.48_2011_05_11.diff `
  1. Configure.
    * ` CFLAGS="-O3" CXX=gcc CXXFLAGS="-O3 -felide-constructors -fno-exceptions -fno-rtti" ./configure --prefix=/usr/mysql --with-extra-charsets=all --with-plugins=partition,heap,innobase,myisam,myisammrg,csv --enable-assembler`
  1. make && make install

## Setting ##

> Setting added in my.cnf(The Row Cache is based on innodb\_plugin):
  * innodb\_row\_cache\_mem\_pool\_size
    * the size of row cache used in memory.
    * default is 1M
  * innodb\_row\_cache\_on
    * the switch for row cache,if you want enable row cache,it need to be ON,
    * default is OFF.
  * innodb\_row\_cache\_cell\_num
    * the row cache used hash table's cell num.
    * default is 1000
  * innodb\_row\_cache\_mutex\_num\_shift
    * the shift for row cache mutex num, more large higher concurrency ,it can refer to `innodb_thread_concurrency` ,recommend `(1<<innodb_row_cache_mutex_num_shift)` slightly higher than `innodb_thread_concurrency `.
    * default is 6
> ### add for row\_cache\_for\_mysql.5.1.48\_2011\_06\_16.diff ###
  * innodb\_row\_cache\_additional\_mem\_pool\_size
    * the size of additional for misc memory allocate
    * default is 1M
  * innodb\_row\_cache\_index
    * you can set index which can be cached!
    * like `test/test_large:idx_age`, `test` is DB , `test_large` is TABLE, `idx_age` is INDEX,if you want to cache primary key, use `PRIMARY`
    * you can use `*` or `?` for wildcard character
    * default is NULL ,mean cache all index
  * innodb\_row\_cache\_clean\_cache
    * this setting is for debug, if you set this to 1 online, it can clean the row cache
    * it always be 0
  * innodb\_row\_cache\_use\_sys\_malloc
    * if you set this variable to ON , it use system malloc instead of mempool in innodb , if you use TC\_Malloc or JE\_malloc ,you can set it to ON for less memory fragment
    * default is OFF



## Status ##

> You can find more status where run `show status`,it is:
  * Innodb\_row\_cache\_n\_get
    * Total number which get from Row Cache, it inculde the missing gets.
  * Innodb\_row\_cache\_n\_geted
    * The number of real geted from Row Cache ,it exculde the missing gets.
  * Innodb\_row\_cache\_lru\_count
    * Total number which Row Cache is cached.
  * Innodb\_row\_cache\_lru\_n\_add
    * Number of row added to Row Cache.
  * Innodb\_row\_cache\_lru\_n\_evict
    * Number of row evicted from Row Cache.
  * Innodb\_row\_cache\_lru\_n\_make\_first
    * Number of row which is made first to LRU list
  * Innodb\_row\_cache\_mem\_pool\_size
    * The mem pool size of Row Cache.
  * Innodb\_row\_cache\_mem\_pool\_used
    * Used size in Row Cache's mem pool

> When you run "`show innodb status`",you can see some message more then original one, which like:
```
----------------------
ROW CACHE
----------------------
Total memory allocated 5368709120; used 5367472576 (999 / 1000); Total LRU count 12569422
Row total add 30544838 , 344.35 add/s
Row total make first 771088189 , 13720.31 mf/s
Row total evict 17975130 , 332.97 evict/s
Row read from cache 802835501, 14085.11 read/s
Row get from cache 772330451, 13740.67 get/s
Row cache hit rate 975 / 1000
```
> you can see cache hit!
