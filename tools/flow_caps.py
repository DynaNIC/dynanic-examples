# flow_caps.py: Show rte_flow patterns and actions supported by the firmware
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.
#
# The script mirrors the rte_flow logic of the DPDK nfb driver (drivers/net/nfb/flow in
# sn4dc-dpdk), branch vodak-mf-support-generic-v5; the comments name the driver functions
# each rule comes from. Check them when the driver changes.

"""Show rte_flow capabilities of the firmware read from the device tree.

Groups are listed per ETH stream, streams with identical groups are merged.
Without COMMAND the summary is shown.

matrix cells:
  x = supported, msb:lsb = supported bit range of the field, . = not supported

patterns mask row, the masks a rule may use, given by the group type:
  any    = any mask (tcam)
  full   = every field of the group needs the full mask, e.g. "ipv4 src is A" (exact);
           IPv4 and IPv6 addresses are alternatives, a protocol with fields cannot be
           matched by its presence only
  prefix = the masks form one contiguous prefix from the MSB, e.g. "dst mask 255.255.0.0" (lpm)

patterns match all row: x = the group accepts a rule matching all packets, "pattern end"
(or "pattern void / end", the same rule)

actions mac_swap row: the group has a swap unit of the whole outermost MAC addresses, the
device tree does not list mac_swap among the actions

modify-field columns are the parameters of testpmd modify_field action, swap units are
left out as they serve no modify_field action. --debug shows all the units as in the
device tree, with their valid bits; a bit shared by several units is marked with '!'.

requirements:
  python-nfb for NFB device, python package 'fdt' for --dts/--dtb

examples:
  python3 flow_caps.py
  python3 flow_caps.py -p 0 patterns -g 0 -g 3
  python3 flow_caps.py --dts DevTree.dts mf
  python3 flow_caps.py mf --debug -g 0
"""

import argparse
import re
import sys
from collections import namedtuple

GROUPS_COMPATIBLE = "cesnet,snic_groups"
ETH_COMPATIBLE = "netcope,eth"

# Pattern key, proto includes "[level][index]" when it differs from [0][0].
# field/msb/lsb are None for protocol presence key such as "ipv4[0][0]".
Key = namedtuple("Key", "proto field msb lsb raw")
# One modify_field unit (mf_action_N node), offsets and widths are in bits,
# win is the width of the MF_VALUES window of its group, None if the driver has none.
MfAction = namedtuple("MfAction", "num oper dst dst_off dst_w src src_off src_w valid win")
# Field of a modify_field unit as rte_flow names it, index is None for a field without tag_index.
RteField = namedtuple("RteField", "name level offset index")
# One rte_flow group (group_N node), index is the rte_flow group ID.
Group = namedtuple("Group", "index type capacity keys actions mf")
# snic_groups nodes with identical groups and ETH ports which use them.
GroupSet = namedtuple("GroupSet", "names eth_ports groups")

# Key grammar follows parse_key_id_expr() in DPDK drivers/net/nfb/flow/nfb_flow_setup.c:
#   <proto>[<level>][<index>]                     protocol presence
#   <proto>[<level>][<index>].<field>(<msb>:<lsb>)  protocol field
#   metadata.<field>[<index>]?(<msb>:<lsb>)         metadata field
KEY_PROTO_RE = re.compile(r"^(\w+)\[(\d+)\]\[(\d+)\](?:\.(\w+)\((\d+):(\d+)\))?$")
KEY_META_RE = re.compile(r"^metadata\.(\w+)(?:\[(\d+)\])?\((\d+):(\d+)\)$")
# modify_field operand, e.g. "ipv4[0][0].ttl(o=0, w=8)" or "value(o=120, w=8)"
MF_OPERAND_RE = re.compile(r"^(.+)\(o=(\d+),\s*w=(\d+)\)$")
MF_NODE_RE = re.compile(r"^mf_action_(\d+)$")
MF_CONST_RE = re.compile(r"^const\.(\d+)$")
MF_FIELD_RE = re.compile(r"^(\w+)\[(\d+)\]\[(\d+)\]\.(\w+)$")
MF_META_RE = re.compile(r"^metadata\.(\w+)(?:\[(\d+)\])?$")
# Fields modify_field can name (mf_field_map in DPDK drivers/net/nfb/flow/nfb_flow_mf.c):
#   (proto, DT field) -> (testpmd field name, DT key width, indexed, pad, host_bits)
# indexed: the DT [<index>] is the rte_flow tag_index; pad: DT key bits before the rte_flow
# field; host_bits: metadata number whose rte_flow offset counts from the LSB, 0 for a packet field
MfField = namedtuple("MfField", "name width indexed pad host_bits")
MF_FIELDS = {
    ("eth", "dst_addr"): MfField("mac_dst", 48, False, 0, 0),
    ("eth", "src_addr"): MfField("mac_src", 48, False, 0, 0),
    ("eth", "type"): MfField("mac_type", 16, False, 0, 0),
    ("vlan", "tci"): MfField("vlan_id", 16, True, 4, 0),
    ("ipv4", "dscp"): MfField("ipv4_dscp", 6, False, 0, 0),
    ("ipv4", "ttl"): MfField("ipv4_ttl", 8, False, 0, 0),
    ("ipv4", "src_addr"): MfField("ipv4_src", 32, False, 0, 0),
    ("ipv4", "dst_addr"): MfField("ipv4_dst", 32, False, 0, 0),
    ("ipv4", "proto"): MfField("ipv4_proto", 8, False, 0, 0),
    ("ipv6", "hop_limit"): MfField("ipv6_hoplimit", 8, False, 0, 0),
    ("ipv6", "src_addr"): MfField("ipv6_src", 128, False, 0, 0),
    ("ipv6", "dst_addr"): MfField("ipv6_dst", 128, False, 0, 0),
    ("ipv6", "next_header"): MfField("ipv6_proto", 8, False, 0, 0),
    ("tcp", "src_port"): MfField("tcp_port_src", 16, False, 0, 0),
    ("tcp", "dst_port"): MfField("tcp_port_dst", 16, False, 0, 0),
    ("udp", "src_port"): MfField("udp_port_src", 16, False, 0, 0),
    ("udp", "dst_port"): MfField("udp_port_dst", 16, False, 0, 0),
    ("metadata", "tag"): MfField("tag", 32, True, 0, 32),
    ("metadata", "mark"): MfField("mark", 32, False, 0, 32),
}
# Masks a rule may use, by group type (engines in nfb_flow_init(), DPDK drivers/net/nfb/flow/nfb_flow.c):
#   exact  nfb_exact_mask_is_valid() in nfb_flow_exact.c
#   lpm    nfb_lpm_mask_is_contiguous() in nfb_flow_lpm.c
GROUP_MASKS = {"tcam": "any", "lutcam": "any", "exact": "full", "lpm": "prefix"}
# MF_VALUES window, mf_value_width of the snic_groups node (mf_value_width_read() in nfb_flow_mf.c):
# whole 32-bit registers up to 1024 bits, otherwise the driver loads no unit
MF_REG_BITS = 32
MF_WINDOW_BITS_MAX = 1024
# Keys which are not rte_flow pattern items, driver fills them itself (nfb_flow_offload.c):
#   metadata.port_num  ingress ETH port of the rule, fill_implicit_port_num_key()
#   metadata.tagged    set together with tag item, process_item_tag()
IMPLICIT_KEYS = {("metadata", "port_num"), ("metadata", "tagged")}
# fdt.parse_dts() does not support "/bits/ N <...>" values, these properties are not needed here
DTS_BITS_PROP_RE = re.compile(r"^[^=\n]*=\s*/bits/[^;]*;[ \t]*\n", re.M)


def prop_list(node, name):
    """Return string-list or cell-list property as a list, [] if missing."""
    prop = node.get_property(name)
    return list(getattr(prop, "data", []))


def prop_value(node, name, default="?"):
    """Return the first value of property, default if missing."""
    values = prop_list(node, name)
    return values[0] if values else default


def unique(items):
    """Return items without duplicates, in order of the first occurrence."""
    seen = []
    for item in items:
        if item not in seen:
            seen.append(item)
    return seen


def bits(offset, width, total):
    """Format bit range as 'msb:lsb' (bit 0 is the LSB) of a total-bit number.

    offset counts from the MSB, as (o, w) in the device tree and dst_offset in rte_flow.
    """
    if offset is None or total is None:
        return "?"
    return f"{total - 1 - offset}:{total - offset - width}"


def print_table(headers, rows, right=()):
    """Print rows as an aligned table, columns with index in right are right-aligned."""
    rows = [[str(cell) for cell in row] for row in rows]
    widths = [max(len(row[i]) for row in [headers] + rows) for i in range(len(headers))]

    def fmt(cells):
        return "  ".join(c.rjust(w) if i in right else c.ljust(w)
                         for i, (c, w) in enumerate(zip(cells, widths))).rstrip()

    print(" " + fmt(headers))
    print(" " + fmt(["-" * w for w in widths]))
    for row in rows:
        print(" " + fmt(row))


def parse_key(raw):
    """Parse one 'keys' string, unknown format gives Key with proto None."""
    m = KEY_PROTO_RE.match(raw)
    if m:
        proto, level, index, field, msb, lsb = m.groups()
        if (level, index) != ("0", "0"):
            proto += f"[{level}][{index}]"
        if field is None:
            return Key(proto, None, None, None, raw)
        return Key(proto, field, int(msb), int(lsb), raw)

    m = KEY_META_RE.match(raw)
    if m:
        field, index, msb, lsb = m.groups()
        # metadata.tag[N] is matched by rte_flow item tag (data with index N)
        if field == "tag":
            return Key("tag", f"data, index {index}", int(msb), int(lsb), raw)
        if index is not None:
            field += f"[{index}]"
        return Key("metadata", field, int(msb), int(lsb), raw)

    return Key(None, None, None, None, raw)


def parse_operand(raw):
    """Split modify_field operand to (name, offset, width), offset and width are None if unknown."""
    m = MF_OPERAND_RE.match(raw)
    if not m:
        return raw, None, None
    return m.group(1), int(m.group(2)), int(m.group(3))


def mf_key(name):
    """Split a DT field name to (proto, field, DT level, index), None if malformed.

    A metadata field has no level, its index is 0 if it has none.
    """
    m = MF_FIELD_RE.match(name)
    if m:
        proto, level, index, field = m.groups()
        return proto, field, int(level), int(index)
    m = MF_META_RE.match(name)
    if m:
        field, index = m.groups()
        return "metadata", field, None, int(index or 0)
    return None


def mf_field_width(name):
    """Return the width of the DT key of a field, None if unknown."""
    key = mf_key(name)
    spec = key and MF_FIELDS.get(key[:2])
    return spec.width if spec else None


def mf_rte_field(name, offset, width):
    """Return the slice (offset, width) of a DT field as RteField, None if rte_flow cannot name it.

    As mf_field_from_key() in nfb_flow_mf.c. DT level 0 is the outermost header, rte_flow
    level 0 or 1; inner DT level L is rte_flow level L + 1. A metadata offset counts from
    the LSB, a packet field offset from the rte_flow field start, past the pad.
    """
    key = mf_key(name)
    spec = key and MF_FIELDS.get(key[:2])
    if not spec or offset is None:
        return None
    _, _, level, index = key
    # The driver skips a further header of its kind in a level ([L][I] with I > 0)
    if not spec.indexed and index != 0:
        return None
    rte_index = index if spec.indexed else None
    if spec.host_bits:
        if offset + width > spec.host_bits:
            return None
        return RteField(spec.name, 0, spec.host_bits - offset - width, rte_index)
    if offset < spec.pad:
        return None
    return RteField(spec.name, 0 if level == 0 else level + 1, offset - spec.pad, rte_index)


def mf_src_bits(mf):
    """Return the bits of the source: the MF_VALUES window slot, the constant or the field."""
    if mf.src == "value":
        return bits(mf.src_off, mf.src_w, mf.win)
    if MF_CONST_RE.match(mf.src):
        return bits(mf.src_off, mf.src_w, mf.src_w)
    return bits(mf.src_off, mf.src_w, mf_field_width(mf.src))


def mf_source(mf):
    """Return the source as given in testpmd modify_field action."""
    if mf.src == "value":
        return "value"
    m = MF_CONST_RE.match(mf.src)
    if m:
        return f"value = {m.group(1)}"
    field = mf_rte_field(mf.src, mf.src_off, mf.src_w)
    if field is None:
        return mf.src
    index = "" if field.index is None else f" src_tag_index {field.index}"
    return f"{field.name}{index} src_level {field.level} src_offset {field.offset}"


def mf_window(groups_node):
    """Return mf_value_width of the snic_groups node, None if the driver loads no unit with it."""
    width = prop_value(groups_node, "mf_value_width", None)
    if not isinstance(width, int) or width == 0 or width % MF_REG_BITS or width > MF_WINDOW_BITS_MAX:
        return None
    return width


def parse_mf_actions(group_node, win):
    """Return modify_field units (mf_action_N subnodes) of the group sorted by N."""
    mf = []
    for node in group_node.nodes:
        m = MF_NODE_RE.match(node.name)
        if not m:
            continue
        dst = parse_operand(prop_value(node, "dst"))
        src = parse_operand(prop_value(node, "src"))
        mf.append(MfAction(int(m.group(1)), prop_value(node, "oper"), *dst, *src,
                           prop_value(node, "valid"), win))
    return tuple(sorted(mf))


def parse_group(index, node, win):
    """Parse one group_N node, implicit keys are left out; win is the MF_VALUES window."""
    keys = tuple(k for k in map(parse_key, prop_list(node, "keys")) if (k.proto, k.field) not in IMPLICIT_KEYS)
    return Group(index, prop_value(node, "type"), prop_value(node, "capacity"), keys,
                 tuple(prop_list(node, "actions")), parse_mf_actions(node, win))


def find_compatible(node, compatible):
    """Return nodes with given compatible string in DT document order.

    The order is the same as in libfdt, so the list index matches the index used by nfb_comp_find().
    """
    found = [node] if compatible in prop_list(node, "compatible") else []
    for subnode in node.nodes:
        found += find_compatible(subnode, compatible)
    return found


def load_fdt(args):
    """Load the device tree from DTS/DTB file or from NFB device."""
    if args.dts or args.dtb:
        import fdt
        if args.dtb:
            with open(args.dtb, "rb") as f:
                return fdt.parse_dtb(f.read())
        with open(args.dts) as f:
            return fdt.parse_dts(DTS_BITS_PROP_RE.sub("", f.read()))

    import nfb
    return nfb.open(args.device).fdt


def load_group_sets(tree):
    """Return group sets with their ETH ports, sets with identical groups are merged."""
    eth_streams = [prop_value(node, "rx_stream", None) for node in find_compatible(tree.root, ETH_COMPATIBLE)]

    group_sets = []
    for set_index, groups_node in enumerate(find_compatible(tree.root, GROUPS_COMPATIBLE)):
        # Same as nfb_flow_init(): ETH port uses the snic_groups node whose index equals its rx_stream
        ports = [port for port, stream in enumerate(eth_streams) if stream == set_index]
        # rte_flow group ID is the order of the subnode, not the number in its name
        win = mf_window(groups_node)
        groups = tuple(parse_group(i, node, win) for i, node in enumerate(groups_node.nodes))

        for group_set in group_sets:
            if group_set.groups == groups:
                group_set.names.append(groups_node.name)
                group_set.eth_ports.extend(ports)
                break
        else:
            group_sets.append(GroupSet([groups_node.name], ports, groups))

    return group_sets


def set_title(group_set):
    """Return heading of the group set."""
    if group_set.eth_ports:
        return "ETH " + ", ".join(str(port) for port in sorted(group_set.eth_ports))
    return ", ".join(group_set.names) + " (no ETH port)"


def cmd_summary(group_set, groups, args):
    """Print one row per group with type, capacity, jump range, protocols, number of actions
    (modify_field included) and number of modify_field units."""
    last = len(group_set.groups) - 1
    rows = []
    for g in groups:
        # Driver allows jump only to higher groups (jump_min/jump_max in nfb_flow_init())
        if "jump" in g.actions and g.index < last:
            jump = str(last) if g.index + 1 == last else f"{g.index + 1}-{last}"
        else:
            jump = "-"
        protos = " ".join(unique(k.proto or "?" for k in g.keys))
        mf_units = len(mf_units_of(g)) if mf_units_of(g) or "modify_field" in g.actions else "-"
        rows.append([g.index, g.type, g.capacity, jump, protos, len(group_actions(g)), mf_units])

    print_table(["Grp", "Type", "Capacity", "Jump to", "Patterns", "Actions", "MF units"], rows, right={0, 2, 5, 6})


def cmd_patterns(group_set, groups, args):
    """Print matrix of pattern items (protocol, field) x groups, with the masks a rule may use."""
    fields = {}  # proto -> fields in order of the first occurrence, None means presence
    cells = {}   # (proto, field) -> {group index: cell}
    for g in groups:
        # Exact group needs the full mask of every field, presence only is a zero mask of them
        field_protos = {k.proto for k in g.keys if k.field is not None} if g.type == "exact" else set()
        for k in g.keys:
            if k.proto is None:
                proto, field = k.raw, "(unparsed)"
            else:
                proto, field = k.proto, k.field
            fields.setdefault(proto, [])
            if field not in fields[proto]:
                fields[proto].append(field)
            if field is None and proto in field_protos:
                continue
            cells.setdefault((proto, field), {})[g.index] = "x" if k.msb is None else f"{k.msb}:{k.lsb}"

    rows = [["", "(mask)"] + [GROUP_MASKS.get(g.type, "?") for g in groups],
            # Rule with no item has a zero mask of every key, exact group refuses it. LPM group
            # accepts it: its key starts with the implicit metadata.port_num, which the driver
            # always fills with the full mask, so the mask is a nonzero prefix, not the /0
            # refused by nfb_lpm_mask_is_contiguous()
            ["", "(match all)"] + ["." if g.type == "exact" and g.keys else "x" for g in groups]]
    for proto, proto_fields in fields.items():
        # Presence row goes first, protocol name is printed only on the first row
        proto_fields.sort(key=lambda field: field is not None)
        for i, field in enumerate(proto_fields):
            row_cells = cells.get((proto, field), {})
            rows.append([proto if i == 0 else "", "(presence)" if field is None else field]
                        + [row_cells.get(g.index, ".") for g in groups])

    print_table(["Item", "Field"] + [f"G{g.index}" for g in groups], rows)


def cmd_actions(group_set, groups, args):
    """Print matrix of actions x groups, the number of modify_field units is printed below."""
    names = unique(a for g in groups for a in group_actions(g))
    rows = [[name] + ["x" if name in group_actions(g) else "." for g in groups] for name in names]
    print_table(["Action"] + [f"G{g.index}" for g in groups], rows)

    mf_counts = [f"G{g.index}: {len(mf_units_of(g))}" for g in groups if mf_units_of(g) or "modify_field" in g.actions]
    if mf_counts:
        print()
        print(" modify_field units: " + ", ".join(mf_counts) + " (see modify-field)")


def mf_rule_row(mf):
    """Return the unit as the parameters of testpmd modify_field action."""
    field = mf_rte_field(mf.dst, mf.dst_off, mf.dst_w)
    if field is None:
        dst, level, offset = mf.dst, "?", "?" if mf.dst_off is None else mf.dst_off
    else:
        index = "" if field.index is None else f" dst_tag_index {field.index}"
        dst, level, offset = field.name + index, field.level, field.offset
    return [mf.num, mf.oper, dst, level, offset, "?" if mf.dst_w is None else mf.dst_w, mf_source(mf)]


def mf_debug_row(mf):
    """Return the unit as in the device tree, with offsets converted to 'msb:lsb'."""
    return [mf.num, mf.oper, mf.dst, bits(mf.dst_off, mf.dst_w, mf_field_width(mf.dst)),
            mf.src, mf_src_bits(mf), mf.valid]


def mf_is_mac_swap(mf):
    """Whether the unit swaps the whole outermost MAC addresses, the one mac_swap uses (nfb_flow_mf_mac_swap())."""
    return mf.oper == "swap" and {(mf.dst, mf.dst_off, mf.dst_w), (mf.src, mf.src_off, mf.src_w)} == \
        {("eth[0][0].dst_addr", 0, 48), ("eth[0][0].src_addr", 0, 48)}


def mf_units_of(group):
    """Return the units serving modify_field actions, a swap unit serves none (mf_parse_unit())."""
    return [mf for mf in group.mf if mf.oper != "swap"]


def group_actions(group):
    """Return the actions of the group, with mac_swap if a swap unit the driver loads serves it."""
    conflicts, _ = mf_shared_flags(group.mf)
    if any(mf_is_mac_swap(mf) and mf.win is not None and mf.valid not in conflicts for mf in group.mf):
        return group.actions + ("mac_swap",)
    return group.actions


def mf_swap_note(mf):
    """Return the note on a swap unit for --debug."""
    if mf_is_mac_swap(mf):
        return f"Unit {mf.num} (swap) serves the mac_swap action."
    return f"Unit {mf.num} (swap) serves no action, only mac_swap of the whole outermost MAC addresses is supported."


def mf_shared_flags(units):
    """Check valid bits shared by several units, as nfb_flow_mf_load() does.

    Every unit needs a valid bit of its own: setting a bit performs every unit which has
    it, so the driver loads none of the units of a shared bit.
    Return (set of conflicting valid bits, notes on them to print below the table).
    """
    users = {}  # valid bit -> [unit]
    for mf in units:
        # Units the driver cannot load do not use their bit
        if mf_rte_field(mf.dst, mf.dst_off, mf.dst_w) is not None:
            users.setdefault(mf.valid, []).append(mf)

    conflicts, notes = set(), []
    for valid, bit_users in sorted(users.items()):
        if len(bit_users) > 1:
            conflicts.add(valid)
            notes.append(f"Valid {valid}!: units {', '.join(str(mf.num) for mf in bit_users)} share the bit; "
                         "the driver does not load them.")
    return conflicts, notes


def cmd_modify_field(group_set, groups, args):
    """Print table of modify_field units for each group which has any, --debug adds swap units."""
    mf_groups = [g for g in groups if (g.mf if args.debug else mf_units_of(g))]
    if not mf_groups:
        print(" No modify_field units.")

    for i, g in enumerate(mf_groups):
        if i:
            print()
        print(f" Group {g.index} ({g.type})")
        notes = []
        if g.mf[0].win is None:
            notes.append("mf_value_width of the groups is missing or invalid; the driver loads no unit.")
        if not args.debug:
            print_table(["#", "Op", "dst_type", "dst_level", "dst_offset", "width", "Source"],
                        [mf_rule_row(mf) for mf in mf_units_of(g)], right={0, 3, 4, 5})
            print_notes(notes)
            continue

        conflicts, shared_notes = mf_shared_flags(g.mf)
        notes += [mf_swap_note(mf) for mf in g.mf if mf.oper == "swap"] + shared_notes
        rows = [mf_debug_row(mf) for mf in g.mf]
        # Valid is the last column
        for mf, row in zip(g.mf, rows):
            if mf.valid in conflicts:
                row[-1] = f"{mf.valid}!"
        print_table(["#", "Op", "Destination", "Dst bits", "Source", "Src bits", "Valid"], rows, right={0, 6})
        print_notes(notes)


def print_notes(notes):
    """Print notes below a table, separated by an empty line."""
    if notes:
        print()
        for note in notes:
            print(" " + note)


def main():
    # Parse options
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("-d", "--device", type=str, help="Path to NFB device. Default: %(default)s", default="/dev/nfb0")
    source.add_argument("--dts", type=str, metavar="FILE", help="Read device tree from DTS file.")
    source.add_argument("--dtb", type=str, metavar="FILE", help="Read device tree from DTB file.")
    parser.add_argument("-p", "--port", type=int, help="Show only groups of this ETH port.")
    parser.add_argument("-g", "--group", type=int, action="append", help="Show only this group, can be repeated.")
    parser.add_argument("--debug", action="store_true",
                        help="modify-field: show units as in the device tree, with bits of fields and of the MF_VALUES window, and valid bits.")
    parser.set_defaults(func=cmd_summary)

    # -g is accepted also after COMMAND, SUPPRESS keeps the value given before COMMAND
    group_opt = argparse.ArgumentParser(add_help=False)
    group_opt.add_argument("-g", "--group", type=int, action="append", default=argparse.SUPPRESS,
                           help="Show only this group, can be repeated.")
    commands = parser.add_subparsers(metavar="COMMAND")
    commands.add_parser("summary", parents=[group_opt], help="Overview of all groups (default).") \
        .set_defaults(func=cmd_summary)
    commands.add_parser("patterns", parents=[group_opt], help="Matrix of pattern items x groups.") \
        .set_defaults(func=cmd_patterns)
    commands.add_parser("actions", parents=[group_opt], help="Matrix of actions x groups.") \
        .set_defaults(func=cmd_actions)
    mf_parser = commands.add_parser("modify-field", aliases=["mf"], parents=[group_opt],
                                    help="modify_field units of groups.")
    mf_parser.add_argument("--debug", action="store_true", default=argparse.SUPPRESS,
                           help="Show units as in the device tree, with bits of fields and of the MF_VALUES window, and valid bits.")
    mf_parser.set_defaults(func=cmd_modify_field)

    args = parser.parse_args()

    try:
        tree = load_fdt(args)
    except Exception as exc:
        sys.exit(f"Error: cannot read device tree: {exc}")

    group_sets = load_group_sets(tree)
    if not group_sets:
        sys.exit(f"Error: no rte_flow groups ({GROUPS_COMPATIBLE}) found in device tree")

    if args.port is not None:
        group_sets = [s for s in group_sets if args.port in s.eth_ports]
        if not group_sets:
            sys.exit(f"Error: ETH port {args.port} has no rte_flow groups")

    group_filter = args.group
    for i, group_set in enumerate(group_sets):
        if i:
            print()
        print(set_title(group_set))
        groups = [g for g in group_set.groups if group_filter is None or g.index in group_filter]
        if groups:
            args.func(group_set, groups, args)
        else:
            print(" No such group.")

    if args.func is cmd_summary:
        print("\nDetails: flow_caps.py [patterns | actions | modify-field] [-g GROUP]")


if __name__ == "__main__":
    main()
