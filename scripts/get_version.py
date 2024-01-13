# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT

Import("env")
import os

# Get the version number from the build environment.
firmware_version = os.environ.get('VERSION', "")

# Clean up the version number
if firmware_version == "":
    # When no version is specified default to "0.0.1"
    firmware_version = "0.0.1"

print(f'Using version {firmware_version} for the build')

# Append the version to the build defines so it gets baked into the firmware
env.Append(CPPDEFINES=[
    f'BUILD_VERSION={firmware_version}',
])

# Set the output filename to the name of the board and the version
env.Replace(PROGNAME=f'firmware_{env["PIOENV"]}_{firmware_version.replace(".", "_")}')
