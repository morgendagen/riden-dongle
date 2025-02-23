import argparse
import pyvisa
import time
import datetime


def test_query(inst, m: str, write_delay_ms: int = 0):
    start_query = datetime.datetime.now()
    if m.endswith("?"):
        print(f"Query \"{m}\" reply: ", end='')
        try:
            r = inst.query(m).strip()
        except Exception as e:
            print(f"\nError on query: {e}")
            return False
        print(f"\"{r}\"", end='')
    else:
        print(f"Write \"{m}\"", end='')
        try:
            inst.write(m)
            # inst.flush(pyvisa.highlevel.constants.BufferOperation.flush_write_buffer)
        except Exception as e:
            print(f"\nError on write: {e}")
            return False                        
    delta_time = datetime.datetime.now() - start_query
    print(f", taking {delta_time.total_seconds() * 1000:.1f} ms.")
    if not m.endswith("?") and write_delay_ms > 0:
        time.sleep(write_delay_ms / 1000)
    
    return True


def test_device(port: str, repeat_query: int, timeout: int):
    rm = pyvisa.ResourceManager()
    start_connect = datetime.datetime.now()
    print(f"Connecting to '{port}'", end='')
    try:
        inst = rm.open_resource(port, timeout=timeout)
    except Exception as e:
        print(f"\nError on connect: {e}")
        return False
    delta_time = datetime.datetime.now() - start_connect
    print(f" succeeded, taking {delta_time.total_seconds() * 1000:.1f} ms.")
    write_delay_ms = 0
    if port.endswith("::SOCKET"):
        inst.read_termination = "\n"
        inst.write_termination = "\n"
        # write_delay_ms = 150  # for socket connections, a delay is needed between writes sometimes
    msgs = ["*IDN?"]
    msgs = ["VOLT 1", "VOLT 2", "VOLT 3", "VOLT 4", "VOLT 5", "VOLT 6", "VOLT 7"]
    if repeat_query > 0:
        print(f"Repeating query tests for {repeat_query} seconds...")
        start = time.time()
        while time.time() - start < repeat_query:
            for m in msgs:
                if not test_query(inst, m, write_delay_ms):
                    inst.close()
                    return False
    else:
        for m in msgs:
            if not test_query(inst, m, write_delay_ms):
                inst.close()
                return False
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Test simple SCPI communication via VISA.",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("port", type=str, nargs='?', default=None, help="The port to use for tests. Must be a Visa compatible connection string.")
    parser.add_argument("-n", action="store_true", default=False, help="No SCPI device discovery preceding the test. Will be ignored when port is not defined.")
    parser.add_argument("-c", type=int, default=0, help="Test repeated connect calls for N seconds. If not specified: will do 1 connect. Will be ignored when port is not defined.")
    parser.add_argument("-q", type=int, default=0, help="After a connect, test repeated query calls for N seconds. If not specified: will do 1 call. Will be ignored when port is not defined.")
    parser.add_argument("-t", type=int, default=10000, help="Timeout for any VISA operation in milliseconds for the repeated tests (-q, -c).")
    args = parser.parse_args()
        
    rm = pyvisa.ResourceManager()
    repeat_query = args.q
    repeat_connect = args.c
    test_timeout = args.t
    skip_scan = args.n
    if not args.port:
        skip_scan = False
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
            inst = None
            try:
                inst = rm.open_resource(m, timeout=test_timeout)
            except:
                print(f"Cannot connect to device on address \"{m}\"")
                continue
            try:
                r = inst.query("*IDN?").strip()
                inst.close()
                print(f"Found \"{r}\" on address \"{m}\"")
            except:
                print(f"Found unknown device on address \"{m}\"")
            inst.close()    
    else:
        print("No scan for VISA resources.")
    if args.port:
        if repeat_connect:
            print(f"Repeating tests for {repeat_connect} seconds...")
            start = time.time()
            while time.time() - start < repeat_connect:
                test_device(args.port, repeat_query, test_timeout)            
        else:
            test_device(args.port, repeat_query, test_timeout)

    print("Done.")
