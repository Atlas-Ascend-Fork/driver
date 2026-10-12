# dcmi\_get\_netdev\_tc\_stat\_info<a name="EN-US_TOPIC_0000002515343914"></a>

**Prototype<a name="section520615312440"></a>**

**dcmi\_get\_netdev\_tc\_stat\_info\(int card\_id, int device\_id, struct dcmi\_tc\_stat\_data \*network\_tc\_stat\_info\)**

**Description<a name="section1720715394411"></a>**

Obtains the total traffic of the eight TCs on each NPU.

**Parameter Description<a name="section1820723204410"></a>**

<a name="table224923194418"></a>
<table><thead align="left"><tr id="row3313153114413"><th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.1"><p id="p83136354416"><a name="p83136354416"></a><a name="p83136354416"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.15%" id="mcps1.1.5.1.2"><p id="p16313732442"><a name="p16313732442"></a><a name="p16313732442"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.3"><p id="p12313103174418"><a name="p12313103174418"></a><a name="p12313103174418"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="50.51%" id="mcps1.1.5.1.4"><p id="p113131039443"><a name="p113131039443"></a><a name="p113131039443"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row193131032444"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p831314310448"><a name="p831314310448"></a><a name="p831314310448"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p7313183184415"><a name="p7313183184415"></a><a name="p7313183184415"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p20313333443"><a name="p20313333443"></a><a name="p20313333443"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p63137316446"><a name="p63137316446"></a><a name="p63137316446"></a>Device ID. The supported IDs can be obtained by calling <strong id="b1027710017102848"><a name="b1027710017102848"></a><a name="b1027710017102848"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row1631317384413"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p731319314416"><a name="p731319314416"></a><a name="p731319314416"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p83131133444"><a name="p83131133444"></a><a name="p83131133444"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p15313534444"><a name="p15313534444"></a><a name="p15313534444"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p13141364416"><a name="p13141364416"></a><a name="p13141364416"></a>Chip ID, which can be obtained by calling <strong id="b1196872434102518"><a name="b1196872434102518"></a><a name="b1196872434102518"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p183141336444"><a name="p183141336444"></a><a name="p183141336444"></a>NPU: [0, <strong id="b1540678659105352"><a name="b1540678659105352"></a><a name="b1540678659105352"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row2031473114410"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p43141931447"><a name="p43141931447"></a><a name="p43141931447"></a>network_tc_stat_info</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p19314537448"><a name="p19314537448"></a><a name="p19314537448"></a>Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p16314833443"><a name="p16314833443"></a><a name="p16314833443"></a>dcmi_tc_stat_data</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p16523132017109"><a name="p16523132017109"></a><a name="p16523132017109"></a>struct dcmi_tc_stat_data {</p>
<p id="p195237205105"><a name="p195237205105"></a><a name="p195237205105"></a>unsigned long long tx_packet[TC_QUE_NUM]; // Obtain the sent data traffic.</p>
<p id="p17523182041013"><a name="p17523182041013"></a><a name="p17523182041013"></a>unsigned long long rx_packet[TC_QUE_NUM]; // Obtain the received data traffic.</p>
<p id="p1352372041019"><a name="p1352372041019"></a><a name="p1352372041019"></a>unsigned long long reserved[TC_QUE_NUM]; // Reserved</p>
<p id="p957693672414"><a name="p957693672414"></a><a name="p957693672414"></a>}</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section1322323124420"></a>**

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
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b190697384485633"><a name="b190697384485633"></a><a name="b190697384485633"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section523218344416"></a>**

None.

**Restrictions<a name="section11232103184419"></a>**

Atlas 200T A2 Box16 heterogeneous subrack, Atlas 800T A2 training server, Atlas 800I A2 inference server,  Atlas 900 A2 PoD cluster basic unit, and A200I A2 Box heterogeneous component: This API can be used in PM + privileged container scenarios.

**Table  1** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row3169171463617"><th class="cellrowborder" valign="top" width="27.02%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p8558112793214"><a name="en-us_topic_0000002515503852_p8558112793214"></a><a name="en-us_topic_0000002515503852_p8558112793214"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="18.94%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p655852717326"><a name="en-us_topic_0000002515503852_p655852717326"></a><a name="en-us_topic_0000002515503852_p655852717326"></a><strong id="en-us_topic_0000002515503852_b2084215313512"><a name="en-us_topic_0000002515503852_b2084215313512"></a><a name="en-us_topic_0000002515503852_b2084215313512"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="27.02%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p10558122715327"><a name="en-us_topic_0000002515503852_p10558122715327"></a><a name="en-us_topic_0000002515503852_p10558122715327"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b14917518517"><a name="en-us_topic_0000002515503852_b14917518517"></a><a name="en-us_topic_0000002515503852_b14917518517"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="27.02%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p15581927103214"><a name="en-us_topic_0000002515503852_p15581927103214"></a><a name="en-us_topic_0000002515503852_p15581927103214"></a><strong id="en-us_topic_0000002515503852_b193961161854"><a name="en-us_topic_0000002515503852_b193961161854"></a><a name="en-us_topic_0000002515503852_b193961161854"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_row1816915147366"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p6169151423619"><a name="en-us_topic_0000002515503852_p6169151423619"></a><a name="en-us_topic_0000002515503852_p6169151423619"></a><span id="en-us_topic_0000002515503852_ph116911144369"><a name="en-us_topic_0000002515503852_ph116911144369"></a><a name="en-us_topic_0000002515503852_ph116911144369"></a><span id="en-us_topic_0000002515503852_text71694149366"><a name="en-us_topic_0000002515503852_text71694149366"></a><a name="en-us_topic_0000002515503852_text71694149366"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p316911142362"><a name="en-us_topic_0000002515503852_p316911142362"></a><a name="en-us_topic_0000002515503852_p316911142362"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p816918145361"><a name="en-us_topic_0000002515503852_p816918145361"></a><a name="en-us_topic_0000002515503852_p816918145361"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p9472202011368"><a name="en-us_topic_0000002515503852_p9472202011368"></a><a name="en-us_topic_0000002515503852_p9472202011368"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row01692149364"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10169141412361"><a name="en-us_topic_0000002515503852_p10169141412361"></a><a name="en-us_topic_0000002515503852_p10169141412361"></a><span id="en-us_topic_0000002515503852_text1516951453611"><a name="en-us_topic_0000002515503852_text1516951453611"></a><a name="en-us_topic_0000002515503852_text1516951453611"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p16169121403617"><a name="en-us_topic_0000002515503852_p16169121403617"></a><a name="en-us_topic_0000002515503852_p16169121403617"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p171698141361"><a name="en-us_topic_0000002515503852_p171698141361"></a><a name="en-us_topic_0000002515503852_p171698141361"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1348119209364"><a name="en-us_topic_0000002515503852_p1348119209364"></a><a name="en-us_topic_0000002515503852_p1348119209364"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row17169814163612"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p2169414113616"><a name="en-us_topic_0000002515503852_p2169414113616"></a><a name="en-us_topic_0000002515503852_p2169414113616"></a><span id="en-us_topic_0000002515503852_text2169111443615"><a name="en-us_topic_0000002515503852_text2169111443615"></a><a name="en-us_topic_0000002515503852_text2169111443615"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p141696146366"><a name="en-us_topic_0000002515503852_p141696146366"></a><a name="en-us_topic_0000002515503852_p141696146366"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p3169114113611"><a name="en-us_topic_0000002515503852_p3169114113611"></a><a name="en-us_topic_0000002515503852_p3169114113611"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p13489162017366"><a name="en-us_topic_0000002515503852_p13489162017366"></a><a name="en-us_topic_0000002515503852_p13489162017366"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row01691314183618"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1616951453614"><a name="en-us_topic_0000002515503852_p1616951453614"></a><a name="en-us_topic_0000002515503852_p1616951453614"></a><span id="en-us_topic_0000002515503852_text17169141413610"><a name="en-us_topic_0000002515503852_text17169141413610"></a><a name="en-us_topic_0000002515503852_text17169141413610"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p316911419369"><a name="en-us_topic_0000002515503852_p316911419369"></a><a name="en-us_topic_0000002515503852_p316911419369"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p141697141368"><a name="en-us_topic_0000002515503852_p141697141368"></a><a name="en-us_topic_0000002515503852_p141697141368"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1949213202364"><a name="en-us_topic_0000002515503852_p1949213202364"></a><a name="en-us_topic_0000002515503852_p1949213202364"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1169151413367"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1716931483615"><a name="en-us_topic_0000002515503852_p1716931483615"></a><a name="en-us_topic_0000002515503852_p1716931483615"></a><span id="en-us_topic_0000002515503852_text81691114103617"><a name="en-us_topic_0000002515503852_text81691114103617"></a><a name="en-us_topic_0000002515503852_text81691114103617"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p816912141365"><a name="en-us_topic_0000002515503852_p816912141365"></a><a name="en-us_topic_0000002515503852_p816912141365"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15169191411369"><a name="en-us_topic_0000002515503852_p15169191411369"></a><a name="en-us_topic_0000002515503852_p15169191411369"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1497112043619"><a name="en-us_topic_0000002515503852_p1497112043619"></a><a name="en-us_topic_0000002515503852_p1497112043619"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row111691014173614"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19169514103610"><a name="en-us_topic_0000002515503852_p19169514103610"></a><a name="en-us_topic_0000002515503852_p19169514103610"></a><span id="en-us_topic_0000002515503852_text01406115324"><a name="en-us_topic_0000002515503852_text01406115324"></a><a name="en-us_topic_0000002515503852_text01406115324"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p181691314163611"><a name="en-us_topic_0000002515503852_p181691314163611"></a><a name="en-us_topic_0000002515503852_p181691314163611"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p516961453612"><a name="en-us_topic_0000002515503852_p516961453612"></a><a name="en-us_topic_0000002515503852_p516961453612"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p25021520143613"><a name="en-us_topic_0000002515503852_p25021520143613"></a><a name="en-us_topic_0000002515503852_p25021520143613"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1816912147365"><td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1916941423613"><a name="en-us_topic_0000002515503852_p1916941423613"></a><a name="en-us_topic_0000002515503852_p1916941423613"></a><span id="en-us_topic_0000002515503852_text121691149365"><a name="en-us_topic_0000002515503852_text121691149365"></a><a name="en-us_topic_0000002515503852_text121691149365"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="18.94%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p14169101413366"><a name="en-us_topic_0000002515503852_p14169101413366"></a><a name="en-us_topic_0000002515503852_p14169101413366"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p10169614163611"><a name="en-us_topic_0000002515503852_p10169614163611"></a><a name="en-us_topic_0000002515503852_p10169614163611"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="27.02%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p12507102016363"><a name="en-us_topic_0000002515503852_p12507102016363"></a><a name="en-us_topic_0000002515503852_p12507102016363"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1317019147361"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2170191493613"><a name="en-us_topic_0000002515503852_p2170191493613"></a><a name="en-us_topic_0000002515503852_p2170191493613"></a><span id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section18238203144413"></a>**

```c
...
int ret = 0;
int card_id = 0;
int device_id = 0;
struct dcmi_tc_stat_data network_tc_stat_info = {0};
ret = dcmi_get_netdev_tc_stat_info (card_id, device_id, &network_tc_stat_info);
if (ret != 0){
    //todo: Record logs.
    return ret;
}
...
```
