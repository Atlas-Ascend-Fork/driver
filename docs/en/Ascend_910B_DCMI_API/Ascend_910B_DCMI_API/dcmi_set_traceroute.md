# dcmi\_set\_traceroute<a name="EN-US_TOPIC_0000002515503770"></a>

**Prototype<a name="section1994468155813"></a>**

**int dcmi\_set\_traceroute \(int card\_id, int device\_id, struct dcmi\_traceroute\_info param\_info, struct dcmi\_network\_node\_info \*ret\_info\[\], unsigned int ret\_info\_size\)**

**Description<a name="section10946138105811"></a>**

Configures traceroute parameters to detect the network nodes where the packets pass through.

**Parameter Description<a name="section1294798155811"></a>**

<a name="table1499319814585"></a>
<table><thead align="left"><tr id="row119149115814"><th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.1"><p id="p191169145811"><a name="p191169145811"></a><a name="p191169145811"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.15%" id="mcps1.1.5.1.2"><p id="p199115925812"><a name="p199115925812"></a><a name="p199115925812"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.3"><p id="p13911190581"><a name="p13911190581"></a><a name="p13911190581"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="50.51%" id="mcps1.1.5.1.4"><p id="p1091129155817"><a name="p1091129155817"></a><a name="p1091129155817"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row7915915813"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p13915975816"><a name="p13915975816"></a><a name="p13915975816"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p39110995818"><a name="p39110995818"></a><a name="p39110995818"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p13911797583"><a name="p13911797583"></a><a name="p13911797583"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p591129165816"><a name="p591129165816"></a><a name="p591129165816"></a>Device ID. The supported IDs can be obtained by calling <strong id="b1178304050102845"><a name="b1178304050102845"></a><a name="b1178304050102845"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row4912099583"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p109169205814"><a name="p109169205814"></a><a name="p109169205814"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p29139115814"><a name="p29139115814"></a><a name="p29139115814"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p1691993587"><a name="p1691993587"></a><a name="p1691993587"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p17911892586"><a name="p17911892586"></a><a name="p17911892586"></a>Chip ID, which can be obtained by calling <strong id="b1098960604102515"><a name="b1098960604102515"></a><a name="b1098960604102515"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p17914911584"><a name="p17914911584"></a><a name="p17914911584"></a>NPU: [0, <strong id="b830617917105350"><a name="b830617917105350"></a><a name="b830617917105350"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row139113916586"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p89210985814"><a name="p89210985814"></a><a name="p89210985814"></a>param_info</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p20928915813"><a name="p20928915813"></a><a name="p20928915813"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p192139195819"><a name="p192139195819"></a><a name="p192139195819"></a>struct traceroute</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p109279165817"><a name="p109279165817"></a><a name="p109279165817"></a>Input parameters of traceroute.</p>
<p id="p292139135817"><a name="p292139135817"></a><a name="p292139135817"></a>struct tracerout_result {</p>
<p id="p592149135818"><a name="p592149135818"></a><a name="p592149135818"></a>int max_ttl; // Maximum number of hops for probe packets. The values are as follows: –1, 1–255.</p>
<p id="p26191750191"><a name="p26191750191"></a><a name="p26191750191"></a>If this parameter is set to <strong id="b129614891719"><a name="b129614891719"></a><a name="b129614891719"></a>–1</strong>, the default value <strong id="b1329674841712"><a name="b1329674841712"></a><a name="b1329674841712"></a>30</strong> is used.</p>
<p id="p2922919582"><a name="p2922919582"></a><a name="p2922919582"></a>int tos; // This parameter can be used to set ToS priority in IPv4. The value ranges from –1 to 63. Or set traffic control value in IPv6. The value ranges from –1 to 255. A larger value indicates a higher priority. If this parameter is set to <strong id="b2528122211812"><a name="b2528122211812"></a><a name="b2528122211812"></a>–1</strong>, the default value <strong id="b1852818227185"><a name="b1852818227185"></a><a name="b1852818227185"></a>0</strong> is used.</p>
<p id="p99215975810"><a name="p99215975810"></a><a name="p99215975810"></a>int waittime; // Set the maximum waiting time for a detection response. The values are as follows: –1, 1–60 (unit: s). If this parameter is set to <strong id="b1916541931918"><a name="b1916541931918"></a><a name="b1916541931918"></a>–1</strong>, the default value <strong id="b17166819151912"><a name="b17166819151912"></a><a name="b17166819151912"></a>3s</strong> is used.</p>
<p id="p9924915818"><a name="p9924915818"></a><a name="p9924915818"></a>int sport; // Set the source port number. The value ranges from –1 to 65535. If this parameter is set to <strong id="b344519536191"><a name="b344519536191"></a><a name="b344519536191"></a>–1</strong>, a random value greater than <strong id="b6445115361919"><a name="b6445115361919"></a><a name="b6445115361919"></a>30000</strong> is used by default.</p>
<p id="p159212995813"><a name="p159212995813"></a><a name="p159212995813"></a>int dport; // Set the destination port number. The value ranges from –1 to 65535. If this parameter is set to <strong id="b67365781915"><a name="b67365781915"></a><a name="b67365781915"></a>–1</strong>, a random value greater than <strong id="b1673195771917"><a name="b1673195771917"></a><a name="b1673195771917"></a>30000</strong> is used by default.</p>
<p id="p492169205820"><a name="p492169205820"></a><a name="p492169205820"></a>char dest_ip[48]; // IP address of the target host</p>
<p id="p620713313587"><a name="p620713313587"></a><a name="p620713313587"></a>bool ipv6_flag; // Whether to use the IPv6 protocol. Value <strong id="b18940122110209"><a name="b18940122110209"></a><a name="b18940122110209"></a>0</strong> indicates no, and value <strong id="b49401521152010"><a name="b49401521152010"></a><a name="b49401521152010"></a>1</strong> indicates yes.</p>
<p id="p29217912582"><a name="p29217912582"></a><a name="p29217912582"></a>bool reset_flag; // End all background traceroute processes on the device. When an exception occurs during the traceroute test, set this parameter to <strong id="b96101302201"><a name="b96101302201"></a><a name="b96101302201"></a>1</strong> to end the processes on the device.</p>
<p id="p49210955812"><a name="p49210955812"></a><a name="p49210955812"></a>};</p>
<div class="note" id="note476213481342"><a name="note476213481342"></a><a name="note476213481342"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="p14762174810349"><a name="p14762174810349"></a><a name="p14762174810349"></a>If the port number is set to <strong id="b2091564910203"><a name="b2091564910203"></a><a name="b2091564910203"></a>0</strong>, the system uses a random value greater than <strong id="b11915149192018"><a name="b11915149192018"></a><a name="b11915149192018"></a>30000</strong>.</p>
</div></div>
</td>
</tr>
<tr id="row18924985817"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p19239165819"><a name="p19239165819"></a><a name="p19239165819"></a>ret_info[]</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p1951094580"><a name="p1951094580"></a><a name="p1951094580"></a>Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p39549175816"><a name="p39549175816"></a><a name="p39549175816"></a>struct dcmi_network_node_info *</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p12955914581"><a name="p12955914581"></a><a name="p12955914581"></a>Returned information of traceroute</p>
<p id="p16951693585"><a name="p16951693585"></a><a name="p16951693585"></a>struct tracerout_result {</p>
<p id="p89511920582"><a name="p89511920582"></a><a name="p89511920582"></a>int mask; // Mask, indicating whether the subsequent data is valid. For example, <strong id="b1071175782011"><a name="b1071175782011"></a><a name="b1071175782011"></a>0xFF</strong> indicates that the eight fields after the mask are valid.</p>
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
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p159344487580"><a name="p159344487580"></a><a name="p159344487580"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p993434845810"><a name="p993434845810"></a><a name="p993434845810"></a>unsigned int</p>
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
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b148744767785631"><a name="b148744767785631"></a><a name="b148744767785631"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
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

<a name="table6665182042413"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1514515016392"><th class="cellrowborder" valign="top" width="27.02%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p15286130203213"><a name="en-us_topic_0000002515503852_p15286130203213"></a><a name="en-us_topic_0000002515503852_p15286130203213"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="18.94%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p132861730113218"><a name="en-us_topic_0000002515503852_p132861730113218"></a><a name="en-us_topic_0000002515503852_p132861730113218"></a><strong id="en-us_topic_0000002515503852_b065416910513"><a name="en-us_topic_0000002515503852_b065416910513"></a><a name="en-us_topic_0000002515503852_b065416910513"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="27.02%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p22861530183212"><a name="en-us_topic_0000002515503852_p22861530183212"></a><a name="en-us_topic_0000002515503852_p22861530183212"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b4800131019511"><a name="en-us_topic_0000002515503852_b4800131019511"></a><a name="en-us_topic_0000002515503852_b4800131019511"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="27.02%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p1928683003217"><a name="en-us_topic_0000002515503852_p1928683003217"></a><a name="en-us_topic_0000002515503852_p1928683003217"></a><strong id="en-us_topic_0000002515503852_b79191511557"><a name="en-us_topic_0000002515503852_b79191511557"></a><a name="en-us_topic_0000002515503852_b79191511557"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_row514512014397"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p14145100183919"><a name="en-us_topic_0000002515503852_p14145100183919"></a><a name="en-us_topic_0000002515503852_p14145100183919"></a><span id="en-us_topic_0000002515503852_ph15145206394"><a name="en-us_topic_0000002515503852_ph15145206394"></a><a name="en-us_topic_0000002515503852_ph15145206394"></a><span id="en-us_topic_0000002515503852_text141451204393"><a name="en-us_topic_0000002515503852_text141451204393"></a><a name="en-us_topic_0000002515503852_text141451204393"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p114519073913"><a name="en-us_topic_0000002515503852_p114519073913"></a><a name="en-us_topic_0000002515503852_p114519073913"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p672045395"><a name="en-us_topic_0000002515503852_p672045395"></a><a name="en-us_topic_0000002515503852_p672045395"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p14102046396"><a name="en-us_topic_0000002515503852_p14102046396"></a><a name="en-us_topic_0000002515503852_p14102046396"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1314590133918"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p914590143915"><a name="en-us_topic_0000002515503852_p914590143915"></a><a name="en-us_topic_0000002515503852_p914590143915"></a><span id="en-us_topic_0000002515503852_text151456017398"><a name="en-us_topic_0000002515503852_text151456017398"></a><a name="en-us_topic_0000002515503852_text151456017398"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p114512012392"><a name="en-us_topic_0000002515503852_p114512012392"></a><a name="en-us_topic_0000002515503852_p114512012392"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p161319419393"><a name="en-us_topic_0000002515503852_p161319419393"></a><a name="en-us_topic_0000002515503852_p161319419393"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1019184143917"><a name="en-us_topic_0000002515503852_p1019184143917"></a><a name="en-us_topic_0000002515503852_p1019184143917"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1814514011392"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p414590193920"><a name="en-us_topic_0000002515503852_p414590193920"></a><a name="en-us_topic_0000002515503852_p414590193920"></a><span id="en-us_topic_0000002515503852_text161451704394"><a name="en-us_topic_0000002515503852_text161451704394"></a><a name="en-us_topic_0000002515503852_text161451704394"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1514516018392"><a name="en-us_topic_0000002515503852_p1514516018392"></a><a name="en-us_topic_0000002515503852_p1514516018392"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p18242403910"><a name="en-us_topic_0000002515503852_p18242403910"></a><a name="en-us_topic_0000002515503852_p18242403910"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p18307415390"><a name="en-us_topic_0000002515503852_p18307415390"></a><a name="en-us_topic_0000002515503852_p18307415390"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1114514093913"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1145407399"><a name="en-us_topic_0000002515503852_p1145407399"></a><a name="en-us_topic_0000002515503852_p1145407399"></a><span id="en-us_topic_0000002515503852_text161451007395"><a name="en-us_topic_0000002515503852_text161451007395"></a><a name="en-us_topic_0000002515503852_text161451007395"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p314518015391"><a name="en-us_topic_0000002515503852_p314518015391"></a><a name="en-us_topic_0000002515503852_p314518015391"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13378413911"><a name="en-us_topic_0000002515503852_p13378413911"></a><a name="en-us_topic_0000002515503852_p13378413911"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p8431344398"><a name="en-us_topic_0000002515503852_p8431344398"></a><a name="en-us_topic_0000002515503852_p8431344398"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1114514017397"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1114510163910"><a name="en-us_topic_0000002515503852_p1114510163910"></a><a name="en-us_topic_0000002515503852_p1114510163910"></a><span id="en-us_topic_0000002515503852_text01466093919"><a name="en-us_topic_0000002515503852_text01466093919"></a><a name="en-us_topic_0000002515503852_text01466093919"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p214613010391"><a name="en-us_topic_0000002515503852_p214613010391"></a><a name="en-us_topic_0000002515503852_p214613010391"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15471840391"><a name="en-us_topic_0000002515503852_p15471840391"></a><a name="en-us_topic_0000002515503852_p15471840391"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p45014483920"><a name="en-us_topic_0000002515503852_p45014483920"></a><a name="en-us_topic_0000002515503852_p45014483920"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row3146140203913"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p0146160143914"><a name="en-us_topic_0000002515503852_p0146160143914"></a><a name="en-us_topic_0000002515503852_p0146160143914"></a><span id="en-us_topic_0000002515503852_text181621218183218"><a name="en-us_topic_0000002515503852_text181621218183218"></a><a name="en-us_topic_0000002515503852_text181621218183218"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p141461506394"><a name="en-us_topic_0000002515503852_p141461506394"></a><a name="en-us_topic_0000002515503852_p141461506394"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p353114123912"><a name="en-us_topic_0000002515503852_p353114123912"></a><a name="en-us_topic_0000002515503852_p353114123912"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p135715415392"><a name="en-us_topic_0000002515503852_p135715415392"></a><a name="en-us_topic_0000002515503852_p135715415392"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row31461604397"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p714619015395"><a name="en-us_topic_0000002515503852_p714619015395"></a><a name="en-us_topic_0000002515503852_p714619015395"></a><span id="en-us_topic_0000002515503852_text111462014399"><a name="en-us_topic_0000002515503852_text111462014399"></a><a name="en-us_topic_0000002515503852_text111462014399"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1314610043913"><a name="en-us_topic_0000002515503852_p1314610043913"></a><a name="en-us_topic_0000002515503852_p1314610043913"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p8602483913"><a name="en-us_topic_0000002515503852_p8602483913"></a><a name="en-us_topic_0000002515503852_p8602483913"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11635413398"><a name="en-us_topic_0000002515503852_p11635413398"></a><a name="en-us_topic_0000002515503852_p11635413398"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row31464013910"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p5146120153917"><a name="en-us_topic_0000002515503852_p5146120153917"></a><a name="en-us_topic_0000002515503852_p5146120153917"></a><span id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
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
    //todo: Record logs.
    return ret;
}
...
```

> **NOTE:** 
>
> _x.x.x.x_  indicates the destination IP address.
