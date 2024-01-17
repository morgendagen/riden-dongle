-- SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
--
-- SPDX-License-Identifier: MIT

-- lxi-tools lua script
-- Example: Test how many queries can be handled per second

-- no localization of numbers, please
os.setlocale("en", "numeric")

-- hostname = "RD6006-00000000.local"
hostname = os.getenv("RIDEN_HOSTNAME")
if hostname == nil then
   print("You must specify Riden hostname through environment variable 'RIDEN_HOSTNAME'.")
   os.exit(-1)
end

print("Connecting to " .. hostname)

-- Connect to power supply
psu = lxi_connect(hostname, 5025, nil, 2000, "RAW")
if psu == -1 then
   print("Failed connecting, exiting.")
   os.exit(-1)
end
print("Power Supply ID = " .. lxi_scpi(psu,"*IDN?"))

-- Wait for power supply and dongle to stabilize
lxi_msleep(1000)

clock0 = lxi_clock_new()
nof_requests = 0

-- Read set voltage for 30 seconds
print("Starting loop")
clock = lxi_clock_read(clock0)
while (nof_requests < 300)
do
   measured_voltage = lxi_scpi(psu, "VOLT?")
   nof_requests = nof_requests + 1
end
clock = lxi_clock_read(clock0)

print("queries/second = " .. nof_requests/clock)

-- Cleanup
lxi_clock_free(clock0)

-- Turn off power supply
lxi_scpi(psu, "output off")

lxi_disconnect(psu)
-- Finish
print("Done")
