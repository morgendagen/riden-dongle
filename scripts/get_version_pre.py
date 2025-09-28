# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT

Import("env")
import os

# Get the version number from the build environment.
firmware_version = os.environ.get('VERSION', "")

# Clean up the version number
if firmware_version == "":
    # When no version is specified default to "DEVELOPMENT"
    firmware_version = "DEVELOPMENT"

print(f'Using version {firmware_version} for the build')

env.Append(FIRMWARE_VERSION=f'{firmware_version}')

if firmware_version != "DEVELOPMENT":
    # Set the output filename to the name of the board and the version
    env.Replace(PROGNAME=f'firmware_{env["PIOENV"]}_{firmware_version.replace(".", "_")}')
