# dcmi\_sm\_decrypt<a name="EN-US_TOPIC_0000002546990447"></a>

**Prototype<a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_toc533412077"></a>**

**int dcmi\_sm\_decrypt\(int card\_id, int device\_id, struct dcmi\_sm\_parm\* parm, struct dcmi\_sm\_data\*data\)**

**Description<a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_toc533412078"></a>**

Obtains the decrypted plaintext by calling this API and entering the encrypted ciphertext, the decryption key, and the Chinese decryption algorithm type.

**Parameter Description<a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_toc533412079"></a>**

<a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_table10480683"></a>
<table><thead align="left"><tr id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_row7580267"><th class="cellrowborder" valign="top" width="17%" id="mcps1.1.5.1.1"><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p10021890"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p10021890"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p10021890"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="16.99%" id="mcps1.1.5.1.2"><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p6466753"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p6466753"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p6466753"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="16.009999999999998%" id="mcps1.1.5.1.3"><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p54045009"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p54045009"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p54045009"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.5.1.4"><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p15569626"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p15569626"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p15569626"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_row10560021192510"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p36741947142813"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p36741947142813"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p36741947142813"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p96741747122818"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p96741747122818"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p96741747122818"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p46747472287"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p46747472287"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p46747472287"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1467413474281"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1467413474281"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1467413474281"></a>Device ID. The supported IDs can be obtained by calling <strong id="b127097719581321"><a name="b127097719581321"></a><a name="b127097719581321"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_row1155711494235"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p7711145152918"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p7711145152918"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p7711145152918"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p671116522914"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p671116522914"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p671116522914"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1771116572910"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1771116572910"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1771116572910"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001148530297_p11747451997"><a name="en-us_topic_0000001148530297_p11747451997"></a><a name="en-us_topic_0000001148530297_p11747451997"></a>Chip ID, which can be obtained by calling <strong id="b30768948733025"><a name="b30768948733025"></a><a name="b30768948733025"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="en-us_topic_0000001148530297_p1377514432141"><a name="en-us_topic_0000001148530297_p1377514432141"></a><a name="en-us_topic_0000001148530297_p1377514432141"></a>NPU: [0, <strong id="b17495364836441"><a name="b17495364836441"></a><a name="b17495364836441"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515350528_p10218227151413"><a name="en-us_topic_0000002515350528_p10218227151413"></a><a name="en-us_topic_0000002515350528_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515350528_b551771083414"><a name="en-us_topic_0000002515350528_b551771083414"></a><a name="en-us_topic_0000002515350528_b551771083414"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515350528_b1263401315345"><a name="en-us_topic_0000002515350528_b1263401315345"></a><a name="en-us_topic_0000002515350528_b1263401315345"></a>2</strong>. A <strong id="en-us_topic_0000002515350528_b544811733415"><a name="en-us_topic_0000002515350528_b544811733415"></a><a name="en-us_topic_0000002515350528_b544811733415"></a>device_id</strong> of <strong id="en-us_topic_0000002515350528_b18565142019343"><a name="en-us_topic_0000002515350528_b18565142019343"></a><a name="en-us_topic_0000002515350528_b18565142019343"></a>0</strong> or <strong id="en-us_topic_0000002515350528_b1255272314342"><a name="en-us_topic_0000002515350528_b1255272314342"></a><a name="en-us_topic_0000002515350528_b1255272314342"></a>1</strong> indicates an NPU. A <strong id="en-us_topic_0000002515350528_b3731133414341"><a name="en-us_topic_0000002515350528_b3731133414341"></a><a name="en-us_topic_0000002515350528_b3731133414341"></a>device_id</strong> of <strong id="en-us_topic_0000002515350528_b174037378345"><a name="en-us_topic_0000002515350528_b174037378345"></a><a name="en-us_topic_0000002515350528_b174037378345"></a>2</strong> indicates an MCU.</p>
</div></div>
</td>
</tr>
<tr id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_row15462171542913"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p8175616161117"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p8175616161117"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p8175616161117"></a>parm</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1817571618112"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1817571618112"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p1817571618112"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p20175191611119"><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p20175191611119"></a><a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_p20175191611119"></a>struct dcmi_sm_parm*</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p2447145210116"><a name="p2447145210116"></a><a name="p2447145210116"></a>struct dcmi_sm_parm {</p>
<p id="p9447105215112"><a name="p9447105215112"></a><a name="p9447105215112"></a>unsigned int key_type;</p>
<p id="p353613473350"><a name="p353613473350"></a><a name="p353613473350"></a>unsigned int key_len;// The length of the SM4 key is 16 bytes. SM3 is not involved.</p>
<p id="p144471352121113"><a name="p144471352121113"></a><a name="p144471352121113"></a>unsigned int iv_len;// The initial length of SM4 is 16 bytes. SM3 is not involved.</p>
<p id="p174471152131111"><a name="p174471152131111"></a><a name="p174471152131111"></a>unsigned int reserves;// Reserved</p>
<p id="p644785213115"><a name="p644785213115"></a><a name="p644785213115"></a>unsigned char iv[64];// Initialization vector of the CBC algorithm</p>
<p id="p4447135212118"><a name="p4447135212118"></a><a name="p4447135212118"></a>unsigned char key[512];// Key</p>
<p id="p104477521114"><a name="p104477521114"></a><a name="p104477521114"></a>unsigned char reserved[512];// Reserved</p>
<p id="p74478527115"><a name="p74478527115"></a><a name="p74478527115"></a>};</p>
<p id="p444755201118"><a name="p444755201118"></a><a name="p444755201118"></a>The arguments of <strong id="b74781096562"><a name="b74781096562"></a><a name="b74781096562"></a>key_type</strong> are as follows:</p>
<p id="p154471252131113"><a name="p154471252131113"></a><a name="p154471252131113"></a>enum sm_key_type{</p>
<p id="p2381738183616"><a name="p2381738183616"></a><a name="p2381738183616"></a>SM3_NORMAL_SUMMARY = 0,// SM3 hash algorithm operation</p>
<p id="p19447205261112"><a name="p19447205261112"></a><a name="p19447205261112"></a>SM4_CBC_ENCRYPT = 1,// SM4 CBC encryption algorithm</p>
<p id="p1447052141119"><a name="p1447052141119"></a><a name="p1447052141119"></a>SM4_CBC_DECRYPT = 2,// SM4 CBC decryption algorithm</p>
<p id="p844875211114"><a name="p844875211114"></a><a name="p844875211114"></a>};</p>
</td>
</tr>
<tr id="row15651816125"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="p1965515129"><a name="p1965515129"></a><a name="p1965515129"></a>data</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="p15244731115812"><a name="p15244731115812"></a><a name="p15244731115812"></a>Input; output</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="p3651712129"><a name="p3651712129"></a><a name="p3651712129"></a>struct dcmi_sm_data*</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p619755115125"><a name="p619755115125"></a><a name="p619755115125"></a>struct dcmi_sm_data {</p>
<p id="p17197155141219"><a name="p17197155141219"></a><a name="p17197155141219"></a>const unsigned char *in_buf;</p>
<p id="p61971551171211"><a name="p61971551171211"></a><a name="p61971551171211"></a>unsigned in_len;// The SM3/SM4 length cannot exceed 3072 bytes, and the SM4 length must be an integer multiple of 16 bytes.</p>
<p id="p1695213111374"><a name="p1695213111374"></a><a name="p1695213111374"></a>unsigned char *out_buf;// Output buffer</p>
<p id="p9197195117127"><a name="p9197195117127"></a><a name="p9197195117127"></a>unsigned int *out_len;// Output buffer length</p>
<p id="p111975516127"><a name="p111975516127"></a><a name="p111975516127"></a>};</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_section1256282115569"></a>**

<a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_table19654399"></a>
<table><thead align="left"><tr id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001819869974_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b176735415372247"><a name="b176735415372247"></a><a name="b176735415372247"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_toc533412081"></a>**

None

**Restrictions<a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_toc533412082"></a>**

**Table  1** Support in different deployment scenarios

<a name="table6183194116432"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_row119194469302"><th class="cellrowborder" rowspan="2" valign="top" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p974813717495"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p974813717495"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p974813717495"></a>Product Model</p>
</th>
<th class="cellrowborder" colspan="2" valign="top" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515350556_p114288580415"><a name="en-us_topic_0000002515350556_p114288580415"></a><a name="en-us_topic_0000002515350556_p114288580415"></a>PM (Bare Metal)</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p992084633012"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p992084633012"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p992084633012"></a>PM + Unprivileged Container</p>
</th>
</tr>
<tr id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_row19216056194814"><th class="cellrowborder" valign="top" id="mcps1.2.5.2.1"><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p792013462306"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p792013462306"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p792013462306"></a>Root User</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.5.2.2"><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p16920346133016"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p16920346133016"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p16920346133016"></a>Running User Group (Non-root User)</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.5.2.3"><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p166440421324"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p166440421324"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p166440421324"></a>Root User</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_p1378374019445"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_p1378374019445"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_p1378374019445"></a><span id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_text262011415447"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_text262011415447"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_text262011415447"></a><span id="en-us_topic_0000002515350556_text1564782665814"><a name="en-us_topic_0000002515350556_text1564782665814"></a><a name="en-us_topic_0000002515350556_text1564782665814"></a>Atlas 900 A3 SuperPoD</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p11683124944919"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p11683124944919"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p11683124944919"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p36831949194913"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p36831949194913"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p36831949194913"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p2683144919499"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p2683144919499"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_p2683144919499"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_row82952324359"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_p137832403444"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_p137832403444"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_p137832403444"></a><span id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_text6814122411310"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_text6814122411310"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001819869974_text6814122411310"></a><span id="en-us_topic_0000002515350556_text1123515513517"><a name="en-us_topic_0000002515350556_text1123515513517"></a><a name="en-us_topic_0000002515350556_text1123515513517"></a>Atlas 9000 A3 SuperPoD Cluster Computing System</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p1285753073419"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p1285753073419"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p1285753073419"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p1985743013346"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p1985743013346"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p1985743013346"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p0857203013412"><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p0857203013412"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000001820029662_en-us_topic_0000001251227155_en-us_topic_0000001178213210_en-us_topic_0000001147728951_p0857203013412"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row12693311552"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p17872181618166"><a name="en-us_topic_0000002515350556_p17872181618166"></a><a name="en-us_topic_0000002515350556_p17872181618166"></a><span id="en-us_topic_0000002515350556_text17872121641614"><a name="en-us_topic_0000002515350556_text17872121641614"></a><a name="en-us_topic_0000002515350556_text17872121641614"></a>Atlas 800T A3 SuperPoD</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p177521042115520"><a name="en-us_topic_0000002515350556_p177521042115520"></a><a name="en-us_topic_0000002515350556_p177521042115520"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p12752114215559"><a name="en-us_topic_0000002515350556_p12752114215559"></a><a name="en-us_topic_0000002515350556_p12752114215559"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p18752104275512"><a name="en-us_topic_0000002515350556_p18752104275512"></a><a name="en-us_topic_0000002515350556_p18752104275512"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row179407221298"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p696322918294"><a name="en-us_topic_0000002515350556_p696322918294"></a><a name="en-us_topic_0000002515350556_p696322918294"></a><span id="en-us_topic_0000002515350556_text15963162972914"><a name="en-us_topic_0000002515350556_text15963162972914"></a><a name="en-us_topic_0000002515350556_text15963162972914"></a>Atlas 800I A3 SuperPoD</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p1560813335296"><a name="en-us_topic_0000002515350556_p1560813335296"></a><a name="en-us_topic_0000002515350556_p1560813335296"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p260873392918"><a name="en-us_topic_0000002515350556_p260873392918"></a><a name="en-us_topic_0000002515350556_p260873392918"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p5608333162914"><a name="en-us_topic_0000002515350556_p5608333162914"></a><a name="en-us_topic_0000002515350556_p5608333162914"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row19346183355516"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 mcps1.2.5.2.1 "><p id="en-us_topic_0000002515350556_p0905133713551"><a name="en-us_topic_0000002515350556_p0905133713551"></a><a name="en-us_topic_0000002515350556_p0905133713551"></a><span id="en-us_topic_0000002515350556_text1190563775520"><a name="en-us_topic_0000002515350556_text1190563775520"></a><a name="en-us_topic_0000002515350556_text1190563775520"></a>A200T A3 Box8 SuperPoD server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.2 "><p id="en-us_topic_0000002515350556_p7204194395516"><a name="en-us_topic_0000002515350556_p7204194395516"></a><a name="en-us_topic_0000002515350556_p7204194395516"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p13205104310553"><a name="en-us_topic_0000002515350556_p13205104310553"></a><a name="en-us_topic_0000002515350556_p13205104310553"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515350556_p32051943125519"><a name="en-us_topic_0000002515350556_p32051943125519"></a><a name="en-us_topic_0000002515350556_p32051943125519"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515350556_row1158213431480"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.2.1 mcps1.2.5.2.2 mcps1.2.5.2.3 "><p id="en-us_topic_0000002515350556_p65418471681"><a name="en-us_topic_0000002515350556_p65418471681"></a><a name="en-us_topic_0000002515350556_p65418471681"></a>Note: <strong id="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b130904228481512"><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b130904228481512"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b130904228481512"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b200767006881512"><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b200767006881512"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b200767006881512"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b90682012381512"><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b90682012381512"></a><a name="en-us_topic_0000002515350556_en-us_topic_0000002515350556_b90682012381512"></a>NA</strong> indicates that the function is not involved and planned currently.</p>
</td>
</tr>
</tbody>
</table>

**Example<a name="en-us_topic_0000001251427211_en-us_topic_0000001178373160_en-us_topic_0000001101290912_toc533412083"></a>**

```c
int ret;
int card_id = 0;
int device_id = 0;
unsigned char data1[] = {   // Ciphertext to be decrypted
   /Ciphertext/
};
unsigned char key_in1[] = { // Key used for encryption/decryption
  /16-byte key/
};
unsigned char iv_in1[] = {  // IV value used for encryption/decryption
  /16-byte IV value/
};
struct dcmi_sm_parm sm_test_param = {0};
sm_test_param.key_type = SM4_CBC_DECRYPT;
memcpy(sm_test_param.key, key_in1, sizeof(key_in1));
memcpy(sm_test_param.iv, iv_in1, sizeof(iv_in1));
sm_test_param.key_len = 16;
sm_test_param.iv_len = 16;
unsigned char *out_buf =  (unsigned char *)malloc(sizeof(data1));
unsigned int *out_len =(unsigned int *)malloc(sizeof(unsigned int));
*out_len = sizeof(data1);
struct dcmi_sm_data sm_test_data = {(const unsigned char *)data1, sizeof(data1), out_buf, out_len};
dcmi_init();
ret = dcmi_sm_decrypt(0, 0, &sm_test_param, &sm_test_data);
if (ret != 0) {
    // todo: Record logs.
    free(out_buf);
    free(out_len);
    return ret;
}
// data.out_buf records the decrypted data, and data.out_len records the length of the decrypted data.
free(out_buf);
free(out_len);
```
