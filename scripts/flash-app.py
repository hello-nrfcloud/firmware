# Copyright (c) 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

'''flash-app.py

Flash just the signed app, leaving the bootloader and most importantly, the UICR, untouched.'''

from west.commands import WestCommand  # your extension must subclass this

class FlashApp(WestCommand):
	def __init__(self):
		super().__init__("flash-app", "Flash the app without bootloader", "")
	def do_add_parser(self, parser_adder):
		parser = parser_adder.add_parser(
			self.name,
			help=self.help,
			description=self.description)
		return parser
	def do_run(self, _args, _unknown_args):
		self.check_call(['west', 'build'])
		# cmd = ['nrfjprog', '--program', 'build/zephyr/app_signed.hex', '--sectorerase', '--verify', '--reset']
		cmd = ['nrf91_flasher', '-p', 'build/zephyr/app_signed.hex']
		self.check_call(cmd)
