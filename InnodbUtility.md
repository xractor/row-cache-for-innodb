## print\_btree.py ##
  * 这个工具会根据数据文件来打印出B-Tree结构来..而且也会打印出每一个Page的详细信息
  * 输出结果如下:
```
index id: h->0|l->2078
|--no:5(29)
|  |--no:12(377)
|  |--no:13(377)
|  |--no:17(400)
|  |--no:24(754)
|  |--no:16(366)
|  |--no:33(377)
|  |--no:36(383)
|  |--no:29(733)
|  |--no:47(753)
|  |--no:39(3)
|  |--no:46(378)
|  |--no:55(378)
|  |--no:54(755)
|  |--no:62(377)
|  |--no:68(755)
|  |--no:77(753)
|  |--no:71(367)
|  |--no:82(377)
|  |--no:87(409)
|  |--no:86(377)
|  |--no:90(377)
|  |--no:92(380)
|  |--no:93(377)
|  |--no:98(382)
|  |--no:94(734)
|  |--no:95(731)
|  |--no:91(366)
|  |--no:97(369)
|  |--no:96(264)
```
  * 其中 no后的数字表示page号,括号里面的数字表示这个page下面有多少条记录
  * 也可以开启debug模式来打印出详细的Page信息 ,如:
```
page no:141
page addr:0x234000h , page type:17855, page space id:1277996950 ,page no:141
page level:0 ,page index id:h->0|l->2076 ,page_btr_top:(0, 0, 0) ,page_btr_leaf:(0, 0, 0)
page_file_prev:140 ,page_file_next:142
page record number:279 ,dir_slots:70
page free:0 ,page_heap_top:15186 ,page_heap_num:33049
page infimum:infimum ,page supremum:supremum
```
  * 还有whole模式可以打印出完全符合B-tree结构的信息来,具体自己研究吧
  * 帮助信息直接运行 print\_btree.py 就能看到啦
## plot\_index.py ##
  * 这个工具是依赖上面的print\_btree.py,一定要放在一起才能运行哦
  * 这个工具的目的是可以画出各个索引在innodb数据文件中的物理位置
  * 画图用的是matplotlib,记得要安装哦
  * 下面的图画出了所有索引的分布,如下:
> > ![http://pic.yupoo.com/zephyrleaves/BdyhyiR9/vCnhI.png](http://pic.yupoo.com/zephyrleaves/BdyhyiR9/vCnhI.png)
  * 不同的颜色代表不同的索引
  * 下面的图是单索引的分布图:
> > ![http://pic.yupoo.com/zephyrleaves/Bdyk4o4n/medish.jpg](http://pic.yupoo.com/zephyrleaves/Bdyk4o4n/medish.jpg)
  * 颜色深浅表示每页的记录数的多少