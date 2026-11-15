#!/usr/bin/env python3
"""Exercise actual update/reboot handlers and main loop without flashing a device."""
import argparse
import pathlib
import subprocess
import tempfile

root=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--reproduce-upstream',action='store_true')
args=parser.parse_args()
def source(path):
    if args.reproduce_upstream:
        return subprocess.check_output(['git','show','e9a060958c9dcf82f24353e36c2282b057690be4:'+path],cwd=root,text=True)
    return (root/path).read_text()
http=source('src/riden_http_server/riden_http_server.cpp')
update=http[http.index('void RidenHttpServer::handle_firmware_update_post()'):
            http.index('\nvoid RidenHttpServer::handle_disconnect_client_post()')]
reboot=http[http.index('void RidenHttpServer::handle_reboot_dongle_get()'):
            http.index('\nvoid RidenHttpServer::handle_control_get(')]
main=source('src/main.cpp')
loop=main[main.index('void loop()'):main.index('\nvoid tick()')]
if args.reproduce_upstream:
    # Older code has no deferred-restart API. The empty POST assertion below
    # fails before these inert compatibility methods can affect the result.
    update+='\nvoid RidenHttpServer::request_restart() {}\nbool RidenHttpServer::handle_pending_restart() { return false; }\n'
with tempfile.TemporaryDirectory(prefix='riden-update-') as tmp:
    tmp=pathlib.Path(tmp)
    source=tmp/'http_update.cpp'
    source.write_text((root/'test/host/http_update.cpp').read_text().replace(
        '// PRODUCTION_UPDATE_HANDLERS',update+'\n'+reboot).replace('// PRODUCTION_MAIN_LOOP',loop))
    binary=tmp/'http_update'
    subprocess.run(['g++','-std=gnu++17','-g','-O1','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(root/'test/host/include'),
                    '-I'+str(root/'include'),'-I'+str(root/'src/riden_http_server'),
                    str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
subprocess.run(['node',str(root/'test/host/reboot_countdown.js')],check=True,cwd=root)
