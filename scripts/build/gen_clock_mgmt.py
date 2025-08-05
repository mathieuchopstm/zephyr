#!/usr/bin/env python3
#
# Copyright 2025 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0
"""

"""

import argparse
import logging
import os
import pickle
import sys
import re

# This is needed to re-use code from gen_defines
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "dts"))
from gen_defines import node_z_path_id, str2ident

# This is needed to load edt.pickle files.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "dts", "python-devicetree", "src"))
from devicetree.edtlib import EDT, Node, EDTError

CONFIGURE_CELLS_PROPERTY_NAME = "#clock-cells"
STATIC_INIT_PROPERTY_NAME = "zephyr,cms-static-init"

ORDERED_INIT_NODE_PATH = "/zephyr,cms-clock-tree-root/initialization-sequence";
ORDERED_INIT_PROPERTY_NAME = "nodes"

class CMSCodeGenerator:
    def _parse_args():
        parser = argparse.ArgumentParser(allow_abbrev=False)
        parser.add_argument("--edt-pickle", required=True,
                            help="path to read pickled edtlib.EDT object from")
        parser.add_argument("--header-out", required=True,
                            help="path to write header to")
        parser.add_argument("--init-code-out", required=True,
                            help="path to write clock static init code to")
        parser.add_argument("--clock-tree-root", required=True,
                            help="path to clock tree root in Device Tree")
        parser.add_argument("--subsystem-owned-compatible",
                            action='append', default=[],
                            help="compatible of subsystem-owned nodes that should be ignored")
        return parser.parse_args()

    def _err(self, msg, *args, **kwargs):
        self.log.error(msg, *args, **kwargs)
        throw RuntimeError(msg)

    def __init__(self):
        self.log = logging.getLogger(__file__)
        # log.addHandler(logging.StreamHandler())

        args = CMSCodeGenerator._parse_args()

        with open(args.edt_pickle, "rb") as f:
            self.edt: EDT = pickle.load(f)

        try:
            self.ct_root = self.edt.get_node(args.clock_tree_root)
        except EDTError:
            self._err(f"Clock tree root node ({args.clock_tree_root}) does not exist!")

        # Flatten the DT clock tree to a dependency order-sorted array
        # The root node itself is not included as it is a mere container.
        # Any node with a compatible matching one of the subsystem-owned
        # compatibles (as specified by script arguments) are also excluded.
        def _flatten_tree(node: Node, include_self=True):
            tree = [node] if include_self else []
            if len(node.children) == 0:
                return tree

            for child in node.children.values():
                tree.extend(CMSCodeGenerator._flatten_tree(child))
            return tree
        def _valid_node(node: Node):
            # A node is valid (read: interesting to us) if:
            #  - it is not owned by the subsystem
            #  - it has a compatible
            #
            #

            # O(n²) but n should remain reasonably small
            # Check only matching_compat to reduce to O(n)
            subsystem_owned = any([subsys_compat == node_compat
                 for subsys_compat in args.subsystem_owned_compatible
                 for node_compat in node.compats])
        fct = _flatten_tree(self.ct_root, include_self=False)
        fct = list(filter(_not_subsystem_owned, fct))
        self.flattened_clock_tree = sorted(fct, key=lambda n: n.dep_ordinal)

        self.nodes_by_compatibles = []




def main():
    return 0

if __name__ == "__main__":
    sys.exit(main())
