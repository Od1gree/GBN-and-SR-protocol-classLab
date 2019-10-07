# GBN-and-SR-protocol-classLab
Computer Networking Lab, implement GBN and SR protocol

###实验目的
理解滑动窗口协议的基本原理;掌握 GBN 的工作原理;掌握基于 UDP 设计并实现一个 GBN 协议的过程与技术。 
###实验内容
* 基于UDP设计一个简单的GBN协议，实现单向可靠数据传输(服 务器到客户的数据传输)。 
* 模拟引入数据包的丢失，验证所设计协议的有效性。 
* 改进所设计的 GBN 协议，支持双向数据传输;
* 将所设计的 GBN 协议改进为 SR 协议。

###实验过程及结果
####数据分组和确认分组格式
	本次实验使用统一的格式来传输信息：
	[1byte 类型][3byte 窗口号][数据],总长度不超过100bytes。
	其中类型为 a 表示 ack即顺序送达，类型为 n 表示 nak即乱序送达，类型为 d 表示 data 即发送端发送的数据。
	窗口号表示此数据数据哪个窗口，为了保证数据顺序送达，检查数据是否丢失。