#!/usr/bin/python3
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author(s): Denis Kurka <kurka@dyna-nic.com>

import nfb
import argparse
import sys
import yaml
from functools import reduce


class nic_rss(nfb.BaseComp):
    DT_COMPATIBLE = "cesnet,nic_rss"

    _REG_CHAN_OFF        = 0x00
    _REG_CONTROL         = 0x04
    _REG_STATUS          = 0x08
    _REG_HASH_INPUT_CONF = 0x10
    _REG_HASH_FUNC_CONF  = 0x14
    _REG_HASH_KEY_RAM    = 0x18
    _REG_RETA_RAM        = 0x1C

    _KEY_DEFAULT   = 0xfa01acbe3bb7426a0cf23080a32dcb77b4307baecb2bcad0b08fa3433d256741c20e5b25da565a6d
    _KEY_SYMMETRIC = 0x5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d

    _RSS_INPUT_BIT_INDEX = {'sorted_input': 0, 'ipv4': 2, 'frag_ipv4': 3, 'nonfrag_ipv4_tcp': 4, 'nonfrag_ipv4_udp': 5, 'nonfrag_ipv4_sctp': 6, 'nonfrag_ipv4_other': 7, 'ipv6': 8, 'frag_ipv6': 9, 'nonfrag_ipv6_tcp': 10, 'nonfrag_ipv6_udp': 11, 'nonfrag_ipv6_sctp': 12, 'nonfrag_ipv6_other': 13, 'l4_dst_only': 28, 'l4_src_only': 29, 'l3_dst_only': 30, 'l3_src_only': 31}

    def __init__(self, **kwargs):
        try:
            super().__init__(**kwargs)
            self._name = "NIC RSS"
            if "index" in kwargs:
                self._name += " " + str(kwargs.get("index"))
                self.index = kwargs.get("index")
        except Exception as exc:
            print("ERROR while opening NIC RSS component!\nMaybe unsupported FPGA firmware?!")
            print(f"Exception msg: {exc}")
            exit(1)

        self.node = self._dev.fdt_get_compatible(self.DT_COMPATIBLE)[self.index]
        self.reta_size = self.node.get_property("reta_capacity").value
        self.key_size = self.node.get_property("key_size").value
        # The two most significant bytes in the status reg are eth channels
        # that are connected to given RSS core (indexes are relative to the APP core)
        self.channels = (self.read_status_reg() >> 16)

    def read_status_reg(self):
        reg = self._comp.read32(self._REG_STATUS)
        return reg

    def get_ifc(self):
        return self.channels

    def set_reg_chan_off(self, channel, offset=0):
        # Write channel and MI word offset
        reg_chan_off_val = ((channel << 16) | offset)
        self._comp.write32(self._REG_CHAN_OFF, reg_chan_off_val)

    def set_hash_type(self, channel, rss_type):
        if not rss_type:
            return

        self.set_reg_chan_off(channel)
        if rss_type == 'toeplitz':
            self._comp.write32(self._REG_HASH_FUNC_CONF, 1)
            print("RSS (IFC " + str(channel) + "): Set Toeplitz hash type")
        else:
            self._comp.write32(self._REG_HASH_FUNC_CONF, 2)
            print("RSS (IFC " + str(channel) + "): Set SimpleXOR hash type")

    def get_hash_type(self, channel):
        self.set_reg_chan_off(channel)
        rss_type = 'unknown'
        rss_type_reg = self._comp.read32(self._REG_HASH_FUNC_CONF)
        if rss_type_reg == 1:
            rss_type = 'toeplitz'
        if rss_type_reg == 2:
            rss_type = 'simple_xor'
        return rss_type

    def set_hash_key(self, channel, rss_key):
        if not rss_key:
            return

        print("RSS (IFC " + str(channel) + "): Use the following RSS key: " + hex(rss_key))
        orig_key_len = max(self.key_size, ((rss_key.bit_length() + 7) // 8))
        orig_key_val = rss_key.to_bytes(orig_key_len, 'little')

        if orig_key_len > self.key_size:
            print("RSS (IFC " + str(channel) + "): RSS key is too long, upper bits will be ignored!")
            selected_key = orig_key_val[:self.key_size]
        else:
            selected_key = orig_key_val

        for i in range(0, int(self.key_size / 4)):
            self.set_reg_chan_off(channel, i)
            # Write key 4B word
            key_word = 0
            for j in range(0, 4):
                key_word += selected_key[(i * 4) + j] * (2**(j * 8))
            self._comp.write32(self._REG_HASH_KEY_RAM, key_word)
            #print("RSS: Key word " + hex(key_word))
        print("RSS (IFC " + str(channel) + "): The RSS key configuration is done.")

    def get_hash_key(self, channel):
        hash_key = 0x0
        for i in range(0, int(self.key_size / 4)):
            self.set_reg_chan_off(channel, i)
            key_word = self._comp.read32(self._REG_HASH_KEY_RAM)
            hash_key += (key_word << (i * 32))
        return hash_key

    def set_input_conf(self, channel, rss_input):
        if not rss_input:
            return

        self.set_reg_chan_off(channel)
        rss_input_dic = {key: 2**(self._RSS_INPUT_BIT_INDEX[key]) for key in self._RSS_INPUT_BIT_INDEX}
        rss_input_reg = reduce(lambda x, y: x + rss_input_dic[y], rss_input, 0)

        self._comp.write32(self._REG_HASH_INPUT_CONF, rss_input_reg)
        print("RSS (IFC " + str(channel) + "): Set input configuration " + hex(rss_input_reg))

    def get_input_conf(self, channel):
        self.set_reg_chan_off(channel)
        rss_input_reg = self._comp.read32(self._REG_HASH_INPUT_CONF)
        rss_input_str = ""

        for key in self._RSS_INPUT_BIT_INDEX:
            if (rss_input_reg & (1 << (self._RSS_INPUT_BIT_INDEX[key]))):
                rss_input_str += key + ", "

        if rss_input_str == "":
            rss_input_str = "none"

        return rss_input_str

    def reta_modify_item(self, channel, reta_index, reta_value):
        self.set_reg_chan_off(channel, reta_index)
        # write RETA value/queue
        self._comp.write32(self._REG_RETA_RAM, reta_value)
        #print("RETA: Set queue number " + str(reta_value) + " to RETA on index " + str(reta_index))

    def reta_edit_item(self, channel, reta_index, reta_value):
        self.reta_modify_item(channel, reta_index, reta_value)
        print("RETA: Set queue number " + str(reta_value) + " to RETA (IFC " + str(channel) + ") on index " + str(reta_index))

    def reta_read_item(self, channel, reta_item):
        self.set_reg_chan_off(channel, reta_item)
        # write RETA value/queue
        return self._comp.read32(self._REG_RETA_RAM)

    def reta_reset(self, channel, reta_rst_val):
        for i in range(0, self.reta_size):
            self.reta_modify_item(channel, i, reta_rst_val)
        print("RETA (IFC " + str(channel) + "): Reset RETA table with value " + str(reta_rst_val))

    def reta_auto(self, channel, reta_qmin, reta_qmax):
        if reta_qmin is None or reta_qmax is None:
            print("RETA (IFC " + str(channel) + "): Auto configuration of RETA table failed!")
            print(reta_qmin)
            print(reta_qmax)
            return
        queue = reta_qmin
        for i in range(0, self.reta_size):
            self.reta_modify_item(channel, i, queue)
            queue += 1
            if queue > reta_qmax:
                queue = reta_qmin
        print("RETA (IFC " + str(channel) + "): Auto configuration of RETA table with queue range " + str(reta_qmin) + " to " + str(reta_qmax))

    def get_reta_conf(self, channel):
        reta_dic = {}
        for i in range(0, self.reta_size):
            reta_dic[i] = self.reta_read_item(channel, i)
        return reta_dic

    def parse_yaml_rss_conf(self, yaml_conf_file):
        reta_queue_min = None
        reta_queue_max = None

        if not yaml_conf_file:
            print("ERROR in RSS YAML configuration file!")
            exit(1)

        stream = open(yaml_conf_file, 'r')
        conf_dic = yaml.safe_load(stream)
        rss_dic = conf_dic.get("rss")

        if not rss_dic:
            print("ERROR in RSS YAML configuration file!")
            exit(1)
        rss_ifc = rss_dic.get("ifc")
        if rss_ifc != "all" and type(rss_ifc) is not int:
            print("ERROR in RSS YAML configuration file, bad IFC!")
            exit(1)
        rss_input = rss_dic.get("input_conf")
        if not rss_input:
            print("ERROR in RSS YAML configuration file, bad input_conf!")
            exit(1)
        rss_hash_type = rss_dic.get("hash_type")
        if not rss_hash_type:
            print("ERROR in RSS YAML configuration file, bad hash_type!")
            exit(1)
        rss_hash_key = rss_dic.get("hash_key")
        if not rss_hash_key:
            print("ERROR in RSS YAML configuration file, bad hash_key!")
            exit(1)
        if type(rss_hash_key) is not int:
            if rss_hash_key == "default":
                rss_hash_key = self._KEY_DEFAULT
            elif rss_hash_key == "symmetric":
                rss_hash_key = self._KEY_SYMMETRIC
            else:
                print("ERROR in RSS YAML configuration file, unsupported value of hash_key!")
                exit(1)

        reta_dic = rss_dic.get("reta")
        if not reta_dic:
            print("ERROR in RSS YAML configuration file, bad reta!")
            exit(1)
        reta_queue_min = reta_dic.get("queue_min")
        reta_queue_max = reta_dic.get("queue_max")

        return rss_ifc, rss_input, rss_hash_type, rss_hash_key, reta_queue_min, reta_queue_max

    def configure(self, yaml_conf_file):
        rss_conf = self.parse_yaml_rss_conf(yaml_conf_file)
        rss_ifc, rss_input, rss_hash_type, rss_hash_key, reta_qmin, reta_qmax = rss_conf

        if rss_ifc == "all":
            channel_list = list(range(0, self.channels))
        else:
            if rss_ifc >= self.channels:
                print("ERROR: ETH IFC " + str(rss_ifc) + " is unavailable in the current firmware!")
                exit(1)
            else:
                channel_list = [rss_ifc]

        for channel in channel_list:
            # RSS configuration
            self.set_hash_type(channel, rss_hash_type)
            self.set_hash_key(channel, rss_hash_key)
            self.set_input_conf(channel, rss_input)
            # RETA configuration
            self.reta_reset(channel, 0)
            self.reta_auto(channel, reta_qmin, reta_qmax)

    def print_reta_table(self, reta_conf):
        for i in range(0, int(self.reta_size / 8)):
            table_str = ""
            for j in range(0, 8):
                table_str += (" " + str(reta_conf[i * 8 + j]).rjust(3))
            print("  " + str(i * 8).rjust(4) + ":" + table_str)

    def dump_conf(self):
        for channel in range(0, self.channels):
            hash_func = self.get_hash_type(channel)
            hash_key = self.get_hash_key(channel)
            hash_input = self.get_input_conf(channel)
            reta_conf = self.get_reta_conf(channel)

            print("RSS configuration of ETH interface " + str(channel) + ":")
            print("========================================")
            print("HASH function : " + hash_func)
            print("HASH key      : " + hex(hash_key))
            print("HASH input    : " + hash_input)
            print("RETA size     : " + str(self.reta_size) + " items")
            print("RETA table    :")
            self.print_reta_table(reta_conf)
            print()

def main():
    parser = argparse.ArgumentParser(description='RSS Configuration Tool for NDK Firmware')

    # Device Selection
    parser.add_argument("-d", "--device", action="store", default='/dev/nfb0',
                        help="Path to NFB device (default: /dev/nfb0)")
    parser.add_argument("-i", "--index", type=int, default=0,
                        help="Index of the RSS component to configure (default: 0)")

    # Configuration Actions
    parser.add_argument("--rss_conf", help="RSS configuration file (YAML)", metavar="FILE")
    parser.add_argument("--reta_rst", nargs=2, metavar=('ifc', 'value'),
                        help="Reset RETA table for specific interface with value")
    parser.add_argument("--reta_edit", nargs=3, metavar=('ifc', 'idx', 'queue'),
                        help="Edit specific RETA item: <interface> <index> <queue>")
    parser.add_argument("--dump", action="store_true", help="Dump current RSS configuration")

    args = parser.parse_args()

    try:
        # Open NFB Device
        with nfb.open(args.device) as dev:
            # Initialize the RSS component only
            # Note: nic_rss uses fdt_get_compatible internally
            rss = nic_rss(dev=dev, index=args.index)

            # 1. Apply YAML Configuration
            if args.rss_conf:
                print(f"Applying configuration from {args.rss_conf}...")
                rss.configure(args.rss_conf)

            # 2. Manual RETA Reset
            if args.reta_rst:
                ifc = int(args.reta_rst[0], 0)
                val = int(args.reta_rst[1], 0)
                rss.reta_reset(ifc, val)

            # 3. Manual RETA Edit
            if args.reta_edit:
                ifc = int(args.reta_edit[0], 0)
                idx = int(args.reta_edit[1], 0)
                que = int(args.reta_edit[2], 0)
                rss.reta_edit_item(ifc, idx, que)

            # 4. Dump Configuration (Default behavior if no specific command given)
            if args.dump or (not args.rss_conf and not args.reta_rst and not args.reta_edit):
                print(f"--- RSS Status (Index {args.index}) ---")
                print(f"Available ETH Channels: {rss.get_ifc()}")
                rss.dump_conf()

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
