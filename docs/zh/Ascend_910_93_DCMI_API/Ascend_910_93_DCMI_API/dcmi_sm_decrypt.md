# dcmi\_sm\_decrypt<a name="ZH-CN_TOPIC_0000002485478696"></a>

**函数原型<a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_toc533412077"></a>**

**int dcmi\_sm\_decrypt\(int card\_id, int device\_id, struct dcmi\_sm\_parm\* parm, struct dcmi\_sm\_data\*data\)**

**功能说明<a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_toc533412078"></a>**

调用此接口，输入加密后的密文、解密的密钥以及国密解密算法类型，获取解密后的明文。

**参数说明<a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_toc533412079"></a>**

<a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_table10480683"></a>
<table><thead align="left"><tr id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_row7580267"><th class="cellrowborder" valign="top" width="17%" id="mcps1.1.5.1.1"><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p10021890"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p10021890"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p10021890"></a>参数名称</p>
</th>
<th class="cellrowborder" valign="top" width="16.99%" id="mcps1.1.5.1.2"><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p6466753"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p6466753"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p6466753"></a>输入/输出</p>
</th>
<th class="cellrowborder" valign="top" width="16.009999999999998%" id="mcps1.1.5.1.3"><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p54045009"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p54045009"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p54045009"></a>类型</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.5.1.4"><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p15569626"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p15569626"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p15569626"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_row10560021192510"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p36741947142813"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p36741947142813"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p36741947142813"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p96741747122818"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p96741747122818"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p96741747122818"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p46747472287"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p46747472287"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p46747472287"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1467413474281"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1467413474281"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1467413474281"></a>设备ID，当前实际支持的ID通过dcmi_get_card_list接口获取。</p>
</td>
</tr>
<tr id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_row1155711494235"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p7711145152918"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p7711145152918"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p7711145152918"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p671116522914"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p671116522914"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p671116522914"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1771116572910"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1771116572910"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1771116572910"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="zh-cn_topic_0000001148530297_p11747451997"><a name="zh-cn_topic_0000001148530297_p11747451997"></a><a name="zh-cn_topic_0000001148530297_p11747451997"></a>芯片ID，通过dcmi_get_device_id_in_card接口获取。取值范围如下：</p>
<p id="zh-cn_topic_0000001148530297_p1377514432141"><a name="zh-cn_topic_0000001148530297_p1377514432141"></a><a name="zh-cn_topic_0000001148530297_p1377514432141"></a>NPU芯片：[0, device_id_max-1]。</p>
<p id="p4399120121817"><a name="p4399120121817"></a><a name="p4399120121817"></a></p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> 说明： </span><div class="notebody"><p id="zh-cn_topic_0000002517638669_p10218227151413"><a name="zh-cn_topic_0000002517638669_p10218227151413"></a><a name="zh-cn_topic_0000002517638669_p10218227151413"></a><span id="zh-cn_topic_0000002517638669_ph833311293312"><a name="zh-cn_topic_0000002517638669_ph833311293312"></a><a name="zh-cn_topic_0000002517638669_ph833311293312"></a>device_id_max值为2，当device_id为0或1时表示NPU芯片；当device_id为2时表示MCU芯片。</span></p>
</div></div>
</td>
</tr>
<tr id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_row15462171542913"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p8175616161117"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p8175616161117"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p8175616161117"></a>parm</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1817571618112"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1817571618112"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p1817571618112"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p20175191611119"><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p20175191611119"></a><a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_p20175191611119"></a>struct dcmi_sm_parm*</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p2447145210116"><a name="p2447145210116"></a><a name="p2447145210116"></a>struct dcmi_sm_parm {</p>
<p id="p9447105215112"><a name="p9447105215112"></a><a name="p9447105215112"></a>unsigned int key_type;</p>
<p id="p353613473350"><a name="p353613473350"></a><a name="p353613473350"></a>unsigned int key_len;//SM4密钥长度为16Byte，不涉及SM3</p>
<p id="p144471352121113"><a name="p144471352121113"></a><a name="p144471352121113"></a>unsigned int iv_len;//SM4初始值长度为16Byte，不涉及SM3</p>
<p id="p174471152131111"><a name="p174471152131111"></a><a name="p174471152131111"></a>unsigned int reserves;//预留</p>
<p id="p644785213115"><a name="p644785213115"></a><a name="p644785213115"></a>unsigned char iv[64];//CBC算法初始化向量</p>
<p id="p4447135212118"><a name="p4447135212118"></a><a name="p4447135212118"></a>unsigned char key[512];//密钥</p>
<p id="p104477521114"><a name="p104477521114"></a><a name="p104477521114"></a>unsigned char reserved[512];//预留</p>
<p id="p74478527115"><a name="p74478527115"></a><a name="p74478527115"></a>};</p>
<p id="p444755201118"><a name="p444755201118"></a><a name="p444755201118"></a>key_type入参范围：</p>
<p id="p154471252131113"><a name="p154471252131113"></a><a name="p154471252131113"></a>enum sm_key_type{</p>
<p id="p2381738183616"><a name="p2381738183616"></a><a name="p2381738183616"></a>SM3_NORMAL_SUMMARY = 0,//SM3杂凑算法操作</p>
<p id="p19447205261112"><a name="p19447205261112"></a><a name="p19447205261112"></a>SM4_CBC_ENCRYPT = 1,//SM4 CBC加密算法</p>
<p id="p1447052141119"><a name="p1447052141119"></a><a name="p1447052141119"></a>SM4_CBC_DECRYPT = 2,//SM4 CBC解密算法</p>
<p id="p844875211114"><a name="p844875211114"></a><a name="p844875211114"></a>};</p>
</td>
</tr>
<tr id="row15651816125"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="p1965515129"><a name="p1965515129"></a><a name="p1965515129"></a>data</p>
</td>
<td class="cellrowborder" valign="top" width="16.99%" headers="mcps1.1.5.1.2 "><p id="p15244731115812"><a name="p15244731115812"></a><a name="p15244731115812"></a>输入；输出</p>
</td>
<td class="cellrowborder" valign="top" width="16.009999999999998%" headers="mcps1.1.5.1.3 "><p id="p3651712129"><a name="p3651712129"></a><a name="p3651712129"></a>struct dcmi_sm_data*</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p619755115125"><a name="p619755115125"></a><a name="p619755115125"></a>struct dcmi_sm_data {</p>
<p id="p17197155141219"><a name="p17197155141219"></a><a name="p17197155141219"></a>const unsigned char *in_buf;</p>
<p id="p61971551171211"><a name="p61971551171211"></a><a name="p61971551171211"></a>unsigned in_len;//SM3长度、SM4长度最多为3072Byte，且SM4长度必须为16Byte的整数倍</p>
<p id="p1695213111374"><a name="p1695213111374"></a><a name="p1695213111374"></a>unsigned char *out_buf;//输出缓存</p>
<p id="p9197195117127"><a name="p9197195117127"></a><a name="p9197195117127"></a>unsigned int *out_len;//输出缓存长度</p>
<p id="p111975516127"><a name="p111975516127"></a><a name="p111975516127"></a>};</p>
</td>
</tr>
</tbody>
</table>

**返回值说明<a name="zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_section1256282115569"></a>**

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

**异常处理<a name="zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_toc533412081"></a>**

无。

**约束说明<a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_toc533412082"></a>**

**表 1** 不同部署场景下的支持情况

<a name="table6183194116432"></a>
<table><thead align="left"><tr id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_row119194469302"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="zh-cn_topic_0000002485318818_p1842495418394"><a name="zh-cn_topic_0000002485318818_p1842495418394"></a><a name="zh-cn_topic_0000002485318818_p1842495418394"></a>产品形态</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="zh-cn_topic_0000002485318818_p1642455415393"><a name="zh-cn_topic_0000002485318818_p1642455415393"></a><a name="zh-cn_topic_0000002485318818_p1642455415393"></a>物理机场景（裸机）root用户</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="zh-cn_topic_0000002485318818_p114246541390"><a name="zh-cn_topic_0000002485318818_p114246541390"></a><a name="zh-cn_topic_0000002485318818_p114246541390"></a>物理机场景（裸机）运行用户组（非root用户）</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="zh-cn_topic_0000002485318818_p64241654123916"><a name="zh-cn_topic_0000002485318818_p64241654123916"></a><a name="zh-cn_topic_0000002485318818_p64241654123916"></a>物理机+普通容器场景root用户</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_p1378374019445"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_p1378374019445"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_p1378374019445"></a><span id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_text262011415447"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_text262011415447"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_text262011415447"></a><span id="zh-cn_topic_0000002485318818_text1564782665814"><a name="zh-cn_topic_0000002485318818_text1564782665814"></a><a name="zh-cn_topic_0000002485318818_text1564782665814"></a>Atlas 900 A3 SuperPoD 超节点</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p11683124944919"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p11683124944919"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p11683124944919"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p36831949194913"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p36831949194913"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p36831949194913"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p2683144919499"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p2683144919499"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_p2683144919499"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_row82952324359"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_p137832403444"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_p137832403444"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_p137832403444"></a><span id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_text6814122411310"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_text6814122411310"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001819869974_text6814122411310"></a><span id="zh-cn_topic_0000002485318818_text1123515513517"><a name="zh-cn_topic_0000002485318818_text1123515513517"></a><a name="zh-cn_topic_0000002485318818_text1123515513517"></a>Atlas 9000 A3 SuperPoD 集群算力系统</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p1285753073419"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p1285753073419"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p1285753073419"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p1985743013346"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p1985743013346"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p1985743013346"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p0857203013412"><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p0857203013412"></a><a name="zh-cn_topic_0000002485318818_zh-cn_topic_0000001820029662_zh-cn_topic_0000001251227155_zh-cn_topic_0000001178213210_zh-cn_topic_0000001147728951_p0857203013412"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row12693311552"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p17872181618166"><a name="zh-cn_topic_0000002485318818_p17872181618166"></a><a name="zh-cn_topic_0000002485318818_p17872181618166"></a><span id="zh-cn_topic_0000002485318818_text17872121641614"><a name="zh-cn_topic_0000002485318818_text17872121641614"></a><a name="zh-cn_topic_0000002485318818_text17872121641614"></a>Atlas 800T A3 超节点</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p177521042115520"><a name="zh-cn_topic_0000002485318818_p177521042115520"></a><a name="zh-cn_topic_0000002485318818_p177521042115520"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p12752114215559"><a name="zh-cn_topic_0000002485318818_p12752114215559"></a><a name="zh-cn_topic_0000002485318818_p12752114215559"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p18752104275512"><a name="zh-cn_topic_0000002485318818_p18752104275512"></a><a name="zh-cn_topic_0000002485318818_p18752104275512"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row179407221298"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p696322918294"><a name="zh-cn_topic_0000002485318818_p696322918294"></a><a name="zh-cn_topic_0000002485318818_p696322918294"></a><span id="zh-cn_topic_0000002485318818_text15963162972914"><a name="zh-cn_topic_0000002485318818_text15963162972914"></a><a name="zh-cn_topic_0000002485318818_text15963162972914"></a>Atlas 800I A3 超节点</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p1560813335296"><a name="zh-cn_topic_0000002485318818_p1560813335296"></a><a name="zh-cn_topic_0000002485318818_p1560813335296"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p260873392918"><a name="zh-cn_topic_0000002485318818_p260873392918"></a><a name="zh-cn_topic_0000002485318818_p260873392918"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p5608333162914"><a name="zh-cn_topic_0000002485318818_p5608333162914"></a><a name="zh-cn_topic_0000002485318818_p5608333162914"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row19346183355516"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="zh-cn_topic_0000002485318818_p0905133713551"><a name="zh-cn_topic_0000002485318818_p0905133713551"></a><a name="zh-cn_topic_0000002485318818_p0905133713551"></a><span id="zh-cn_topic_0000002485318818_text1190563775520"><a name="zh-cn_topic_0000002485318818_text1190563775520"></a><a name="zh-cn_topic_0000002485318818_text1190563775520"></a>A200T A3 Box8 超节点服务器</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="zh-cn_topic_0000002485318818_p7204194395516"><a name="zh-cn_topic_0000002485318818_p7204194395516"></a><a name="zh-cn_topic_0000002485318818_p7204194395516"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="zh-cn_topic_0000002485318818_p13205104310553"><a name="zh-cn_topic_0000002485318818_p13205104310553"></a><a name="zh-cn_topic_0000002485318818_p13205104310553"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p32051943125519"><a name="zh-cn_topic_0000002485318818_p32051943125519"></a><a name="zh-cn_topic_0000002485318818_p32051943125519"></a>N</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002485318818_row1158213431480"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="zh-cn_topic_0000002485318818_p65418471681"><a name="zh-cn_topic_0000002485318818_p65418471681"></a><a name="zh-cn_topic_0000002485318818_p65418471681"></a>注：Y表示支持；N表示不支持；NA表示不涉及，当前未规划此场景。</p>
</td>
</tr>
</tbody>
</table>

**调用示例<a name="zh-cn_topic_0000001251427211_zh-cn_topic_0000001178373160_zh-cn_topic_0000001101290912_toc533412083"></a>**

```c
int ret;
int card_id = 0;
int device_id = 0;
unsigned char data1[] = {   // 待解密的密文
   /密文/
};
unsigned char key_in1[] = { // 加密/解密输入的key
  /16Byte的密钥/
};
unsigned char iv_in1[] = {  // 加密/解密输入的iv值
  /16Byte的iv值/
};
struct dcmi_sm_parm sm_test_param = {0};
sm_test_param.key_type = SM4_CBC_DECRYPT;
memcpy(sm_test_param.key, key_in1, sizeof(key_in1));
memcpy(sm_test_param.key, iv_in1, sizeof(iv_in1));
sm_test_param.key_len = 16;
sm_test_param.iv_len = 16;
unsigned char *out_buf =  (unsigned char *)malloc(sizeof(data1));
unsigned int *out_len =(unsigned int *)malloc(sizeof(unsigned int));
*out_len = sizeof(data1);
struct dcmi_sm_data sm_test_data = {(const unsigned char *)data1, sizeof(data1), out_buf, out_len};
dcmi_init();
ret = dcmi_sm_decrypt(0, 0, &sm_test_param, &sm_test_data);
if (ret != 0) {
    //todo:记录日志
    free(out_buf);
    free(out_len);
    return ret;
}
//data.out_buf中记录解密后的数据,data.out_len记录解密后的数据长度
free(out_buf);
free(out_len);
```
