# dcmi\_set\_traceroute<a name="EN-US_TOPIC_0000002546910381"></a>

**Prototype<a name="section1994468155813"></a>**

**int dcmi\_set\_traceroute \(int card\_id, int device\_id, struct dcmi\_traceroute\_info param\_info, struct dcmi\_network\_node\_info \*ret\_info\[\], unsigned int ret\_info\_size\)**

**Description<a name="section10946138105811"></a>**

Configures traceroute parameters to detect the network nodes where the packets pass through.

**Parameter Description<a name="section1294798155811"></a>**

<a name="table1499319814585"></a>
<table><thead align="left"><tr id="row119149115814"><th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.1"><p id="p191169145811"><a name="p191169145811"></a><a name="p191169145811"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.129999999999999%" id="mcps1.1.5.1.2"><p id="p199115925812"><a name="p199115925812"></a><a name="p199115925812"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="17.19%" id="mcps1.1.5.1.3"><p id="p13911190581"><a name="p13911190581"></a><a name="p13911190581"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="50.51%" id="mcps1.1.5.1.4"><p id="p1091129155817"><a name="p1091129155817"></a><a name="p1091129155817"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row7915915813"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p13915975816"><a name="p13915975816"></a><a name="p13915975816"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p39110995818"><a name="p39110995818"></a><a name="p39110995818"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p13911797583"><a name="p13911797583"></a><a name="p13911797583"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p591129165816"><a name="p591129165816"></a><a name="p591129165816"></a>Device ID. The supported IDs can be obtained by calling <strong id="b161481308172"><a name="b161481308172"></a><a name="b161481308172"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row4912099583"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p109169205814"><a name="p109169205814"></a><a name="p109169205814"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p29139115814"><a name="p29139115814"></a><a name="p29139115814"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p1691993587"><a name="p1691993587"></a><a name="p1691993587"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p17911892586"><a name="p17911892586"></a><a name="p17911892586"></a>Chip ID, which can be obtained by calling <strong id="b72761491533042"><a name="b72761491533042"></a><a name="b72761491533042"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p17914911584"><a name="p17914911584"></a><a name="p17914911584"></a>NPU: [0, <strong id="b122286584625449"><a name="b122286584625449"></a><a name="b122286584625449"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515350528_p10218227151413"><a name="en-us_topic_0000002515350528_p10218227151413"></a><a name="en-us_topic_0000002515350528_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515350528_b551771083414"><a name="en-us_topic_0000002515350528_b551771083414"></a><a name="en-us_topic_0000002515350528_b551771083414"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515350528_b1263401315345"><a name="en-us_topic_0000002515350528_b1263401315345"></a><a name="en-us_topic_0000002515350528_b1263401315345"></a>2</strong>. A <strong id="en-us_topic_0000002515350528_b544811733415"><a name="en-us_topic_0000002515350528_b544811733415"></a><a name="en-us_topic_0000002515350528_b544811733415"></a>device_id</strong> of <strong id="en-us_topic_0000002515350528_b18565142019343"><a name="en-us_topic_0000002515350528_b18565142019343"></a><a name="en-us_topic_0000002515350528_b18565142019343"></a>0</strong> or <strong id="en-us_topic_0000002515350528_b1255272314342"><a name="en-us_topic_0000002515350528_b1255272314342"></a><a name="en-us_topic_0000002515350528_b1255272314342"></a>1</strong> indicates an NPU. A <strong id="en-us_topic_0000002515350528_b3731133414341"><a name="en-us_topic_0000002515350528_b3731133414341"></a><a name="en-us_topic_0000002515350528_b3731133414341"></a>device_id</strong> of <strong id="en-us_topic_0000002515350528_b174037378345"><a name="en-us_topic_0000002515350528_b174037378345"></a><a name="en-us_topic_0000002515350528_b174037378345"></a>2</strong> indicates an MCU.</p>
</div></div>
</td>
</tr>
<tr id="row139113916586"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p89210985814"><a name="p89210985814"></a><a name="p89210985814"></a>param_info</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p20928915813"><a name="p20928915813"></a><a name="p20928915813"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p192139195819"><a name="p192139195819"></a><a name="p192139195819"></a>struct traceroute</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p109279165817"><a name="p109279165817"></a><a name="p109279165817"></a>Input parameters of traceroute.</p>
<p id="p292139135817"><a name="p292139135817"></a><a name="p292139135817"></a>struct tracerout_result {</p>
<p id="p592149135818"><a name="p592149135818"></a><a name="p592149135818"></a>int max_ttl; // Maximum number of hops for probe packets. The values are as follows: –1, 1–255.</p>
<p id="p26191750191"><a name="p26191750191"></a><a name="p26191750191"></a>If this parameter is set to <strong id="b459151481715"><a name="b459151481715"></a><a name="b459151481715"></a>–1</strong>, the default value <strong id="b1059101481716"><a name="b1059101481716"></a><a name="b1059101481716"></a>30</strong> is used.</p>
<p id="p2922919582"><a name="p2922919582"></a><a name="p2922919582"></a>int tos; // This parameter can be used to set ToS priority in IPv4. The value ranges from –1 to 63. Or set traffic control value in IPv6. The value ranges from –1 to 255. A larger value indicates a higher priority. If this parameter is set to <strong id="b19379145810182"><a name="b19379145810182"></a><a name="b19379145810182"></a>-1</strong>, the default value <strong id="b8379135821811"><a name="b8379135821811"></a><a name="b8379135821811"></a>0</strong> is used.</p>
<p id="p99215975810"><a name="p99215975810"></a><a name="p99215975810"></a>int waittime; // Set the maximum waiting time for a detection response. The values are as follows: –1, 1–60 (unit: s). If this parameter is set to <strong id="b142551316191"><a name="b142551316191"></a><a name="b142551316191"></a>–1</strong>, the default value <strong id="b132517133193"><a name="b132517133193"></a><a name="b132517133193"></a>3s</strong> is used.</p>
<p id="p9924915818"><a name="p9924915818"></a><a name="p9924915818"></a>int sport; // Set the source port number. The value ranges from –1 to 65535. If this parameter is set to <strong id="b15993171871913"><a name="b15993171871913"></a><a name="b15993171871913"></a>-1</strong>, a random value greater than <strong id="b1899461811918"><a name="b1899461811918"></a><a name="b1899461811918"></a>30000</strong> is used by default.</p>
<p id="p159212995813"><a name="p159212995813"></a><a name="p159212995813"></a>int dport; // Set the destination port number. The value ranges from –1 to 65535. If this parameter is set to <strong id="b848202215190"><a name="b848202215190"></a><a name="b848202215190"></a>-1</strong>, a random value greater than <strong id="b049202219193"><a name="b049202219193"></a><a name="b049202219193"></a>30000</strong> is used by default.</p>
<p id="p492169205820"><a name="p492169205820"></a><a name="p492169205820"></a>char dest_ip[48]; // IP address of the target host</p>
<p id="p620713313587"><a name="p620713313587"></a><a name="p620713313587"></a>bool ipv6_flag; // Indicates whether to use the IPv6 protocol. Value <strong id="b1388922891910"><a name="b1388922891910"></a><a name="b1388922891910"></a>0</strong> indicates no, and value <strong id="b15889128181916"><a name="b15889128181916"></a><a name="b15889128181916"></a>1</strong> indicates yes.</p>
<p id="p29217912582"><a name="p29217912582"></a><a name="p29217912582"></a>bool reset_flag; // End all background traceroute processes on the device. When an exception occurs during the traceroute test, set this parameter to <strong id="b10658123271913"><a name="b10658123271913"></a><a name="b10658123271913"></a>1</strong> to end the processes on the device.</p>
<p id="p49210955812"><a name="p49210955812"></a><a name="p49210955812"></a>};</p>
<div class="note" id="note476213481342"><a name="note476213481342"></a><a name="note476213481342"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="p14762174810349"><a name="p14762174810349"></a><a name="p14762174810349"></a>If the port number is set to <strong id="b749355196"><a name="b749355196"></a><a name="b749355196"></a>0</strong>, the system uses a random value greater than <strong id="b1541335191911"><a name="b1541335191911"></a><a name="b1541335191911"></a>30000</strong>.</p>
</div></div>
</td>
</tr>
<tr id="row18924985817"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p19239165819"><a name="p19239165819"></a><a name="p19239165819"></a>ret_info[]</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p1951094580"><a name="p1951094580"></a><a name="p1951094580"></a>Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p39549175816"><a name="p39549175816"></a><a name="p39549175816"></a>struct dcmi_network_node_info *</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p12955914581"><a name="p12955914581"></a><a name="p12955914581"></a>Returned information of traceroute</p>
<p id="p16951693585"><a name="p16951693585"></a><a name="p16951693585"></a>struct tracerout_result {</p>
<p id="p89511920582"><a name="p89511920582"></a><a name="p89511920582"></a>int mask; // Mask, indicating whether the subsequent data is valid. For example, <strong id="b18170154211911"><a name="b18170154211911"></a><a name="b18170154211911"></a>0xFF</strong> indicates that the eight fields after the mask are valid.</p>
<p id="p179559135812"><a name="p179559135812"></a><a name="p179559135812"></a>char ip[48]; // IP address of the routing node</p>
<p id="p12959965815"><a name="p12959965815"></a><a name="p12959965815"></a>int snt; // Number of sent ICMP request packets</p>
<p id="p139520910583"><a name="p139520910583"></a><a name="p139520910583"></a>double loss; // Packet loss rate of the corresponding node</p>
<p id="p109518912588"><a name="p109518912588"></a><a name="p109518912588"></a>double last; // Response time of the latest packet (ms)</p>
<p id="p6951591588"><a name="p6951591588"></a><a name="p6951591588"></a>double avg; // Average response time of all packets (ms)</p>
<p id="p9958918587"><a name="p9958918587"></a><a name="p9958918587"></a>double best; // Fastest packet response time (ms)</p>
<p id="p199512910587"><a name="p199512910587"></a><a name="p199512910587"></a>double wrst; // Slowest packet response time (ms)</p>
<p id="p14950917585"><a name="p14950917585"></a><a name="p14950917585"></a>double stdev; // Standard deviation. A larger value indicates a less stable node.</p>
<p id="p9951396582"><a name="p9951396582"></a><a name="p9951396582"></a>char reserve[64]; // Additional extended information</p>
<p id="p1495119155818"><a name="p1495119155818"></a><a name="p1495119155818"></a>};</p>
<div class="note" id="note725719328015"><a name="note725719328015"></a><a name="note725719328015"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="p122571321305"><a name="p122571321305"></a><a name="p122571321305"></a>The returned information is stored in a structure array, with each node's information stored in a separate structure.</p>
</div></div>
</td>
</tr>
<tr id="row7933548175819"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p093464855812"><a name="p093464855812"></a><a name="p093464855812"></a>ret_info_size</p>
</td>
<td class="cellrowborder" valign="top" width="15.129999999999999%" headers="mcps1.1.5.1.2 "><p id="p159344487580"><a name="p159344487580"></a><a name="p159344487580"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.19%" headers="mcps1.1.5.1.3 "><p id="p993434845810"><a name="p993434845810"></a><a name="p993434845810"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p1934184817586"><a name="p1934184817586"></a><a name="p1934184817586"></a>Size of the structure for the node information.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section3642631135017"></a>**

<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_table19654399"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b3995934447231"><a name="b3995934447231"></a><a name="b3995934447231"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section498311885817"></a>**

- To prevent the traceroute command from being suspended due to ICMP packet loss, you are advised to run the two commands for IPv4 addresses at an interval of 3 seconds.
- If the command is forcibly interrupted on the host or the return value is  **-8020**, set  **param\_info.reset\_flag**  to  **1**  and call the  **dcmi\_set\_traceroute**  API to end the process on the device.
- After the traceroute command execution finishes, set  **param\_info.reset\_flag**  to  **1**  and call the  **dcmi\_set\_traceroute**  API to end the process on the device. This prevents the NPU performance from being affected by continuous process running.
- If  **param\_info.reset\_flag**  is set to  **1**, other parameters do not take effect.

**Restrictions<a name="section1298518810584"></a>**

**Table  1** Support in different deployment scenarios

<a name="table206991325174917"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515350556_row2051415544912"><th class="cellrowborder" rowspan="2" valign="top" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515350556_p20514155144920"><a name="en-us_topic_0000002515350556_p20514155144920"></a><a name="en-us_topic_0000002515350556_p20514155144920"></a>Product Model</p>
</th>
<th class="cellrowborder" colspan="2" valign="top" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515350556_p10514165204915"><a name="en-us_topic_0000002515350556_p10514165204915"></a><a name="en-us_topic_0000002515350556_p10514165204915"></a>PM (Bare Metal)</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515350556_p4514956491"><a name="en-us_topic_0000002515350556_p4514956491"></a><a name="en-us_topic_0000002515350556_p4514956491"></a>PM + Unprivileged Container</p>
</th>
</tr>
<tr id="en-us_topic_0000002515350556_row145145524918"><th class="cellrowborder" valign="top" id="mcps1.2.5.2.1"><p id="en-us_topic_0000002515350556_p85141551498"><a name="en-us_topic_0000002515350556_p85141551498"></a><a name="en-us_topic_0000002515350556_p85141551498"></a>Root User</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.5.2.2"><p id="en-us_topic_0000002515350556_p10514185134916"><a name="en-us_topic_0000002515350556_p10514185134916"></a><a name="en-us_topic_0000002515350556_p10514185134916"></a>Running User Group (Non-root User)</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.5.2.3"><p id="en-us_topic_0000002515350556_p7514656494"><a name="en-us_topic_0000002515350556_p7514656494"></a><a name="en-us_topic_0000002515350556_p7514656494"></a>Root User</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515350556_row135148510490"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p11514155490"><a name="en-us_topic_0000002515350556_p11514155490"></a><a name="en-us_topic_0000002515350556_p11514155490"></a><span id="en-us_topic_0000002515350556_text551465114917"><a name="en-us_topic_0000002515350556_text551465114917"></a><a name="en-us_topic_0000002515350556_text551465114917"></a>Atlas 900 A3 SuperPoD</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p185143564913"><a name="en-us_topic_0000002515350556_p185143564913"></a><a name="en-us_topic_0000002515350556_p185143564913"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p185145519497"><a name="en-us_topic_0000002515350556_p185145519497"></a><a name="en-us_topic_0000002515350556_p185145519497"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p1751413513498"><a name="en-us_topic_0000002515350556_p1751413513498"></a><a name="en-us_topic_0000002515350556_p1751413513498"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row135141553491"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p1351411524913"><a name="en-us_topic_0000002515350556_p1351411524913"></a><a name="en-us_topic_0000002515350556_p1351411524913"></a><span id="en-us_topic_0000002515350556_text175141751499"><a name="en-us_topic_0000002515350556_text175141751499"></a><a name="en-us_topic_0000002515350556_text175141751499"></a>Atlas 9000 A3 SuperPoD Cluster Computing System</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p1034521615495"><a name="en-us_topic_0000002515350556_p1034521615495"></a><a name="en-us_topic_0000002515350556_p1034521615495"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p6514145154913"><a name="en-us_topic_0000002515350556_p6514145154913"></a><a name="en-us_topic_0000002515350556_p6514145154913"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p17514145184914"><a name="en-us_topic_0000002515350556_p17514145184914"></a><a name="en-us_topic_0000002515350556_p17514145184914"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row183721515155518"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p923812139165"><a name="en-us_topic_0000002515350556_p923812139165"></a><a name="en-us_topic_0000002515350556_p923812139165"></a><span id="en-us_topic_0000002515350556_text8238111381610"><a name="en-us_topic_0000002515350556_text8238111381610"></a><a name="en-us_topic_0000002515350556_text8238111381610"></a>Atlas 800T A3 SuperPoD</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p1858019263555"><a name="en-us_topic_0000002515350556_p1858019263555"></a><a name="en-us_topic_0000002515350556_p1858019263555"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p125801926195517"><a name="en-us_topic_0000002515350556_p125801926195517"></a><a name="en-us_topic_0000002515350556_p125801926195517"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p125808265554"><a name="en-us_topic_0000002515350556_p125808265554"></a><a name="en-us_topic_0000002515350556_p125808265554"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row1828812107299"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p4288010192914"><a name="en-us_topic_0000002515350556_p4288010192914"></a><a name="en-us_topic_0000002515350556_p4288010192914"></a><span id="en-us_topic_0000002515350556_text17252201592911"><a name="en-us_topic_0000002515350556_text17252201592911"></a><a name="en-us_topic_0000002515350556_text17252201592911"></a>Atlas 800I A3 SuperPoD</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p14729171810297"><a name="en-us_topic_0000002515350556_p14729171810297"></a><a name="en-us_topic_0000002515350556_p14729171810297"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p187294185296"><a name="en-us_topic_0000002515350556_p187294185296"></a><a name="en-us_topic_0000002515350556_p187294185296"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p12729918182916"><a name="en-us_topic_0000002515350556_p12729918182916"></a><a name="en-us_topic_0000002515350556_p12729918182916"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row8768181557"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p1563212213554"><a name="en-us_topic_0000002515350556_p1563212213554"></a><a name="en-us_topic_0000002515350556_p1563212213554"></a><span id="en-us_topic_0000002515350556_text1963217219554"><a name="en-us_topic_0000002515350556_text1963217219554"></a><a name="en-us_topic_0000002515350556_text1963217219554"></a>A200T A3 Box8 SuperPoD server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p18671727165520"><a name="en-us_topic_0000002515350556_p18671727165520"></a><a name="en-us_topic_0000002515350556_p18671727165520"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p967152775514"><a name="en-us_topic_0000002515350556_p967152775514"></a><a name="en-us_topic_0000002515350556_p967152775514"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p667192712555"><a name="en-us_topic_0000002515350556_p667192712555"></a><a name="en-us_topic_0000002515350556_p667192712555"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row207580351817"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.2.1 mcps1.2.5.2.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p1339643920818"><a name="en-us_topic_0000002515350556_p1339643920818"></a><a name="en-us_topic_0000002515350556_p1339643920818"></a>Note: <strong id="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b130904228481512"><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b130904228481512"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b130904228481512"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b200767006881512"><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b200767006881512"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b200767006881512"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b90682012381512"><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b90682012381512"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b90682012381512"></a>NA</strong> indicates that the function is not involved and planned currently.</p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section1598913835819"></a>**

```c
...
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
    // todo: Record logs.
    return ret;
}
...
```

> **NOTE:** 
>
> _x.x.x.x_  indicates the destination IP address.
