#!/usr/bin/env python3
"""Exercise production setpoint handlers/helpers with simulated HTTP/register I/O."""
import argparse
import pathlib
import subprocess
import tempfile

root=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--reproduce-upstream',action='store_true')
args=parser.parse_args()
base='e9a060958c9dcf82f24353e36c2282b057690be4'

def source(path):
    if args.reproduce_upstream:
        return subprocess.check_output(['git','show',base+':'+path],cwd=root,text=True)
    return (root/path).read_text()

def function(text,name):
    start=text.index(name)
    return text[start:text.index('\n}',start)+2]

http=source('src/riden_http_server/riden_http_server.cpp')
bus=source('src/riden_modbus/riden_modbus.cpp')
functions=[function(http,'void RidenHttpServer::'+name+'()') for name in ['handle_set_v','handle_set_i','handle_set_i_range']]
functions += [function(bus,signature) for signature in [
    'bool RidenModbus::set_voltage_set(', 'bool RidenModbus::set_current_set(',
    'bool RidenModbus::set_current_range(', 'bool RidenModbus::write_voltage(',
    'bool RidenModbus::write_current(', 'uint16_t RidenModbus::voltage_to_value(',
    'uint16_t RidenModbus::current_to_value(']]
with tempfile.TemporaryDirectory(prefix='riden-setpoints-') as tmp:
    tmp=pathlib.Path(tmp)
    test=tmp/'http_setpoints.cpp'
    test.write_text((root/'test/host/http_setpoints.cpp').read_text().replace(
        '// PRODUCTION_SETPOINT_FUNCTIONS','\n\n'.join(functions)))
    binary=tmp/'http_setpoints'
    subprocess.run(['g++','-std=gnu++17','-g','-O1','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(root/'test/host/include'),
                    '-I'+str(root/'include'),str(test),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
