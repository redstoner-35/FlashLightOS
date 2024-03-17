powered by 35's Embedded Systems Inc. A next generation Flashlight firmware solution.                                                                                                                        

### 概述

这是由redstoner_35(from 35's Embedded Systems Inc.)开发的适用于高阶手电筒的固件系统。该系统采用裸机方式开发
并运行在合泰公司的32位HT32F52352单片机上。此系统的最大特点是采用了一套最终用户友好的console来完成固件系统的挡
位、温控曲线和固件偏好设置的可视化结果调节，而不需要像目前安德鲁之类的主流手电固件通过侧按的大量组合键来设置。
这避免了用户日常使用时意外弄乱配置的问题。同时此系统还有内置的自检和错误监控以及运行日志机制，当驱动本身检测到
自身的硬件出现异常或外部配件（例如LED本身，电池，LED温度传感器等）出现问题时，驱动将会自我保护避免损坏且将损坏
时的遥测参数存储到EEPROM中可以下载到电脑通过配套的软件进行分析。

### 硬件需求

目前此系统针对的是全程降压恒流的硬件平台，对于其他类型驱动(升压/线性恒流和直驱拓扑等)暂未做适配。在编写此系统时我使用的是自行研发的[Xtern Ripper v3.0](https://gitee.com/redstoner-thirty-five/Xtern-Ripper)的硬件平台（全程降压恒流+模数结合）。对于移植到其他硬件平台的详细硬件需求如下所示：

+ MCU : HT32F52352 (QFN-32或LQFP-64) 基于CM0+内核 48MHz主频(**也可以移植到同等内核的MCU上面，需要的MCU配置请参考下面的配置**)
+ EEPROM ：至少64KByte容量（工程里面提供了24C512和24C1024可选）建议使用FM24C512D。
+ OTP安全存储器[可选] ：至少128字节容量（用于保存FRU信息），且具有OTP功能。**注:系统中对于OTP存储器提供的驱动面向FM24C512D附带的安全扇区,如果您使用的EEPROM没有该功能,则需要在工程配置内禁用安全存储器改用EEPROM存储FRU信息**
+ 电池功率计 : 至少12bit精度,同时具有电压电流测量功能,I2C接口。工程中提供了INA219AIDCNR的驱动,您可以自行移植其他型号(例如INA226)
+ 线性调光DAC ：需要2组，12-16bit精度，I2C接口，输出范围根据你的DCDC的压控电流输入来选择（工程中提供了AD5693R的驱动,但是此芯片极贵约60-70￥每片,您可以自行移植其它型号的I2C DAC）
+ LED温度传感器 ：10K额定阻值，B值3450/3950的NTC热敏电阻。使用电阻分压方式测量（NTC电阻的基本参数[B值、标定温度和校准值等]可在工程中调整）
+ 功率级 ：目前支持全程降压恒流DCDC方案的功率级，**升压/线性恒流和直驱拓扑暂未适配**。需要具有输出电流/温度的电压量反馈接口和电压控制电流输入。
+ LED电压采样器 ：需要具有1/3精确比例分压和滤波功能。

#### MCU的硬件需求
+ USART : 至少1个
+ General Purpose Timer:至少2个，其中一个用于系统的8Hz心跳时钟,另一个负责给特殊挡位提供Time base
+ DMA : 至少2通道(分别处理串口的TX和RX)且具有循环地址模式和传输完成中断
+ GPIO : 至少6个GPIO
+ ADC ： 至少4通道 12bit及以上精度 具有转换完毕(EOC)中断
+ 硬件I2C : 可选，支持400Kbps Fast mode。工程中我使用了480Kbps的软件I2C来增加可移植性，如果需要提高效率也可以用硬件I2C。
注意：目前工程中输出电流，温度采样的实现基于Intel DrMOS(SPS)方式实现。如果您使用不包含DrMOS的常规DCDC模块，您需要自己实现温度和电流采样算法。

### 工程结构

此工程的代码结构如下：

+ CMSIS ：包含MCU使用到的一些CMSIS库
+ Command ：包含shell后端实现各个命令处理的handler
+ ConfFileMgmt ： 实现EEPROM中配置文件的操作以及系统的自检、运行和错误日志的相关逻辑
+ Config ：包含系统的固件基本参数和硬件配置(例如IO分配等)的头文件
+ ExtPeriphal ：包含系统中一些外设（电池监视器，ADC等）的底层驱动以及顶层的代码实现（例如挡位电流调节，故障保护，侧按按键检测和指示灯控制）逻辑
+ I2C ：包含软件实现的I2C底层协议，PMBUS协议栈和一些系统中用到的I2C从设备的驱动
+ Library ：合泰单片机官方提供的外设驱动和初始化相关的库函数
+ MCUConfig ：合泰官方提供的一些MCU相关的配置，汇编文件等
+ MDK-ARM ：合泰官方提供的HT32的启动文件
+ ModeLogic ：实现系统中的挡位和开关逻辑切换以及一些特殊功能（SOS，爆闪，可编程闪，呼吸，摩尔斯发送）的逻辑
+ Shell ：系统负责调参的intractable shell的底层逻辑和顶层框架的基础实现，以及在驱动出现自检错误时的tinyshell实现
+ User ：系统主函数和一些底层库，以及一些底层需要的基础函数（例如延时，阈值表查表等）
+ Xmodem ：Xmodem协议栈的实现

### 换挡和调光逻辑

此系统使用了一套原创的，较为偏战术的换挡和调光逻辑。最大的特点是换挡和调光逻辑为两套独立的系统。这允许用户在特殊场合下在不开启手电的情况通过侧按按钮的闪光提前选好需要的挡位然后长按直接开启手电按照设定的挡位运行，而不需要将手电开启后在点亮状态下才能换挡。这使得一键强光/爆闪压制之类的特殊功能可以很轻易的实现。

#### 换挡逻辑
    
系统一共有三个挡位组，每个挡位组内包含若干个挡位。通过特定次数的短按按键可以在这些挡位组内切换。这些挡位组内每个挡位都可以使用shell任意编程功能。并不局限于出厂配置。挡位组配置如下：

+ 常规挡位组（至少1个挡位，最多8个挡位）
+ 双击功能挡位组（1个挡位，通常用于实现双击极亮功能）
+ 三击特殊功能挡位组（0-4个挡位，可以放置爆闪/SOS之类的特殊功能）

![挡位切换逻辑图](/img/2.jpg)

挡位切换逻辑如上图所示，手电筒启动后默认位于常规挡位组的第1个挡位（当然这个不是固定的，您可以自己调整默认启动的挡位），您可以通过短按侧按按钮指定次数切换挡位组和挡位，切换的逻辑如下：

+ 位于常规挡位组：**单击**选择该组下一个挡位、**双击**切换到双击功能挡位组，**三击**切换到特殊功能挡位组。
+ 双击功能挡位组：**单击或双击**回到常规功能挡位组，**三击**切换到特殊功能挡位组。
+ 位于常规挡位组：**单击**选择该组下一个挡位、**双击**切换到双击功能挡位组，**三击**切换到常规挡位组。

#### 点亮逻辑

对于点亮逻辑和特殊模式（例如战术模式）的切换，可以参考下面的操作手册以及逻辑图。点亮逻辑的切换可以通过对侧按进行指定次数的短按和长按去操作。

![点亮逻辑图](/img/1.jpg)

+ 单击：开启手电（仅阿木UI模式有效）
+ 长按1秒：开启(位于战术模式下开启手电，松开按钮后手电将立即熄灭)/关闭手电、出现错误时重置错误。
+ 单击并迅速按住按钮(持续1秒以上)：无极调光模式下调整亮度，其他模式和待机状态下会调用电池电压指示器，显示电池电压（如果库仑计已校准则按照百分比显示电池容量）。
+ 双击并迅速按住按钮(持续1秒以上)：启用和禁用自动关机计时器。如果是开启了鸡血功能的极亮档，则会强制关闭温控并输出最大功率直到手电筒再次达到温控启动温度。
+ 三击并迅速按住按钮(持续1秒以上)：触发手电筒的温度显示。此时手电筒会通过侧按LED播报LED基板温度和驱动温度，以及是否降档的提示。
+ 四击：进入和退出战术模式
+ 五击：锁定或解锁手电（锁定后即使断电也不会解除，需要再次五击方可解除）

**注：在锁定模式下，您可通过单击单击并迅速按住按钮(持续1秒以上)让手电进入紧急月光照明模式，而无需进行解锁操作。进入紧急照明月光模式后可通过单击或长按1秒关闭手电。**

### 挡位逻辑

此系统最大的特点就是挡位逻辑中的每个挡位都是可编程的，并没有规定双击,特殊功能组和常规功能组的模式使得用户拥有极高的自由度。对于高级用户而言，这允许用户通过编程轻易的模拟自己喜欢的某款绝版手电的UI逻辑。同时每个挡位还有大量可以调节的偏好设置，可以参考如下的内容。

#### 挡位通用的偏好设置

每个挡位都有一些通用的偏好设置，这些设置可以设置挡位的记忆,温控功能等，详细的设置如下：

+ 自动关机定时器整定值[1-360分钟]：设置该挡位的自动关机时间。当用户通过双击+长按启用自动关机定时器且手电处于工作状态，手电将会开始按照你设定的时间倒计时。倒计时结束后手电就会自动关机。如果该参数设置为0则该挡位不支持自动关机功能。
+ 是否记忆：当手电筒位于该挡位并关闭手电筒后，如果该功能关闭，则手电筒会自动回到默认的挡位。（注意：战术模式下此功能无效）
+ 温控功能：当该功能启用时手电筒将会按照当前驱动器和LED的温度调节输出电流避免过热。如果关闭则手电将会全功率输出直到LED或驱动器的温度达到编程的跳闸保护点后关机保护。**注意：出于安全考虑，对于挡位电流超过自然散热极限的挡位，温控将不允许普通用户关闭。如果以厂家工程师权限关闭则会使保修失效。**
+ 是否启用: 允许用户根据需要临时禁用或启用挡位组中的某个挡位。当某挡位被禁用时，调档循环会直接跳过该挡位。例如常规挡位组中[1 4 7 8]四个挡位为启用状态，其余的挡位禁用则挡位循环会变成[1->4->7->8->1...]以此类推。
+ 温控温度负偏移值[0-20℃]：允许用户配置当前挡位的温控负偏移值。例如该参数为10度且手电的原始温控参数为70度起跳时，手电在当前挡位的温控起跳温度将会为**70-10=60℃**。这个功能可以让用户在非极亮档使用较低的温控阈值来获得更舒适的握持体验，并且在极亮档时使用较高的温控参数来获得更长的极亮满功率运行时间。
+ 挡位运行模式：设置挡位的工作模式（一共8种），可以参考下面的内容。

#### 常亮模式

此模式也就是最常规的一种模式，在此模式下，手电的主LED输出将按照您设定的运行电流以及温控设置综合计算出的电流持续点亮。对于此挡位，您可以通过编程LED电流来获得需要的亮度。此挡位可编程的部分如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] : 设置主LED的电流（以及亮度）

**注意：由于温控功能的缘故，对于高电流挡位，驱动不一定会按照您设置的电流运行。**

#### 爆闪模式

此模式下，主LED按照您设置的频率以50%的占空比持续输出光脉冲。这种模式下会造成人短暂失明以及眩晕。此挡位的可编程部分如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] : 设置主LED的电流（以及亮度）
+ 爆闪频率 [0.5-16.0(Hz)] : 设置主LED闪烁的频率

#### 呼吸/信标模式

此模式下，主LED会按照您编程的时序参数以及电流参数以四个阶段为一循环反复的变化亮度。这种模式一般用于生成定时的循环光脉冲以标记位置或者模拟呼吸灯。对于一个亮度循环的结构如下：

- 爬升->保持最高电流->下滑->保持最低电流
-  |-<--------------<----------<---|

此模式下可编程的部分如下：

+ LED最大电流 [0.05-FusedCurrentLimit(A)] ：设置主LED在电流由低到高爬升结束后的最大电流（最高亮度）
+ LED最小电流 [0-LED最大电流(A)] ：设置主LED在电流由高到低下滑结束后的最小电流（最低亮度）如果该参数输入0，则LED将会熄灭
+ LED电流爬升时间 [0.5-7200(秒)] ：设置主LED电流从最低点爬升到最高点的总用时
+ LED最大电流保持时间 [0-7200(秒)]：当主LED爬升到最大电流后，在下滑到最低电流前需要在该电流下保持的时长。如果该数值设置为0则在LED达到最大电流后将会立即开始下滑到最低电流的过程。
+ LED电流下滑时间 [0.5-7200(秒)] ：设置主LED电流从最高点下滑到最低点的总用时
+ LED最小电流保持时间 [0-7200(秒)]：当主LED下滑到最小电流后，在开始爬升前需要在该电流下保持的时长。如果该数值设置为0则在LED达到最小电流后将会立即开始爬升过程。

#### 无极调光模式

此模式允许用户通过单击并迅速按住按钮(持续2秒以上)以连续的方式在天花板和地板亮度的范围内任意选取1个喜欢的亮度等级。由于系统使用了超高精度的调光DAC，相比常用手电固件系统的100-150阶的亮度调整，此系统的无极亮度调整范围可达30000阶以上。此挡位下LED将会持续按照用户指定的亮度等级以及温控设置调整输出亮度。此模式的操作和提示逻辑如下：

+ 单击并迅速按住按钮(持续2秒以上)：亮度按照设定的变化率平滑的从地板升高到天花板或者从天花板降低到地板。
+ **注：当LED亮度已经到达天花板或地板亮度后，LED将会熄灭0.5秒后重新点亮以提示用户已到达亮度范围限制**


对于此模式的可编程参数分别如下：
+ LED最大电流 [LED最小电流-FusedCurrentLimit(A)] ：设置主LED的最大电流（天花板亮度）
+ LED最小电流 [0.05-LED最大电流(A)] ：设置主LED的最小电流（地板亮度）
+ LED电流爬升/下滑时间 [1-15(秒)] ：设置当用户按下按钮后，LED亮度从地板爬升到天花板或者从天花板下滑到地板的总用时。

#### 可编程（自定义）闪烁模式

此模式向高级用户提供了一个类似序列发生器的功能，允许高级用户通过ASCII字符序列自行编程手电实现特殊的闪烁序列。这个功能可以允许用户编程手电以模拟雷电，烛光，自行车，警灯和派对爆闪灯等生活中常见光源的闪烁或者用于发送特殊的光脉冲序列。此模式一共提供了31个字节的命令空间。可用的编程字符定义如下：

+ [1-9,A] : **指定亮度命令**-使主LED点亮并按照指定的亮度运行(1-9表示10-90%,A表示100%)，此时LED亮度将会保持到下一个亮度命令。
+ [B] : **随机分支命令**-当序列中执行到此命令时，系统会随机决定是否执行该指令后面的内容。系统会有50%的概率执行该指令后方的所有序列。如果不执行后面的序列，则会直接从序列头部开始重新执行。
+ [R] : **随机亮度命令**-使主LED点亮并且随机设置一个在10-99%之间的亮度，此时LED亮度将会保持到下一个亮度命令。
+ [-] : **熄灭命令**-此命令会强制LED熄灭直到碰到下一个亮度命令。
+ [W] : **延时0.5秒命令**-此命令会在执行下一条指令前插入0.5秒延时。在延时期间LED亮度和状态保持不变。
+  X  : **延时1秒命令**-此命令会在执行下一条指令前插入1秒延时。在延时期间LED亮度和状态保持不变。
+ [Y] : **延时2秒命令**-此命令会在执行下一条指令前插入2秒延时。在延时期间LED亮度和状态保持不变。
+ [Z] : **延时4秒命令**-此命令会在执行下一条指令前插入4秒延时。在延时期间LED亮度和状态保持不变。
+ [T] : **循环长度控制命令**-当序列中执行到此命令时，如果该序列的当前执行次数为奇数次，则该命令后面的命令会被直接跳过不再执行。若执行次数为偶数次，则该命令后面的指令会被继续执行直到序列结束。例如 `1234T5678` 这样的序列。在第1,3,5...次(后面以此类推)只会执行序列前面的四个亮度命令[1234]，后面的内容会被直接跳过。如果是偶数次(2,4,6,8...)则会执行后面的[5678]这四个亮度命令。
+ [U] : **随机延时命令**-此命令在执行下一条指令前插入一个1-30秒的随机延时。在延时期间LED亮度和状态保持不变。

例如`8-4-2-1-Z`这个序列表示LED以80,40,20,10%亮度分别闪烁四次，延时4秒后再次闪烁并以此反复循环。该命令可编程的参数如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] ：设置主LED在亮度命令为100%时的电流（以及亮度）
+ 自定义闪字符序列[1-31字符长度的字符串] : 设置主LED闪烁的序列。
+ 序列执行频率[4-25(Hz)]：设置自定义闪的序列执行的速度。设置为4Hz则序列每秒钟执行4个指令。对于`1010`这样的序列则会产生
1Hz的闪烁。
**注：此设置对于[WXYZ]四个延时命令的延时时间没有影响。**

#### SOS模式

此模式下系统将会按照用户指定的发送速度和LED电流驱动主LED发出国际通用的SOS信号的摩尔斯代码(... --- ...)该模式一般用于求救使用。此模式可编程的内容如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] : 设置主LED的电流（以及亮度）
+ 发送时'.'的长度 [0.05-2.0(秒)] : 设置主LED在发送SOS时'.'的时长。时长越短发送速度越快，但是越难以看清。

#### 自定义摩尔斯码发送模式

此模式下系统将会按照用户指定的发送速度，自定义字符串内容和LED电流驱动通过摩尔斯代码的方式送出用户编程的字符串。该模式一般用于远程的光学通信，以及向远处通过光学方式发送您的识别码。此模式可编程的内容如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] : 设置主LED的电流（以及亮度）
+ 发送时'.'的长度 [0.05-2.0(秒)] : 设置主LED在发送自定义字符串时'.'的时长。时长越短发送速度越快，但是越难以看清。
+ 欲发送的自定义字符串[1-31字符长度的字符串]: 设置通过摩尔斯代码以光学发送的自定义字符串内容。**注意,该内容仅能包含部分字符且不支持中文**

#### 随机变频爆闪模式

此模式下系统将会按照用户指定的上下限频率和电流生成频率和占空比完全随机的光脉冲，这个模式是一些战术手电筒上面的短时间强光压制所使用的功能，随机的爆闪会使人眼无法适应从而可以使人短时间失明和眩晕。此模式可编程的内容如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] : 设置主LED的电流（以及亮度）
+ 随机爆闪的下限频率 [4-随机爆闪上限频率(Hz)] : 设置随机爆闪的下限频率。
+ 随机爆闪的上限频率 [随机爆闪下限频率-16(Hz)] : 设置随机爆闪的上限频率。

#### 线性变频爆闪模式

此模式下系统将会按照用户指定的上下限频率和电流生成频率从下限逐渐变快到达上限，停留一会后逐渐变慢回到下限并反复循环的变频光脉冲。此模式可编程的内容如下：

+ LED电流 [0.05-FusedCurrentLimit(A)] : 设置主LED的电流（以及亮度）
+ 随机爆闪的下限频率 [4-随机爆闪上限频率(Hz)] : 设置随机爆闪的下限频率。
+ 随机爆闪的上限频率 [随机爆闪下限频率-16(Hz)] : 设置随机爆闪的上限频率。

### 指示灯逻辑

本系统支持红绿双色的侧按指示灯，用于提供简易的故障信息输出和手电筒状态的信息。由于单片机资源限制的原因，指示灯亮度暂时不支持软件调整。

#### 运行状态指示

系统运行和待机操作时的指示信息如下：

+ 手电降档和反向战术模式提示:在手电筒开启的操作途中。如果手电进入反向战术模式或因为电池质量过低触发告警或者温控启动进入降档状态，则侧按LED将会每3秒左右闪烁指定次数以提示您手电已处于降档（功率限制）状态，对应的闪烁规律如下所示。
- 闪烁1次：手电进入反向战术模式，此时长按侧按将会执行用户选择的操作（一键极亮/降低亮度或短暂关闭手电）
- 闪烁2次：手电因为温度过高，温控已介入。此时手电将会自动根据散热情况调整功率使外壳恒温避免过热。
- 闪烁3次：手电因为电池质量过低满足不了放电需求，为了保护电池避免电池损坏，已将功率限制为最大值的60%。

+ 温度指示器：当用户对手电筒侧按实施三击并按住的动作后，手电筒将通过侧按LED指示LED的温度以及LED和驱动的降档情况。首先系统会通过红黄绿快速切换并绿色闪烁一次（**注意：如果绿色闪烁一次后紧跟着黄色闪烁一次，则表示温度读数为负数**）表示即将播报温度信息。延迟1秒后系统将开始播报LED温度信息。播报方式是先以红色播报百位,以黄色播报十位,最后以绿色播报个位，每组播报之间有1秒延迟。LED温度播报结束之后将会有两次较慢的闪烁分别表示LED温度和驱动自身的温度所对应的降档情况。例如`黄-黄-黄(停顿1秒)绿-绿-绿(停顿1秒)绿(0.5秒停顿)绿`表示当前LED温度为33℃,且驱动和LED都未达到降档阈值。对于后面的两个较慢闪烁的降档指示，各颜色所指示的内容可参考下表：
- 绿色：对应部件的温度未达到PID恒温阈值，部件散热良好
- 黄色：对应部件的温度达到PID恒温阈值，部件散热良好
- 红色：对应部件的温度超过温控启动阈值，部件散热不良请检查

+ 电量指示器：当手电筒在待机模式和开启并处于非无极调光的挡位时，用户通过单击+长按激活电量指示器之后，系统会通过侧按指示灯指示用户当前电池组剩余的电量是多少。对于库仑计未激活的时候，系统将指示电池电压。如果库仑计激活，系统将指示电池剩余容量百分比。

- 电压指示：侧按LED会从红切换到绿色，短时间熄灭然后马上黄色闪一下表示显示的是电压序列，延迟一秒后系统将开始通过侧按LED播报电池电压。播报方式是先以红色播报十位,以黄色播报个位,最后以绿色播报小数点后一位，每组播报之间有1秒延迟。例如`红(停顿1秒)黄(停顿1秒)绿-绿-绿`表示当前电池电压为11.3V。

- 剩余容量指示：侧按LED会从红切换到黄色，熄灭然后马上绿色闪一下，延迟一秒后系统将开始通过侧按LED播报电池剩余容量百分比。播报方式是先以红色播报百位,以黄色播报十位,最后以绿色播报个位，每组播报之间有1秒延迟。例如`红...`表示当前电池电量为100%，对于`黄-黄-黄(停顿1秒)绿-绿-绿-绿`表示电池剩余容量为34%。

+ 挡位切换提示:此固件在关闭手电时选择挡位，以及在非记忆挡位关闭手电后，侧按LED会通过如下的序列提示用户挡位已发生变更：`绿-绿...(停顿1.5秒)黄-黄...`其中，绿色的闪烁次数提示用户c处于哪个挡位组，黄色的闪烁次数表示用户位于该组的第几个挡位。例如`绿(停顿1.5秒)黄-黄-黄`指示用户当前位于常规功能挡位组的第三个挡位。绿色LED闪烁次数和挡位组的关系如下表：
-  | 次数 |      所在挡位组        |
-  |  1   |  常规功能(最多8个挡位) |
-  |  2   |      双击特殊功能      |
-  |  3   |  三击特殊功能(4个挡位) |

+ 自动关机计时器启用提示：当用户通过双击+长按启用了自动关机计时器之后,侧按将会通过指示灯序列提示用户定时器激活以及定时器的延时时间。此时侧按会以黄色闪烁2次然后紧接着以绿色闪烁2次，停顿1秒后通过一定次数的闪烁提示用户当前自动关机定时器的时间整定值。对于在60分钟内的时间，侧按将以黄色闪烁，一次代表时间5分钟。超过60分钟的时间则以红色闪烁，一次代表30分钟。
+ 自动关机定时器禁用提示: 当用户在定时器启用后双击+长按,则定时器会被关闭。此时侧按将通过黄色闪烁2次然后紧接着以绿色闪烁1次的序列提示用户。
+ 绿色持续点亮：手电正常运行中，电量充足
+ 黄色持续点亮：手电正常运行中，电量较为充足
+ 红色持续点亮：手电正常运行中，电量不足
+ 红色慢速闪烁：手电电量严重不足，请充电
+ 黄色闪烁2次后绿色立即闪烁1次：手电已进入战术模式（也叫点射模式，长按按键开启手电后只要松开按键则立即熄灭）
+ 黄色闪烁2次后绿色立即闪烁2次：手电已退出战术模式
+ 绿色慢速闪烁：手电已通过USB连接到电脑，进入参数调整模式(此时除了shell以外的所有功能禁用)
+ 红灯亮0.5秒后转为绿灯：手电已成功解锁，此时所有功能可用
+ 绿灯亮0.5秒后转为红灯：手电已成功锁定，此时侧按按钮的调档和开关功能被禁用，需要再次5击侧按方可解锁。
+ 红灯快速闪3次 ：手电已被锁定，您的操作无效，请解锁手电后再试
+ 黄色快速闪1次紧接着红色闪1次：鸡血技能（关闭温控并强制手电筒恢复为挡位的额定亮度直到温度达到温控启动点）次数已经用尽暂时无法使用。您需要等待手电筒冷却后才能再次使用此技能。
+ 绿色快速闪2次紧接着红色闪1次：手电已退出反向战术模式
+ 绿色快速闪2次紧接着黄色闪1次：手电已进入反向战术模式

#### 运行时发生故障的指示

这个手电的侧按指示灯同时提供了驱动出现异常的指示。在Pre-Bias-StartUp以及正常运行中如果出现错误，则驱动会通过侧按指示灯的特定闪烁提示用户驱动出现异常。详细的指示信息如下：

+ 红色慢闪(2秒一循环):电池电量严重不足，驱动已关机保护，请给手电充电
+ 红色闪3次，停顿2秒，闪1次：驱动器本身严重散热不良过导致MOS温度超过热关机温度，此时需要检查驱动器的散热。
+ 红色闪3次，停顿2秒，闪2次：电池输入电压过高，请检查输入电压是否低于保护值。通常这个错误是因为用户错误装入四锂串联18650电池板所致。
+ 红色闪3次，停顿2秒，闪3次：驱动输出过流保护。这通常是因为驱动内部逻辑错误导致输出电流失控，为了避免烧毁LED而自动保护。
+ 红色闪3次，停顿2秒，闪4次：驱动检测到LED发生开路故障。对于DIY玩家而言通常是因为驱动输出线和LED基板焊接不良。
+ 红色闪3次，停顿2秒，闪5次：驱动检测到LED发生短路故障。对于DIY玩家而言通常是因为驱动输出线破皮和外壳或者光杯短路所致。
+ 红色闪3次，停顿2秒，闪6次：LED散热不良过导致驱动探测到的LED温度超过热关机温度，此时需要检查LED基板是否和外壳完美贴合且涂了导热硅脂。
+ 红色闪3次，黄色闪2次，绿色闪1次：驱动内部发生灾难性逻辑错误.这个故障基本不会出现,如果出现则检查PWM发生器和DrMOS以及辅助电源。
+ 红色闪2次，停顿1秒，闪2次：DAC配置失败。出现这个错误可以试试降低I2C速率，如果仍未解决则说明你买到了假的DAC芯片导致驱动异常。
+ 红色闪2次，停顿1秒，闪3次：驱动的ADC以及INA219在测量过程中出现异常。此错误基本不会出现，若出现则需要检查ADC相关代码以及INA219芯片是否正品。
+ 红绿色交替闪烁两次，黄绿交替闪烁两次然后暂停3秒重复：内部挡位逻辑错误，这个错误主要是因为驱动的默认挡位是无效挡位所致,通常不出现。
+ 红绿色交替闪烁两次，橙绿交替闪烁3次然后暂停3秒重复：电池过流保护。驱动检测到电池端通过的电流超过了允许值使驱动关闭。
+ 红色以10Hz速度持续闪烁：程序因内部错误触发单片机hardfault，检查触发hardfault之前所执行的功能对应的函数并修正代码（固件问题，基本不出现）

#### 自检故障指示

系统中自检时显示的故障信息相关的侧按闪烁序列如下：（如没有特别说明，则以下序列是反复循环）

+ 红灯闪烁3次, 暂停2秒, 再度闪2次：电池端I2C功率计初始化失败
+ 红灯闪烁3次，暂停2秒，再度闪3次：电池端I2C功率计初次测量时出现异常（一般报这个错误可能是你买到假的INA219芯片了）
+ 红灯闪烁3次，暂停2秒，绿灯闪7次：驱动在初始化DrMOS的时候出现异常(DrMOS没有提供温度信号反馈或将温度信号上拉到3.3V指示内部致命错误)
+ 红灯闪烁2次，暂停2秒，再度闪1次：驱动无法和EEPROM通信
+ 红灯闪烁2次，暂停2秒，再度闪2次：驱动内部的AES256解密例程出现错误（一般不会出现，出现的话检查AES256部分的代码）
+ 红灯闪烁2次，暂停2秒，再度闪3次：固件在初始化片内ADC时失败(一般不出现,出现的话检查你是不是改过ADC部分的初始化代码之后出问题了)
+ 红灯闪烁4次，暂停2秒，再度闪2次：驱动在初始化线性调光DAC的时候出错(出现这个错误排除接线问题后一般是你买到假的AD5693R芯片了)
+ 红灯闪烁4次，暂停2秒，再度闪3次：驱动在进行线性和PWM调光电路自检时出错
+ 红灯快速闪烁5次：驱动检测不到负责测量LED温度的NTC热敏电阻
+ 红色闪3次，停顿2秒，闪5次：驱动检测到LED发生短路故障。对于DIY玩家而言通常是因为驱动输出线破皮和外壳或者光杯短路所致。
+ 红色以10Hz速度持续闪烁：程序因内部错误触发单片机hardfault，检查触发hardfault之前所执行的功能对应的函数并修正代码（固件问题，基本不出现）

### 反向战术模式功能说明

当用户在任意挡位正常点亮的阶段四击侧按，手电的指示灯将会绿色快速闪2次紧接着黄色闪1次指示用户反向战术模式已激活并自动进入到反向战术模式中，此时用户可以通过长按手电开关触发程序设定好的操作并短时间覆盖当前挡位的亮度。当用户松开开关之后就会恢复到设置的亮度。对于长按后所执行的操作同样是可编程的，用户可以在CLI配置环境中通过`misccfg -rtac <模式>`选择需要的操作。进入反向战术模式后如果需要关闭手电，可以再次四击侧按退出反向战术模式然后长按关机，或直接五击侧按。目前反向战术模式支持的操作如下：

+ 一键极亮：手电在按键松开时按照当前当前挡位运行，按下按钮后手电将会直接按照极亮运行并强制启动温控。
+ 关闭手电：手电在按键按下后会令主LED熄灭，松开后恢复正常。
+ 降低到30%亮度：手电在按键按下后会令主LED的亮度降低到当前设置的30%，松开后恢复正常。
+ 降低到50%亮度：手电在按键按下后会令主LED的亮度降低到当前设置的50%，松开后恢复正常。
+ 降低到70%亮度：手电在按键按下后会令主LED的亮度降低到当前设置的70%，松开后恢复正常。

### 矫正运行报告和LED电参数报告获取

位于debug模式编译的固件内置了全自动的校准运行和效率报告生成的功能。为了进行校准运行，首先您需要搭建用于校准运行的治具（校准治具板，一个足够大的铝散热器以及能承受极亮电流的LED，对于6V版本建议使用昌达SFP55），然后将驱动刷入debug模式的固件。刷入固件后您需要将驱动输出线连接到治具的输入端，然后给驱动提供一个稳定的12V，电流至少25A的直流电源（可以用750W服务器电源供电）。电源供应接通后用数据线连接驱动的typec接口并打开终端软件打印串口的内容。准备就绪之后长按侧按按钮2秒钟，当看到LED点亮后即可松手，此时校准运行将会自动开始。校准运行和效率测试的全过程大约持续1分钟左右，当LED熄灭后即可以root身份登录并执行`eepedit -r`读取整个EEPROM文件到电脑，然后使用Utility文件夹中的**EEPDumpTool.exe**进行解密后就会输出对应的csv文件可以用excel查看驱动的电参数。

### 恢复出厂设置

此固件内置了一个用于恢复出厂设置的机制，以便于在用户意外忘记配置CLI的管理员登录凭据时可以重置手电的设置以及凭据到出厂状态。为了确保安全，此机制需要通过重置PIN码（密码）的方式验证发起重置操作的操作者是否具有该手电的所有权。触发该机制的方式如下：

+ 第一步：拧松手电的电池包并按压侧按键一次，直到侧按的微光熄灭表示驱动已完成放电
+ 第二步：拧紧手电的电池包并观察手电的主LED以及侧按指示灯，当主灯快速闪烁两次且侧按指示灯开始`红-黄-绿`过渡的瞬间立即双击侧按
+ 第三步：如果成功触发，则手电侧按指示灯会立即以绿色闪烁4次，延迟1秒后闪烁1次指示您可以开始输入重置密码了。

#### 如何输入重置密码

当手电指示您可以输入重置密码时，您就可以按照从左到右的顺序输入附赠在手电包装内的重置密码卡上的5位数字（对于最终用户而言），如果您没有通过fruedit命令以及[固件参数头文件](/Config/FirmwareConf.h)中的RESETPassword宏对重置密码进行自定义，则您需要输入系统默认的重置密码`12345`。输入的方式是按照当前所需要输入的数字连续的按压侧按指定次数，例如当前需要输入的数字为5，则快速按压侧按5次，数字2则快速按压五次并以此类推。
按压结束后手电将会用红色闪烁来提示您本次输入的数字，例如您按压了5次（5击）则侧按LED将会红色闪烁5次。闪烁结束后1秒，手电将会用黄色闪烁提示您下一个需要输入的数字是密码中的第几个数字，例如闪烁2次则表示您接下来需要输入重置密码中的第二个数字。
如果您觉得您输入的该数字有误，则可长按侧按开关直到侧按指示灯以绿色亮起后松开。此时手电将会返回上一位需要输入的数字并通过侧按的黄色闪烁次数指示您需要输入密码中的第几个数字，例如闪烁2次则表示您接下来需要输入重置密码中的第二个数字。

**注：如果您决定放弃密码输入并结束本次重置过程，则您可单击+长按侧按按钮直到侧按LED以绿色闪烁2次，此时系统将退出重置模式并自动进入正常工作阶段。**

#### 如何确认重置密码并开始重置过程

当5个数字全部输入完毕后，侧按LED将会以`红-黄-绿-熄灭1秒`反复循环的方式提示您密码已经输入完毕，如果您认为密码不正确，则可则可长按侧按开关直到侧按指示灯以绿色亮起后松开。此时手电将会返回最后一位需要输入的数字并通过侧按的黄色闪烁次数指示您需要输入密码中的第几个数字，例如闪烁2次则表示您接下来需要输入重置密码中的第二个数字。如果您认为密码正确，则您可以单击侧按确认输入的密码开始重置进程，此时您需要观察指示灯状态以确认是否成功开始重置。指示灯状态如下表：

+ 红色闪烁三次后熄灭：您输入的重置密码不正确。
+ 红色闪烁两次，停顿一秒后黄色闪烁四次：系统再重置设置时无法向ROM提交新设置，请重试。
+ 绿色持续闪烁：您输入的重置密码正确，系统正在重置设置，请稍等。

当指示灯开始以绿色持续闪烁的时候则说明重置进程进行中，此时您需要耐心等待大约30秒让手电完成操作，如果绿色指示灯持续闪烁超过2分钟则说明重置出现异常，此时您需要拧松电池包断开电源并重新尝试重置操作。当完成重置操作后侧按开关的指示灯将会转为绿色常亮持续3秒，接着手电将会自动重启并重新自检。重新自检结束后，您可以使用系统的默认凭据（用户名和密码都是ADMIN）进行登录。

**警告：切勿在手电重置过程中断开电源，否则ROM内的数据将会损坏导致您的旧配置彻底丢失！**

----------------------------------------------------------------------------------------------------------------------------------
© redstoner_35 @ 35's Embedded Systems Inc.  2024