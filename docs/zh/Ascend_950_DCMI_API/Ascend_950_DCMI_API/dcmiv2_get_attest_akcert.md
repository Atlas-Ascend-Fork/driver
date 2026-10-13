# dcmiv2\_get\_attest\_akcert<a name="ZH-CN_TOPIC_0000002565332137"></a>

## 函数原型<a name="zh-cn_topic_0000001206147244_zh-cn_topic_0000001223414441_zh-cn_topic_0000001099619850_toc533412077"></a>

**int dcmiv2\_get\_attest\_akcert\(int dev\_id, unsigned char \*ak\_cert, unsigned int \*ak\_cert\_len\)**

## 功能说明<a name="section1468480762"></a>

获取设备的IAK（Initial Attestation Key）证书以及证书长度。

## 参数说明<a name="zh-cn_topic_0000001206147244_zh-cn_topic_0000001223414441_zh-cn_topic_0000001099619850_toc533412079"></a>

<a name="table2736627173213"></a>
<table><thead align="left"><tr id="row37511727123212"><th class="cellrowborder" valign="top" width="17%" id="mcps1.1.5.1.1"><p id="p3751827143219"><a name="p3751827143219"></a><a name="p3751827143219"></a>参数名称</p>
</th>
<th class="cellrowborder" valign="top" width="17%" id="mcps1.1.5.1.2"><p id="p10751142783216"><a name="p10751142783216"></a><a name="p10751142783216"></a>输入/输出</p>
</th>
<th class="cellrowborder" valign="top" width="16%" id="mcps1.1.5.1.3"><p id="p1575112718322"><a name="p1575112718322"></a><a name="p1575112718322"></a>类型</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.5.1.4"><p id="p13751142783210"><a name="p13751142783210"></a><a name="p13751142783210"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="row87511127163211"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="p157511227143210"><a name="p157511227143210"></a><a name="p157511227143210"></a>dev_id</p>
</td>
<td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.2 "><p id="p875111272328"><a name="p875111272328"></a><a name="p875111272328"></a>输入</p>
</td>
<td class="cellrowborder" valign="top" width="16%" headers="mcps1.1.5.1.3 "><p id="p6751162712320"><a name="p6751162712320"></a><a name="p6751162712320"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p075152718324"><a name="p075152718324"></a><a name="p075152718324"></a><span id="zh-cn_topic_0000002348043873_ph1263073113614"><a name="zh-cn_topic_0000002348043873_ph1263073113614"></a><a name="zh-cn_topic_0000002348043873_ph1263073113614"></a>软件的逻辑ID</span>，通过<a href="dcmiv2_get_device_list.md">dcmiv2_get_device_list</a>接口获取。取值范围为[0, device_cnt-1]。</p>
</td>
</tr>
<tr id="row7751112703217"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="p12787612134616"><a name="p12787612134616"></a><a name="p12787612134616"></a>ak_cert</p>
</td>
<td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.2 "><p id="p8265132123911"><a name="p8265132123911"></a><a name="p8265132123911"></a>输出</p>
</td>
<td class="cellrowborder" valign="top" width="16%" headers="mcps1.1.5.1.3 "><p id="p1674131819463"><a name="p1674131819463"></a><a name="p1674131819463"></a>unsigned char *</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p102381130124616"><a name="p102381130124616"></a><a name="p102381130124616"></a>返回IAK证书，证书长度最大为2KB。</p>
</td>
</tr>
<tr id="row13163103243816"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="p37875127468"><a name="p37875127468"></a><a name="p37875127468"></a>ak_cert_len</p>
</td>
<td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.2 "><p id="p112665220399"><a name="p112665220399"></a><a name="p112665220399"></a>输入/输出</p>
</td>
<td class="cellrowborder" valign="top" width="16%" headers="mcps1.1.5.1.3 "><p id="p2741171820462"><a name="p2741171820462"></a><a name="p2741171820462"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.5.1.4 "><p id="p923816309467"><a name="p923816309467"></a><a name="p923816309467"></a>输入用户分配的空间大小，返回真实数据的长度。</p>
</td>
</tr>
</tbody>
</table>

## 返回值说明<a name="zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_section1256282115569"></a>

<a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_table19654399"></a>
<table><thead align="left"><tr id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p16312252"><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p16312252"></a><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p16312252"></a>类型</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p46224020"><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p46224020"></a><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p46224020"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p8661001"><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p8661001"></a><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p79561110476"><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p79561110476"></a><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_p79561110476"></a>处理结果：</p>
<a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_ul55711364478"></a><a name="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_ul55711364478"></a><ul id="zh-cn_topic_0000002348043941_zh-cn_topic_0000001251227149_zh-cn_topic_0000001178213202_zh-cn_topic_0000001097675636_zh-cn_topic_0000001170223803_zh-cn_topic_0160151812_ul55711364478"><li>成功：返回0。</li><li>失败：返回码请参见<a href="return_codes.md">return_codes</a>。</li></ul>
</td>
</tr>
</tbody>
</table>

## 约束说明<a name="section8765321374"></a>

**表 1** 不同部署场景下的支持情况

<a name="table1891871242416"></a>
<table><thead align="left"><tr id="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_row119194469302"><th class="cellrowborder" rowspan="2" valign="top" id="mcps1.2.7.1.1"><p id="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_p67828402447"><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_p67828402447"></a><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_p67828402447"></a>产品形态</p>
</th>
<th class="cellrowborder" colspan="2" valign="top" id="mcps1.2.7.1.2"><p id="zh-cn_topic_0000002347924121_p65514914017"><a name="zh-cn_topic_0000002347924121_p65514914017"></a><a name="zh-cn_topic_0000002347924121_p65514914017"></a>物理机场景（裸机）</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.1.3"><p id="zh-cn_topic_0000002347924121_p56108061"><a name="zh-cn_topic_0000002347924121_p56108061"></a><a name="zh-cn_topic_0000002347924121_p56108061"></a>物理机+特权容器场景</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.1.4"><p id="zh-cn_topic_0000002347924121_p1855189908"><a name="zh-cn_topic_0000002347924121_p1855189908"></a><a name="zh-cn_topic_0000002347924121_p1855189908"></a>物理机+普通容器场景</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.1.5"><p id="zh-cn_topic_0000002347924121_p10771327123917"><a name="zh-cn_topic_0000002347924121_p10771327123917"></a><a name="zh-cn_topic_0000002347924121_p10771327123917"></a>直通虚拟机场景</p>
</th>
</tr>
<tr id="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_row116673024514"><th class="cellrowborder" valign="top" id="mcps1.2.7.2.1"><p id="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p792013462306"><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p792013462306"></a><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p792013462306"></a>root用户</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.2.2"><p id="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p16920346133016"><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p16920346133016"></a><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p16920346133016"></a>运行用户组（非root用户）</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.2.3"><p id="zh-cn_topic_0000002347924121_p56110815619"><a name="zh-cn_topic_0000002347924121_p56110815619"></a><a name="zh-cn_topic_0000002347924121_p56110815619"></a>root用户</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.2.4"><p id="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p166440421324"><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p166440421324"></a><a name="zh-cn_topic_0000002347924121_zh-cn_topic_0000002347924121_zh-cn_topic_0000001206627196_zh-cn_topic_0000001178213188_zh-cn_topic_0000001146459833_p166440421324"></a>root用户</p>
</th>
<th class="cellrowborder" valign="top" id="mcps1.2.7.2.5"><p id="zh-cn_topic_0000002347924121_p977116273398"><a name="zh-cn_topic_0000002347924121_p977116273398"></a><a name="zh-cn_topic_0000002347924121_p977116273398"></a>root用户</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0000002347924121_row57331624612"><td class="cellrowborder" valign="top" width="18.47630473905219%" headers="mcps1.2.7.1.1 mcps1.2.7.2.1 "><p id="zh-cn_topic_0000002347924121_p144147361596"><a name="zh-cn_topic_0000002347924121_p144147361596"></a><a name="zh-cn_topic_0000002347924121_p144147361596"></a><span id="zh-cn_topic_0000002347924121_ph12140626193614"><a name="zh-cn_topic_0000002347924121_ph12140626193614"></a><a name="zh-cn_topic_0000002347924121_ph12140626193614"></a>Atlas 350 加速卡</span></p>
</td>
<td class="cellrowborder" valign="top" width="14.857028594281147%" headers="mcps1.2.7.1.2 mcps1.2.7.2.2 "><p id="zh-cn_topic_0000002347924121_p13356146465"><a name="zh-cn_topic_0000002347924121_p13356146465"></a><a name="zh-cn_topic_0000002347924121_p13356146465"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="16.666666666666664%" headers="mcps1.2.7.1.2 mcps1.2.7.2.3 "><p id="zh-cn_topic_0000002347924121_p19351314124620"><a name="zh-cn_topic_0000002347924121_p19351314124620"></a><a name="zh-cn_topic_0000002347924121_p19351314124620"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="16.666666666666664%" headers="mcps1.2.7.1.3 mcps1.2.7.2.4 "><p id="zh-cn_topic_0000002347924121_p1577514751713"><a name="zh-cn_topic_0000002347924121_p1577514751713"></a><a name="zh-cn_topic_0000002347924121_p1577514751713"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="16.666666666666664%" headers="mcps1.2.7.1.4 mcps1.2.7.2.5 "><p id="zh-cn_topic_0000002347924121_p1435121434611"><a name="zh-cn_topic_0000002347924121_p1435121434611"></a><a name="zh-cn_topic_0000002347924121_p1435121434611"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="16.666666666666664%" headers="mcps1.2.7.1.5 "><p id="zh-cn_topic_0000002347924121_p8318151920427"><a name="zh-cn_topic_0000002347924121_p8318151920427"></a><a name="zh-cn_topic_0000002347924121_p8318151920427"></a>Y</p>
</td>
</tr>
<tr id="zh-cn_topic_0000002347924121_row165961813903"><td class="cellrowborder" colspan="6" valign="top" headers="mcps1.2.7.1.1 mcps1.2.7.1.2 mcps1.2.7.1.3 mcps1.2.7.1.4 mcps1.2.7.1.5 mcps1.2.7.2.1 mcps1.2.7.2.2 mcps1.2.7.2.3 mcps1.2.7.2.4 mcps1.2.7.2.5 "><p id="zh-cn_topic_0000002347924121_p56258171305"><a name="zh-cn_topic_0000002347924121_p56258171305"></a><a name="zh-cn_topic_0000002347924121_p56258171305"></a><span id="zh-cn_topic_0000002347924121_ph15391145912019"><a name="zh-cn_topic_0000002347924121_ph15391145912019"></a><a name="zh-cn_topic_0000002347924121_ph15391145912019"></a>注：Y表示支持；N表示不支持；NA表示不涉及，当前未规划此场景。</span></p>
</td>
</tr>
</tbody>
</table>

## 调用示例<a name="section1888234213618"></a>

```c
…   
int ret;
int dev_id = 0;
unsigned char ak_cert[65536] = {0};
unsigned int ak_cert_len = sizeof(ak_cert);
ret = dcmiv2_init();
if (ret != 0) {
    printf("dcmiv2_init failed, ret = %d\n", ret);
    return ret;
}
ret = dcmiv2_get_attest_akcert(dev_id, ak_cert, &ak_cert_len);
if (ret != 0) {
    printf("dcmiv2_get_attest_akcert failed, ret = %d\n", ret);
    return ret;
}
…
```
