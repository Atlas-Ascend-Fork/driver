#!/usr/bin/env python

# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

#-*- coding: utf-8 -*-
#----------------------------------------------------------------------------
# Purpose: merge driver.xml
# Rule:
# 1. If the XML element (e.g, '<file_info> </file_info>') does not exist or is different,
#    add the entire element (without checking sub-attributes).
# 2. If the XML element (e.g, '<file_info> </file_info>') are the same,
#    the entire element is ignored (no need to check the sub-attributes).
# 3. If the attributes (e.g, '<file />') except src_path are the same, update the value of src_path.
#----------------------------------------------------------------------------
import os
import sys
import xml.etree.ElementTree as ET
import logging
logging.basicConfig(level=logging.INFO)


class CommentedTreeBuilder(ET.TreeBuilder):
    def __init__(self, *args, **kwargs):
        super(CommentedTreeBuilder, self).__init__(*args, **kwargs)

    def comment(self, data):
        self.start(ET.Comment, {})
        self.data(data)
        self.end(ET.Comment)


def compare_attribs(src, dst, exclude_attr=None):
    exclude_attr = exclude_attr if exclude_attr else []
    src_attrs = {k: v for k, v in src.attrib.items() if k not in exclude_attr}
    dst_attrs = {k: v for k, v in dst.attrib.items() if k not in exclude_attr}
    return src_attrs == dst_attrs


def copy_node_to_filesystem(it_root_node, root_node):
    for it_n in it_root_node:
        tag = it_n.tag
        attrib = it_n.attrib
        des_node = root_node.findall(tag)
        if len(des_node) == 0:
            logging.debug("Add node: %s, %s", it_n.tag, it_n.attrib)
            root_node.append(it_n)
        else:
            flag = True
            node = None
            for n in des_node:
                # update the src_path attribute when the src_path of the file is different.
                if tag == 'file' and it_n.get('src_path') is not None and compare_attribs(it_n, n, 'src_path') == True:
                    logging.debug("Update src_path attr from %s to %s", it_n.attrib, n.attrib)
                    n.set('src_path', it_n.get('src_path'))
                    node = n
                    flag = False
                    break

                if attrib == n.attrib:
                    node = n
                    flag = False
                    break
                if (attrib['value'] == n.attrib['value']) and ('install_type' in attrib.keys()) and ('install_type' in n.attrib.keys()):
                    if tag == "path":
                        n.attrib['install_type'] = attrib['install_type']
            if flag:
                logging.debug("Append node: %s, %s", it_n.tag, it_n.attrib)
                root_node.append(it_n)
            else:
                copy_node_to_filesystem(it_n, node)


def adjust_xml(xml_file, xml_it_file):
    if not os.path.exists(xml_file):
        print("%s no such file" % xml_file)
        sys.exit(-1)
    if not os.path.exists(xml_it_file):
        print ("%s no such file" % xml_it_file)
        sys.exit(-1)
    tree = ET.parse(xml_file)
    root_node = tree.getroot()
    it_root_node = ET.parse(xml_it_file).getroot()
    tag = it_root_node.tag
    attrib = it_root_node.attrib
    if tag != root_node.tag or attrib != root_node.attrib:
        raise ValueError(f"XML root mismatch: {xml_file} and {xml_it_file}")

    # 将drvier.xml中的develop开发形态从解压的driver run包中获取
    for item in root_node.findall('file_info'):
        dict = item.attrib
        if "develop" in dict.get("dst_path"):
            dict["copy_type"] = "source"
            dict["src_path"] = "".join(["package/run/driver/", dict.get("dst_path")])
            for sub_item in list(item.iter())[1:]:
                sub_dict = sub_item.attrib
                sub_dict['value'] = os.path.basename(sub_dict.get('value'))

    copy_node_to_filesystem(it_root_node, root_node)
    tree.write(xml_file)

def main():
    if len(sys.argv) != 3:
        sys.exit(-1)
    base_path_xml = sys.argv[1]
    it_path_xml = sys.argv[2]
    adjust_xml(base_path_xml, it_path_xml)

if __name__ == '__main__':
    main()
