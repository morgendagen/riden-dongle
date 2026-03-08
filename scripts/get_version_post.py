# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT

Import("env", "projenv")

# Get the version number from the build environment.
firmware_version = env.get('FIRMWARE_VERSION', "")

# Append the version to the build defines so it gets baked into the firmware
projenv.Append(CPPDEFINES=[
    f'BUILD_VERSION={firmware_version}',
])

