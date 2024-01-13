# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT

Import("env")
from datetime import datetime, timezone

# get current datetime
now = datetime.now(timezone.utc)
build_time = now.astimezone().isoformat(timespec='seconds')

# Append the timestamp to the build defines so it gets baked into the firmware
env.Append(CPPDEFINES=[
    f'BUILD_TIME={build_time}',
])
