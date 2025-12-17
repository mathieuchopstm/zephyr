# Copyright (c) 2025 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0
'''
Common code shared by the STMicroelectronics runners
(stlink_gdbserver, stm32cubeprogrammer)
'''

import json
import re
import shutil
import subprocess

from pathlib import Path

class CubeToolInfo:
    @staticmethod
    def parse_version_string(version_str: str) -> tuple[int, int, int, str]:
        # Ignore leading '^' if present
        s = version_str.lstrip("^")

        # Parse version string: '0.0.0' or '0.0.0+<...>'
        if s.find("+") != -1:
            mmr, tag = s.split("+", maxsplit=1)
        else:
            mmr, tag = s, ""

        if mmr.count(".") != 2:
            raise ValueError(f"Version number '{version_str}' has invalid format")

        try:
            major, minor, patch = mmr.split(".")
            return int(major), int(minor), int(patch), tag
        except ValueError:
            raise ValueError(f"Version number '{version_str}' has invalid format")

    @staticmethod
    def make_version_number(maj, min, ptc) -> int:
        return (1000000 * maj + 1000 * min + ptc)

    class Bundle:
        @staticmethod
        def _get(d, key):
            '''
            Use only when schema requires property present.
            '''
            if not (e := d.get(key)):
                raise ValueError(f"Bundle info lacks required property `{key}`")
            return e

        def __init__(self, descriptor):
            self.path = Path(self._get(descriptor, "path"))

            details = self._get(descriptor, "details")
            self.name: str = self._get(details, "name")
            self.version_str: str = self._get(details, "version")

            (self.version_maj, self.version_min, self.version_ptc,
             self.version_tag) = CubeToolInfo.parse_version_string(self.version_str)

            self.commands: dict[str, Path] = {}

            # `commands` key is not always present
            if (commands := details.get("commands")):
                for c in commands:
                    cmd_name = self._get(c, "name")
                    cmd_exec = self._get(c, "exec")

                    assert cmd_exec.startswith("${CUBE_BUNDLE_ROOT}/"), \
                        "Command path should be in bundle?"
                    cmd_exe_path = self.path / cmd_exec.replace("${CUBE_BUNDLE_ROOT}/", "", 1)

                    self.commands[cmd_name] = cmd_exe_path
            pass

        def __lt__(self, other: 'CubeToolInfo.Bundle'):
            # Sort by version by default
            if self.version_num < other.version_num:
                return True
            elif self.version_num > other.version_num:
                return False
            else:
                return self.version_tag < other.version_tag

        @property
        def version_num(self):
            return CubeToolInfo.make_version_number(
                self.version_maj,
                self.version_min,
                self.version_ptc
            )

        @property
        def canonical_command(self):
            return self.commands.get(self.name)

    class UserBundle:
        '''
        Python-ified object of `user_bundles` array
        as returned by `cube bundle --project show`
        '''
        @staticmethod
        def _get(d, key):
            '''
            Use only when schema requires property present.
            '''
            if not (e := d.get(key)):
                raise ValueError(f"User bundle info lacks required property `{key}`")
            return e

        def __init__(self, obj: dict):
            self.name: str = self._get(obj, "name")
            self._ver_str: str = self._get(obj, "version")

            # Handle `^`-prefixed versions indicating
            # a higher version of the tool is allowed (???)
            maj, min, ptc, self.version_tag = CubeToolInfo.parse_version_string(self._ver_str)
            self.version_num = CubeToolInfo.make_version_number(maj, min, ptc)
            self.allow_higher_versions = (self._ver_str[0] == '^')

        def find_compatible_bundle(self, bundles: list['CubeToolInfo.Bundle']):
            # Search for exact version match first
            def _exact_match(b: 'CubeToolInfo.Bundle'):
                return (b.version_num == self.version_num and
                        b.version_tag == self.version_tag)
            def _higher_match(b: 'CubeToolInfo.Bundle'):
                return (b.version_num > self.version_num or
                        (b.version_num == self.version_num and
                         b.version_tag >= self.version_tag))
            match = list(filter(_exact_match, bundles))

            assert len(match) <= 1, "More than one exact match?!"
            if len(match) == 1:
                return match[0]

            # If higher versions are allowed, pick a higher version
            # as close as possible to the requested one
            match = list(filter(_higher_match, bundles))
            if len(match) > 0:
                # CubeToolInfo.Bundle sorts by ascending version
                return sorted(match)[0]

            return None

    @staticmethod
    def get_tool_info():
        cmd = shutil.which("cube")
        if not cmd:
            return None
        try:
            return CubeToolInfo(cmd)
        except IOError | ValueError:
            return None

    def get_bundle_latest(self, bundle_name: str):
        if (bl := self.bundles.get(bundle_name)):
            return bl[-1]
        return None

    def get_exe_path(self, bundle_name: str):
        '''
        Returns `exec` (path to executable) of the command
        named `bundle_name` inside bundle `bundle_name`.

        If a local project is available and pins a specific version
        of `bundle_name`, the pinned version of the bundle is used
        (or an error is raised if not installed). Otherwise, the
        latest installed version of the bundle is used.
        '''
        if (bundles_list := self.bundles.get(bundle_name)):
            if (pinned := self.pinned_bundles.get(bundle_name)):
                if (b := pinned.find_compatible_bundle(bundles_list)):
                    return b.canonical_command
            else:
                # Bundles lists are sorted by ascending version.
                # Return the latest version if none is pinned.
                return bundles_list[-1].canonical_command

        return None

    def __init__(self, cube_tool: str):
        self.bundles: dict[str, list[CubeToolInfo.Bundle]] = {}
        # Obtain list of bundles and search for `programmer`
        try:
            p = subprocess.run(
                [cube_tool, "--json", "bundle", "list"],
                capture_output=True, check=True, timeout=1000
            )
            bundles_json = p.stdout.decode()
        except subprocess.TimeoutExpired | subprocess.CalledProcessError:
            raise IOError("could not get bundles list")

        try:
            bundles_info = json.loads(bundles_json)
        except json.decoder.JSONDecodeError:
            raise ValueError("could not decode bundles list")

        for bi in bundles_info:
            b = CubeToolInfo.Bundle(bi)

            bundle_list = self.bundles.get(b.name, [])
            bundle_list.append(b)
            self.bundles[b.name] = sorted(bundle_list)

        # Obtain list of local/project bundles
        # (used to select a specific tool version)
        self.pinned_bundles: dict[str, CubeToolInfo.UserBundle] = {}
        try:
            p = subprocess.run(
                [cube_tool, "--json", "bundle", "show", "--project"],
                capture_output=True, check=True, timeout=1000
            )
            # When there's no project, a non-JSON (!) error message
            # is printed, then the tool exits with status code 0 (!!)
            s = p.stdout.decode()

            if s.strip() == "Can't find a project":
                pass
            else:
                prj_bundles_info = json.loads(s)
                if not (usr_bundles := prj_bundles_info.get("user_bundles")):
                    raise ValueError("no user bundles in project")

                for bundle in usr_bundles:
                    ub = CubeToolInfo.UserBundle(bundle)
                    self.pinned_bundles[ub.name] = ub
        except subprocess.TimeoutExpired | subprocess.CalledProcessError:
            raise IOError("could not get project bundles info")
