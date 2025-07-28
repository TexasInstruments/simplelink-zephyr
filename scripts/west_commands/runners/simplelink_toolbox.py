# Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for simplelink-wifi-toolbox.'''

import os
import subprocess
from pathlib import Path
from runners.core import ZephyrBinaryRunner


class SimpleLinkBinaryRunner(ZephyrBinaryRunner):
    '''Runner front-end for simplelink-wifi-toolbox.'''

    def __init__(self, cfg, simplelink_tool):
        super().__init__(cfg)

        self.simplelink_tool = simplelink_tool
        self.vendor_file = (Path(cfg.build_dir) / 'zephyr' / 'flash' / 'vendor_image.sign.bin').resolve()
        self.tool_setting_file = (Path(cfg.board_dir) / 'config' / 'tool_settings.json').resolve()
        self.programming_report_file = (Path(self.cfg.build_dir) / 'simplelink_toolbox_report.txt').resolve()

    @classmethod
    def name(cls):
        return 'simplelink_toolbox'

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--simplelink-tool', default='simplelink-wifi-toolbox',
                            help='path to simplelink tool, default is simplelink-wifi-toolbox')

    @classmethod
    def do_create(cls, cfg, args):
        return SimpleLinkBinaryRunner(cfg, simplelink_tool=args.simplelink_tool)

    def do_run(self, command):
        self.require(self.simplelink_tool)
        if command == 'flash':
            self.flash()

    def flash(self):
        if self.vendor_file is not None and os.path.isfile(self.vendor_file):
            fname = self.vendor_file
        else:
            raise ValueError(
                f'Cannot flash; no vendor ({self.vendor_file}) file found. ')

        cmd = [
            str(self.simplelink_tool),
            'programmer',
            '-i', 'XDS110',
            '-param1', 'auto',
            'programming',
            '--tool_settings', str(self.tool_setting_file),
            '--report_file_name_path', str(self.programming_report_file)
        ]

        self.logger.info(f'Flashing file: {fname}')

        try:
            self.check_output(cmd, cwd=Path(self.cfg.build_dir).resolve())
            self.logger.info('Success')
        except subprocess.CalledProcessError as grepexc:
            self.logger.error(f"Failure {grepexc.returncode}")
