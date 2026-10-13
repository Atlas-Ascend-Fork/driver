# dcmi\_get\_device\_info<a name="EN-US_TOPIC_0000002546983805"></a>

**Prototype<a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_toc533412077"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_toc533412078"></a>**

Obtains device information and the status of each module.

**Parameter Description<a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_toc533412079"></a>**

<a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_table10480683"></a>
<table><thead align="left"><tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row7580267"><th class="cellrowborder" valign="top" width="17%" id="mcps1.1.5.1.1"><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p10021890"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p10021890"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p10021890"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.02%" id="mcps1.1.5.1.2"><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p6466753"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p6466753"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p6466753"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="17.96%" id="mcps1.1.5.1.3"><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p54045009"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p54045009"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p54045009"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="50.019999999999996%" id="mcps1.1.5.1.4"><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p15569626"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p15569626"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p15569626"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row10560021192510"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p36741947142813"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p36741947142813"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p36741947142813"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.02%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p96741747122818"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p96741747122818"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p96741747122818"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.96%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p46747472287"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p46747472287"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p46747472287"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.019999999999996%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1467413474281"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1467413474281"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1467413474281"></a>Device ID. The supported IDs can be obtained by calling <strong id="b172817944210288"><a name="b172817944210288"></a><a name="b172817944210288"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row1155711494235"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p7711145152918"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p7711145152918"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p7711145152918"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.02%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p671116522914"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p671116522914"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p671116522914"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.96%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1771116572910"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1771116572910"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1771116572910"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.019999999999996%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001148530297_p11747451997"><a name="en-us_topic_0000001148530297_p11747451997"></a><a name="en-us_topic_0000001148530297_p11747451997"></a>Chip ID, which can be obtained by calling <strong id="b1534874530102432"><a name="b1534874530102432"></a><a name="b1534874530102432"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="en-us_topic_0000001148530297_p1377514432141"><a name="en-us_topic_0000001148530297_p1377514432141"></a><a name="en-us_topic_0000001148530297_p1377514432141"></a>NPU: [0, <strong id="b1460984651105322"><a name="b1460984651105322"></a><a name="b1460984651105322"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row15462171542913"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p5522164215178"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p5522164215178"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p5522164215178"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.02%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p8522242101715"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p8522242101715"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p8522242101715"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.96%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p17522114220174"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p17522114220174"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p17522114220174"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="50.019999999999996%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p167001165556"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p167001165556"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p167001165556"></a>Main command word corresponding to the specified query item.</p>
<p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p37001865558"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p37001865558"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p37001865558"></a>CMD information, which is used to obtain the information about the corresponding module.</p>
<p id="p126896223344"><a name="p126896223344"></a><a name="p126896223344"></a>enum dcmi_main_cmd {</p>
<p id="p2068942213411"><a name="p2068942213411"></a><a name="p2068942213411"></a>DCMI_MAIN_CMD_DVPP = 0,</p>
<p id="p9689132218348"><a name="p9689132218348"></a><a name="p9689132218348"></a>DCMI_MAIN_CMD_ISP,</p>
<p id="p768918223342"><a name="p768918223342"></a><a name="p768918223342"></a>DCMI_MAIN_CMD_TS_GROUP_NUM,</p>
<p id="p1968916225345"><a name="p1968916225345"></a><a name="p1968916225345"></a>DCMI_MAIN_CMD_CAN,</p>
<p id="p12689122219349"><a name="p12689122219349"></a><a name="p12689122219349"></a>DCMI_MAIN_CMD_UART,</p>
<p id="p068972215342"><a name="p068972215342"></a><a name="p068972215342"></a>DCMI_MAIN_CMD_UPGRADE = 5,</p>
<p id="p13689162219341"><a name="p13689162219341"></a><a name="p13689162219341"></a>DCMI_MAIN_CMD_UFS,</p>
<p id="p13689172220347"><a name="p13689172220347"></a><a name="p13689172220347"></a>DCMI_MAIN_CMD_OS_POWER,</p>
<p id="p86891422103414"><a name="p86891422103414"></a><a name="p86891422103414"></a>DCMI_MAIN_CMD_LP,</p>
<p id="p18689422193413"><a name="p18689422193413"></a><a name="p18689422193413"></a>DCMI_MAIN_CMD_MEMORY,</p>
<p id="p468932216342"><a name="p468932216342"></a><a name="p468932216342"></a>DCMI_MAIN_CMD_RECOVERY,</p>
<p id="p868972293411"><a name="p868972293411"></a><a name="p868972293411"></a>DCMI_MAIN_CMD_TS,</p>
<p id="p66896222345"><a name="p66896222345"></a><a name="p66896222345"></a>DCMI_MAIN_CMD_CHIP_INF,</p>
<p id="p186898229343"><a name="p186898229343"></a><a name="p186898229343"></a>DCMI_MAIN_CMD_QOS,</p>
<p id="p1268919226347"><a name="p1268919226347"></a><a name="p1268919226347"></a>DCMI_MAIN_CMD_SOC_INFO,</p>
<p id="p12689162215349"><a name="p12689162215349"></a><a name="p12689162215349"></a>DCMI_MAIN_CMD_SILS,</p>
<p id="p1068910226341"><a name="p1068910226341"></a><a name="p1068910226341"></a>DCMI_MAIN_CMD_HCCS,</p>
<p id="p5689122123416"><a name="p5689122123416"></a><a name="p5689122123416"></a>DCMI_MAIN_CMD_TEMP = 50,</p>
<p id="p1668992253419"><a name="p1668992253419"></a><a name="p1668992253419"></a>DCMI_MAIN_CMD_SVM,</p>
<p id="p166893224341"><a name="p166893224341"></a><a name="p166893224341"></a>DCMI_MAIN_CMD_VDEV_MNG,</p>
<p id="p368952243412"><a name="p368952243412"></a><a name="p368952243412"></a>DCMI_MAIN_CMD_SEC,</p>
<p id="p194422345516"><a name="p194422345516"></a><a name="p194422345516"></a>DCMI_MAIN_CMD_PCIE = 55,</p>
<p id="p1896837121219"><a name="p1896837121219"></a><a name="p1896837121219"></a>DCMI_MAIN_CMD_SIO = 56,</p>
<p id="p1968962263412"><a name="p1968962263412"></a><a name="p1968962263412"></a>DCMI_MAIN_CMD_EX_COMPUTING = 0x8000,</p>
<p id="p56898220340"><a name="p56898220340"></a><a name="p56898220340"></a>DCMI_MAIN_CMD_DEVICE_SHARE = 0x8001,</p>
<p id="p4555151501415"><a name="p4555151501415"></a><a name="p4555151501415"></a>DCMI_MAIN_CMD_EX_CERT = 0x8003,</p>
<p id="p76891322143411"><a name="p76891322143411"></a><a name="p76891322143411"></a>DCMI_MAIN_CMD_MAX</p>
<p id="p136891022183420"><a name="p136891022183420"></a><a name="p136891022183420"></a>};</p>
<p id="p9932195894319"><a name="p9932195894319"></a><a name="p9932195894319"></a>Only the main command words of the following modules are supported:</p>
<p id="p186721544154019"><a name="p186721544154019"></a><a name="p186721544154019"></a>DCMI_MAIN_CMD_DVPP // Main command word of the DVPP operator module</p>
<p id="p16843046164010"><a name="p16843046164010"></a><a name="p16843046164010"></a>DCMI_MAIN_CMD_LP // Main command word of the low power consumption module</p>
<p id="p113601548204019"><a name="p113601548204019"></a><a name="p113601548204019"></a>DCMI_MAIN_CMD_TS // Main command word of the task scheduling module</p>
<p id="p1340535011403"><a name="p1340535011403"></a><a name="p1340535011403"></a>DCMI_MAIN_CMD_QOS // Main command word of the QoS module</p>
<p id="p415319724117"><a name="p415319724117"></a><a name="p415319724117"></a>DCMI_MAIN_CMD_HCCS // Main command word of the HCCS module DCMI_MAIN_CMD_EX_COMPUTING // Main command word of the computing power extension module</p>
<p id="p193245812437"><a name="p193245812437"></a><a name="p193245812437"></a>DCMI_MAIN_CMD_EX_CERT // Main command word of the certificate management module</p>
<p id="p19199172716408"><a name="p19199172716408"></a><a name="p19199172716408"></a>DCMI_MAIN_CMD_DEVICE_SHARE // Main command word for container sharing</p>
</td>
</tr>
<tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row118352016305"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p19161539205"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p19161539205"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p19161539205"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.02%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1816115315209"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1816115315209"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1816115315209"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.96%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1216853142018"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1216853142018"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1216853142018"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="50.019999999999996%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p75475304476"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p75475304476"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p75475304476"></a>For details, see the function description in the subsections.</p>
</td>
</tr>
<tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row148973243016"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p8864185062018"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p8864185062018"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p8864185062018"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.02%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p886417500202"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p886417500202"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p886417500202"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.96%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1386445092020"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1386445092020"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1386445092020"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="50.019999999999996%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p15864145012200"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p15864145012200"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p15864145012200"></a>Input/Output buffer.</p>
</td>
</tr>
<tr id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_row20221172313016"><td class="cellrowborder" valign="top" width="17%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p7749184816202"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p7749184816202"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p7749184816202"></a>Size</p>
</td>
<td class="cellrowborder" valign="top" width="15.02%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p0749104811207"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p0749104811207"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p0749104811207"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.96%" headers="mcps1.1.5.1.3 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1374944862018"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1374944862018"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p1374944862018"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="50.019999999999996%" headers="mcps1.1.5.1.4 "><p id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p074915488209"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p074915488209"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_p074915488209"></a>Input/Output length of the <strong id="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_b1610715481949"><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_b1610715481949"></a><a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_b1610715481949"></a>buf</strong> array.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_section1256282115569"></a>**

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
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b110241933085543"><a name="b110241933085543"></a><a name="b110241933085543"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_toc533412081"></a>**

None.

**Restrictions<a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_toc533412082"></a>**

**Table  1** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="en-us_topic_0000001206467202_en-us_topic_0000001178373156_en-us_topic_0000001101204718_toc533412083"></a>**

```C
...
int ret = 0;
int card_id = 0;
int device_id = 0;
int buf = 0;
unsigned int size = sizeof(int);
unsigned int sub_cmd = 0;
ret = dcmi_get_device_info(card_id, device_id, DCMI_MAIN_CMD_DVPP, sub_cmd, &buf, &size);
if (ret != 0){
    //todo: Record logs.
    return ret;
}
...
```

## DCMI\_MAIN\_CMD\_DVPP Command<a name="EN-US_TOPIC_0000002546983783"></a>

**Prototype<a name="section1045115919305"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section184521595304"></a>**

Obtains DVPP status and configuration.

**Parameter Description<a name="section104533963015"></a>**

<a name="table104768953018"></a>
<table><thead align="left"><tr id="row19544594301"><th class="cellrowborder" valign="top" width="17.17171717171717%" id="mcps1.1.5.1.1"><p id="p115440913011"><a name="p115440913011"></a><a name="p115440913011"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.151515151515152%" id="mcps1.1.5.1.2"><p id="p754417916309"><a name="p754417916309"></a><a name="p754417916309"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="23.232323232323232%" id="mcps1.1.5.1.3"><p id="p135449903012"><a name="p135449903012"></a><a name="p135449903012"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="44.44444444444445%" id="mcps1.1.5.1.4"><p id="p254412913309"><a name="p254412913309"></a><a name="p254412913309"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row55442993013"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p12544109113015"><a name="p12544109113015"></a><a name="p12544109113015"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p185448903012"><a name="p185448903012"></a><a name="p185448903012"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="23.232323232323232%" headers="mcps1.1.5.1.3 "><p id="p195441497307"><a name="p195441497307"></a><a name="p195441497307"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="44.44444444444445%" headers="mcps1.1.5.1.4 "><p id="p105445933016"><a name="p105445933016"></a><a name="p105445933016"></a>Device ID. The supported IDs can be obtained by calling <strong id="b93164561310289"><a name="b93164561310289"></a><a name="b93164561310289"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row1544592304"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p195445917304"><a name="p195445917304"></a><a name="p195445917304"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p185446910306"><a name="p185446910306"></a><a name="p185446910306"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="23.232323232323232%" headers="mcps1.1.5.1.3 "><p id="p175441294306"><a name="p175441294306"></a><a name="p175441294306"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="44.44444444444445%" headers="mcps1.1.5.1.4 "><p id="p1854417933011"><a name="p1854417933011"></a><a name="p1854417933011"></a>Chip ID, which can be obtained by calling <strong id="b762627819102433"><a name="b762627819102433"></a><a name="b762627819102433"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p1054449203011"><a name="p1054449203011"></a><a name="p1054449203011"></a>NPU: [0, <strong id="b1041901847105323"><a name="b1041901847105323"></a><a name="b1041901847105323"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row35441953014"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p1254413913016"><a name="p1254413913016"></a><a name="p1254413913016"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p154509173018"><a name="p154509173018"></a><a name="p154509173018"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="23.232323232323232%" headers="mcps1.1.5.1.3 "><p id="p135451198309"><a name="p135451198309"></a><a name="p135451198309"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="44.44444444444445%" headers="mcps1.1.5.1.4 "><p id="p165456953016"><a name="p165456953016"></a><a name="p165456953016"></a>DCMI_MAIN_CMD_DVPP</p>
</td>
</tr>
<tr id="row754518963011"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p85451396307"><a name="p85451396307"></a><a name="p85451396307"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p1654518910302"><a name="p1654518910302"></a><a name="p1654518910302"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="23.232323232323232%" headers="mcps1.1.5.1.3 "><p id="p354519916301"><a name="p354519916301"></a><a name="p354519916301"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="44.44444444444445%" headers="mcps1.1.5.1.4 "><p id="p135451197302"><a name="p135451197302"></a><a name="p135451197302"></a>Sub-command information, which is used to obtain the sub-attribute information of each module.</p>
<p id="p1154589163019"><a name="p1154589163019"></a><a name="p1154589163019"></a>/* DCMI sub command for DVPP module */</p>
<p id="p145452913308"><a name="p145452913308"></a><a name="p145452913308"></a>#define DCMI_SUB_CMD_DVPP_STATUS 0  // DVPP status. If the value of <strong id="b6775172913529"><a name="b6775172913529"></a><a name="b6775172913529"></a>buf</strong> is <strong id="b137752295523"><a name="b137752295523"></a><a name="b137752295523"></a>0</strong>, the status is normal. Otherwise, the status is abnormal.</p>
<p id="p17545699303"><a name="p17545699303"></a><a name="p17545699303"></a>#define DCMI_SUB_CMD_DVPP_VDEC_RATE 1 // VDEC utilization. The normal value ranges from 0 to 100.</p>
<p id="p0545119193019"><a name="p0545119193019"></a><a name="p0545119193019"></a>#define DCMI_SUB_CMD_DVPP_VPC_RATE 2 // VPC utilization. The normal value ranges from 0 to 100.</p>
<p id="p13545397302"><a name="p13545397302"></a><a name="p13545397302"></a>#define DCMI_SUB_CMD_DVPP_VENC_RATE 3 // VENC utilization. The normal value ranges from 0 to 100.</p>
<p id="p154509103011"><a name="p154509103011"></a><a name="p154509103011"></a>#define DCMI_SUB_CMD_DVPP_JPEGE_RATE 4 // JPEGE utilization. The normal value ranges from 0 to 100.</p>
<p id="p1054579163010"><a name="p1054579163010"></a><a name="p1054579163010"></a>#define DCMI_SUB_CMD_DVPP_JPEGD_RATE 5 // JPEGD utilization. The normal value ranges from 0 to 100.</p>
<p id="p1754517943015"><a name="p1754517943015"></a><a name="p1754517943015"></a>Currently, the <strong id="b9757728448629"><a name="b9757728448629"></a><a name="b9757728448629"></a>DCMI_SUB_CMD_DVPP_VENC_RATE</strong> command cannot be used for query.</p>
</td>
</tr>
<tr id="row1954515903016"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p155455915303"><a name="p155455915303"></a><a name="p155455915303"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p554510903013"><a name="p554510903013"></a><a name="p554510903013"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="23.232323232323232%" headers="mcps1.1.5.1.3 "><p id="p19545129143015"><a name="p19545129143015"></a><a name="p19545129143015"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="44.44444444444445%" headers="mcps1.1.5.1.4 "><p id="p185454953015"><a name="p185454953015"></a><a name="p185454953015"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row2054549113013"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p25451983018"><a name="p25451983018"></a><a name="p25451983018"></a>Size</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p12545169113011"><a name="p12545169113011"></a><a name="p12545169113011"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="23.232323232323232%" headers="mcps1.1.5.1.3 "><p id="p1754515943013"><a name="p1754515943013"></a><a name="p1754515943013"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="44.44444444444445%" headers="mcps1.1.5.1.4 "><p id="p2054559153019"><a name="p2054559153019"></a><a name="p2054559153019"></a>Length of the <strong id="b18651916498629"><a name="b18651916498629"></a><a name="b18651916498629"></a>buf</strong> array/Length of the returned result.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section104687916306"></a>**

<a name="table34861192309"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b170178047085544"><a name="b170178047085544"></a><a name="b170178047085544"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section8924415552"></a>**

None.

**Restrictions<a name="section20247271550"></a>**

To obtain the DVPP device settings,  **sub\_cmd**,  **buf**, and  **size**  must meet the following requirements. Failure to do so may impact API calls.

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table34881973017"></a>
<table><thead align="left"><tr id="row654609173016"><th class="cellrowborder" valign="top" width="33.333333333333336%" id="mcps1.2.4.1.1"><p id="p654609173010"><a name="p654609173010"></a><a name="p654609173010"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="33.333333333333336%" id="mcps1.2.4.1.2"><p id="p1254617983019"><a name="p1254617983019"></a><a name="p1254617983019"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="33.333333333333336%" id="mcps1.2.4.1.3"><p id="p2054620963017"><a name="p2054620963017"></a><a name="p2054620963017"></a>Size</p>
</th>
</tr>
</thead>
<tbody><tr id="row1954613919308"><td class="cellrowborder" valign="top" width="33.333333333333336%" headers="mcps1.2.4.1.1 "><p id="p45462912305"><a name="p45462912305"></a><a name="p45462912305"></a>DCMI_SUB_CMD_DVPP_STATUS</p>
</td>
<td class="cellrowborder" valign="top" width="33.333333333333336%" headers="mcps1.2.4.1.2 "><p id="p1554659163015"><a name="p1554659163015"></a><a name="p1554659163015"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="33.333333333333336%" headers="mcps1.2.4.1.3 "><p id="p354616913304"><a name="p354616913304"></a><a name="p354616913304"></a>Length: sizeof(unsigned int)</p>
</td>
</tr>
<tr id="row2054611914303"><td class="cellrowborder" valign="top" width="33.333333333333336%" headers="mcps1.2.4.1.1 "><p id="p1754613917303"><a name="p1754613917303"></a><a name="p1754613917303"></a>DCMI_SUB_CMD_DVPP_VDEC_RATE</p>
<p id="p954615953017"><a name="p954615953017"></a><a name="p954615953017"></a>DCMI_SUB_CMD_DVPP_VPC_RATE</p>
<p id="p754649173014"><a name="p754649173014"></a><a name="p754649173014"></a>DCMI_SUB_CMD_DVPP_VENC_RATE</p>
<p id="p15462094304"><a name="p15462094304"></a><a name="p15462094304"></a>DCMI_SUB_CMD_DVPP_JPEGE_RATE</p>
<p id="p85468914304"><a name="p85468914304"></a><a name="p85468914304"></a>DCMI_SUB_CMD_DVPP_JPEGD_RATE</p>
</td>
<td class="cellrowborder" valign="top" width="33.333333333333336%" headers="mcps1.2.4.1.2 "><p id="p135461398309"><a name="p135461398309"></a><a name="p135461398309"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="33.333333333333336%" headers="mcps1.2.4.1.3 "><p id="p1254699103018"><a name="p1254699103018"></a><a name="p1254699103018"></a>Length: sizeof(unsigned int)</p>
</td>
</tr>
</tbody>
</table>

**Table  2** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section2133155511568"></a>**

```C
...
int ret = 0;
int card_id = 0;
int dev_id = 0;
int ratio = 0;
int sub_cmd = 0;
unsigned int ratio_size = sizeof(int);
ret = dcmi_get_device_info(card_id, dev_id, DCMI_MAIN_CMD_DVPP, sub_cmd, (void *)&ratio, &ratio_size);
if (ret != 0) {
    printf("DCMI_MAIN_CMD_DVPP error\n")
    printf("card_id : %d, dev_id : %d, sub_cmd:0x%x, ratio:(0x%x,%d) fail, value = %d\n", card_id, dev_id, sub_cmd, ratio, ratio_size);
    return ret;
} else {
    //todo
    return ret;
...
```

## DCMI\_MAIN\_CMD\_LP Command<a name="EN-US_TOPIC_0000002515503766"></a>

**Prototype<a name="section158495435301"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section7850154317304"></a>**

Obtains LP information in the system, such as the voltage and current register values of the AI Core, Hybrid, CPU, and DDR.

**Parameter Description<a name="section28501043183017"></a>**

<a name="table15882143133020"></a>
<table><thead align="left"><tr id="row11974164343015"><th class="cellrowborder" valign="top" width="17.17171717171717%" id="mcps1.1.5.1.1"><p id="p797519436303"><a name="p797519436303"></a><a name="p797519436303"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.151515151515152%" id="mcps1.1.5.1.2"><p id="p697512439305"><a name="p697512439305"></a><a name="p697512439305"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.19191919191919%" id="mcps1.1.5.1.3"><p id="p19975543163019"><a name="p19975543163019"></a><a name="p19975543163019"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="48.484848484848484%" id="mcps1.1.5.1.4"><p id="p15975184353016"><a name="p15975184353016"></a><a name="p15975184353016"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row159751431302"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p497524313301"><a name="p497524313301"></a><a name="p497524313301"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p1397564318309"><a name="p1397564318309"></a><a name="p1397564318309"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p1697510439301"><a name="p1697510439301"></a><a name="p1697510439301"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p15975114317309"><a name="p15975114317309"></a><a name="p15975114317309"></a>Device ID. The supported IDs can be obtained by calling <strong id="b1876127025102810"><a name="b1876127025102810"></a><a name="b1876127025102810"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row109751843163011"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p39751943113018"><a name="p39751943113018"></a><a name="p39751943113018"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p2975124363011"><a name="p2975124363011"></a><a name="p2975124363011"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p997554315307"><a name="p997554315307"></a><a name="p997554315307"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p49753435303"><a name="p49753435303"></a><a name="p49753435303"></a>Chip ID, which can be obtained by calling <strong id="b212438064102434"><a name="b212438064102434"></a><a name="b212438064102434"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p69753439303"><a name="p69753439303"></a><a name="p69753439303"></a>NPU: [0, <strong id="b1315851809105324"><a name="b1315851809105324"></a><a name="b1315851809105324"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row12975443183013"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p1097534333012"><a name="p1097534333012"></a><a name="p1097534333012"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p19975124303012"><a name="p19975124303012"></a><a name="p19975124303012"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p2975184311304"><a name="p2975184311304"></a><a name="p2975184311304"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p9975194310307"><a name="p9975194310307"></a><a name="p9975194310307"></a>DCMI_MAIN_CMD_LP</p>
</td>
</tr>
<tr id="row2975194333013"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p1497534343010"><a name="p1497534343010"></a><a name="p1497534343010"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p597594314306"><a name="p597594314306"></a><a name="p597594314306"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p6975743153011"><a name="p6975743153011"></a><a name="p6975743153011"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p18975343113017"><a name="p18975343113017"></a><a name="p18975343113017"></a>/* DCMI sub commond for Low power */</p>
<pre class="codeblock" id="codeblock971395520334"><a name="codeblock971395520334"></a><a name="codeblock971395520334"></a>typedef enum {
// Obtain the register values indicating the voltage and current of the AI Core.
DCMI_LP_SUB_CMD_AICORE_VOLTAGE_CURRENT = 0,
// Obtain the register values indicating the voltage and current of the Hybird.
DCMI_LP_SUB_CMD_HYBIRD_VOLTAGE_CURRENT,
// Obtain the register values indicating the voltage and current of the CPU.
DCMI_LP_SUB_CMD_TAISHAN_VOLTAGE_CURRENT,
// Obtain the register values indicating the voltage and current of the DDR.
DCMI_LP_SUB_CMD_DDR_VOLTAGE_CURRENT,
// Obtain the count of ACG frequency modulations.
DCMI_LP_SUB_CMD_ACG,
// Obtain the overall low-power status.
DCMI_LP_SUB_CMD_STATUS,
// Obtain all working levels.
DCMI_LP_SUB_CMD_TOPS_DETAILS,
// Set the working level.
DCMI_LP_SUB_CMD_SET_WORK_TOPS,
// Obtain the current working level.
DCMI_LP_SUB_CMD_GET_WORK_TOPS,
// Obtain the reason for the current frequency reduction.
DCMI_LP_SUB_CMD_AICORE_FREQREDUC_CAUSE,
// Obtain the power consumption information.
DCMI_LP_SUB_CMD_GET_POWER_INFO,
// Set the idle mode switch.
DCMI_LP_SUB_CMD_SET_IDLE_SWITCH,
DCMI_LP_SUB_CMD_MAX,
} DCMI_LP_SUB_CMD;</pre>
<p id="p0976643203017"><a name="p0976643203017"></a><a name="p0976643203017"></a><strong id="b731682217014"><a name="b731682217014"></a><a name="b731682217014"></a>DCMI_LP_SUB_CMD_AICORE_FREQREDUC_CAUSE</strong>, <strong id="b153171622207"><a name="b153171622207"></a><a name="b153171622207"></a>DCMI_LP_SUB_CMD_GET_POWER_INFO</strong>, and <strong id="b1131719221904"><a name="b1131719221904"></a><a name="b1131719221904"></a>DCMI_LP_SUB_CMD_STATUS</strong> commands are supported.</p>
</td>
</tr>
<tr id="row119761843173017"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p0976184343012"><a name="p0976184343012"></a><a name="p0976184343012"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p397614363010"><a name="p397614363010"></a><a name="p397614363010"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p29762043173012"><a name="p29762043173012"></a><a name="p29762043173012"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p179761943143013"><a name="p179761943143013"></a><a name="p179761943143013"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row6976543103019"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p119765433306"><a name="p119765433306"></a><a name="p119765433306"></a>Size</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p79761543173017"><a name="p79761543173017"></a><a name="p79761543173017"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p1976204317307"><a name="p1976204317307"></a><a name="p1976204317307"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p2976943123017"><a name="p2976943123017"></a><a name="p2976943123017"></a>Length of the <strong id="b198641439905"><a name="b198641439905"></a><a name="b198641439905"></a>buf</strong> array/Length of the returned result.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section386564317309"></a>**

<a name="table1689294312308"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b115029055685545"><a name="b115029055685545"></a><a name="b115029055685545"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section581311341081"></a>**

None.

**Restrictions<a name="section216210381817"></a>**

- Length of the  **buf**  array/Length of the returned result.
- The reason of AI Core frequency reduction is updated every 100 ms, and historical frequency reduction reasons cannot be queried.

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table9893154314306"></a>
<table><thead align="left"><tr id="row1097794373014"><th class="cellrowborder" valign="top" width="32.629999999999995%" id="mcps1.2.5.1.1"><p id="p597724343018"><a name="p597724343018"></a><a name="p597724343018"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="23.200000000000003%" id="mcps1.2.5.1.2"><p id="p15977743113017"><a name="p15977743113017"></a><a name="p15977743113017"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="18.6%" id="mcps1.2.5.1.3"><p id="p1977104363011"><a name="p1977104363011"></a><a name="p1977104363011"></a>Size</p>
</th>
<th class="cellrowborder" valign="top" width="25.569999999999997%" id="mcps1.2.5.1.4"><p id="p1097718435302"><a name="p1097718435302"></a><a name="p1097718435302"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row497744313309"><td class="cellrowborder" valign="top" width="32.629999999999995%" headers="mcps1.2.5.1.1 "><p id="p1897794353018"><a name="p1897794353018"></a><a name="p1897794353018"></a>DCMI_LP_SUB_CMD_AICORE_FREQREDUC_CAUSE</p>
</td>
<td class="cellrowborder" valign="top" width="23.200000000000003%" headers="mcps1.2.5.1.2 "><p id="p159771243123013"><a name="p159771243123013"></a><a name="p159771243123013"></a>Unsigned long long</p>
</td>
<td class="cellrowborder" valign="top" width="18.6%" headers="mcps1.2.5.1.3 "><p id="p15977204323010"><a name="p15977204323010"></a><a name="p15977204323010"></a>sizeof(unsigned long long)</p>
</td>
<td class="cellrowborder" valign="top" width="25.569999999999997%" headers="mcps1.2.5.1.4 "><p id="p197784315304"><a name="p197784315304"></a><a name="p197784315304"></a><strong id="b21345214266"><a name="b21345214266"></a><a name="b21345214266"></a>buf</strong> indicates the 8-byte memory space. Each bit corresponds to a frequency reduction reason.</p>
</td>
</tr>
<tr id="row697713436302"><td class="cellrowborder" valign="top" width="32.629999999999995%" headers="mcps1.2.5.1.1 "><p id="p1797774310308"><a name="p1797774310308"></a><a name="p1797774310308"></a>DCMI_LP_SUB_CMD_GET_POWER_INFO</p>
</td>
<td class="cellrowborder" valign="top" width="23.200000000000003%" headers="mcps1.2.5.1.2 "><p id="p1977043163011"><a name="p1977043163011"></a><a name="p1977043163011"></a>DCMI_LP_POWER_INFO_STRU</p>
</td>
<td class="cellrowborder" valign="top" width="18.6%" headers="mcps1.2.5.1.3 "><p id="p6977144314302"><a name="p6977144314302"></a><a name="p6977144314302"></a>sizeof(DCMI_LP_POWER_INFO_STRU)</p>
</td>
<td class="cellrowborder" valign="top" width="25.569999999999997%" headers="mcps1.2.5.1.4 "><p id="p397764343016"><a name="p397764343016"></a><a name="p397764343016"></a>#define DCMI_LP_POWER_RESERVED_LEN 32</p>
<p id="p12977124373012"><a name="p12977124373012"></a><a name="p12977124373012"></a>typedef struct DCMI_lp_power_info {</p>
<p id="p1697714323019"><a name="p1697714323019"></a><a name="p1697714323019"></a>unsigned int soc_rated_power;</p>
<p id="p797713439301"><a name="p797713439301"></a><a name="p797713439301"></a>unsigned char reserved[DCMI_LP_POWER_RESERVED_LEN];</p>
<p id="p197774318301"><a name="p197774318301"></a><a name="p197774318301"></a>} DCMI_LP_POWER_INFO_STRU;</p>
<p id="p1197794310304"><a name="p1197794310304"></a><a name="p1197794310304"></a><strong id="b6851883058723"><a name="b6851883058723"></a><a name="b6851883058723"></a>soc_rated_power</strong> indicates the rated power of the SoC, and other values are reserved for expansion.</p>
<p id="p7977184393012"><a name="p7977184393012"></a><a name="p7977184393012"></a>Correct range of the SoC rated power: [150000, 600000]</p>
</td>
</tr>
<tr id="row4977124333016"><td class="cellrowborder" valign="top" width="32.629999999999995%" headers="mcps1.2.5.1.1 "><p id="p12977743173013"><a name="p12977743173013"></a><a name="p12977743173013"></a>DCMI_LP_SUB_CMD_STATUS</p>
</td>
<td class="cellrowborder" valign="top" width="23.200000000000003%" headers="mcps1.2.5.1.2 "><p id="p597754320305"><a name="p597754320305"></a><a name="p597754320305"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="18.6%" headers="mcps1.2.5.1.3 "><p id="p097724315305"><a name="p097724315305"></a><a name="p097724315305"></a>sizeof(unsigned int)</p>
</td>
<td class="cellrowborder" valign="top" width="25.569999999999997%" headers="mcps1.2.5.1.4 "><p id="p16977243183018"><a name="p16977243183018"></a><a name="p16977243183018"></a><strong id="b2818985458723"><a name="b2818985458723"></a><a name="b2818985458723"></a>0</strong>: non-idle; <strong id="b9526010988723"><a name="b9526010988723"></a><a name="b9526010988723"></a>1</strong>: idle</p>
</td>
</tr>
</tbody>
</table>

In the subcommand  **DCMI\_LP\_SUB\_CMD\_AICORE\_FREQREDUC\_CAUSE**, the reason of AI Core frequency reduction is represented by a 64-bit value. Each bit corresponds to a frequency reduction reason. If the value is 0, the AI Core runs at a rated frequency. If the value is 1, the AI Core cannot run at a rated frequency. Frequency reduction may be caused by multiple factors. Therefore, multiple bits may be set to 1.

**Table  2**  Meaning of each bit in the buffer value

<a name="table188972043193011"></a>
<table><thead align="left"><tr id="row8977543133019"><th class="cellrowborder" valign="top" width="25.27%" id="mcps1.2.4.1.1"><p id="p7978843103017"><a name="p7978843103017"></a><a name="p7978843103017"></a>Name</p>
</th>
<th class="cellrowborder" valign="top" width="25.27%" id="mcps1.2.4.1.2"><p id="p59781843103015"><a name="p59781843103015"></a><a name="p59781843103015"></a>Bit</p>
</th>
<th class="cellrowborder" valign="top" width="49.46%" id="mcps1.2.4.1.3"><p id="p797834319309"><a name="p797834319309"></a><a name="p797834319309"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row7978143143012"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="p1497834315307"><a name="p1497834315307"></a><a name="p1497834315307"></a>IDLE</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="p129787433303"><a name="p129787433303"></a><a name="p129787433303"></a>0</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p17978164383013"><a name="p17978164383013"></a><a name="p17978164383013"></a>The AI Core is in the idle state. The power consumption is reduced by reducing the AI Core frequency to the idle state frequency. The frequency will be switched only after the idle state lasts for a period of time.</p>
</td>
</tr>
<tr id="row397814314309"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="p12978194317301"><a name="p12978194317301"></a><a name="p12978194317301"></a>THERMAL</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="p16978194383019"><a name="p16978194383019"></a><a name="p16978194383019"></a>2</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p897894393014"><a name="p897894393014"></a><a name="p897894393014"></a>The temperature of the Ascend AI Processor exceeds the allowed range. As a result, the lower layer software imposes a range limit on the AI Core frequency to reduce the processor temperature.</p>
</td>
</tr>
<tr id="row297804343016"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="p6978124373015"><a name="p6978124373015"></a><a name="p6978124373015"></a>SW_EDP</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="p1097824314304"><a name="p1097824314304"></a><a name="p1097824314304"></a>3</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p997864373012"><a name="p997864373012"></a><a name="p997864373012"></a>On the Ascend AI Processor, the power supply current of the AI Core module exceeds the allowed range. As a result, the lower layer software imposes a range limit on the AI Core frequency to ensure stable power supply to the AI Core module.</p>
</td>
</tr>
<tr id="row1897894313010"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="p1897816431303"><a name="p1897816431303"></a><a name="p1897816431303"></a>HW_EDP</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="p59781343123016"><a name="p59781343123016"></a><a name="p59781343123016"></a>4</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p1497804343013"><a name="p1497804343013"></a><a name="p1497804343013"></a>On the Ascend AI Processor, the transient power supply current of the AI Core module exceeds the allowed range. As a result, the current sensor on the mainboard triggers a quick frequency reduce of the AI Core module.</p>
</td>
</tr>
<tr id="row119787436308"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="p197813434307"><a name="p197813434307"></a><a name="p197813434307"></a>POWER_BREAK</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="p4978144310304"><a name="p4978144310304"></a><a name="p4978144310304"></a>5</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p597894323019"><a name="p597894323019"></a><a name="p597894323019"></a>After detecting that the power exceeds the upper limit, the power monitoring module on the mainboard instructs the Ascend AI Processor to quickly reduce the AI Core frequency to maintain stable power supply.</p>
</td>
</tr>
<tr id="row4978144318302"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="p897874303013"><a name="p897874303013"></a><a name="p897874303013"></a>SVFD</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="p11978134316303"><a name="p11978134316303"></a><a name="p11978134316303"></a>8</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p997804363012"><a name="p997804363012"></a><a name="p997804363012"></a>When the power supply monitoring module of the AI Core on the Ascend AI Processor detects that the power supply is unstable (with noises), the AI Core frequency is quickly scaled down to maintain stable power supply.</p>
</td>
</tr>
<tr id="row490292718112"><td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.1 "><p id="en-us_topic_0000001312397937_p11311634212"><a name="en-us_topic_0000001312397937_p11311634212"></a><a name="en-us_topic_0000001312397937_p11311634212"></a>POWERCAPPING</p>
</td>
<td class="cellrowborder" valign="top" width="25.27%" headers="mcps1.2.4.1.2 "><p id="en-us_topic_0000001312397937_p169341949144114"><a name="en-us_topic_0000001312397937_p169341949144114"></a><a name="en-us_topic_0000001312397937_p169341949144114"></a>10</p>
</td>
<td class="cellrowborder" valign="top" width="49.46%" headers="mcps1.2.4.1.3 "><p id="p13043467813"><a name="p13043467813"></a><a name="p13043467813"></a>The Ascend AI Processor performs out-of-band power consumption control through the iBMC.</p>
</td>
</tr>
</tbody>
</table>

**Table  3** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section207091751457"></a>**

```C
...
int ret = 0;
int card_id = 0;
int dev_id = 0;
int sub_cmd=0;
unsigned int voltage_cruuent_buf = 0;
unsigned int buf_size = 8;
ret = dcmi_get_device_info(card_id, dev_id, DCMI_MAIN_CMD_LP, sub_cmd, &voltage_cruuent_buf, &buf_size);
if (ret != 0) {
//todo
return ret;
} else {
// todo
return ret;
}
...
```

## DCMI\_MAIN\_CMD\_TS Command<a name="EN-US_TOPIC_0000002546903719"></a>

**Prototype<a name="section782265716308"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section19822105710305"></a>**

Obtains TS information in the system.

**Parameter Description<a name="section188227576305"></a>**

<a name="table584175773016"></a>
<table><thead align="left"><tr id="row158991057133012"><th class="cellrowborder" valign="top" width="17.17171717171717%" id="mcps1.1.5.1.1"><p id="p1189945783015"><a name="p1189945783015"></a><a name="p1189945783015"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.151515151515152%" id="mcps1.1.5.1.2"><p id="p13899165713012"><a name="p13899165713012"></a><a name="p13899165713012"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.19191919191919%" id="mcps1.1.5.1.3"><p id="p789919571306"><a name="p789919571306"></a><a name="p789919571306"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="48.484848484848484%" id="mcps1.1.5.1.4"><p id="p38991757123015"><a name="p38991757123015"></a><a name="p38991757123015"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row14899115710307"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p289935717300"><a name="p289935717300"></a><a name="p289935717300"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p158990579308"><a name="p158990579308"></a><a name="p158990579308"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p890025715306"><a name="p890025715306"></a><a name="p890025715306"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p1290095773011"><a name="p1290095773011"></a><a name="p1290095773011"></a>Device ID. The supported IDs can be obtained by calling <strong id="b859481033102811"><a name="b859481033102811"></a><a name="b859481033102811"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row1990019577303"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p109001457133015"><a name="p109001457133015"></a><a name="p109001457133015"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p1900175783013"><a name="p1900175783013"></a><a name="p1900175783013"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p13900185710307"><a name="p13900185710307"></a><a name="p13900185710307"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p18900957193015"><a name="p18900957193015"></a><a name="p18900957193015"></a>Chip ID, which can be obtained by calling <strong id="b803886844102435"><a name="b803886844102435"></a><a name="b803886844102435"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p390015577304"><a name="p390015577304"></a><a name="p390015577304"></a>NPU: [0, <strong id="b1556231144105325"><a name="b1556231144105325"></a><a name="b1556231144105325"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row19900125763017"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p18900657173012"><a name="p18900657173012"></a><a name="p18900657173012"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p5900155783016"><a name="p5900155783016"></a><a name="p5900155783016"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p89002574308"><a name="p89002574308"></a><a name="p89002574308"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p590010578307"><a name="p590010578307"></a><a name="p590010578307"></a>DCMI_MAIN_CMD_TS</p>
</td>
</tr>
<tr id="row0900155743018"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p139001157183018"><a name="p139001157183018"></a><a name="p139001157183018"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p18900165743017"><a name="p18900165743017"></a><a name="p18900165743017"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p1690018576308"><a name="p1690018576308"></a><a name="p1690018576308"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><pre class="codeblock" id="en-us_topic_0000002513346901_codeblock84586272352"><a name="en-us_topic_0000002513346901_codeblock84586272352"></a><a name="en-us_topic_0000002513346901_codeblock84586272352"></a>typedef enum {
// Obtain the single-core utilization of the AI Core. The normal value ranges from 0 to 100.
DCMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE = 0,
// Obtain the single-core utilization of the Vector Core/Obtain the utilization of the Vector unit in the single AI Core. The normal value ranges from 0 to 100.
DCMI_TS_SUB_CMD_VECTORCORE_UTILIZATION_RATE,
// Specify the FFTS or FFTS+ type. The value <strong id="en-us_topic_0000002513346901_b8995550272"><a name="en-us_topic_0000002513346901_b8995550272"></a><a name="en-us_topic_0000002513346901_b8995550272"></a>0</strong> indicates FFTS, and the value <strong id="en-us_topic_0000002513346901_b09951650672"><a name="en-us_topic_0000002513346901_b09951650672"></a><a name="en-us_topic_0000002513346901_b09951650672"></a>1</strong> indicates FFTS+.
DCMI_TS_SUB_CMD_FFTS_TYPE,
// Set the hardware-level mask to block AI Core errors.
DCMI_TS_SUB_CMD_SET_FAULT_MASK,
// Obtain the hardware-level mask to block AI Core errors.
DCMI_TS_SUB_CMD_GET_FAULT_MASK,
// Obtain the operator timeout scale. It defaults to <strong id="en-us_topic_0000002513346901_b650763211286"><a name="en-us_topic_0000002513346901_b650763211286"></a><a name="en-us_topic_0000002513346901_b650763211286"></a>32</strong>, and the value range is [20, 32].
DCMI_TS_SUB_CMD_COMMON_MSG,
DCMI_TS_SUB_CMD_MAX,
} DCMI_TS_SUB_CMD;</pre>
<p id="en-us_topic_0000002513346901_p20900857113015"><a name="en-us_topic_0000002513346901_p20900857113015"></a><a name="en-us_topic_0000002513346901_p20900857113015"></a><strong id="en-us_topic_0000002513346901_b8730615748530"><a name="en-us_topic_0000002513346901_b8730615748530"></a><a name="en-us_topic_0000002513346901_b8730615748530"></a>DCMI_TS_SUB_CMD_SET_FAULT_MASK</strong> and <strong id="en-us_topic_0000002513346901_b12414376578530"><a name="en-us_topic_0000002513346901_b12414376578530"></a><a name="en-us_topic_0000002513346901_b12414376578530"></a>DCMI_TS_SUB_CMD_GET_FAULT_MASK</strong> are not supported.</p>
<p id="en-us_topic_0000002513346901_p7900115703014"><a name="en-us_topic_0000002513346901_p7900115703014"></a><a name="en-us_topic_0000002513346901_p7900115703014"></a>If Profiling is enabled, the queried single-core utilization will be <strong id="en-us_topic_0000002513346901_b187470161920"><a name="en-us_topic_0000002513346901_b187470161920"></a><a name="en-us_topic_0000002513346901_b187470161920"></a>0xEF</strong>.</p>
</td>
</tr>
<tr id="row1190010575303"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p1190018574307"><a name="p1190018574307"></a><a name="p1190018574307"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p19007577307"><a name="p19007577307"></a><a name="p19007577307"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p17900175711302"><a name="p17900175711302"></a><a name="p17900175711302"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p390012575308"><a name="p390012575308"></a><a name="p390012575308"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row16900145719304"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p9900155716309"><a name="p9900155716309"></a><a name="p9900155716309"></a>size</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p0900185719304"><a name="p0900185719304"></a><a name="p0900185719304"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p6901257173010"><a name="p6901257173010"></a><a name="p6901257173010"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p090113577301"><a name="p090113577301"></a><a name="p090113577301"></a>Length of the <strong id="b39621213111915"><a name="b39621213111915"></a><a name="b39621213111915"></a>buf</strong> array/Length of the returned result.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section283355715302"></a>**

<a name="table1985018574309"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b179553645485546"><a name="b179553645485546"></a><a name="b179553645485546"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section6438163417415"></a>**

None.

**Restrictions<a name="section2889103794114"></a>**

For the query of single-core utilization of the Vector Core,  **buf**  must be at least 50 bytes. For the query of single-core utilization of the AI Core,  **buf**  must be at least 25 bytes.

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table08529574303"></a>
<table><thead align="left"><tr id="en-us_topic_0000002513346901_row19012578309"><th class="cellrowborder" valign="top" width="39.269999999999996%" id="mcps1.2.4.1.1"><p id="en-us_topic_0000002513346901_p59011457173015"><a name="en-us_topic_0000002513346901_p59011457173015"></a><a name="en-us_topic_0000002513346901_p59011457173015"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="36.96%" id="mcps1.2.4.1.2"><p id="en-us_topic_0000002513346901_p159011357193019"><a name="en-us_topic_0000002513346901_p159011357193019"></a><a name="en-us_topic_0000002513346901_p159011357193019"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="23.77%" id="mcps1.2.4.1.3"><p id="en-us_topic_0000002513346901_p1990165743011"><a name="en-us_topic_0000002513346901_p1990165743011"></a><a name="en-us_topic_0000002513346901_p1990165743011"></a>size</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002513346901_row12901185711304"><td class="cellrowborder" valign="top" width="39.269999999999996%" headers="mcps1.2.4.1.1 "><p id="en-us_topic_0000002513346901_p8901195718303"><a name="en-us_topic_0000002513346901_p8901195718303"></a><a name="en-us_topic_0000002513346901_p8901195718303"></a>DCMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE, DCMI_TS_SUB_CMD_VECTORCORE_UTILIZATION_RATE</p>
</td>
<td class="cellrowborder" valign="top" width="36.96%" headers="mcps1.2.4.1.2 "><p id="en-us_topic_0000002513346901_p1290105717308"><a name="en-us_topic_0000002513346901_p1290105717308"></a><a name="en-us_topic_0000002513346901_p1290105717308"></a>For the query of single-core utilization of the Vector Core, <strong id="en-us_topic_0000002513346901_b6866515181613"><a name="en-us_topic_0000002513346901_b6866515181613"></a><a name="en-us_topic_0000002513346901_b6866515181613"></a>buf</strong> must be at least 50 bytes. For the query of single-core utilization of the AI Core, <strong id="en-us_topic_0000002513346901_b1866151520168"><a name="en-us_topic_0000002513346901_b1866151520168"></a><a name="en-us_topic_0000002513346901_b1866151520168"></a>buf</strong> must be at least 25 bytes.</p>
<p id="en-us_topic_0000002513346901_p79011657193018"><a name="en-us_topic_0000002513346901_p79011657193018"></a><a name="en-us_topic_0000002513346901_p79011657193018"></a>Error values:</p>
<p id="en-us_topic_0000002513346901_p29014575305"><a name="en-us_topic_0000002513346901_p29014575305"></a><a name="en-us_topic_0000002513346901_p29014575305"></a><strong id="en-us_topic_0000002513346901_b17706454308530"><a name="en-us_topic_0000002513346901_b17706454308530"></a><a name="en-us_topic_0000002513346901_b17706454308530"></a>0xEE</strong>: The corresponding core is damaged.</p>
<p id="en-us_topic_0000002513346901_p4901115718309"><a name="en-us_topic_0000002513346901_p4901115718309"></a><a name="en-us_topic_0000002513346901_p4901115718309"></a><strong id="en-us_topic_0000002513346901_b1123339738530"><a name="en-us_topic_0000002513346901_b1123339738530"></a><a name="en-us_topic_0000002513346901_b1123339738530"></a>0xEF</strong>: invalid.</p>
<p id="en-us_topic_0000002513346901_p290185713306"><a name="en-us_topic_0000002513346901_p290185713306"></a><a name="en-us_topic_0000002513346901_p290185713306"></a>As an output, each byte indicates the utilization of a core. The value ranges from 0 to 100.</p>
</td>
<td class="cellrowborder" valign="top" width="23.77%" headers="mcps1.2.4.1.3 "><p id="en-us_topic_0000002513346901_p490115713018"><a name="en-us_topic_0000002513346901_p490115713018"></a><a name="en-us_topic_0000002513346901_p490115713018"></a>As an input, it indicates the buffer size.</p>
<p id="en-us_topic_0000002513346901_p2090125710307"><a name="en-us_topic_0000002513346901_p2090125710307"></a><a name="en-us_topic_0000002513346901_p2090125710307"></a>As an output, it indicates the number of valid values in the buffer.</p>
</td>
</tr>
<tr id="en-us_topic_0000002513346901_row6901185719303"><td class="cellrowborder" valign="top" width="39.269999999999996%" headers="mcps1.2.4.1.1 "><p id="en-us_topic_0000002513346901_p179011357143018"><a name="en-us_topic_0000002513346901_p179011357143018"></a><a name="en-us_topic_0000002513346901_p179011357143018"></a>DCMI_TS_SUB_CMD_FFTS_TYPE</p>
</td>
<td class="cellrowborder" valign="top" width="36.96%" headers="mcps1.2.4.1.2 "><p id="en-us_topic_0000002513346901_p89011657113020"><a name="en-us_topic_0000002513346901_p89011657113020"></a><a name="en-us_topic_0000002513346901_p89011657113020"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="23.77%" headers="mcps1.2.4.1.3 "><p id="en-us_topic_0000002513346901_p12901115712303"><a name="en-us_topic_0000002513346901_p12901115712303"></a><a name="en-us_topic_0000002513346901_p12901115712303"></a>unsigned int</p>
</td>
</tr>
<tr id="en-us_topic_0000002513346901_row19471316207"><td class="cellrowborder" valign="top" width="39.269999999999996%" headers="mcps1.2.4.1.1 "><p id="en-us_topic_0000002513346901_p1047117132015"><a name="en-us_topic_0000002513346901_p1047117132015"></a><a name="en-us_topic_0000002513346901_p1047117132015"></a>DCMI_TS_SUB_CMD_COMMON_MSG</p>
</td>
<td class="cellrowborder" valign="top" width="36.96%" headers="mcps1.2.4.1.2 "><p id="en-us_topic_0000002513346901_p19469111244813"><a name="en-us_topic_0000002513346901_p19469111244813"></a><a name="en-us_topic_0000002513346901_p19469111244813"></a>struct ts_dcmi_ctrl_msg_body_t {</p>
<p id="en-us_topic_0000002513346901_p1246961219488"><a name="en-us_topic_0000002513346901_p1246961219488"></a><a name="en-us_topic_0000002513346901_p1246961219488"></a>unsigned int msg_type;</p>
<p id="en-us_topic_0000002513346901_p246915123487"><a name="en-us_topic_0000002513346901_p246915123487"></a><a name="en-us_topic_0000002513346901_p246915123487"></a>union {</p>
<p id="en-us_topic_0000002513346901_p1746901219481"><a name="en-us_topic_0000002513346901_p1746901219481"></a><a name="en-us_topic_0000002513346901_p1746901219481"></a>ts_dcmi_task_timeout_t set_task_timeout_info;</p>
<p id="en-us_topic_0000002513346901_p64691512154816"><a name="en-us_topic_0000002513346901_p64691512154816"></a><a name="en-us_topic_0000002513346901_p64691512154816"></a>ts_dcmi_task_timeout_t get_task_timeout_info;</p>
<p id="en-us_topic_0000002513346901_p13469131244812"><a name="en-us_topic_0000002513346901_p13469131244812"></a><a name="en-us_topic_0000002513346901_p13469131244812"></a>} u;</p>
<p id="en-us_topic_0000002513346901_p134691512204817"><a name="en-us_topic_0000002513346901_p134691512204817"></a><a name="en-us_topic_0000002513346901_p134691512204817"></a>};</p>
<p id="en-us_topic_0000002513346901_p1719316326501"><a name="en-us_topic_0000002513346901_p1719316326501"></a><a name="en-us_topic_0000002513346901_p1719316326501"></a>The ts_dcmi_task_timeout_t structure is as follows:</p>
<p id="en-us_topic_0000002513346901_p522516213502"><a name="en-us_topic_0000002513346901_p522516213502"></a><a name="en-us_topic_0000002513346901_p522516213502"></a>typedef struct {</p>
<p id="en-us_topic_0000002513346901_p32251323501"><a name="en-us_topic_0000002513346901_p32251323501"></a><a name="en-us_topic_0000002513346901_p32251323501"></a>unsigned int timeout_limit_exp; // Value range: [20, 32]</p>
<p id="en-us_topic_0000002513346901_p222512155013"><a name="en-us_topic_0000002513346901_p222512155013"></a><a name="en-us_topic_0000002513346901_p222512155013"></a>unsigned char rsev[TS_RSEV_MAX_LENTH]; // TS_RSEV_MAX_LENTH is 36.</p>
<p id="en-us_topic_0000002513346901_p10225725506"><a name="en-us_topic_0000002513346901_p10225725506"></a><a name="en-us_topic_0000002513346901_p10225725506"></a>} ts_dcmi_task_timeout_t;</p>
</td>
<td class="cellrowborder" valign="top" width="23.77%" headers="mcps1.2.4.1.3 "><p id="en-us_topic_0000002513346901_p19359252485"><a name="en-us_topic_0000002513346901_p19359252485"></a><a name="en-us_topic_0000002513346901_p19359252485"></a>As an input, it indicates the <strong id="en-us_topic_0000002513346901_b112687066942119"><a name="en-us_topic_0000002513346901_b112687066942119"></a><a name="en-us_topic_0000002513346901_b112687066942119"></a>buf</strong> size. The <strong id="en-us_topic_0000002513346901_b10783267042119"><a name="en-us_topic_0000002513346901_b10783267042119"></a><a name="en-us_topic_0000002513346901_b10783267042119"></a>buf</strong> must be at least 44 bytes.</p>
<p id="en-us_topic_0000002513346901_p19218195310"><a name="en-us_topic_0000002513346901_p19218195310"></a><a name="en-us_topic_0000002513346901_p19218195310"></a><strong id="en-us_topic_0000002513346901_b111214241842121"><a name="en-us_topic_0000002513346901_b111214241842121"></a><a name="en-us_topic_0000002513346901_b111214241842121"></a>msg_type</strong> is <strong id="en-us_topic_0000002513346901_b48388923942121"><a name="en-us_topic_0000002513346901_b48388923942121"></a><a name="en-us_topic_0000002513346901_b48388923942121"></a>0</strong> or <strong id="en-us_topic_0000002513346901_b121005026642121"><a name="en-us_topic_0000002513346901_b121005026642121"></a><a name="en-us_topic_0000002513346901_b121005026642121"></a>1</strong>.</p>
<a name="en-us_topic_0000002513346901_ul16848433145315"></a><a name="en-us_topic_0000002513346901_ul16848433145315"></a><ul id="en-us_topic_0000002513346901_ul16848433145315"><li>The value <strong id="en-us_topic_0000002513346901_b196570210742125"><a name="en-us_topic_0000002513346901_b196570210742125"></a><a name="en-us_topic_0000002513346901_b196570210742125"></a>1</strong> indicates setting the AICPU operator timeout value.</li><li>The value <strong id="en-us_topic_0000002513346901_b491854372117"><a name="en-us_topic_0000002513346901_b491854372117"></a><a name="en-us_topic_0000002513346901_b491854372117"></a>0</strong> indicates querying the AICPU operator timeout value.</li></ul>
</td>
</tr>
</tbody>
</table>

**Table  2** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section49611431655"></a>**

```C
...
int ret;
int card_id = 0;
int dev_id = 0;
void *buf = NULL;
int buf_size = 10;
buf = calloc(buf_size, sizeof(char));
if (buf == NULL) {
printf("calloc buf failed.\n");
return -1;
}
ret = dcmi_get_device_info(card_id,dev_id,DCMI_MAIN_CMD_TS, DCMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE, buf, &buf_size);
if (ret != 0) {
printf("dcmi_get_device_info failed, ret = %d.\n", ret);
return -1;
}
return 0;
...
```

## DCMI\_MAIN\_CMD\_QOS Command<a name="EN-US_TOPIC_0000002515343930"></a>

**Prototype<a name="section14467101743119"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section204687171315"></a>**

Obtains QoS configuration, including mpamid and master QoS configuration and bandwidth statistics.

**Parameter Description<a name="section1546861713117"></a>**

<a name="table150171718316"></a>
<table><thead align="left"><tr id="row45967179312"><th class="cellrowborder" valign="top" width="17.17171717171717%" id="mcps1.1.5.1.1"><p id="p1359641710311"><a name="p1359641710311"></a><a name="p1359641710311"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.151515151515152%" id="mcps1.1.5.1.2"><p id="p1596181753116"><a name="p1596181753116"></a><a name="p1596181753116"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.19191919191919%" id="mcps1.1.5.1.3"><p id="p13596161712315"><a name="p13596161712315"></a><a name="p13596161712315"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="48.484848484848484%" id="mcps1.1.5.1.4"><p id="p1359691733113"><a name="p1359691733113"></a><a name="p1359691733113"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row1159661713116"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p259611174316"><a name="p259611174316"></a><a name="p259611174316"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p3596151773118"><a name="p3596151773118"></a><a name="p3596151773118"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p059611170317"><a name="p059611170317"></a><a name="p059611170317"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p959612177316"><a name="p959612177316"></a><a name="p959612177316"></a>Device ID. The supported IDs can be obtained by calling <strong id="b2100981364102812"><a name="b2100981364102812"></a><a name="b2100981364102812"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row19596817133118"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p185961717183110"><a name="p185961717183110"></a><a name="p185961717183110"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p195961517143118"><a name="p195961517143118"></a><a name="p195961517143118"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p1359617171318"><a name="p1359617171318"></a><a name="p1359617171318"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p059671763113"><a name="p059671763113"></a><a name="p059671763113"></a>Chip ID, which can be obtained by calling <strong id="b2077553915102436"><a name="b2077553915102436"></a><a name="b2077553915102436"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p20596131743115"><a name="p20596131743115"></a><a name="p20596131743115"></a>NPU: [0, <strong id="b1579807409105326"><a name="b1579807409105326"></a><a name="b1579807409105326"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row459661713310"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p105971917113120"><a name="p105971917113120"></a><a name="p105971917113120"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p165973173319"><a name="p165973173319"></a><a name="p165973173319"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p3597617163117"><a name="p3597617163117"></a><a name="p3597617163117"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p45971417163119"><a name="p45971417163119"></a><a name="p45971417163119"></a>DCMI_MAIN_CMD_QOS</p>
</td>
</tr>
<tr id="row25975175318"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p659741718311"><a name="p659741718311"></a><a name="p659741718311"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p185971617163113"><a name="p185971617163113"></a><a name="p185971617163113"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p175971117123115"><a name="p175971117123115"></a><a name="p175971117123115"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><pre class="codeblock" id="codeblock13311208103610"><a name="codeblock13311208103610"></a><a name="codeblock13311208103610"></a>typedef enum {
// MATA configuration
DCMI_QOS_SUB_MATA_CONFIG,
// Master configuration
DCMI_QOS_SUB_MASTER_CONFIG,
// Bandwidth statistics
DCMI_QOS_SUB_BW_DATA,
// Global configuration
DCMI_QOS_SUB_GLOBAL_CONFIG,
// Completion instruction
DCMI_QOS_SUB_CONFIG_DONE,
} DCMI_QOS_SUB_INFO;</pre>
<p id="p5929173053615"><a name="p5929173053615"></a><a name="p5929173053615"></a>Currently, only <strong id="b19153729102718"><a name="b19153729102718"></a><a name="b19153729102718"></a>DCMI_QOS_SUB_MATA_CONFIG</strong>, <strong id="b915362902715"><a name="b915362902715"></a><a name="b915362902715"></a>DCMI_QOS_SUB_MASTER_CONFIG</strong>, <strong id="b10154729172711"><a name="b10154729172711"></a><a name="b10154729172711"></a>DCMI_QOS_SUB_BW_DATA</strong>, and <strong id="b15154132902714"><a name="b15154132902714"></a><a name="b15154132902714"></a>DCMI_QOS_SUB_GLOBAL_CONFIG</strong> are supported.</p>
</td>
</tr>
<tr id="row1059751753117"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p165976179310"><a name="p165976179310"></a><a name="p165976179310"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p105971717153116"><a name="p105971717153116"></a><a name="p105971717153116"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p13597191753119"><a name="p13597191753119"></a><a name="p13597191753119"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p13597161723117"><a name="p13597161723117"></a><a name="p13597161723117"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row1597151743112"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p05974176319"><a name="p05974176319"></a><a name="p05974176319"></a>size</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p45972017113117"><a name="p45972017113117"></a><a name="p45972017113117"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p8597717103117"><a name="p8597717103117"></a><a name="p8597717103117"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p1759791793116"><a name="p1759791793116"></a><a name="p1759791793116"></a>Length of the <strong id="b61221334284"><a name="b61221334284"></a><a name="b61221334284"></a>buf</strong> array/Length of the returned result.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section34771217163116"></a>**

<a name="table165071517183118"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b140690750585547"><a name="b140690750585547"></a><a name="b140690750585547"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section9677135115517"></a>**

None.

**Restrictions<a name="section2977135411511"></a>**

- Restricted by the SoC feature, call  **DCMI\_set\_device\_info**  before calling the corresponding  **get**  API to read whether the configuration takes effect. Otherwise, the read value is unreliable.
- The function of obtaining the bandwidth by using  **resctrl**  conflicts with the function of obtaining the real-time bandwidth by using the DCMI API. Do not use both simultaneously.

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table1450912173312"></a>
<table><thead align="left"><tr id="row1359861743110"><th class="cellrowborder" valign="top" width="31.403140314031404%" id="mcps1.2.5.1.1"><p id="p16598617103119"><a name="p16598617103119"></a><a name="p16598617103119"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="20.122012201220123%" id="mcps1.2.5.1.2"><p id="p8598131713110"><a name="p8598131713110"></a><a name="p8598131713110"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="15.701570157015702%" id="mcps1.2.5.1.3"><p id="p195981717153117"><a name="p195981717153117"></a><a name="p195981717153117"></a>buf_size</p>
</th>
<th class="cellrowborder" valign="top" width="32.77327732773277%" id="mcps1.2.5.1.4"><p id="p559831773117"><a name="p559831773117"></a><a name="p559831773117"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row1459821713316"><td class="cellrowborder" valign="top" width="31.403140314031404%" headers="mcps1.2.5.1.1 "><p id="p659871712318"><a name="p659871712318"></a><a name="p659871712318"></a>DCMI_QOS_SUB_MATA_CONFIG</p>
</td>
<td class="cellrowborder" valign="top" width="20.122012201220123%" headers="mcps1.2.5.1.2 "><p id="p105981117163113"><a name="p105981117163113"></a><a name="p105981117163113"></a>struct qos_mata_config</p>
<p id="p3598917133118"><a name="p3598917133118"></a><a name="p3598917133118"></a>{</p>
<p id="p15598181793119"><a name="p15598181793119"></a><a name="p15598181793119"></a>int mpamid;</p>
<p id="p14598121753118"><a name="p14598121753118"></a><a name="p14598121753118"></a>u32 bw_high;</p>
<p id="p105981917153115"><a name="p105981917153115"></a><a name="p105981917153115"></a>u32 bw_low;</p>
<p id="p7598117133110"><a name="p7598117133110"></a><a name="p7598117133110"></a>int hardlimit;</p>
<p id="p359881793112"><a name="p359881793112"></a><a name="p359881793112"></a>int reserved[8];</p>
<p id="p45981317203119"><a name="p45981317203119"></a><a name="p45981317203119"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="15.701570157015702%" headers="mcps1.2.5.1.3 "><p id="p85982017103119"><a name="p85982017103119"></a><a name="p85982017103119"></a>sizeof(qos_mata_</p>
<p id="p65987174315"><a name="p65987174315"></a><a name="p65987174315"></a>config)</p>
</td>
<td class="cellrowborder" valign="top" width="32.77327732773277%" headers="mcps1.2.5.1.4 "><p id="p55981017153116"><a name="p55981017153116"></a><a name="p55981017153116"></a><strong id="b3334165711589"><a name="b3334165711589"></a><a name="b3334165711589"></a>mpamid</strong>: The value range is [0, 127].</p>
<p id="p959881723117"><a name="p959881723117"></a><a name="p959881723117"></a><strong id="b15114728768735"><a name="b15114728768735"></a><a name="b15114728768735"></a>bw_high</strong>: high waterline (GB/s).</p>
<p id="p15598141713319"><a name="p15598141713319"></a><a name="p15598141713319"></a>The value range is [0, 1638].</p>
<p id="p1959917172315"><a name="p1959917172315"></a><a name="p1959917172315"></a><strong id="b52297019595"><a name="b52297019595"></a><a name="b52297019595"></a>bw_low</strong>: low waterline (GB/s). The value range is [0, <strong id="b622960145910"><a name="b622960145910"></a><a name="b622960145910"></a>bw_high</strong>].</p>
<p id="p20599121793112"><a name="p20599121793112"></a><a name="p20599121793112"></a><strong id="b4285153155912"><a name="b4285153155912"></a><a name="b4285153155912"></a>hardlimit</strong>: The value <strong id="b1928514312591"><a name="b1928514312591"></a><a name="b1928514312591"></a>1</strong> indicates enabled, whereas the value <strong id="b16285036592"><a name="b16285036592"></a><a name="b16285036592"></a>0</strong> indicates disabled.</p>
<p id="p13599191773117"><a name="p13599191773117"></a><a name="p13599191773117"></a>The waterlines you read by using this subcommand may be different from those you set. Difference = Maximum processor bandwidth/<strong id="b75141916192911"><a name="b75141916192911"></a><a name="b75141916192911"></a>MAX_REG_VALUE</strong>. The value of <strong id="b58411121122918"><a name="b58411121122918"></a><a name="b58411121122918"></a>MAX_REG_VALUE</strong> is 1024.</p>
</td>
</tr>
<tr id="row1599121733110"><td class="cellrowborder" valign="top" width="31.403140314031404%" headers="mcps1.2.5.1.1 "><p id="p359910173318"><a name="p359910173318"></a><a name="p359910173318"></a>DCMI_QOS_SUB_MASTER_CONFIG</p>
</td>
<td class="cellrowborder" valign="top" width="20.122012201220123%" headers="mcps1.2.5.1.2 "><p id="p125995179319"><a name="p125995179319"></a><a name="p125995179319"></a>struct qos_master_config</p>
<p id="p1659941753115"><a name="p1659941753115"></a><a name="p1659941753115"></a>{</p>
<p id="p165991417173110"><a name="p165991417173110"></a><a name="p165991417173110"></a>int master;</p>
<p id="p205995175318"><a name="p205995175318"></a><a name="p205995175318"></a>int mpamid;</p>
<p id="p45991117143117"><a name="p45991117143117"></a><a name="p45991117143117"></a>int qos;</p>
<p id="p145992172312"><a name="p145992172312"></a><a name="p145992172312"></a>int pmg;</p>
<p id="p1859961783120"><a name="p1859961783120"></a><a name="p1859961783120"></a>u64 bitmap[4];</p>
<p id="p4599217193119"><a name="p4599217193119"></a><a name="p4599217193119"></a>int reserved[8];</p>
<p id="p4599171710317"><a name="p4599171710317"></a><a name="p4599171710317"></a>};</p>
<p id="p1599101711319"><a name="p1599101711319"></a><a name="p1599101711319"></a></p>
</td>
<td class="cellrowborder" valign="top" width="15.701570157015702%" headers="mcps1.2.5.1.3 "><p id="p259917179318"><a name="p259917179318"></a><a name="p259917179318"></a>sizeof(qos_master</p>
<p id="p18599121713116"><a name="p18599121713116"></a><a name="p18599121713116"></a>_config)</p>
</td>
<td class="cellrowborder" valign="top" width="32.77327732773277%" headers="mcps1.2.5.1.4 "><p id="p1659911753116"><a name="p1659911753116"></a><a name="p1659911753116"></a><strong id="b1623681263418"><a name="b1623681263418"></a><a name="b1623681263418"></a>master</strong>: master ID. The options are as follows:</p>
<p id="p13599317193112"><a name="p13599317193112"></a><a name="p13599317193112"></a>vdec=1,vpc=2,jpge=3,</p>
<p id="p1759951773118"><a name="p1759951773118"></a><a name="p1759951773118"></a>jpgd=4,pcie=7,sdma=13</p>
<p id="p959901703114"><a name="p959901703114"></a><a name="p959901703114"></a><strong id="b15910102716590"><a name="b15910102716590"></a><a name="b15910102716590"></a>mpamid</strong>: The value range is [0, 127].</p>
<p id="p9599517133119"><a name="p9599517133119"></a><a name="p9599517133119"></a><strong id="b1898144719295"><a name="b1898144719295"></a><a name="b1898144719295"></a>qos</strong>: bandwidth scheduling priority. The value range is [0, 7]. The value <strong id="b152891941195918"><a name="b152891941195918"></a><a name="b152891941195918"></a>0</strong> indicates hardlimit QoS, and the value <strong id="b92954511593"><a name="b92954511593"></a><a name="b92954511593"></a>7</strong> indicates green channel QoS.</p>
<p id="p1459901712318"><a name="p1459901712318"></a><a name="p1459901712318"></a><strong id="b1864216508735"><a name="b1864216508735"></a><a name="b1864216508735"></a>pmg</strong>: mpamid group. The value range is [0, 3] (not supported currently).</p>
<p id="p11599171718314"><a name="p11599171718314"></a><a name="p11599171718314"></a><strong id="b12990125065914"><a name="b12990125065914"></a><a name="b12990125065914"></a>bitmap</strong>: not supported due to framework restrictions.</p>
</td>
</tr>
<tr id="row185991417153117"><td class="cellrowborder" valign="top" width="31.403140314031404%" headers="mcps1.2.5.1.1 "><p id="p259931753110"><a name="p259931753110"></a><a name="p259931753110"></a>DCMI_QOS_SUB_BW_DATA</p>
</td>
<td class="cellrowborder" valign="top" width="20.122012201220123%" headers="mcps1.2.5.1.2 "><p id="p8599817113120"><a name="p8599817113120"></a><a name="p8599817113120"></a>struct qos_bw_result</p>
<p id="p12600917113118"><a name="p12600917113118"></a><a name="p12600917113118"></a>{</p>
<p id="p18600131743116"><a name="p18600131743116"></a><a name="p18600131743116"></a>int mpamid;</p>
<p id="p4600101763114"><a name="p4600101763114"></a><a name="p4600101763114"></a>u32 curr;</p>
<p id="p11600191714316"><a name="p11600191714316"></a><a name="p11600191714316"></a>u32 bw_max;</p>
<p id="p060018172318"><a name="p060018172318"></a><a name="p060018172318"></a>u32 bw_min;</p>
<p id="p960051723117"><a name="p960051723117"></a><a name="p960051723117"></a>u32 bw_mean;</p>
<p id="p86000174315"><a name="p86000174315"></a><a name="p86000174315"></a>int reserved[8];</p>
<p id="p16003175318"><a name="p16003175318"></a><a name="p16003175318"></a>};</p>
<p id="p260018171313"><a name="p260018171313"></a><a name="p260018171313"></a></p>
</td>
<td class="cellrowborder" valign="top" width="15.701570157015702%" headers="mcps1.2.5.1.3 "><p id="p156001179310"><a name="p156001179310"></a><a name="p156001179310"></a>sizeof(qos_bw_result)</p>
</td>
<td class="cellrowborder" valign="top" width="32.77327732773277%" headers="mcps1.2.5.1.4 "><p id="p136002017103117"><a name="p136002017103117"></a><a name="p136002017103117"></a><strong id="b46391751155914"><a name="b46391751155914"></a><a name="b46391751155914"></a>mpamid</strong>: target mpamid to obtain the bandwidth. The value range is [0, 127].</p>
<p id="p1460019179315"><a name="p1460019179315"></a><a name="p1460019179315"></a><strong id="b1079620715010"><a name="b1079620715010"></a><a name="b1079620715010"></a>curr</strong>: latest bandwidth (MB/s).</p>
<p id="p5600101793110"><a name="p5600101793110"></a><a name="p5600101793110"></a><strong id="b15151295398735"><a name="b15151295398735"></a><a name="b15151295398735"></a>bw_max</strong>: maximum value (MB/s) over the sampling period.</p>
<p id="p1060010171314"><a name="p1060010171314"></a><a name="p1060010171314"></a><strong id="b168212037173020"><a name="b168212037173020"></a><a name="b168212037173020"></a>bw_min</strong>: minimum value (MB/s) over the sampling period.</p>
<p id="p10600617153119"><a name="p10600617153119"></a><a name="p10600617153119"></a><strong id="b5535401306"><a name="b5535401306"></a><a name="b5535401306"></a>bw_mean</strong>: mean value (MB/s) over the sampling period.</p>
</td>
</tr>
<tr id="row1560041716314"><td class="cellrowborder" valign="top" width="31.403140314031404%" headers="mcps1.2.5.1.1 "><p id="p12600917143112"><a name="p12600917143112"></a><a name="p12600917143112"></a>DCMI_QOS_SUB_GLOBAL_CONFIG</p>
</td>
<td class="cellrowborder" valign="top" width="20.122012201220123%" headers="mcps1.2.5.1.2 "><p id="p3600717123114"><a name="p3600717123114"></a><a name="p3600717123114"></a>struct qos_gbl_config</p>
<p id="p76008172312"><a name="p76008172312"></a><a name="p76008172312"></a>{</p>
<p id="p3600131783116"><a name="p3600131783116"></a><a name="p3600131783116"></a>u32 enable;</p>
<p id="p126002177317"><a name="p126002177317"></a><a name="p126002177317"></a>u32 autoqos_fuse_en;</p>
<p id="p860021763110"><a name="p860021763110"></a><a name="p860021763110"></a>u32 mpamqos_fuse_mode;</p>
<p id="p16001617133115"><a name="p16001617133115"></a><a name="p16001617133115"></a>u32 mpam_subtype;</p>
<p id="p1600117143115"><a name="p1600117143115"></a><a name="p1600117143115"></a>int reserved[8];</p>
<p id="p126006174314"><a name="p126006174314"></a><a name="p126006174314"></a>};</p>
<p id="p4600121710319"><a name="p4600121710319"></a><a name="p4600121710319"></a></p>
</td>
<td class="cellrowborder" valign="top" width="15.701570157015702%" headers="mcps1.2.5.1.3 "><p id="p14600617163113"><a name="p14600617163113"></a><a name="p14600617163113"></a>sizeof(qos_gbl_config)</p>
</td>
<td class="cellrowborder" valign="top" width="32.77327732773277%" headers="mcps1.2.5.1.4 "><a name="ul15992846123713"></a><a name="ul15992846123713"></a><ul id="ul15992846123713"><li><strong id="b4151131423414"><a name="b4151131423414"></a><a name="b4151131423414"></a>enable</strong>: whether to enable the QoS function.<a name="ul1426935273713"></a><a name="ul1426935273713"></a><ul id="ul1426935273713"><li><strong id="b9076233248735"><a name="b9076233248735"></a><a name="b9076233248735"></a>0</strong>: disabled</li><li><strong id="b13081403288735"><a name="b13081403288735"></a><a name="b13081403288735"></a>1</strong>: enabled</li></ul>
</li><li><strong id="b4251225788735"><a name="b4251225788735"></a><a name="b4251225788735"></a>autoqos_fuse_en</strong>: whether to enable QoS fusion.<a name="ul161181811163814"></a><a name="ul161181811163814"></a><ul id="ul161181811163814"><li><strong id="b162198017314"><a name="b162198017314"></a><a name="b162198017314"></a>0</strong>: QoS fusion is disabled.</li><li><strong id="b275414383116"><a name="b275414383116"></a><a name="b275414383116"></a>1</strong>: QoS fusion starts.</li></ul>
</li><li><strong id="b182112913311"><a name="b182112913311"></a><a name="b182112913311"></a>mpamqos_fuse_mode</strong>: QoS fusion mode. This parameter takes effect when <strong id="b9211149173111"><a name="b9211149173111"></a><a name="b9211149173111"></a>autoqos_fuse_en</strong> is enabled.<a name="ul879644411386"></a><a name="ul879644411386"></a><ul id="ul879644411386"><li><strong id="b16170783408735"><a name="b16170783408735"></a><a name="b16170783408735"></a>0</strong>: average fusion</li><li><strong id="b1696503611182"><a name="b1696503611182"></a><a name="b1696503611182"></a>1</strong>: maximum value between the associated QoS and MPAM QoS</li><li><strong id="b3257858608735"><a name="b3257858608735"></a><a name="b3257858608735"></a>2</strong>: associated QoS as mpamqos</li></ul>
</li><li><strong id="b8837144678735"><a name="b8837144678735"></a><a name="b8837144678735"></a>mpam_subtype</strong>: bandwidth statistics mode<a name="ul16435328393"></a><a name="ul16435328393"></a><ul id="ul16435328393"><li><strong id="b4378217158735"><a name="b4378217158735"></a><a name="b4378217158735"></a>0</strong>: read and write bandwidth statistics</li><li><strong id="b13358349858735"><a name="b13358349858735"></a><a name="b13358349858735"></a>1</strong>: write bandwidth statistics</li><li><strong id="b20699086758735"><a name="b20699086758735"></a><a name="b20699086758735"></a>2</strong>: read bandwidth statistics</li></ul>
</li></ul>
</td>
</tr>
</tbody>
</table>

**Table  2** Support in different deployment scenarios

<a name="table1831828113412"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row19901425175211"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p1555917570362"><a name="en-us_topic_0000002515503852_p1555917570362"></a><a name="en-us_topic_0000002515503852_p1555917570362"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p201384912311"><a name="en-us_topic_0000002515503852_p201384912311"></a><a name="en-us_topic_0000002515503852_p201384912311"></a><strong id="en-us_topic_0000002515503852_b446915610411"><a name="en-us_topic_0000002515503852_b446915610411"></a><a name="en-us_topic_0000002515503852_b446915610411"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p513124915316"><a name="en-us_topic_0000002515503852_p513124915316"></a><a name="en-us_topic_0000002515503852_p513124915316"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b18560720419"><a name="en-us_topic_0000002515503852_b18560720419"></a><a name="en-us_topic_0000002515503852_b18560720419"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p1131549173112"><a name="en-us_topic_0000002515503852_p1131549173112"></a><a name="en-us_topic_0000002515503852_p1131549173112"></a><strong id="en-us_topic_0000002515503852_b14967981844"><a name="en-us_topic_0000002515503852_b14967981844"></a><a name="en-us_topic_0000002515503852_b14967981844"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p232525365317"><a name="en-us_topic_0000002515503852_p232525365317"></a><a name="en-us_topic_0000002515503852_p232525365317"></a><span id="en-us_topic_0000002515503852_ph3325165314533"><a name="en-us_topic_0000002515503852_ph3325165314533"></a><a name="en-us_topic_0000002515503852_ph3325165314533"></a><span id="en-us_topic_0000002515503852_text2032565316533"><a name="en-us_topic_0000002515503852_text2032565316533"></a><a name="en-us_topic_0000002515503852_text2032565316533"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001777715122_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p33251753135313"><a name="en-us_topic_0000002515503852_p33251753135313"></a><a name="en-us_topic_0000002515503852_p33251753135313"></a><span id="en-us_topic_0000002515503852_text232555315538"><a name="en-us_topic_0000002515503852_text232555315538"></a><a name="en-us_topic_0000002515503852_text232555315538"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p862174514211"><a name="en-us_topic_0000002515503852_p862174514211"></a><a name="en-us_topic_0000002515503852_p862174514211"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p12621104519215"><a name="en-us_topic_0000002515503852_p12621104519215"></a><a name="en-us_topic_0000002515503852_p12621104519215"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p6621204519220"><a name="en-us_topic_0000002515503852_p6621204519220"></a><a name="en-us_topic_0000002515503852_p6621204519220"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row293019912529"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p532515315535"><a name="en-us_topic_0000002515503852_p532515315535"></a><a name="en-us_topic_0000002515503852_p532515315535"></a><span id="en-us_topic_0000002515503852_text732545315315"><a name="en-us_topic_0000002515503852_text732545315315"></a><a name="en-us_topic_0000002515503852_text732545315315"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p386884075314"><a name="en-us_topic_0000002515503852_p386884075314"></a><a name="en-us_topic_0000002515503852_p386884075314"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p12868184075310"><a name="en-us_topic_0000002515503852_p12868184075310"></a><a name="en-us_topic_0000002515503852_p12868184075310"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p986814025319"><a name="en-us_topic_0000002515503852_p986814025319"></a><a name="en-us_topic_0000002515503852_p986814025319"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row29531122175210"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1832516539533"><a name="en-us_topic_0000002515503852_p1832516539533"></a><a name="en-us_topic_0000002515503852_p1832516539533"></a><span id="en-us_topic_0000002515503852_text143251353185313"><a name="en-us_topic_0000002515503852_text143251353185313"></a><a name="en-us_topic_0000002515503852_text143251353185313"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p16576467538"><a name="en-us_topic_0000002515503852_p16576467538"></a><a name="en-us_topic_0000002515503852_p16576467538"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13657346135315"><a name="en-us_topic_0000002515503852_p13657346135315"></a><a name="en-us_topic_0000002515503852_p13657346135315"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p10657164695315"><a name="en-us_topic_0000002515503852_p10657164695315"></a><a name="en-us_topic_0000002515503852_p10657164695315"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row2737141919528"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1632695317538"><a name="en-us_topic_0000002515503852_p1632695317538"></a><a name="en-us_topic_0000002515503852_p1632695317538"></a><span id="en-us_topic_0000002515503852_text532612537537"><a name="en-us_topic_0000002515503852_text532612537537"></a><a name="en-us_topic_0000002515503852_text532612537537"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1844654714531"><a name="en-us_topic_0000002515503852_p1844654714531"></a><a name="en-us_topic_0000002515503852_p1844654714531"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p64461747205318"><a name="en-us_topic_0000002515503852_p64461747205318"></a><a name="en-us_topic_0000002515503852_p64461747205318"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1446184725320"><a name="en-us_topic_0000002515503852_p1446184725320"></a><a name="en-us_topic_0000002515503852_p1446184725320"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row78171617135218"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p3326853195318"><a name="en-us_topic_0000002515503852_p3326853195318"></a><a name="en-us_topic_0000002515503852_p3326853195318"></a><span id="en-us_topic_0000002515503852_text912513032714"><a name="en-us_topic_0000002515503852_text912513032714"></a><a name="en-us_topic_0000002515503852_text912513032714"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p2967114725312"><a name="en-us_topic_0000002515503852_p2967114725312"></a><a name="en-us_topic_0000002515503852_p2967114725312"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1396754795312"><a name="en-us_topic_0000002515503852_p1396754795312"></a><a name="en-us_topic_0000002515503852_p1396754795312"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p596734725316"><a name="en-us_topic_0000002515503852_p596734725316"></a><a name="en-us_topic_0000002515503852_p596734725316"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1913121595213"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p532613533533"><a name="en-us_topic_0000002515503852_p532613533533"></a><a name="en-us_topic_0000002515503852_p532613533533"></a><span id="en-us_topic_0000002515503852_text332611538530"><a name="en-us_topic_0000002515503852_text332611538530"></a><a name="en-us_topic_0000002515503852_text332611538530"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1452484814533"><a name="en-us_topic_0000002515503852_p1452484814533"></a><a name="en-us_topic_0000002515503852_p1452484814533"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p19524174835320"><a name="en-us_topic_0000002515503852_p19524174835320"></a><a name="en-us_topic_0000002515503852_p19524174835320"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p652419482532"><a name="en-us_topic_0000002515503852_p652419482532"></a><a name="en-us_topic_0000002515503852_p652419482532"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row71831758163218"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p28486114331"><a name="en-us_topic_0000002515503852_p28486114331"></a><a name="en-us_topic_0000002515503852_p28486114331"></a><span id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section176618596619"></a>**

```C
...
int ret = 0;
int card_id = 0;
int dev_id = 0;
int sub_cmd=0;
int size =  sizeof(struct dcmi_qos_mata_config);
struct dcmi_qos_mata_config mataCfg = {0};
mataCfg.mpamid = 127;
unsigned int subCmd = (unsigned int)(DCMI_QOS_SUB_CMD_MAKE(mataCfg.mpamid, DCMI_QOS_SUB_MATA_CONFIG));
ret = dcmi_get_device_info(card_id, dev_id, DCMI_MAIN_CMD_QOS, subCmd, (void *)&mataCfg, &size);
if (ret != 0) {
//todo
return ret;
} else {
// todo
return ret;
}
...
```

## DCMI\_MAIN\_CMD\_HCCS Command<a name="EN-US_TOPIC_0000002515343864"></a>

**Prototype<a name="section208203016314"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section15811309317"></a>**

Obtains HCCS information.

**Parameter Description<a name="section198123020315"></a>**

<a name="table72213073115"></a>
<table><thead align="left"><tr id="row1672203023119"><th class="cellrowborder" valign="top" width="17.17171717171717%" id="mcps1.1.5.1.1"><p id="p187273012319"><a name="p187273012319"></a><a name="p187273012319"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.151515151515152%" id="mcps1.1.5.1.2"><p id="p173143013316"><a name="p173143013316"></a><a name="p173143013316"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.19191919191919%" id="mcps1.1.5.1.3"><p id="p873163083118"><a name="p873163083118"></a><a name="p873163083118"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="48.484848484848484%" id="mcps1.1.5.1.4"><p id="p2073630143115"><a name="p2073630143115"></a><a name="p2073630143115"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row12731304319"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p4730301317"><a name="p4730301317"></a><a name="p4730301317"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p12731730103114"><a name="p12731730103114"></a><a name="p12731730103114"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p20731830153112"><a name="p20731830153112"></a><a name="p20731830153112"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p1873930173118"><a name="p1873930173118"></a><a name="p1873930173118"></a>Device ID. The supported IDs can be obtained by calling <strong id="b972870490102813"><a name="b972870490102813"></a><a name="b972870490102813"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row1073230183110"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p17731830153111"><a name="p17731830153111"></a><a name="p17731830153111"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p12731330203114"><a name="p12731330203114"></a><a name="p12731330203114"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p77314303319"><a name="p77314303319"></a><a name="p77314303319"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p117320301313"><a name="p117320301313"></a><a name="p117320301313"></a>Chip ID, which can be obtained by calling <strong id="b1571067584102438"><a name="b1571067584102438"></a><a name="b1571067584102438"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p1073163019313"><a name="p1073163019313"></a><a name="p1073163019313"></a>NPU: [0, <strong id="b15462728105327"><a name="b15462728105327"></a><a name="b15462728105327"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row1973830183110"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p373130163117"><a name="p373130163117"></a><a name="p373130163117"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p127317307311"><a name="p127317307311"></a><a name="p127317307311"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p117310304314"><a name="p117310304314"></a><a name="p117310304314"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p1273193015315"><a name="p1273193015315"></a><a name="p1273193015315"></a>DCMI_MAIN_CMD_HCCS</p>
</td>
</tr>
<tr id="row373830113118"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p187333003113"><a name="p187333003113"></a><a name="p187333003113"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p873130123111"><a name="p873130123111"></a><a name="p873130123111"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p1773153003117"><a name="p1773153003117"></a><a name="p1773153003117"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p2591111610485"><a name="p2591111610485"></a><a name="p2591111610485"></a>typedef enum {</p>
<p id="p1659181664812"><a name="p1659181664812"></a><a name="p1659181664812"></a>DCMI_HCCS_CMD_GET_STATUS = 0,</p>
<p id="p18591416144813"><a name="p18591416144813"></a><a name="p18591416144813"></a>DCMI_HCCS_CMD_GET_LANE_INFO = 1,</p>
<p id="p326311521339"><a name="p326311521339"></a><a name="p326311521339"></a>DCMI_HCCS_CMD_GET_STATISTIC_INFO = 3,</p>
<p id="p14556241162211"><a name="p14556241162211"></a><a name="p14556241162211"></a>DCMI_HCCS_CMD_GET_STATISTIC_INFO_U64 = 5,</p>
<p id="p125911116174815"><a name="p125911116174815"></a><a name="p125911116174815"></a>DCMI_HCCS_CMD_MAX,</p>
<p id="p359181694813"><a name="p359181694813"></a><a name="p359181694813"></a>} DCMI_HCCS_SUB_CMD;</p>
</td>
</tr>
<tr id="row10735307313"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p127343014311"><a name="p127343014311"></a><a name="p127343014311"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p37353017317"><a name="p37353017317"></a><a name="p37353017317"></a>Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p373193010317"><a name="p373193010317"></a><a name="p373193010317"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p107415309318"><a name="p107415309318"></a><a name="p107415309318"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row1074230113112"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p97423015311"><a name="p97423015311"></a><a name="p97423015311"></a>Size</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p674183073114"><a name="p674183073114"></a><a name="p674183073114"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p874183073116"><a name="p874183073116"></a><a name="p874183073116"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p11741830133113"><a name="p11741830133113"></a><a name="p11741830133113"></a>Length of the returned result.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section111593003115"></a>**

<a name="table92723063116"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b110224060485548"><a name="b110224060485548"></a><a name="b110224060485548"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section77912615717"></a>**

None.

**Restrictions<a name="section09411081372"></a>**

For the PCIe inference and training hardware, HCCS is not supported. The query result of this API has no specific meaning.

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table12297302311"></a>
<table><thead align="left"><tr id="row37413013317"><th class="cellrowborder" valign="top" width="31.4%" id="mcps1.2.5.1.1"><p id="p10741330113120"><a name="p10741330113120"></a><a name="p10741330113120"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="31.4%" id="mcps1.2.5.1.2"><p id="p1874030103112"><a name="p1874030103112"></a><a name="p1874030103112"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="13.11%" id="mcps1.2.5.1.3"><p id="p1374530193111"><a name="p1374530193111"></a><a name="p1374530193111"></a>Size</p>
</th>
<th class="cellrowborder" valign="top" width="24.09%" id="mcps1.2.5.1.4"><p id="p1674030173116"><a name="p1674030173116"></a><a name="p1674030173116"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row1274193013316"><td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.1 "><p id="p1274203019311"><a name="p1274203019311"></a><a name="p1274203019311"></a>DCMI_HCCS_CMD_GET_STATUS</p>
</td>
<td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.2 "><p id="p156861185517"><a name="p156861185517"></a><a name="p156861185517"></a>struct dcmi_hccs_statues {</p>
<p id="p9681811135520"><a name="p9681811135520"></a><a name="p9681811135520"></a>unsigned int pcs_status;</p>
<p id="p10797175091019"><a name="p10797175091019"></a><a name="p10797175091019"></a>unsigned int hdlc_status;</p>
<p id="p1668151135514"><a name="p1668151135514"></a><a name="p1668151135514"></a>unsigned char reserve[DCMI_HCCS_STATUS_RESERVED_LEN];</p>
<p id="p1868151113554"><a name="p1868151113554"></a><a name="p1868151113554"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="13.11%" headers="mcps1.2.5.1.3 "><p id="p1474123013119"><a name="p1474123013119"></a><a name="p1474123013119"></a>sizeof(struct dcmi_hccs_statues)</p>
</td>
<td class="cellrowborder" valign="top" width="24.09%" headers="mcps1.2.5.1.4 "><p id="p275203013113"><a name="p275203013113"></a><a name="p275203013113"></a>Queries the HCCS status. Currently, the PCS and HDLC status can be queried.</p>
<a name="ul5663163334913"></a><a name="ul5663163334913"></a><ul id="ul5663163334913"><li><strong id="b19793242163413"><a name="b19793242163413"></a><a name="b19793242163413"></a>pcs_status</strong> indicates the HCCS PCS status. The meanings of the values are as follows:<a name="ul101881658173719"></a><a name="ul101881658173719"></a><ul id="ul101881658173719"><li><strong id="b1378219485358"><a name="b1378219485358"></a><a name="b1378219485358"></a>0</strong> indicates that the status is normal.</li><li>A non-zero value indicates that the status is abnormal.</li></ul>
<p id="p2469153944918"><a name="p2469153944918"></a><a name="p2469153944918"></a>Specifically:</p>
<a name="ul1693262125011"></a><a name="ul1693262125011"></a><ul id="ul1693262125011"><li>Bit 0 is fixed at <strong id="b6257165016443"><a name="b6257165016443"></a><a name="b6257165016443"></a>1</strong> when the status is abnormal.</li><li>Bits 1 to 7 are fixed at <strong id="b139735304513"><a name="b139735304513"></a><a name="b139735304513"></a>0</strong> and are reserved for extension.</li><li>Bits 8 to 15 indicate the sequence number of the faulty PCS. The sequence number ranges from 0 to 7 and indicates the index number of the first faulty PCS starting from 0.</li><li>Bits 16 to 23 indicate the current lane mode of the PCS.<p id="p86611229115012"><a name="p86611229115012"></a><a name="p86611229115012"></a><strong id="b634814514517"><a name="b634814514517"></a><a name="b634814514517"></a>0</strong>: 0lane</p>
<p id="p176877310505"><a name="p176877310505"></a><a name="p176877310505"></a><strong id="b1052315501454"><a name="b1052315501454"></a><a name="b1052315501454"></a>1</strong>: 1lane</p>
<p id="p0883143345017"><a name="p0883143345017"></a><a name="p0883143345017"></a><strong id="b12086527456"><a name="b12086527456"></a><a name="b12086527456"></a>2</strong>: 4lane</p>
<p id="p199651627105011"><a name="p199651627105011"></a><a name="p199651627105011"></a><strong id="b1543412531457"><a name="b1543412531457"></a><a name="b1543412531457"></a>3</strong>: 8lane</p>
</li><li>Bits 24 to 31 indicate the current lane of the PCS.</li></ul>
</li><li><strong id="b959945319353"><a name="b959945319353"></a><a name="b959945319353"></a>pcs_status</strong> indicates the HCCS PCS status. The meanings of the values are as follows:<a name="ul15277145372620"></a><a name="ul15277145372620"></a><ul id="ul15277145372620"><li>Bits 2 to 31 are fixed at 0 and are reserved for extension.</li><li>Bits 0 to 1 indicate the HCCS HDLC initialization status.<a name="ul11478295276"></a><a name="ul11478295276"></a><ul id="ul11478295276"><li><strong id="b080062820149"><a name="b080062820149"></a><a name="b080062820149"></a>00</strong>: waiting for initialization</li><li><strong id="b7660183001412"><a name="b7660183001412"></a><a name="b7660183001412"></a>01</strong>: initialization packets exchanged successfully</li><li><strong id="b19905332181413"><a name="b19905332181413"></a><a name="b19905332181413"></a>10</strong>: waiting for initialization to complete</li><li><strong id="b2786637151412"><a name="b2786637151412"></a><a name="b2786637151412"></a>11</strong>: successful initialization (the HCCS HDLC status is normal.)</li></ul>
</li></ul>
</li></ul>
</td>
</tr>
<tr id="row3561143218479"><td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.1 "><p id="p2266153518472"><a name="p2266153518472"></a><a name="p2266153518472"></a>DCMI_HCCS_CMD_GET_LANE_INFO</p>
</td>
<td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.2 "><p id="p1126619351474"><a name="p1126619351474"></a><a name="p1126619351474"></a>struct dcmi_hccs_lane_info {</p>
<p id="p926603514474"><a name="p926603514474"></a><a name="p926603514474"></a>unsigned int hccs_port_pcs_bitmap;</p>
<p id="p11266133513473"><a name="p11266133513473"></a><a name="p11266133513473"></a>unsigned int pcs_lane_bitmap[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p626673519479"><a name="p626673519479"></a><a name="p626673519479"></a>unsigned int reserve[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p10266135194714"><a name="p10266135194714"></a><a name="p10266135194714"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="13.11%" headers="mcps1.2.5.1.3 "><p id="p14266435114716"><a name="p14266435114716"></a><a name="p14266435114716"></a>sizeof(struct dcmi_hccs_lane_info)</p>
</td>
<td class="cellrowborder" valign="top" width="24.09%" headers="mcps1.2.5.1.4 "><a name="ul8832161775120"></a><a name="ul8832161775120"></a><ul id="ul8832161775120"><li><strong id="b081655184620"><a name="b081655184620"></a><a name="b081655184620"></a>hccs_port_pcs_bitmap</strong> indicates whether a port is used for connecting the HCCS to the PCS. The less significant a bit in the bitmap, the smaller the port number. For each bit, the value <strong id="b148167534610"><a name="b148167534610"></a><a name="b148167534610"></a>0</strong> indicates that the port corresponding to the bit is not used for the HCCS, and the value <strong id="b781610574618"><a name="b781610574618"></a><a name="b781610574618"></a>1</strong> indicates that the port corresponding to the bit is used for the HCCS.</li><li><strong id="b13919173919465"><a name="b13919173919465"></a><a name="b13919173919465"></a>pcs_lane_bitmap</strong> indicates the lane information array used for HCCS connection. The sequence of the values in this array is the same as that of the bits set to <strong id="b2919133984613"><a name="b2919133984613"></a><a name="b2919133984613"></a>1</strong> in <strong id="b20919123904614"><a name="b20919123904614"></a><a name="b20919123904614"></a>hccs_port_pcs_bitmap</strong> (starting from the least significant bit that is set to <strong id="b20920183914619"><a name="b20920183914619"></a><a name="b20920183914619"></a>1</strong>). The meanings of the values are as follows:<a name="ul132662358472"></a><a name="ul132662358472"></a><ul id="ul132662358472"><li>Bit 0 indicates whether the switching to the HCCS is complete.<p id="p88104120528"><a name="p88104120528"></a><a name="p88104120528"></a>The value <strong id="b114151118471"><a name="b114151118471"></a><a name="b114151118471"></a>1</strong> indicates that it is complete.</p>
<p id="p920790135211"><a name="p920790135211"></a><a name="p920790135211"></a>The value <strong id="b5842131413474"><a name="b5842131413474"></a><a name="b5842131413474"></a>0</strong> indicates that the values of other bits are invalid.</p>
</li><li>Bits 1 to 8 indicate the lane number of the current HCCS connection. The value ranges from 0 to 3, starting from the least significant bit.<p id="p152591425165312"><a name="p152591425165312"></a><a name="p152591425165312"></a>The value <strong id="b52951755104714"><a name="b52951755104714"></a><a name="b52951755104714"></a>1</strong> indicates that a lane is being used for data transmission.</p>
<p id="p257122111530"><a name="p257122111530"></a><a name="p257122111530"></a>The value <strong id="b726005944717"><a name="b726005944717"></a><a name="b726005944717"></a>0</strong> indicates that it is not being used.</p>
</li><li>Bits 9 to 10 indicate the lane mode of the current HCCS connection.<p id="p131154611532"><a name="p131154611532"></a><a name="p131154611532"></a><strong id="b126182051115118"><a name="b126182051115118"></a><a name="b126182051115118"></a>00</strong>: 0lane</p>
<p id="p5608347145319"><a name="p5608347145319"></a><a name="p5608347145319"></a><strong id="b7874205205117"><a name="b7874205205117"></a><a name="b7874205205117"></a>01</strong>: 1lane</p>
<p id="p0579103145413"><a name="p0579103145413"></a><a name="p0579103145413"></a><strong id="b5727540519"><a name="b5727540519"></a><a name="b5727540519"></a>10</strong>: 4lane</p>
<p id="p184709442533"><a name="p184709442533"></a><a name="p184709442533"></a><strong id="b5107555135114"><a name="b5107555135114"></a><a name="b5107555135114"></a>11</strong>: 2lane</p>
</li></ul>
</li></ul>
</td>
</tr>
<tr id="row10253184015115"><td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.1 "><p id="p7151111413315"><a name="p7151111413315"></a><a name="p7151111413315"></a>DCMI_HCCS_CMD_GET_STATISTIC_INFO</p>
</td>
<td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.2 "><p id="p290151111549"><a name="p290151111549"></a><a name="p290151111549"></a>struct dcmi_hccs_statistic_info {</p>
<p id="p190118116546"><a name="p190118116546"></a><a name="p190118116546"></a>unsigned int tx_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p18901161120541"><a name="p18901161120541"></a><a name="p18901161120541"></a>unsigned int rx_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p119013111542"><a name="p119013111542"></a><a name="p119013111542"></a>unsigned int crc_err_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p119015111545"><a name="p119015111545"></a><a name="p119015111545"></a>unsigned int retry_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p2090141185415"><a name="p2090141185415"></a><a name="p2090141185415"></a>unsigned int reserved_field_cnt[DCMI_HCCS_RES_FIELD_NUM];  // 64 fields are reserved for use.</p>
<p id="p8901191145412"><a name="p8901191145412"></a><a name="p8901191145412"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="13.11%" headers="mcps1.2.5.1.3 "><p id="p20151121412314"><a name="p20151121412314"></a><a name="p20151121412314"></a>sizeof(struct dcmi_hccs_statistic_info)</p>
</td>
<td class="cellrowborder" valign="top" width="24.09%" headers="mcps1.2.5.1.4 "><p id="p1311994219353"><a name="p1311994219353"></a><a name="p1311994219353"></a>Obtains the statistics of packets received and sent by the HCCS and bit errors.</p>
<a name="ul18736115213512"></a><a name="ul18736115213512"></a><ul id="ul18736115213512"><li>Currently, the chip uses only the x4 mode and obtains the statistics of eight HDLC links (per device).</li><li>The statistics of each link contain three u32 integers, which are the number of transmitted packets, number of received packets, and number of received packets with CRC errors. The unit is flit.</li><li><strong id="b1321711227505"><a name="b1321711227505"></a><a name="b1321711227505"></a>tx_cnt</strong>: total number of sent packets. The value ranges from 0 to 4,294,967,295.</li><li><strong id="b921612316502"><a name="b921612316502"></a><a name="b921612316502"></a>rx_cnt</strong>: total number of received packets. The value ranges from 0 to 4,294,967,295.</li><li><strong id="b11661334195017"><a name="b11661334195017"></a><a name="b11661334195017"></a>crc_err_cnt</strong>: number of bit errors. The value ranges from 0 to 4,294,967,295.</li><li><strong id="b1531143665018"><a name="b1531143665018"></a><a name="b1531143665018"></a>retry_cnt</strong>: number of times that data packets are retransmitted. The value ranges from 0 to 4,294,967,295.<a name="ul1352101671119"></a><a name="ul1352101671119"></a><ul id="ul1352101671119"><li><strong id="b153247310127"><a name="b153247310127"></a><a name="b153247310127"></a>0</strong>: no transmission retries</li><li>Another value: number of retries</li></ul>
</li></ul>
</td>
</tr>
<tr id="row143081255102318"><td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.1 "><p id="p18308185552312"><a name="p18308185552312"></a><a name="p18308185552312"></a>DCMI_HCCS_CMD_GET_STATISTIC_INFO_U64</p>
</td>
<td class="cellrowborder" valign="top" width="31.4%" headers="mcps1.2.5.1.2 "><p id="p88016523368"><a name="p88016523368"></a><a name="p88016523368"></a>struct dcmi_hccs_statistic_info_u64 {</p>
<p id="p08015525362"><a name="p08015525362"></a><a name="p08015525362"></a>unsigned long long tx_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p18016526363"><a name="p18016526363"></a><a name="p18016526363"></a>unsigned long long rx_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p580145218362"><a name="p580145218362"></a><a name="p580145218362"></a>unsigned long long crc_err_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p1780155243612"><a name="p1780155243612"></a><a name="p1780155243612"></a>unsigned long long retry_cnt[DCMI_HCCS_MAX_PCS_NUM];</p>
<p id="p3801052183611"><a name="p3801052183611"></a><a name="p3801052183611"></a>unsigned long long reserved[DCMI_HCCS_RES_FIELD_NUM];  // 64 fields are reserved for use.</p>
<p id="p980125217364"><a name="p980125217364"></a><a name="p980125217364"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="13.11%" headers="mcps1.2.5.1.3 "><p id="p10308155132311"><a name="p10308155132311"></a><a name="p10308155132311"></a>sizeof(struct dcmi_hccs_statistic_info_u64)</p>
</td>
<td class="cellrowborder" valign="top" width="24.09%" headers="mcps1.2.5.1.4 "><p id="p72181373246"><a name="p72181373246"></a><a name="p72181373246"></a>Obtains the statistics of packets received and sent by the HCCS and bit errors.</p>
<a name="ul15381435182416"></a><a name="ul15381435182416"></a><ul id="ul15381435182416"><li>Currently, the chip uses only the x4 mode. The statistics of eight HDLC links (per device) are obtained.</li><li>The statistics of each link contain three u64 integers, which are the number of transmitted packets, number of received packets, and number of received packets with CRC errors. The unit is flit.</li><li><strong id="b55751853105415"><a name="b55751853105415"></a><a name="b55751853105415"></a>tx_cnt</strong>: accumulated number of transmitted packets. The value range is 0 to 18,446,744,073,709,551,615.</li><li><strong id="b52645813543"><a name="b52645813543"></a><a name="b52645813543"></a>rx_cnt</strong>: accumulated number of received packets. The value range is 0 to 18,446,744,073,709,551,615.</li><li><strong id="b156371341557"><a name="b156371341557"></a><a name="b156371341557"></a>crc_err_cnt</strong>: number of CRC errors. The value range is 0 to 18,446,744,073,709,551,615.</li><li><strong id="b956561355512"><a name="b956561355512"></a><a name="b956561355512"></a>retry_cnt</strong>: number of packet transmission retries. The value range is 0 to 18,446,744,073,709,551,615.<a name="ul1121152584819"></a><a name="ul1121152584819"></a><ul id="ul1121152584819"><li><strong id="b1056219156555"><a name="b1056219156555"></a><a name="b1056219156555"></a>0</strong>: no transmission retries</li><li>Another value: number of retries</li></ul>
</li></ul>
</td>
</tr>
</tbody>
</table>

**Table  2** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section1782016178717"></a>**

```C
...
int ret = 0;
int card_id = 0;
int dev_id = 0;
struct dcmi_hccs_statues status = {0};
unsigned int buf_size = sizeof(struct dcmi_hccs_statues);
ret = dcmi_get_device_info(card_id, dev_id, DCMI_MAIN_CMD_HCCS,
DCMI_HCCS_CMD_GET_STATUS, &status, &buf_size);
if (ret != 0) {
//todo
return ret;
} else {
// todo
return ret;
}
...
```

## DCMI\_MAIN\_CMD\_EX\_COMPUTING Command<a name="EN-US_TOPIC_0000002546983799"></a>

**Prototype<a name="section251144503117"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section4511154520314"></a>**

Obtains the token value of the computing power.

**Parameter Description<a name="section195111645193117"></a>**

<a name="table145283455316"></a>
<table><thead align="left"><tr id="row963614518316"><th class="cellrowborder" valign="top" width="17.17171717171717%" id="mcps1.1.5.1.1"><p id="p96361345163111"><a name="p96361345163111"></a><a name="p96361345163111"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.151515151515152%" id="mcps1.1.5.1.2"><p id="p1963644563115"><a name="p1963644563115"></a><a name="p1963644563115"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.19191919191919%" id="mcps1.1.5.1.3"><p id="p46361745193112"><a name="p46361745193112"></a><a name="p46361745193112"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="48.484848484848484%" id="mcps1.1.5.1.4"><p id="p15636144519316"><a name="p15636144519316"></a><a name="p15636144519316"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row1663613450314"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p16367453317"><a name="p16367453317"></a><a name="p16367453317"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p1863654517314"><a name="p1863654517314"></a><a name="p1863654517314"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p17636345203114"><a name="p17636345203114"></a><a name="p17636345203114"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p14636945193116"><a name="p14636945193116"></a><a name="p14636945193116"></a>Device ID. The supported IDs can be obtained by calling <strong id="b157293595102815"><a name="b157293595102815"></a><a name="b157293595102815"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row163617450316"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p126362454318"><a name="p126362454318"></a><a name="p126362454318"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p6636174518315"><a name="p6636174518315"></a><a name="p6636174518315"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p4636194553115"><a name="p4636194553115"></a><a name="p4636194553115"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p1563674543113"><a name="p1563674543113"></a><a name="p1563674543113"></a>Chip ID, which can be obtained by calling <strong id="b187245145102439"><a name="b187245145102439"></a><a name="b187245145102439"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p36365456315"><a name="p36365456315"></a><a name="p36365456315"></a>NPU: [0, <strong id="b1792765781105328"><a name="b1792765781105328"></a><a name="b1792765781105328"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row166363454319"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p16636145193116"><a name="p16636145193116"></a><a name="p16636145193116"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p6637545163119"><a name="p6637545163119"></a><a name="p6637545163119"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p20637104516316"><a name="p20637104516316"></a><a name="p20637104516316"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p16371245203115"><a name="p16371245203115"></a><a name="p16371245203115"></a>DCMI_MAIN_CMD_EX_COMPUTING</p>
</td>
</tr>
<tr id="row663754533112"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p106371145153114"><a name="p106371145153114"></a><a name="p106371145153114"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p5637144510311"><a name="p5637144510311"></a><a name="p5637144510311"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p106374457316"><a name="p106374457316"></a><a name="p106374457316"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p11637204512315"><a name="p11637204512315"></a><a name="p11637204512315"></a>DCMI_EX_COMPUTING_SUB_CMD_TOKEN</p>
</td>
</tr>
<tr id="row20637114553111"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p1163734513119"><a name="p1163734513119"></a><a name="p1163734513119"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p17637545203112"><a name="p17637545203112"></a><a name="p17637545203112"></a>Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p1163719452314"><a name="p1163719452314"></a><a name="p1163719452314"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p106372452314"><a name="p106372452314"></a><a name="p106372452314"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row146374453314"><td class="cellrowborder" valign="top" width="17.17171717171717%" headers="mcps1.1.5.1.1 "><p id="p10637184563116"><a name="p10637184563116"></a><a name="p10637184563116"></a>Size</p>
</td>
<td class="cellrowborder" valign="top" width="15.151515151515152%" headers="mcps1.1.5.1.2 "><p id="p126373451313"><a name="p126373451313"></a><a name="p126373451313"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.19191919191919%" headers="mcps1.1.5.1.3 "><p id="p13637445163120"><a name="p13637445163120"></a><a name="p13637445163120"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="48.484848484848484%" headers="mcps1.1.5.1.4 "><p id="p3637184518311"><a name="p3637184518311"></a><a name="p3637184518311"></a>Length of the returned result.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section1551994503119"></a>**

<a name="table6533174517314"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b129028041685549"><a name="b129028041685549"></a><a name="b129028041685549"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section15209424870"></a>**

None.

**Restrictions<a name="section1535162716712"></a>**

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table353514454317"></a>
<table><thead align="left"><tr id="row263834543117"><th class="cellrowborder" valign="top" width="25.380000000000003%" id="mcps1.2.5.1.1"><p id="p166382458312"><a name="p166382458312"></a><a name="p166382458312"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="28.88%" id="mcps1.2.5.1.2"><p id="p16638645113120"><a name="p16638645113120"></a><a name="p16638645113120"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="18.69%" id="mcps1.2.5.1.3"><p id="p263819453310"><a name="p263819453310"></a><a name="p263819453310"></a>Size</p>
</th>
<th class="cellrowborder" valign="top" width="27.05%" id="mcps1.2.5.1.4"><p id="p1638145183117"><a name="p1638145183117"></a><a name="p1638145183117"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row1263884503118"><td class="cellrowborder" valign="top" width="25.380000000000003%" headers="mcps1.2.5.1.1 "><p id="p763884573120"><a name="p763884573120"></a><a name="p763884573120"></a>DCMI_EX_COMPUTING</p>
<p id="p3638184563117"><a name="p3638184563117"></a><a name="p3638184563117"></a>_SUB_CMD_TOKEN</p>
</td>
<td class="cellrowborder" valign="top" width="28.88%" headers="mcps1.2.5.1.2 "><p id="p1963817451315"><a name="p1963817451315"></a><a name="p1963817451315"></a>struct dcmi_computing_token_stru {</p>
<p id="p1863844515316"><a name="p1863844515316"></a><a name="p1863844515316"></a>float value;</p>
<p id="p126384453317"><a name="p126384453317"></a><a name="p126384453317"></a>u8 type;</p>
<p id="p1563804518310"><a name="p1563804518310"></a><a name="p1563804518310"></a>u8 reserve_c;</p>
<p id="p763874510313"><a name="p763874510313"></a><a name="p763874510313"></a>u16 reserve_s;</p>
<p id="p56381459316"><a name="p56381459316"></a><a name="p56381459316"></a>}</p>
<p id="p6638164513117"><a name="p6638164513117"></a><a name="p6638164513117"></a>a. <strong id="b11183290138811"><a name="b11183290138811"></a><a name="b11183290138811"></a>value</strong>: computing power</p>
<p id="p863819458316"><a name="p863819458316"></a><a name="p863819458316"></a>b. <strong id="b812485213452"><a name="b812485213452"></a><a name="b812485213452"></a>type</strong>: type value after the license BOM code is converted</p>
<p id="p20638345123117"><a name="p20638345123117"></a><a name="p20638345123117"></a>c. <strong id="b12900064418811"><a name="b12900064418811"></a><a name="b12900064418811"></a>reserve_c</strong>: reserved currently</p>
<p id="p1663810451310"><a name="p1663810451310"></a><a name="p1663810451310"></a>d. <strong id="b143329114612"><a name="b143329114612"></a><a name="b143329114612"></a>reserve_s</strong>: reserved currently</p>
</td>
<td class="cellrowborder" valign="top" width="18.69%" headers="mcps1.2.5.1.3 "><p id="p663854523113"><a name="p663854523113"></a><a name="p663854523113"></a>sizeof(dcmi_</p>
<p id="p46381445153118"><a name="p46381445153118"></a><a name="p46381445153118"></a>computing_token_stru)</p>
</td>
<td class="cellrowborder" valign="top" width="27.05%" headers="mcps1.2.5.1.4 "><p id="p1638144514312"><a name="p1638144514312"></a><a name="p1638144514312"></a>Obtains the token value of the computing power.</p>
<p id="p163864515312"><a name="p163864515312"></a><a name="p163864515312"></a>Value range: [0, 65535]</p>
</td>
</tr>
</tbody>
</table>

**Table  2** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section172981043978"></a>**

```C
...
int ret = 0;
int card_id =0;int device_id = 0;
struct dcmi_computing_token_stru dcmi_computing_token = {0};
unsigned int buf_size = sizeof(struct dcmi_computing_token_stru);
ret = dcmi_get_device_info(card_id, device_id, DCMI_MAIN_CMD_EX_COMPUTING,
DCMI_EX_COMPUTING_SUB_CMD_TOKEN, &dcmi_computing_token, &buf_size);
if (ret != 0) {
//todo
return ret;
} else {
// todo
return ret;
}
...
```

## DCMI\_MAIN\_CMD\_EX\_CERT Command<a name="EN-US_TOPIC_0000002546983781"></a>

**Prototype<a name="section5922172553416"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf, unsigned int \*size\)**

**Description<a name="section367034213346"></a>**

Obtains TLS certificate information.

**Parameter Description<a name="section19821164963414"></a>**

<a name="table6879815113410"></a>
<table><thead align="left"><tr id="row911191616342"><th class="cellrowborder" valign="top" width="10.100000000000001%" id="mcps1.1.5.1.1"><p id="p81111693412"><a name="p81111693412"></a><a name="p81111693412"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="8.08%" id="mcps1.1.5.1.2"><p id="p811111619348"><a name="p811111619348"></a><a name="p811111619348"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.189999999999998%" id="mcps1.1.5.1.3"><p id="p1911516173411"><a name="p1911516173411"></a><a name="p1911516173411"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="62.629999999999995%" id="mcps1.1.5.1.4"><p id="p15113163349"><a name="p15113163349"></a><a name="p15113163349"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row211191623413"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p21161643412"><a name="p21161643412"></a><a name="p21161643412"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="8.08%" headers="mcps1.1.5.1.2 "><p id="p1111191615343"><a name="p1111191615343"></a><a name="p1111191615343"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.189999999999998%" headers="mcps1.1.5.1.3 "><p id="p14111816193419"><a name="p14111816193419"></a><a name="p14111816193419"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="62.629999999999995%" headers="mcps1.1.5.1.4 "><p id="p311131619344"><a name="p311131619344"></a><a name="p311131619344"></a>Device ID. The supported IDs can be obtained by calling <strong id="b678482226102818"><a name="b678482226102818"></a><a name="b678482226102818"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row9111316183415"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p13111716133419"><a name="p13111716133419"></a><a name="p13111716133419"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="8.08%" headers="mcps1.1.5.1.2 "><p id="p81117168341"><a name="p81117168341"></a><a name="p81117168341"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.189999999999998%" headers="mcps1.1.5.1.3 "><p id="p151171615344"><a name="p151171615344"></a><a name="p151171615344"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="62.629999999999995%" headers="mcps1.1.5.1.4 "><p id="p13111166347"><a name="p13111166347"></a><a name="p13111166347"></a>Chip ID, which can be obtained by calling <strong id="b1044773831102442"><a name="b1044773831102442"></a><a name="b1044773831102442"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p121212165348"><a name="p121212165348"></a><a name="p121212165348"></a>NPU: [0, <strong id="b902986363105331"><a name="b902986363105331"></a><a name="b902986363105331"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row15128165343"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p81241613416"><a name="p81241613416"></a><a name="p81241613416"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="8.08%" headers="mcps1.1.5.1.2 "><p id="p19121016153415"><a name="p19121016153415"></a><a name="p19121016153415"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.189999999999998%" headers="mcps1.1.5.1.3 "><p id="p212616113417"><a name="p212616113417"></a><a name="p212616113417"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="62.629999999999995%" headers="mcps1.1.5.1.4 "><p id="p1012101643419"><a name="p1012101643419"></a><a name="p1012101643419"></a>DCMI_MAIN_CMD_EX_CERT</p>
</td>
</tr>
<tr id="row1121016153416"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p912111603417"><a name="p912111603417"></a><a name="p912111603417"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="8.08%" headers="mcps1.1.5.1.2 "><p id="p1812151618349"><a name="p1812151618349"></a><a name="p1812151618349"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.189999999999998%" headers="mcps1.1.5.1.3 "><p id="p10121016133413"><a name="p10121016133413"></a><a name="p10121016133413"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="62.629999999999995%" headers="mcps1.1.5.1.4 "><p id="p1909144281914"><a name="p1909144281914"></a><a name="p1909144281914"></a>typedef enum {</p>
<p id="p16909134251918"><a name="p16909134251918"></a><a name="p16909134251918"></a>DCMI_CERT_SUB_CMD_INIT_TLS_PUB_KEY = 0, // Initialization of the TLS certificate (used to obtain the CSR)</p>
<p id="p189091942131912"><a name="p189091942131912"></a><a name="p189091942131912"></a>DCMI_CERT_SUB_CMD_INIT_RESERVE, // Reserved field for initialization</p>
<p id="p99091942161915"><a name="p99091942161915"></a><a name="p99091942161915"></a>DCMI_CERT_SUB_CMD_TLS_CERT_INFO, // Preconfiguration, update, or obtaining of the TLS certificate information</p>
<p id="p89094428194"><a name="p89094428194"></a><a name="p89094428194"></a>DCMI_CERT_SUB_CMD_MAX,</p>
<p id="p790919427194"><a name="p790919427194"></a><a name="p790919427194"></a>} DCMI_EX_CERT_SUB_CMD;</p>
</td>
</tr>
<tr id="row612121619349"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p5129164343"><a name="p5129164343"></a><a name="p5129164343"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="8.08%" headers="mcps1.1.5.1.2 "><p id="p312616163411"><a name="p312616163411"></a><a name="p312616163411"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.189999999999998%" headers="mcps1.1.5.1.3 "><p id="p151219163343"><a name="p151219163343"></a><a name="p151219163343"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="62.629999999999995%" headers="mcps1.1.5.1.4 "><p id="p1512516103412"><a name="p1512516103412"></a><a name="p1512516103412"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="row1912101611341"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p112151617346"><a name="p112151617346"></a><a name="p112151617346"></a>size</p>
</td>
<td class="cellrowborder" valign="top" width="8.08%" headers="mcps1.1.5.1.2 "><p id="p11121616193413"><a name="p11121616193413"></a><a name="p11121616193413"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="19.189999999999998%" headers="mcps1.1.5.1.3 "><p id="p512151616341"><a name="p512151616341"></a><a name="p512151616341"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="62.629999999999995%" headers="mcps1.1.5.1.4 "><p id="p191317165344"><a name="p191317165344"></a><a name="p191317165344"></a>Length of the <strong id="b47590999735210"><a name="b47590999735210"></a><a name="b47590999735210"></a>buf</strong> array.</p>
</td>
</tr>
</tbody>
</table>

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table1892515103416"></a>
<table><thead align="left"><tr id="row1713716153413"><th class="cellrowborder" valign="top" width="27.55%" id="mcps1.2.5.1.1"><p id="p81313161345"><a name="p81313161345"></a><a name="p81313161345"></a><strong id="b913916193413"><a name="b913916193413"></a><a name="b913916193413"></a>sub_cmd</strong></p>
</th>
<th class="cellrowborder" valign="top" width="39.050000000000004%" id="mcps1.2.5.1.2"><p id="p813201610342"><a name="p813201610342"></a><a name="p813201610342"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="17.52%" id="mcps1.2.5.1.3"><p id="p151351673417"><a name="p151351673417"></a><a name="p151351673417"></a>Size</p>
</th>
<th class="cellrowborder" valign="top" width="15.879999999999999%" id="mcps1.2.5.1.4"><p id="p7131516203415"><a name="p7131516203415"></a><a name="p7131516203415"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row1013191663416"><td class="cellrowborder" valign="top" width="27.55%" headers="mcps1.2.5.1.1 "><p id="p913121617349"><a name="p913121617349"></a><a name="p913121617349"></a>DCMI_CERT_SUB_CMD_INIT_TLS_PUB_KEY</p>
</td>
<td class="cellrowborder" valign="top" width="39.050000000000004%" headers="mcps1.2.5.1.2 "><p id="p121331617349"><a name="p121331617349"></a><a name="p121331617349"></a>#define CERT_NAME_MAX_LEN    16</p>
<p id="p1413816103411"><a name="p1413816103411"></a><a name="p1413816103411"></a>#define CERT_COMMON_NAME_LEN 64</p>
<p id="p131311165341"><a name="p131311165341"></a><a name="p131311165341"></a>struct dcmi_csr_info {</p>
<p id="p1713161613417"><a name="p1713161613417"></a><a name="p1713161613417"></a>char country_name[CERT_NAME_MAX_LEN];</p>
<p id="p614216183418"><a name="p614216183418"></a><a name="p614216183418"></a>char province_name[CERT_NAME_MAX_LEN];</p>
<p id="p14147168341"><a name="p14147168341"></a><a name="p14147168341"></a>char city_name[CERT_NAME_MAX_LEN];</p>
<p id="p11141516183413"><a name="p11141516183413"></a><a name="p11141516183413"></a>char organization_name[CERT_NAME_MAX_LEN];</p>
<p id="p21481613344"><a name="p21481613344"></a><a name="p21481613344"></a>char department_name[CERT_NAME_MAX_LEN];</p>
<p id="p18141116113418"><a name="p18141116113418"></a><a name="p18141116113418"></a>char reserve_name[CERT_COMMON_NAME_LEN];</p>
<p id="p13142161345"><a name="p13142161345"></a><a name="p13142161345"></a>int reserve;</p>
<p id="p81414169347"><a name="p81414169347"></a><a name="p81414169347"></a>int csr_len;</p>
<p id="p161411616349"><a name="p161411616349"></a><a name="p161411616349"></a>char csr_data[NPU_CERT_MAX_SIZE];</p>
<p id="p151415161342"><a name="p151415161342"></a><a name="p151415161342"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="17.52%" headers="mcps1.2.5.1.3 "><p id="p2141116103414"><a name="p2141116103414"></a><a name="p2141116103414"></a>Returns the length of the public key or certificate information.</p>
</td>
<td class="cellrowborder" valign="top" width="15.879999999999999%" headers="mcps1.2.5.1.4 "><a name="ul57333280138"></a><a name="ul57333280138"></a><ul id="ul57333280138"><li><strong id="b158215289541"><a name="b158215289541"></a><a name="b158215289541"></a>country_name</strong>: country name</li><li><strong id="b1818653415412"><a name="b1818653415412"></a><a name="b1818653415412"></a>province_name</strong>: state or province name</li><li><strong id="b1731511393547"><a name="b1731511393547"></a><a name="b1731511393547"></a>city_name</strong>: city name</li><li><strong id="b5350144125413"><a name="b5350144125413"></a><a name="b5350144125413"></a>organization_name</strong>: organization name</li><li><strong id="b19587815205513"><a name="b19587815205513"></a><a name="b19587815205513"></a>department_name</strong>: name of a department in an organization</li><li><strong id="b59085555618"><a name="b59085555618"></a><a name="b59085555618"></a>reserve_name</strong>: reserved name</li><li><strong id="b20237111125618"><a name="b20237111125618"></a><a name="b20237111125618"></a>reserve</strong>: reserved</li><li><strong id="b37651715135614"><a name="b37651715135614"></a><a name="b37651715135614"></a>csr_len</strong>: length of the CSR content</li><li><strong id="b98635206561"><a name="b98635206561"></a><a name="b98635206561"></a>csr_data</strong>: CSR content</li></ul>
</td>
</tr>
<tr id="row19141016183410"><td class="cellrowborder" valign="top" width="27.55%" headers="mcps1.2.5.1.1 "><p id="p514131615349"><a name="p514131615349"></a><a name="p514131615349"></a>DCMI_CERT_SUB_CMD_TLS_CERT_INFO</p>
</td>
<td class="cellrowborder" valign="top" width="39.050000000000004%" headers="mcps1.2.5.1.2 "><p id="p141418165347"><a name="p141418165347"></a><a name="p141418165347"></a>#define CERT_COMMON_NAME_LEN 64</p>
<p id="p91420169348"><a name="p91420169348"></a><a name="p91420169348"></a>#define CERT_NAME_MAX_LEN    16</p>
<p id="p714216103412"><a name="p714216103412"></a><a name="p714216103412"></a>#define TIME_LEN 32</p>
<p id="p21415163341"><a name="p21415163341"></a><a name="p21415163341"></a>typedef struct dcmi_cert_info  {</p>
<p id="p171431623415"><a name="p171431623415"></a><a name="p171431623415"></a>unsigned int alarm_stat;</p>
<p id="p1146167340"><a name="p1146167340"></a><a name="p1146167340"></a>unsigned int reserve;</p>
<p id="p51421663412"><a name="p51421663412"></a><a name="p51421663412"></a>char start_time[TIME_LEN];</p>
<p id="p114101613414"><a name="p114101613414"></a><a name="p114101613414"></a>char end_time[TIME_LEN];</p>
<p id="p10149162343"><a name="p10149162343"></a><a name="p10149162343"></a>char country_name[CERT_NAME_MAX_LEN];</p>
<p id="p8142169344"><a name="p8142169344"></a><a name="p8142169344"></a>char province_name[CERT_NAME_MAX_LEN];</p>
<p id="p3144167347"><a name="p3144167347"></a><a name="p3144167347"></a>char city_name[CERT_NAME_MAX_LEN];</p>
<p id="p201451653418"><a name="p201451653418"></a><a name="p201451653418"></a>char organization_name[CERT_NAME_MAX_LEN];</p>
<p id="p1014191615345"><a name="p1014191615345"></a><a name="p1014191615345"></a>char department_name[CERT_NAME_MAX_LEN];</p>
<p id="p91471643415"><a name="p91471643415"></a><a name="p91471643415"></a>char reserve_name[CERT_COMMON_NAME_LEN];</p>
<p id="p4141161342"><a name="p4141161342"></a><a name="p4141161342"></a>char common_name[CERT_COMMON_NAME_LEN];</p>
<p id="p1814916143411"><a name="p1814916143411"></a><a name="p1814916143411"></a>} CERT_INFO;</p>
</td>
<td class="cellrowborder" valign="top" width="17.52%" headers="mcps1.2.5.1.3 "><p id="p614141693419"><a name="p614141693419"></a><a name="p614141693419"></a>Returns the length of the public key or certificate information.</p>
</td>
<td class="cellrowborder" valign="top" width="15.879999999999999%" headers="mcps1.2.5.1.4 "><a name="ul11930122181818"></a><a name="ul11930122181818"></a><ul id="ul11930122181818"><li><strong id="b168310271819"><a name="b168310271819"></a><a name="b168310271819"></a>alarm_stat</strong>: certificate alarm status. <strong id="b983112270113"><a name="b983112270113"></a><a name="b983112270113"></a>0</strong>: normal; <strong id="b1283162711118"><a name="b1283162711118"></a><a name="b1283162711118"></a>1</strong>: about to expire; <strong id="b28311127716"><a name="b28311127716"></a><a name="b28311127716"></a>2</strong>: expired/invalid ("about to expire" is displayed only when the certificate is read.)</li><li><strong id="b1427010133567"><a name="b1427010133567"></a><a name="b1427010133567"></a>reserve</strong>: reserved</li><li><strong id="b133217775712"><a name="b133217775712"></a><a name="b133217775712"></a>start_time</strong>: time when the certificate takes effect</li><li><strong id="b1498981215715"><a name="b1498981215715"></a><a name="b1498981215715"></a>end_time</strong>: time when the certificate expires</li><li><strong id="b18872113018545"><a name="b18872113018545"></a><a name="b18872113018545"></a>country_name</strong>: country name</li><li><strong id="b674833618546"><a name="b674833618546"></a><a name="b674833618546"></a>province_name</strong>: state or province name</li><li><strong id="b998184116549"><a name="b998184116549"></a><a name="b998184116549"></a>city_name</strong>: city name</li><li><strong id="b15341946185418"><a name="b15341946185418"></a><a name="b15341946185418"></a>organization_name</strong>: organization name</li><li><strong id="b17081618145513"><a name="b17081618145513"></a><a name="b17081618145513"></a>department_name</strong>: name of a department in an organization</li><li><strong id="b15644231210"><a name="b15644231210"></a><a name="b15644231210"></a>reserve_name</strong>: reserved name</li><li><strong id="b45155135720"><a name="b45155135720"></a><a name="b45155135720"></a>common_name</strong>: common name of the certificate. The DIE-ID of the device is used here.</li></ul>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section189807595317"></a>**

<a name="table154712012328"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b90772282985552"><a name="b90772282985552"></a><a name="b90772282985552"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section13668537161318"></a>**

None.

**Restrictions<a name="section114215306366"></a>**

**Table  2** Support in different deployment scenarios

<a name="table6665182042413"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row192401338610"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p6884135713319"><a name="en-us_topic_0000002515503852_p6884135713319"></a><a name="en-us_topic_0000002515503852_p6884135713319"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p188841657113119"><a name="en-us_topic_0000002515503852_p188841657113119"></a><a name="en-us_topic_0000002515503852_p188841657113119"></a><strong id="en-us_topic_0000002515503852_b1680418281142"><a name="en-us_topic_0000002515503852_b1680418281142"></a><a name="en-us_topic_0000002515503852_b1680418281142"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p198849575317"><a name="en-us_topic_0000002515503852_p198849575317"></a><a name="en-us_topic_0000002515503852_p198849575317"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b9295305413"><a name="en-us_topic_0000002515503852_b9295305413"></a><a name="en-us_topic_0000002515503852_b9295305413"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p288445716317"><a name="en-us_topic_0000002515503852_p288445716317"></a><a name="en-us_topic_0000002515503852_p288445716317"></a><strong id="en-us_topic_0000002515503852_b17190133113410"><a name="en-us_topic_0000002515503852_b17190133113410"></a><a name="en-us_topic_0000002515503852_b17190133113410"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_en-us_topic_0000001167913765_row82952324359"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1759118207718"><a name="en-us_topic_0000002515503852_p1759118207718"></a><a name="en-us_topic_0000002515503852_p1759118207718"></a><span id="en-us_topic_0000002515503852_ph05911020372"><a name="en-us_topic_0000002515503852_ph05911020372"></a><a name="en-us_topic_0000002515503852_ph05911020372"></a><span id="en-us_topic_0000002515503852_text12591192010713"><a name="en-us_topic_0000002515503852_text12591192010713"></a><a name="en-us_topic_0000002515503852_text12591192010713"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p1018612250597"><a name="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p1018612250597"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p1018612250597"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p16903175117312"><a name="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p16903175117312"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p16903175117312"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p89579531835"><a name="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p89579531835"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001251427187_en-us_topic_0000001188446388_p89579531835"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row72645420615"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p95911420177"><a name="en-us_topic_0000002515503852_p95911420177"></a><a name="en-us_topic_0000002515503852_p95911420177"></a><span id="en-us_topic_0000002515503852_text6591220876"><a name="en-us_topic_0000002515503852_text6591220876"></a><a name="en-us_topic_0000002515503852_text6591220876"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p291810521262"><a name="en-us_topic_0000002515503852_p291810521262"></a><a name="en-us_topic_0000002515503852_p291810521262"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p199181852061"><a name="en-us_topic_0000002515503852_p199181852061"></a><a name="en-us_topic_0000002515503852_p199181852061"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p89189521765"><a name="en-us_topic_0000002515503852_p89189521765"></a><a name="en-us_topic_0000002515503852_p89189521765"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row278413126616"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p559162010720"><a name="en-us_topic_0000002515503852_p559162010720"></a><a name="en-us_topic_0000002515503852_p559162010720"></a><span id="en-us_topic_0000002515503852_text165912204716"><a name="en-us_topic_0000002515503852_text165912204716"></a><a name="en-us_topic_0000002515503852_text165912204716"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1852110531661"><a name="en-us_topic_0000002515503852_p1852110531661"></a><a name="en-us_topic_0000002515503852_p1852110531661"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1252125311612"><a name="en-us_topic_0000002515503852_p1252125311612"></a><a name="en-us_topic_0000002515503852_p1252125311612"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p652117531767"><a name="en-us_topic_0000002515503852_p652117531767"></a><a name="en-us_topic_0000002515503852_p652117531767"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row878911101267"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p259119201579"><a name="en-us_topic_0000002515503852_p259119201579"></a><a name="en-us_topic_0000002515503852_p259119201579"></a><span id="en-us_topic_0000002515503852_text55915207713"><a name="en-us_topic_0000002515503852_text55915207713"></a><a name="en-us_topic_0000002515503852_text55915207713"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p18619541161"><a name="en-us_topic_0000002515503852_p18619541161"></a><a name="en-us_topic_0000002515503852_p18619541161"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p2864541766"><a name="en-us_topic_0000002515503852_p2864541766"></a><a name="en-us_topic_0000002515503852_p2864541766"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1186854962"><a name="en-us_topic_0000002515503852_p1186854962"></a><a name="en-us_topic_0000002515503852_p1186854962"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row283215811614"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p12591202014711"><a name="en-us_topic_0000002515503852_p12591202014711"></a><a name="en-us_topic_0000002515503852_p12591202014711"></a><span id="en-us_topic_0000002515503852_text65911120871"><a name="en-us_topic_0000002515503852_text65911120871"></a><a name="en-us_topic_0000002515503852_text65911120871"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p15638154267"><a name="en-us_topic_0000002515503852_p15638154267"></a><a name="en-us_topic_0000002515503852_p15638154267"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p18638354369"><a name="en-us_topic_0000002515503852_p18638354369"></a><a name="en-us_topic_0000002515503852_p18638354369"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p16638154060"><a name="en-us_topic_0000002515503852_p16638154060"></a><a name="en-us_topic_0000002515503852_p16638154060"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row1057696667"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p959110201477"><a name="en-us_topic_0000002515503852_p959110201477"></a><a name="en-us_topic_0000002515503852_p959110201477"></a><span id="en-us_topic_0000002515503852_text7981112472810"><a name="en-us_topic_0000002515503852_text7981112472810"></a><a name="en-us_topic_0000002515503852_text7981112472810"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p18230255869"><a name="en-us_topic_0000002515503852_p18230255869"></a><a name="en-us_topic_0000002515503852_p18230255869"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p123055512620"><a name="en-us_topic_0000002515503852_p123055512620"></a><a name="en-us_topic_0000002515503852_p123055512620"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p623011552618"><a name="en-us_topic_0000002515503852_p623011552618"></a><a name="en-us_topic_0000002515503852_p623011552618"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row8655214617"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p14591202016712"><a name="en-us_topic_0000002515503852_p14591202016712"></a><a name="en-us_topic_0000002515503852_p14591202016712"></a><span id="en-us_topic_0000002515503852_text1659110201379"><a name="en-us_topic_0000002515503852_text1659110201379"></a><a name="en-us_topic_0000002515503852_text1659110201379"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p38158552616"><a name="en-us_topic_0000002515503852_p38158552616"></a><a name="en-us_topic_0000002515503852_p38158552616"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p18158552613"><a name="en-us_topic_0000002515503852_p18158552613"></a><a name="en-us_topic_0000002515503852_p18158552613"></a>N</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p19815255367"><a name="en-us_topic_0000002515503852_p19815255367"></a><a name="en-us_topic_0000002515503852_p19815255367"></a>N</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row67064433311"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15712048183315"><a name="en-us_topic_0000002515503852_p15712048183315"></a><a name="en-us_topic_0000002515503852_p15712048183315"></a><span id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section5271151515373"></a>**

```C
...
int ret;
int card_id = 0;
int device_id = 0;
struct dcmi_cert_info cert_info = { 0 };
DCMI_MAIN_CMD main_cmd = DCMI_MAIN_CMD_EX_CERT;
unsigned int sub_cmd = DCMI_CERT_SUB_CMD_TLS_CERT_INFO;
unsigned int size = sizeof(struct dcmi_cert_info);
ret = dcmi_get_device_info(card_id, device_id, main_cmd, sub_cmd, (void *)&cert_info, &size);
if (ret) {
   // todo
}
// todo
...
```

## DCMI\_MAIN\_CMD\_PCIE Command<a name="EN-US_TOPIC_0000002515343940"></a>

**Prototype<a name="section224642513574"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf,unsigned int \*size\)**

**Description<a name="section162477254573"></a>**

Obtains PCIe information.

**Parameter Description<a name="section16248625165719"></a>**

<a name="table23011825165718"></a>
<table><thead align="left"><tr id="row1644220264578"><th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.1"><p id="p19442192665715"><a name="p19442192665715"></a><a name="p19442192665715"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="15.15%" id="mcps1.1.5.1.2"><p id="p84426269577"><a name="p84426269577"></a><a name="p84426269577"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="17.169999999999998%" id="mcps1.1.5.1.3"><p id="p164426264575"><a name="p164426264575"></a><a name="p164426264575"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="50.51%" id="mcps1.1.5.1.4"><p id="p14424263576"><a name="p14424263576"></a><a name="p14424263576"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row144428269571"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p11442192614578"><a name="p11442192614578"></a><a name="p11442192614578"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p544232625716"><a name="p544232625716"></a><a name="p544232625716"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p17442526165714"><a name="p17442526165714"></a><a name="p17442526165714"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p16442182635713"><a name="p16442182635713"></a><a name="p16442182635713"></a>Device ID. The supported IDs can be obtained by calling <strong id="b1229087367102819"><a name="b1229087367102819"></a><a name="b1229087367102819"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="row8442182617573"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p134421126155713"><a name="p134421126155713"></a><a name="p134421126155713"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p16442142613570"><a name="p16442142613570"></a><a name="p16442142613570"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p6442142615710"><a name="p6442142615710"></a><a name="p6442142615710"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p11442226105712"><a name="p11442226105712"></a><a name="p11442226105712"></a>Chip ID, which can be obtained by calling <strong id="b1856777269102443"><a name="b1856777269102443"></a><a name="b1856777269102443"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p17442112616571"><a name="p17442112616571"></a><a name="p17442112616571"></a>NPU: [0, <strong id="b1854799388105332"><a name="b1854799388105332"></a><a name="b1854799388105332"></a>device_id_max</strong> – 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="row1544262610576"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p2044352611575"><a name="p2044352611575"></a><a name="p2044352611575"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p7443152613571"><a name="p7443152613571"></a><a name="p7443152613571"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p104439262577"><a name="p104439262577"></a><a name="p104439262577"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p844312611573"><a name="p844312611573"></a><a name="p844312611573"></a>DCMI_MAIN_CMD_PCIE</p>
</td>
</tr>
<tr id="row6443426185717"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p14431726185711"><a name="p14431726185711"></a><a name="p14431726185711"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p84431426165711"><a name="p84431426165711"></a><a name="p84431426165711"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p1844312619576"><a name="p1844312619576"></a><a name="p1844312619576"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p11443162655718"><a name="p11443162655718"></a><a name="p11443162655718"></a>DCMI_PCIE_SUB_CMD_PCIE_ERROR_INFO</p>
</td>
</tr>
<tr id="row144342617576"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p0443172665711"><a name="p0443172665711"></a><a name="p0443172665711"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p644362612579"><a name="p644362612579"></a><a name="p644362612579"></a>Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p154431526205714"><a name="p154431526205714"></a><a name="p154431526205714"></a>void *</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p194431626195715"><a name="p194431626195715"></a><a name="p194431626195715"></a>For details, see <a href="#section142847254570">Restrictions</a>.</p>
</td>
</tr>
<tr id="row1144315264573"><td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.1 "><p id="p2443526125719"><a name="p2443526125719"></a><a name="p2443526125719"></a>Size</p>
</td>
<td class="cellrowborder" valign="top" width="15.15%" headers="mcps1.1.5.1.2 "><p id="p12443326145716"><a name="p12443326145716"></a><a name="p12443326145716"></a>Input/Output</p>
</td>
<td class="cellrowborder" valign="top" width="17.169999999999998%" headers="mcps1.1.5.1.3 "><p id="p4443526135714"><a name="p4443526135714"></a><a name="p4443526135714"></a>unsigned int *</p>
</td>
<td class="cellrowborder" valign="top" width="50.51%" headers="mcps1.1.5.1.4 "><p id="p3443102675716"><a name="p3443102675716"></a><a name="p3443102675716"></a>Length of the <strong id="b117381391993"><a name="b117381391993"></a><a name="b117381391993"></a>buf</strong> array.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section1276192518575"></a>**

<a name="table154712012328"></a>
<table><thead align="left"><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000001917887173_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="b200687611685553"><a name="b200687611685553"></a><a name="b200687611685553"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section10284025165720"></a>**

None.

**Restrictions<a name="section142847254570"></a>**

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table131652519571"></a>
<table><thead align="left"><tr id="row144441226125711"><th class="cellrowborder" valign="top" width="30.303030303030305%" id="mcps1.2.4.1.1"><p id="p64441826115711"><a name="p64441826115711"></a><a name="p64441826115711"></a><strong id="b3444192610574"><a name="b3444192610574"></a><a name="b3444192610574"></a>sub_cmd</strong></p>
</th>
<th class="cellrowborder" valign="top" width="47.474747474747474%" id="mcps1.2.4.1.2"><p id="p64444261570"><a name="p64444261570"></a><a name="p64444261570"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="22.222222222222225%" id="mcps1.2.4.1.3"><p id="p1644432645711"><a name="p1644432645711"></a><a name="p1644432645711"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row12444826175711"><td class="cellrowborder" valign="top" width="30.303030303030305%" headers="mcps1.2.4.1.1 "><p id="p17444112611573"><a name="p17444112611573"></a><a name="p17444112611573"></a>DCMI_PCIE_SUB_CMD_PCIE_ERROR_INFO</p>
</td>
<td class="cellrowborder" valign="top" width="47.474747474747474%" headers="mcps1.2.4.1.2 "><p id="p124454268570"><a name="p124454268570"></a><a name="p124454268570"></a>struct dcmi_pcie_link_error_info {</p>
<p id="p94452262574"><a name="p94452262574"></a><a name="p94452262574"></a>unsigned int tx_err_cnt;</p>
<p id="p2044522615576"><a name="p2044522615576"></a><a name="p2044522615576"></a>unsigned int rx_err_cnt;</p>
<p id="p34456263573"><a name="p34456263573"></a><a name="p34456263573"></a>unsigned int lcrc_err_cnt;</p>
<p id="p34450264576"><a name="p34450264576"></a><a name="p34450264576"></a>unsigned int ecrc_err_cnt;</p>
<p id="p24455263576"><a name="p24455263576"></a><a name="p24455263576"></a>unsigned int retry_cnt;</p>
<p id="p15445152619575"><a name="p15445152619575"></a><a name="p15445152619575"></a>unsigned int rsv[32];</p>
<p id="p1445182613571"><a name="p1445182613571"></a><a name="p1445182613571"></a>};</p>
</td>
<td class="cellrowborder" valign="top" width="22.222222222222225%" headers="mcps1.2.4.1.3 "><p id="p7445142665715"><a name="p7445142665715"></a><a name="p7445142665715"></a>Obtains the values related to pcie_link_error.</p>
<a name="ul19681694120"></a><a name="ul19681694120"></a><ul id="ul19681694120"><li><strong id="b73022017018"><a name="b73022017018"></a><a name="b73022017018"></a>tx_err_cnt</strong>: number of PCIe transmission errors</li><li><strong id="b2024621715110"><a name="b2024621715110"></a><a name="b2024621715110"></a>rx_err_cnt</strong>: number of PCIe receiving errors</li><li><strong id="b1048320352024"><a name="b1048320352024"></a><a name="b1048320352024"></a>lcrc_err_cnt</strong>: number of PCIe DLLP LCRC errors</li><li><strong id="b15371105614212"><a name="b15371105614212"></a><a name="b15371105614212"></a>ecrc_err_cnt</strong>: number of PCIe TLP ECRC errors</li><li><strong id="b763210114315"><a name="b763210114315"></a><a name="b763210114315"></a>retry_cnt</strong>: number of PCIe link retransmissions</li><li><strong id="b0378151018313"><a name="b0378151018313"></a><a name="b0378151018313"></a>rsv</strong>: reserved field</li></ul>
</td>
</tr>
</tbody>
</table>

If the host and device are connected through HCCS, run the HCCS command to query link information.

This API can be used in PM + privileged containers.

**Table  2** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section8297125115718"></a>**

```C
...
int ret = 0;
int card_id = 0;
int dev_id = 0;
struct dcmi_pcie_link_error_info pcie_link_error_info = {0};
unsigned int info_leng = sizeof(struct dcmi_pcie_link_error_info);
ret = dcmi_get_device_info(card_id, dev_id, DCMI_MAIN_CMD_PCIE, DCMI_PCIE_SUB_CMD_PCIE_ERROR_INFO, &pcie_link_error_info, &info_leng);
if (ret != 0){
    //todo: Record logs.
    return ret;
}
...
```

## DCMI\_MAIN\_CMD\_DEVICE\_SHARE Command<a name="EN-US_TOPIC_0000002515343840"></a>

**Prototype<a name="section15145210204513"></a>**

**int dcmi\_get\_device\_info\(int card\_id, int device\_id, enum dcmi\_main\_cmd main\_cmd, unsigned int sub\_cmd, void \*buf,unsigned int \*size\)**

**Description<a name="section19730233114514"></a>**

Queries the container sharing enable flag for a chip.

**Parameter Description<a name="section5671124612912"></a>**

<a name="en-us_topic_0257495783_table45028263"></a>
<table><thead align="left"><tr id="en-us_topic_0257495783_row60695621"><th class="cellrowborder" valign="top" width="10.100000000000001%" id="mcps1.1.5.1.1"><p id="en-us_topic_0257495783_p17398306"><a name="en-us_topic_0257495783_p17398306"></a><a name="en-us_topic_0257495783_p17398306"></a>Parameter</p>
</th>
<th class="cellrowborder" valign="top" width="8.41%" id="mcps1.1.5.1.2"><p id="en-us_topic_0257495783_p67085535"><a name="en-us_topic_0257495783_p67085535"></a><a name="en-us_topic_0257495783_p67085535"></a>Input/Output</p>
</th>
<th class="cellrowborder" valign="top" width="19.470000000000002%" id="mcps1.1.5.1.3"><p id="p153645385214"><a name="p153645385214"></a><a name="p153645385214"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="62.019999999999996%" id="mcps1.1.5.1.4"><p id="en-us_topic_0257495783_p48266778"><a name="en-us_topic_0257495783_p48266778"></a><a name="en-us_topic_0257495783_p48266778"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row884163595218"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p121951513267"><a name="p121951513267"></a><a name="p121951513267"></a>card_id</p>
</td>
<td class="cellrowborder" valign="top" width="8.41%" headers="mcps1.1.5.1.2 "><p id="p11956515261"><a name="p11956515261"></a><a name="p11956515261"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.470000000000002%" headers="mcps1.1.5.1.3 "><p id="p1919510562619"><a name="p1919510562619"></a><a name="p1919510562619"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="62.019999999999996%" headers="mcps1.1.5.1.4 "><p id="p1619510511265"><a name="p1619510511265"></a><a name="p1619510511265"></a>Device ID. The supported IDs can be obtained by calling <strong id="b43557219102820"><a name="b43557219102820"></a><a name="b43557219102820"></a>dcmi_get_card_list</strong>.</p>
</td>
</tr>
<tr id="en-us_topic_0257495783_row31747823"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p191957519266"><a name="p191957519266"></a><a name="p191957519266"></a>device_id</p>
</td>
<td class="cellrowborder" valign="top" width="8.41%" headers="mcps1.1.5.1.2 "><p id="p1319517522613"><a name="p1319517522613"></a><a name="p1319517522613"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.470000000000002%" headers="mcps1.1.5.1.3 "><p id="p419514512261"><a name="p419514512261"></a><a name="p419514512261"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="62.019999999999996%" headers="mcps1.1.5.1.4 "><p id="p161951552616"><a name="p161951552616"></a><a name="p161951552616"></a>Chip ID, which can be obtained by calling <strong id="b419101903102444"><a name="b419101903102444"></a><a name="b419101903102444"></a>dcmi_get_device_id_in_card</strong>. Value range:</p>
<p id="p8195155132618"><a name="p8195155132618"></a><a name="p8195155132618"></a>NPU chip: [0, <strong id="b2062083162111937"><a name="b2062083162111937"></a><a name="b2062083162111937"></a>device_id_max</strong> - 1]</p>
<div class="note" id="note102181227171418"><a name="note102181227171418"></a><a name="note102181227171418"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="en-us_topic_0000002515503822_p10218227151413"><a name="en-us_topic_0000002515503822_p10218227151413"></a><a name="en-us_topic_0000002515503822_p10218227151413"></a>The value of <strong id="en-us_topic_0000002515503822_b07404719231"><a name="en-us_topic_0000002515503822_b07404719231"></a><a name="en-us_topic_0000002515503822_b07404719231"></a>device_id_max</strong> is <strong id="en-us_topic_0000002515503822_b18691134810239"><a name="en-us_topic_0000002515503822_b18691134810239"></a><a name="en-us_topic_0000002515503822_b18691134810239"></a>1</strong>. When <strong id="en-us_topic_0000002515503822_b20866105012236"><a name="en-us_topic_0000002515503822_b20866105012236"></a><a name="en-us_topic_0000002515503822_b20866105012236"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b106252524232"><a name="en-us_topic_0000002515503822_b106252524232"></a><a name="en-us_topic_0000002515503822_b106252524232"></a>0</strong>, the NPU chip is used. When <strong id="en-us_topic_0000002515503822_b97321556102310"><a name="en-us_topic_0000002515503822_b97321556102310"></a><a name="en-us_topic_0000002515503822_b97321556102310"></a>device_id</strong> is <strong id="en-us_topic_0000002515503822_b426285819235"><a name="en-us_topic_0000002515503822_b426285819235"></a><a name="en-us_topic_0000002515503822_b426285819235"></a>1</strong>, the MCU chip is used.</p>
</div></div>
</td>
</tr>
<tr id="en-us_topic_0257495783_row7743426585"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0257495783_p15756282814"><a name="en-us_topic_0257495783_p15756282814"></a><a name="en-us_topic_0257495783_p15756282814"></a>main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="8.41%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0257495783_p1574417263816"><a name="en-us_topic_0257495783_p1574417263816"></a><a name="en-us_topic_0257495783_p1574417263816"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.470000000000002%" headers="mcps1.1.5.1.3 "><p id="p1519518513263"><a name="p1519518513263"></a><a name="p1519518513263"></a>enum dcmi_main_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="62.019999999999996%" headers="mcps1.1.5.1.4 "><p id="p466445534710"><a name="p466445534710"></a><a name="p466445534710"></a>DCMI_MAIN_CMD_DEVICE_SHARE</p>
<div class="note" id="note1218455521019"><a name="note1218455521019"></a><a name="note1218455521019"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="p14728105710819"><a name="p14728105710819"></a><a name="p14728105710819"></a>To query the container sharing function, see <a href="dcmi_get_device_share_enable.md">dcmi_get_device_share_enable Prototype</a>.</p>
</div></div>
</td>
</tr>
<tr id="en-us_topic_0257495783_row42644663"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="en-us_topic_0257495783_p851813475811"><a name="en-us_topic_0257495783_p851813475811"></a><a name="en-us_topic_0257495783_p851813475811"></a>sub_cmd</p>
</td>
<td class="cellrowborder" valign="top" width="8.41%" headers="mcps1.1.5.1.2 "><p id="en-us_topic_0257495783_p133891038900"><a name="en-us_topic_0257495783_p133891038900"></a><a name="en-us_topic_0257495783_p133891038900"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.470000000000002%" headers="mcps1.1.5.1.3 "><p id="p17196155202616"><a name="p17196155202616"></a><a name="p17196155202616"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="62.019999999999996%" headers="mcps1.1.5.1.4 "><p id="p1528419185118"><a name="p1528419185118"></a><a name="p1528419185118"></a>typedef enum {</p>
<p id="p1228415916510"><a name="p1228415916510"></a><a name="p1228415916510"></a>DCMI_DEVICE_SHARE_SUB_CMD_COMMON = 0,</p>
<p id="p1028409165114"><a name="p1028409165114"></a><a name="p1028409165114"></a>DCMI_DEVICE_SHARE_SUB_CMD_MAX,</p>
<p id="p328411917514"><a name="p328411917514"></a><a name="p328411917514"></a>} DCMI_DEVICE_SHARE_SUB_CMD;</p>
<div class="note" id="note690717511525"><a name="note690717511525"></a><a name="note690717511525"></a><span class="notetitle"> NOTE: </span><div class="notebody"><p id="p12648821111120"><a name="p12648821111120"></a><a name="p12648821111120"></a>Currently, this API supports only the <strong id="b1377321863112142"><a name="b1377321863112142"></a><a name="b1377321863112142"></a>DCMI_DEVICE_SHARE_SUB_CMD_COMMON</strong> command.</p>
</div></div>
</td>
</tr>
<tr id="en-us_topic_0257495783_row17526454687"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p123108571525"><a name="p123108571525"></a><a name="p123108571525"></a>buf</p>
</td>
<td class="cellrowborder" valign="top" width="8.41%" headers="mcps1.1.5.1.2 "><p id="p13102579214"><a name="p13102579214"></a><a name="p13102579214"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.470000000000002%" headers="mcps1.1.5.1.3 "><p id="p331005717211"><a name="p331005717211"></a><a name="p331005717211"></a>const void *</p>
</td>
<td class="cellrowborder" valign="top" width="62.019999999999996%" headers="mcps1.1.5.1.4 "><p id="p12310115715214"><a name="p12310115715214"></a><a name="p12310115715214"></a>For details, see the restrictions.</p>
</td>
</tr>
<tr id="en-us_topic_0257495783_row19825197914"><td class="cellrowborder" valign="top" width="10.100000000000001%" headers="mcps1.1.5.1.1 "><p id="p1231019570213"><a name="p1231019570213"></a><a name="p1231019570213"></a>buf_size</p>
</td>
<td class="cellrowborder" valign="top" width="8.41%" headers="mcps1.1.5.1.2 "><p id="p12310115715213"><a name="p12310115715213"></a><a name="p12310115715213"></a>Input</p>
</td>
<td class="cellrowborder" valign="top" width="19.470000000000002%" headers="mcps1.1.5.1.3 "><p id="p1531045710217"><a name="p1531045710217"></a><a name="p1531045710217"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="62.019999999999996%" headers="mcps1.1.5.1.4 "><p id="p13107576214"><a name="p13107576214"></a><a name="p13107576214"></a>Length of the <strong id="b1788943055112146"><a name="b1788943055112146"></a><a name="b1788943055112146"></a>buf</strong> array.</p>
</td>
</tr>
</tbody>
</table>

**Return Value<a name="section283355715302"></a>**

<a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_table19654399"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row59025204"><th class="cellrowborder" valign="top" width="24.18%" id="mcps1.1.3.1.1"><p id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p16312252"></a>Type</p>
</th>
<th class="cellrowborder" valign="top" width="75.82%" id="mcps1.1.3.1.2"><p id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p46224020"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_row13362997"><td class="cellrowborder" valign="top" width="24.18%" headers="mcps1.1.3.1.1 "><p id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p8661001"></a>int</p>
</td>
<td class="cellrowborder" valign="top" width="75.82%" headers="mcps1.1.3.1.2 "><p id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_p79561110476"></a>Results:</p>
<a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><a name="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"></a><ul id="en-us_topic_0000002515343916_en-us_topic_0000001251227149_en-us_topic_0000001178213202_en-us_topic_0000001097675636_en-us_topic_0000001170223803_en-us_topic_0160151812_ul55711364478"><li>Success: The value <strong id="en-us_topic_0000002515343916_b154714878985455"><a name="en-us_topic_0000002515343916_b154714878985455"></a><a name="en-us_topic_0000002515343916_b154714878985455"></a>0</strong> is returned.</li><li>Failure: For details about the return codes, see <a href="return-codes.md">Return Codes</a>.</li></ul>
</td>
</tr>
</tbody>
</table>

**Exception Handling<a name="section6438163417415"></a>**

None.

**Restrictions<a name="section13645113118442"></a>**

- After the container sharing mode is set, the container is disabled by default after system restart.
- This API can be used in PM + privileged containers.

**Table  1**  buf formats corresponding to sub\_cmd

<a name="table08529574303"></a>
<table><thead align="left"><tr id="row19012578309"><th class="cellrowborder" valign="top" width="39.269999999999996%" id="mcps1.2.4.1.1"><p id="p59011457173015"><a name="p59011457173015"></a><a name="p59011457173015"></a>sub_cmd</p>
</th>
<th class="cellrowborder" valign="top" width="36.96%" id="mcps1.2.4.1.2"><p id="p159011357193019"><a name="p159011357193019"></a><a name="p159011357193019"></a>buf Data Type</p>
</th>
<th class="cellrowborder" valign="top" width="23.77%" id="mcps1.2.4.1.3"><p id="p1990165743011"><a name="p1990165743011"></a><a name="p1990165743011"></a>size</p>
</th>
</tr>
</thead>
<tbody><tr id="row15743174685218"><td class="cellrowborder" valign="top" width="39.269999999999996%" headers="mcps1.2.4.1.1 "><p id="p17744144675215"><a name="p17744144675215"></a><a name="p17744144675215"></a>DCMI_DEVICE_SHARE_SUB_CMD_COMMON</p>
</td>
<td class="cellrowborder" valign="top" width="36.96%" headers="mcps1.2.4.1.2 "><p id="p6898117165315"><a name="p6898117165315"></a><a name="p6898117165315"></a>unsigned int</p>
</td>
<td class="cellrowborder" valign="top" width="23.77%" headers="mcps1.2.4.1.3 "><p id="p789841705311"><a name="p789841705311"></a><a name="p789841705311"></a>unsigned int</p>
</td>
</tr>
</tbody>
</table>

**Table  2** Support in different deployment scenarios

<a name="table1113417173519"></a>
<table><thead align="left"><tr id="en-us_topic_0000002515503852_row1548132517501"><th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.1"><p id="en-us_topic_0000002515503852_p16154843125019"><a name="en-us_topic_0000002515503852_p16154843125019"></a><a name="en-us_topic_0000002515503852_p16154843125019"></a>Product Model</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.2"><p id="en-us_topic_0000002515503852_p3123182915595"><a name="en-us_topic_0000002515503852_p3123182915595"></a><a name="en-us_topic_0000002515503852_p3123182915595"></a><strong id="en-us_topic_0000002515503852_b181336402033"><a name="en-us_topic_0000002515503852_b181336402033"></a><a name="en-us_topic_0000002515503852_b181336402033"></a>root</strong> User in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.3"><p id="en-us_topic_0000002515503852_p6271950600"><a name="en-us_topic_0000002515503852_p6271950600"></a><a name="en-us_topic_0000002515503852_p6271950600"></a>Running User Group (Non-<strong id="en-us_topic_0000002515503852_b56571847339"><a name="en-us_topic_0000002515503852_b56571847339"></a><a name="en-us_topic_0000002515503852_b56571847339"></a>root</strong> User) in PM (Bare Metal) Scenarios</p>
</th>
<th class="cellrowborder" valign="top" width="25%" id="mcps1.2.5.1.4"><p id="en-us_topic_0000002515503852_p41541743195013"><a name="en-us_topic_0000002515503852_p41541743195013"></a><a name="en-us_topic_0000002515503852_p41541743195013"></a><strong id="en-us_topic_0000002515503852_b6856448332"><a name="en-us_topic_0000002515503852_b6856448332"></a><a name="en-us_topic_0000002515503852_b6856448332"></a>root</strong> User in PM + Unprivileged Container Scenarios</p>
</th>
</tr>
</thead>
<tbody><tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row18920204653019"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p19444162914516"><a name="en-us_topic_0000002515503852_p19444162914516"></a><a name="en-us_topic_0000002515503852_p19444162914516"></a><span id="en-us_topic_0000002515503852_ph1944412296514"><a name="en-us_topic_0000002515503852_ph1944412296514"></a><a name="en-us_topic_0000002515503852_ph1944412296514"></a><span id="en-us_topic_0000002515503852_text944432913516"><a name="en-us_topic_0000002515503852_text944432913516"></a><a name="en-us_topic_0000002515503852_text944432913516"></a>Atlas 900 A2 PoD cluster basic unit</span></span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p162792612478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p172742619478"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a><a name="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_p527162654713"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_en-us_topic_0000001917887173_en-us_topic_0000001206627196_en-us_topic_0000001178213188_en-us_topic_0000001146459833_row6920114611302"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p194441629165117"><a name="en-us_topic_0000002515503852_p194441629165117"></a><a name="en-us_topic_0000002515503852_p194441629165117"></a><span id="en-us_topic_0000002515503852_text124449291511"><a name="en-us_topic_0000002515503852_text124449291511"></a><a name="en-us_topic_0000002515503852_text124449291511"></a>Atlas 800T A2 training server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p461320458219"><a name="en-us_topic_0000002515503852_p461320458219"></a><a name="en-us_topic_0000002515503852_p461320458219"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p13613134519220"><a name="en-us_topic_0000002515503852_p13613134519220"></a><a name="en-us_topic_0000002515503852_p13613134519220"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p96133454211"><a name="en-us_topic_0000002515503852_p96133454211"></a><a name="en-us_topic_0000002515503852_p96133454211"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row14873174604910"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p10444329155118"><a name="en-us_topic_0000002515503852_p10444329155118"></a><a name="en-us_topic_0000002515503852_p10444329155118"></a><span id="en-us_topic_0000002515503852_text1044482925119"><a name="en-us_topic_0000002515503852_text1044482925119"></a><a name="en-us_topic_0000002515503852_text1044482925119"></a>Atlas 800I A2 inference server</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p896391111511"><a name="en-us_topic_0000002515503852_p896391111511"></a><a name="en-us_topic_0000002515503852_p896391111511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p119682011105113"><a name="en-us_topic_0000002515503852_p119682011105113"></a><a name="en-us_topic_0000002515503852_p119682011105113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p11975911195114"><a name="en-us_topic_0000002515503852_p11975911195114"></a><a name="en-us_topic_0000002515503852_p11975911195114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row23051923114915"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p174441129175117"><a name="en-us_topic_0000002515503852_p174441129175117"></a><a name="en-us_topic_0000002515503852_p174441129175117"></a><span id="en-us_topic_0000002515503852_text34441729175119"><a name="en-us_topic_0000002515503852_text34441729175119"></a><a name="en-us_topic_0000002515503852_text34441729175119"></a>Atlas 200T A2 Box16 heterogeneous subrack</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1298231135114"><a name="en-us_topic_0000002515503852_p1298231135114"></a><a name="en-us_topic_0000002515503852_p1298231135114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1298921117511"><a name="en-us_topic_0000002515503852_p1298921117511"></a><a name="en-us_topic_0000002515503852_p1298921117511"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p1799561165114"><a name="en-us_topic_0000002515503852_p1799561165114"></a><a name="en-us_topic_0000002515503852_p1799561165114"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row44814564912"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1044414295516"><a name="en-us_topic_0000002515503852_p1044414295516"></a><a name="en-us_topic_0000002515503852_p1044414295516"></a><span id="en-us_topic_0000002515503852_text44441829105114"><a name="en-us_topic_0000002515503852_text44441829105114"></a><a name="en-us_topic_0000002515503852_text44441829105114"></a>A200I A2 Box heterogeneous component</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p17181285116"><a name="en-us_topic_0000002515503852_p17181285116"></a><a name="en-us_topic_0000002515503852_p17181285116"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p15131255114"><a name="en-us_topic_0000002515503852_p15131255114"></a><a name="en-us_topic_0000002515503852_p15131255114"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p15917127513"><a name="en-us_topic_0000002515503852_p15917127513"></a><a name="en-us_topic_0000002515503852_p15917127513"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row11171321114917"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p94441829165113"><a name="en-us_topic_0000002515503852_p94441829165113"></a><a name="en-us_topic_0000002515503852_p94441829165113"></a><span id="en-us_topic_0000002515503852_text16445122985112"><a name="en-us_topic_0000002515503852_text16445122985112"></a><a name="en-us_topic_0000002515503852_text16445122985112"></a>A300I A2 inference card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p01211275113"><a name="en-us_topic_0000002515503852_p01211275113"></a><a name="en-us_topic_0000002515503852_p01211275113"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p916161212512"><a name="en-us_topic_0000002515503852_p916161212512"></a><a name="en-us_topic_0000002515503852_p916161212512"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p191911211511"><a name="en-us_topic_0000002515503852_p191911211511"></a><a name="en-us_topic_0000002515503852_p191911211511"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row18385115320492"><td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.1 "><p id="en-us_topic_0000002515503852_p1644572918513"><a name="en-us_topic_0000002515503852_p1644572918513"></a><a name="en-us_topic_0000002515503852_p1644572918513"></a><span id="en-us_topic_0000002515503852_text1744532925112"><a name="en-us_topic_0000002515503852_text1744532925112"></a><a name="en-us_topic_0000002515503852_text1744532925112"></a>Atlas 300T A2 training card</span></p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.2 "><p id="en-us_topic_0000002515503852_p1923112115119"><a name="en-us_topic_0000002515503852_p1923112115119"></a><a name="en-us_topic_0000002515503852_p1923112115119"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.3 "><p id="en-us_topic_0000002515503852_p1269122515"><a name="en-us_topic_0000002515503852_p1269122515"></a><a name="en-us_topic_0000002515503852_p1269122515"></a>Y</p>
</td>
<td class="cellrowborder" valign="top" width="25%" headers="mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p43014126516"><a name="en-us_topic_0000002515503852_p43014126516"></a><a name="en-us_topic_0000002515503852_p43014126516"></a>Y</p>
</td>
</tr>
<tr id="en-us_topic_0000002515503852_row557915117191"><td class="cellrowborder" colspan="4" valign="top" headers="mcps1.2.5.1.1 mcps1.2.5.1.2 mcps1.2.5.1.3 mcps1.2.5.1.4 "><p id="en-us_topic_0000002515503852_p2042757191916"><a name="en-us_topic_0000002515503852_p2042757191916"></a><a name="en-us_topic_0000002515503852_p2042757191916"></a><span id="en-us_topic_0000002515503852_ph209063317554"><a name="en-us_topic_0000002515503852_ph209063317554"></a><a name="en-us_topic_0000002515503852_ph209063317554"></a>Note: <strong id="en-us_topic_0000002515503852_b169314010451"><a name="en-us_topic_0000002515503852_b169314010451"></a><a name="en-us_topic_0000002515503852_b169314010451"></a>Y</strong> indicates that the function is supported, <strong id="en-us_topic_0000002515503852_b2931140104515"><a name="en-us_topic_0000002515503852_b2931140104515"></a><a name="en-us_topic_0000002515503852_b2931140104515"></a>N</strong> indicates that the function is not supported, and <strong id="en-us_topic_0000002515503852_b16931503456"><a name="en-us_topic_0000002515503852_b16931503456"></a><a name="en-us_topic_0000002515503852_b16931503456"></a>NA</strong> indicates that the function is not involved and planned currently.</span></p>
</td>
</tr>
</tbody>
</table>

**Example<a name="section0362658184619"></a>**

```c
...
    int ret = 0;
    int card_id = 0;
    int dev_id = 0;
    unsigned int enable_flag = 1;
    unsigned int size = (unsigned int)sizeof(int);
    ret = dcmi_get_device_info(card_id, dev_id, DCMI_MAIN_CMD_DEVICE_SHARE, 
        DCMI_DEVICE_SHARE_SUB_CMD_COMMON, &enable_flag, &size);
    if (ret != DCMI_OK){
        // todo: Record logs.
        return ret;
    }
...
```
