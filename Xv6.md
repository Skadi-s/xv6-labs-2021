# Xv6 DIY

## Lab1 Xv6 and Unix Utilities

### 1.Sleep

**实现xv6的UNIX程序**`sleep`**：您的**`sleep`**应该暂停到用户指定的计时数。一个滴答(tick)是由xv6内核定义的时间概念，即来自定时器芯片的两个中断之间的时间。您的解决方案应该在文件\*user/sleep.c\*中**

### 2.Pingpong

编写一个使用UNIX系统调用的程序来在两个进程之间“ping-pong”一个字节，请使用两个管道，每个方向一个。父进程应该向子进程发送一个字节;子进程应该打印“`<pid>: received ping`”，其中`<pid>`是进程ID，并在管道中写入字节发送给父进程，然后退出;父级应该从读取从子进程而来的字节，打印“`<pid>: received pong`”，然后退出。您的解决方案应该在文件\*user/pingpong.c\*中。

### 3.Primes

使用管道编写`prime sieve`(筛选素数)的并发版本。这个想法是由Unix管道的发明者Doug McIlroy提出的。请查看[这个网站](http://swtch.com/~rsc/thread/)(翻译在下面)，该网页中间的图片和周围的文字解释了如何做到这一点。您的解决方案应该在\*user/primes.c\*文件中。

### 4.find

写一个简化版本的UNIX的`find`程序：查找目录树中具有特定名称的所有文件，你的解决方案应该放在\*user/find.c\*

### 5.xargs

编写一个简化版UNIX的`xargs`程序：它从标准输入中按行读取，并且为每一行执行一个命令，将行作为参数提供给命令。你的解决方案应该在***user/xargs.c\***

### 6.uptime

使用`uptime`系统调用以滴答为单位打印计算机正常运行时间

### 7.shell

- 在处理**文件中的**shell命令时，将shell修改为不打印$（moderate）
- 将shell修改为支持`wait`（easy）
- 将shell修改为支持用“`;`”分隔的命令列表（moderate）
- 通过实现左括号“`(`” 以及右括号“`)`”来修改shell以支持子shell（moderate）
- 将shell修改为支持`tab`键补全（easy）
- 修改shell使其支持命令历史记录（moderate）
