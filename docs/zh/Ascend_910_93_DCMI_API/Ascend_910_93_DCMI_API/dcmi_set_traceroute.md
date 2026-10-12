# dcmi\_set\_traceroute<a name="ZH-CN_TOPIC_0000002517558631"></a>

**函数原型<a name="section1994468155813"></a>**

**int dcmi\_set\_traceroute \(int card\_id, int device\_id, struct dcmi\_traceroute\_info param\_info, struct dcmi\_network\_node\_info \*ret\_info\[\], unsigned int ret\_info\_size\)**

**功能说明<a name="section10946138105811"></a>**

配置traceroute参数探测报文途径的网络节点信息。

**参数说明<a name="section1294798155811"></a>**

<a name="table1499319814585"></a>
<table><thead align="left"><tr id="row119149115814"><th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.1"><p id="p191169145811"><a name="p191169145811"></a><a name="p191169145811"></a>参数名称</p>
</th>
<th class="cellrowborder" valign="top" width="15.129999999999999%" id="mcps1.1.5.1.2"><p id="p199115925812"><a name="p199115925812"></a><a name="p199115925812"></a>输入/输出</p>
</th>
<th class="cellrowborder" valign="top" width="17.19%" id="mcps1.1.5.1.3"><p id="p13911190581"><a name="p13911190581"></a><a name="p13911190581"></a>类型</p>
</th>
<th class="cellrowborder" valign="top" width="50.51%" id="mcps1.1.5.1.4"><p id="p1091129155817"><a name="p1091129155817"></a><a name="p1091129155817"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="row7915915813"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p13915975816"><a name="p13915975816"></a><a name="p13915975816"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p39110995818"><a name="p39110995818"></a><a name="p39110995818"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p13911797583"><a name="p13911797583"></a><a name="p13911797583"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p591129165816"><a name="p591129165816"></a><a name="p591129165816"></a>设备ID，当前实际支持的ID通过dcmi_get_card_list接口获取。</p>
</td>
</tr>
<tr id="row4912099583"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p109169205814"><a name="p109169205814"></a><a name="p109169205814"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p29139115814"><a name="p29139115814"></a><a name="p29139115814"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p1691993587"><a name="p1691993587"></a><a name="p1691993587"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p17911892586"><a name="p17911892586"></a><a name="p17911892586"></a>芯片ID，通过dcmi_get_device_id_in_card接口获取。取值范围如下：</p>
<p id="p17914911584"><a name="p17914911584"></a><a name="p17914911584"></a>NPU芯片：[0, device_id_max-1]。</p>
<p id="p13167343121910"><a name="p13167343121910"></a><a name="p13167343121910"></a></p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> 说明： </span><div class="notebody"><p id="zh-cn_topic_0000002517638669_p10218227151413"><a name="zh-cn_topic_0000002517638669_p10218227151413"></a><a name="zh-cn_topic_0000002517638669_p10218227151413"></a><span id="zh-cn_topic_0000002517638669_ph833311293312"><a name="zh-cn_topic_0000002517638669_ph833311293312"></a><a name="zh-cn_topic_0000002517638669_ph833311293312"></a>device_id_max值为2，当device_id为0或1时表示NPU芯片；当device_id为2时表示MCU芯片。</span></p>
</div></div>
</td>
</tr>
<tr id="row139113916586"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p89210985814"><a name="p89210985814"></a><a name="p89210985814"></a>param_info</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p20928915813"><a name="p20928915813"></a><a name="p20928915813"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p192139195819"><a name="p192139195819"></a><a name="p192139195819"></a>struct traceroute</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p109279165817"><a name="p109279165817"></a><a name="p109279165817"></a>traceroute接口的入参</p>
<p id="p292139135817"><a name="p292139135817"></a><a name="p292139135817"></a>struct tracerout_result {</p>
<p id="p592149135818"><a name="p592149135818"></a><a name="p592149135818"></a>int max_ttl; //探测报文的最大跳数。取值范围：-1，1~255。</p>
<p id="p26191750191"><a name="p26191750191"></a><a name="p26191750191"></a>设置为-1时，表示默认值为30</p>
<p id="p2922919582"><a name="p2922919582"></a><a name="p2922919582"></a>int tos; //IPv4设置TOS优先级，取值范围：-1~63。IPv6设置流量控制值，取值范围：-1~255。数值越大优先级越高，设置为-1时，表示默认值为0</p>
<p id="p99215975810"><a name="p99215975810"></a><a name="p99215975810"></a>int waittime; //设置等待探测响应的最大时间。取值范围：-1，1~60，单位：s，设置为-1时，表示默认值为3s</p>
<p id="p9924915818"><a name="p9924915818"></a><a name="p9924915818"></a>int sport; //设置源端口号，取值范围：-1~65535。设置为-1时，表示默认值为大于30000的随机值</p>
<p id="p159212995813"><a name="p159212995813"></a><a name="p159212995813"></a>int dport; //设置目的端口号，取值范围：-1~65535。设置为-1时，表示默认值为大于30000的随机值</p>
<p id="p492169205820"><a name="p492169205820"></a><a name="p492169205820"></a>char dest_ip[48]; //目标主机IP</p>
<p id="p620713313587"><a name="p620713313587"></a><a name="p620713313587"></a>bool ipv6_flag; //是否使用IPv6协议。0表示不使用，1表示使用</p>
<p id="p29217912582"><a name="p29217912582"></a><a name="p29217912582"></a>bool reset_flag; //终止device侧traceroute所有后台进程，当traceroute测试异常时配置为1结束device侧进程。</p>
<p id="p49210955812"><a name="p49210955812"></a><a name="p49210955812"></a>};</p>
<p id="p1733024531910"><a name="p1733024531910"></a><a name="p1733024531910"></a></p>
<div class="note" id="note476213481342"><a name="note476213481342"></a><a name="note476213481342"></a><span class="notetitle"> 说明： </span><div class="notebody"><p id="p14762174810349"><a name="p14762174810349"></a><a name="p14762174810349"></a>端口号设置为0时，系统会使用大于30000的随机值。</p>
</div></div>
</td>
</tr>
<tr id="row18924985817"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p19239165819"><a name="p19239165819"></a><a name="p19239165819"></a>ret_info[]</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p1951094580"><a name="p1951094580"></a><a name="p1951094580"></a>输出</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p39549175816"><a name="p39549175816"></a><a name="p39549175816"></a>struct dcmi_network_node_info *</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p12955914581"><a name="p12955914581"></a><a name="p12955914581"></a>traceroute接口的返回信息</p>
<p id="p16951693585"><a name="p16951693585"></a><a name="p16951693585"></a>struct tracerout_result {</p>
<p id="p89511920582"><a name="p89511920582"></a><a name="p89511920582"></a>int mask; //掩码，表示后面的数据是否有效，例如：0xFF代表mask后的8个字段是有效的</p>
<p id="p179559135812"><a name="p179559135812"></a><a name="p179559135812"></a>char ip[48]; //路由节点的ip地址</p>
<p id="p12959965815"><a name="p12959965815"></a><a name="p12959965815"></a>int snt; //发送ICMP请求报文的数量</p>
<p id="p139520910583"><a name="p139520910583"></a><a name="p139520910583"></a>double loss; //对应节点的丢包率</p>
<p id="p109518912588"><a name="p109518912588"></a><a name="p109518912588"></a>double last; //最新报文的响应时间，单位ms</p>
<p id="p6951591588"><a name="p6951591588"></a><a name="p6951591588"></a>double avg; //所有报文的平均响应时间，单位ms</p>
<p id="p9958918587"><a name="p9958918587"></a><a name="p9958918587"></a>double best; //报文最快响应时间，单位ms</p>
<p id="p199512910587"><a name="p199512910587"></a><a name="p199512910587"></a>double wrst; //报文最慢响应时间，单位ms</p>
<p id="p14950917585"><a name="p14950917585"></a><a name="p14950917585"></a>double stdev; //标准偏差值，越大说明相应节点越不稳定</p>
<p id="p9951396582"><a name="p9951396582"></a><a name="p9951396582"></a>char reserve[64]; //附加扩展信息</p>
<p id="p1495119155818"><a name="p1495119155818"></a><a name="p1495119155818"></a>};</p>
<p id="p13625347121917"><a name="p13625347121917"></a><a name="p13625347121917"></a></p>
<div class="note" id="note725719328015"><a name="note725719328015"></a><a name="note725719328015"></a><span class="notetitle"> 说明： </span><div class="notebody"><p id="p122571321305"><a name="p122571321305"></a><a name="p122571321305"></a>返回信息的使用结构体数组存储，每个节点信息使用一个结构体。</p>
</div></div>
</td>
</tr>
<tr id="row7933548175819"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p093464855812"><a name="p093464855812"></a><a name="p093464855812"></a>ret_info_size</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p159344487580"><a name="p159344487580"></a><a name="p159344487580"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p993434845810"><a name="p993434845810"></a><a name="p993434845810"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p1934184817586"><a name="p1934184817586"></a><a name="p1934184817586"></a>传入的节点信息结构体的大小。</p>
</td>
</tr>
</tbody>
</table>

**返回值说明<a name="section3642631135017"></a>**

<a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_table19654399"></a>
<table><thead align="left"><tr id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p16312252"><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p16312252"></a><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p16312252"></a>类型</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p46224020"><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p46224020"></a><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p46224020"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p8661001"><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p8661001"></a><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p79561110476"><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p79561110476"></a><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p79561110476"></a>处理结果：</p>
<a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_ul55711364478"></a><a name="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_ul55711364478"></a><ul id="zh-cn_topic_0000002517558717_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_ul55711364478"><li>成功：返回0。</li><li>失败：返回码请参见<a href="return_codes.md">return_codes</a>。</li></ul>
</td>
</tr>
</tbody>
</table>

**异常处理<a name="section498311885817"></a>**

- 为防止由于ICMP报文被丢弃出现traceroute指令卡死的情况，建议IPv4的两条指令间隔3s执行。
- 若强制中断host侧指令或执行指令时返回值显示为-8020，则需配置param\_info.reset\_flag=1，调用dcmi\_set\_traceroute接口结束device侧进程。
- traceroute指令结束后，建议配置param\_info.reset\_flag=1，调用dcmi\_set\_traceroute接口结束device侧进程，防止进程持续运行影响NPU性能。
- 若param\_info.reset\_flag=1，则其他参数不生效。

**约束说明<a name="section1298518810584"></a>**

**表 1** 不同部署场景下的支持情况

<a name="table206991325174917"></a>
<table><thead align="left"><tr id="zh-cn_topic_0000002485318818_row2051415544912"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="zh-cn_topic_0000002485318818_p5723115216395"><a name="zh-cn_topic_0000002485318818_p5723115216395"></a><a name="zh-cn_topic_0000002485318818_p5723115216395"></a>产品形态</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="zh-cn_topic_0000002485318818_p67235521393"><a name="zh-cn_topic_0000002485318818_p67235521393"></a><a name="zh-cn_topic_0000002485318818_p67235521393"></a>物理机场景（裸机）root用户</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="zh-cn_topic_0000002485318818_p1072317520390"><a name="zh-cn_topic_0000002485318818_p1072317520390"></a><a name="zh-cn_topic_0000002485318818_p1072317520390"></a>物理机场景（裸机）运行用户组（非root用户）</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="zh-cn_topic_0000002485318818_p272335243919"><a name="zh-cn_topic_0000002485318818_p272335243919"></a><a name="zh-cn_topic_0000002485318818_p272335243919"></a>物理机+普通容器场景root用户</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0000002485318818_row135148510490"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p11514155490"><a name="zh-cn_topic_0000002485318818_p11514155490"></a><a name="zh-cn_topic_0000002485318818_p11514155490"></a><span id="zh-cn_topic_0000002485318818_text551465114917"><a name="zh-cn_topic_0000002485318818_text551465114917"></a><a name="zh-cn_topic_0000002485318818_text551465114917"></a>Atlas 900 A3 SuperPoD 超节点</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p185143564913"><a name="zh-cn_topic_0000002485318818_p185143564913"></a><a name="zh-cn_topic_0000002485318818_p185143564913"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p185145519497"><a name="zh-cn_topic_0000002485318818_p185145519497"></a><a name="zh-cn_topic_0000002485318818_p185145519497"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p1751413513498"><a name="zh-cn_topic_0000002485318818_p1751413513498"></a><a name="zh-cn_topic_0000002485318818_p1751413513498"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row135141553491"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p1351411524913"><a name="zh-cn_topic_0000002485318818_p1351411524913"></a><a name="zh-cn_topic_0000002485318818_p1351411524913"></a><span id="zh-cn_topic_0000002485318818_text175141751499"><a name="zh-cn_topic_0000002485318818_text175141751499"></a><a name="zh-cn_topic_0000002485318818_text175141751499"></a>Atlas 9000 A3 SuperPoD 集群算力系统</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p1034521615495"><a name="zh-cn_topic_0000002485318818_p1034521615495"></a><a name="zh-cn_topic_0000002485318818_p1034521615495"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p6514145154913"><a name="zh-cn_topic_0000002485318818_p6514145154913"></a><a name="zh-cn_topic_0000002485318818_p6514145154913"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p17514145184914"><a name="zh-cn_topic_0000002485318818_p17514145184914"></a><a name="zh-cn_topic_0000002485318818_p17514145184914"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row183721515155518"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p923812139165"><a name="zh-cn_topic_0000002485318818_p923812139165"></a><a name="zh-cn_topic_0000002485318818_p923812139165"></a><span id="zh-cn_topic_0000002485318818_text8238111381610"><a name="zh-cn_topic_0000002485318818_text8238111381610"></a><a name="zh-cn_topic_0000002485318818_text8238111381610"></a>Atlas 800T A3 超节点</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p1858019263555"><a name="zh-cn_topic_0000002485318818_p1858019263555"></a><a name="zh-cn_topic_0000002485318818_p1858019263555"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p125801926195517"><a name="zh-cn_topic_0000002485318818_p125801926195517"></a><a name="zh-cn_topic_0000002485318818_p125801926195517"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p125808265554"><a name="zh-cn_topic_0000002485318818_p125808265554"></a><a name="zh-cn_topic_0000002485318818_p125808265554"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row1828812107299"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p4288010192914"><a name="zh-cn_topic_0000002485318818_p4288010192914"></a><a name="zh-cn_topic_0000002485318818_p4288010192914"></a><span id="zh-cn_topic_0000002485318818_text17252201592911"><a name="zh-cn_topic_0000002485318818_text17252201592911"></a><a name="zh-cn_topic_0000002485318818_text17252201592911"></a>Atlas 800I A3 超节点</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p14729171810297"><a name="zh-cn_topic_0000002485318818_p14729171810297"></a><a name="zh-cn_topic_0000002485318818_p14729171810297"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p187294185296"><a name="zh-cn_topic_0000002485318818_p187294185296"></a><a name="zh-cn_topic_0000002485318818_p187294185296"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p12729918182916"><a name="zh-cn_topic_0000002485318818_p12729918182916"></a><a name="zh-cn_topic_0000002485318818_p12729918182916"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row8768181557"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p1563212213554"><a name="zh-cn_topic_0000002485318818_p1563212213554"></a><a name="zh-cn_topic_0000002485318818_p1563212213554"></a><span id="zh-cn_topic_0000002485318818_text1963217219554"><a name="zh-cn_topic_0000002485318818_text1963217219554"></a><a name="zh-cn_topic_0000002485318818_text1963217219554"></a>A200T A3 Box8 超节点服务器</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p18671727165520"><a name="zh-cn_topic_0000002485318818_p18671727165520"></a><a name="zh-cn_topic_0000002485318818_p18671727165520"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p967152775514"><a name="zh-cn_topic_0000002485318818_p967152775514"></a><a name="zh-cn_topic_0000002485318818_p967152775514"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p667192712555"><a name="zh-cn_topic_0000002485318818_p667192712555"></a><a name="zh-cn_topic_0000002485318818_p667192712555"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row207580351817"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p1339643920818"><a name="zh-cn_topic_0000002485318818_p1339643920818"></a><a name="zh-cn_topic_0000002485318818_p1339643920818"></a>注：Y表示支持；N表示不支持；NA表示不涉及，当前未规划此场景。</p>
</td>
</tr>
</tbody>
</table>

**调用示例<a name="section1598913835819"></a>**

```c
… 
int ret = 0;
int card_id = 0;
int device_id = 0;
struct dcmi\_traceroute\_info param_info = {0};
struct dcmi_network_node_info ret_info[10] = {0};
size_t ret_info_size = sizeof(ret_info);
param_info.sport = 40000;
param_info.waittime = 5;
strncpy_s(param_info.dest_ip, sizeof(param_info.dest_ip), "x.x.x.x", strlen("x.x.x.x"));

ret = dcmi_set_traceroute_get_info(card_id, device_id, param_info, &ret_info, ret_info_size);
if (ret != 0){
    //todo：记录日志
    return ret;
}
…
```

> **说明：** 
>
> _x.x.x.x_表示目的IP地址。
