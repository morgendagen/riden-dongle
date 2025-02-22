import argparse
import pyvisa

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Test simple SCPI communication via VISA.",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("port", type=str, nargs='?', default=None, help="The port to use. Must be a Visa compatible connection string.")
    parser.add_argument("-n", action="store_true", default=False, help="No scan for test SCPI devices. Will be ignored when port is not defined.")
    parser.add_argument("-t", action="store_true", default=False, help="Test repeated calls for 10 seconds. Will be ignored when port is not defined.")
    args = parser.parse_args()
        
    rm = pyvisa.ResourceManager()
    repeat_tests = args.t
    skip_scan = args.n
    if not args.port:
        skip_scan = False
        repeat_tests = False
    if not skip_scan:
        print("Scanning for VISA resources...")
        print("VISA Resources found: ", end='')
        resources = rm.list_resources(query="?*")
        print(resources)
        for m in resources:            
            if m.startswith("ASRL/dev/cu."):
                # some of these devices are not SCPI compatible, like "ASRL/dev/cu.Bluetooth-Incoming-Port::INSTR"
                print(f"Skipping serial port \"{m}\"")
                continue
            try:
                inst = rm.open_resource(m, timeout=1000)
                r = inst.query("*IDN?").strip()
                print(f"Found \"{r}\" on address \"{m}\"")
            except:
                print(f"Found unknown device on address \"{m}\"")
    else:
        print("No scan for VISA resources.")
    if args.port:
        print(f"Connecting to '{args.port}'")
        inst = rm.open_resource(args.port, timeout=2000)
        if args.port.endswith("::SOCKET"):
            inst.read_termination = "\n"
            inst.write_termination = "\n"
        print("Connected.")
        msgs = ["*IDN?"]
        if repeat_tests:
            print("Repeating tests for 10 seconds...")
            import time
            start = time.time()
            while time.time() - start < 10:
                for m in msgs:
                    if m.endswith("?"):
                        print(f"Query \"{m}\" reply: ", end='')
                        r = inst.query(m).strip()
                        print(f"\"{r}\"")
                    else:
                        print(f"Write \"{m}\"")
                        inst.write(m)
        else:
            for m in msgs:
                if m.endswith("?"):
                    print(f"Query \"{m}\" reply: ", end='')
                    r = inst.query(m).strip()
                    print(f"\"{r}\"")
                else:
                    print(f"Write \"{m}\"")
                    inst.write(m)
        inst.close()
    print("Done.")
