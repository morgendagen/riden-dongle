#include "vxi_server.h"
#include "rpc_enums.h"
#include "rpc_packets.h"

#include <ESP8266mDNS.h>
#include <SCPI_Parser.h>

VXI_Server::VXI_Server(SCPI_handler &scpi_handler)
    : vxi_port(rpc::VXI_PORT_START, rpc::VXI_PORT_END),
    scpi_handler(scpi_handler)
{
    /*  We do not start the tcp_server port here, because
        WiFi has likely not yet been initialized. Instead,
        we wait until the begin() command.  */
}

VXI_Server::~VXI_Server()
{
}

uint32_t VXI_Server::allocate()
{
    uint32_t port = 0;

    if (available()) {
        port = vxi_port; // This is a cyclic counter, not a simple integer
    }
    return port;
}

void VXI_Server::begin(bool bNext)
{
    if (bNext) {
        client.stop();
        tcp_server.stop();

        /*  Note that vxi_port is not an ordinary uint32_t. It is
            an instance of class cyclic_uint32_t, defined in utilities.h,
            which is constrained to a range of values. The increment
            operator will cause it to go to the next value, automatically
            going back to the starting value once it exceeds the maximum
            of its range.  */

        vxi_port++;
    }

    tcp_server.begin(vxi_port);

    LOG_F("Listening for VXI commands on TCP port %u\n", (uint32_t)vxi_port);
    if (rpc::VXI_PORT_START == rpc::VXI_PORT_END) {
        if (MDNS.isRunning()) {
            LOG_LN("VXI_Server advertising as vxi-11.");
            auto scpi_service = MDNS.addService(NULL, "vxi-11", "tcp", (uint32_t)vxi_port);
            MDNS.addServiceTxt(scpi_service, "version", SCPI_STD_VERSION_REVISION);
        }
    }
    // else: no mDNS, port changes too often
}

void VXI_Server::loop()
{
    if (client) // if a connection has been established on port
    {
        bool bClose = false;
        int len = get_vxi_packet(client);

        if (len > 0) {
            bClose = handle_packet();
        }

        if (bClose) {
            LOG_F("Closing VXI connection on port %u\n", (uint32_t)vxi_port);
            /*  this method will stop the client and the tcp_server, then rotate
                to the next port (within the specified range) and restart the
                tcp_server to listen on that port.  */
            begin_next();
        }
    } else // i.e., if ! client
    {
        client = tcp_server.accept(); // see if a client is available (data has been sent on port)

        if (client) {
            LOG_F("\nVXI connection established on port %u\n", (uint32_t)vxi_port);
        }
    }
}

bool VXI_Server::handle_packet()
{
    bool bClose = false;
    uint32_t rc = rpc::SUCCESS;

    if (vxi_request->program != rpc::VXI_11_CORE) {
        rc = rpc::PROG_UNAVAIL;

        LOG_F("ERROR: Invalid program (expected VXI_11_CORE = 0x607AF; received 0x%08x)\n", (uint32_t)(vxi_request->program));

    } else
        switch (vxi_request->procedure) {
        case rpc::VXI_11_CREATE_LINK:
            create_link();
            break;
        case rpc::VXI_11_DEV_READ:
            read();
            break;
        case rpc::VXI_11_DEV_WRITE:
            write();
            break;
        case rpc::VXI_11_DESTROY_LINK:
            destroy_link();
            bClose = true;
            break;
        default:
            LOG_F("Invalid VXI-11 procedure (received %u)\n", (uint32_t)(vxi_request->procedure));
            rc = rpc::PROC_UNAVAIL;
            break;
        }

    /*  Response messages will be sent by the various routines above
        when the program and procedure are recognized (and therefore
        rc == rpc::SUCCESS). We only need to send a response here
        if rc != rpc::SUCCESS.  */

    if (rc != rpc::SUCCESS) {
        vxi_response->rpc_status = rc;
        send_vxi_packet(client, sizeof(rpc_response_packet));
    }

    /*  signal to caller whether the connection should be close (i.e., DESTROY_LINK)  */

    return bClose;
}

void VXI_Server::create_link()
{
    /*  The data field in a link request should contain a string
        with the name of the requesting device. It may already
        be null-terminated, but just in case, we will put in
        the terminator.  */

    create_request->data[create_request->data_len] = 0;
    LOG_F("CREATE LINK request from \"%s\" on port %u\n", create_request->data, (uint32_t)vxi_port);
    /*  Generate the response  */
    create_response->rpc_status = rpc::SUCCESS;
    create_response->error = rpc::NO_ERROR;
    create_response->link_id = 0;
    create_response->abort_port = 0;
    create_response->max_receive_size = VXI_READ_SIZE - 4;
    send_vxi_packet(client, sizeof(create_response_packet));
}

void VXI_Server::destroy_link()
{
    LOG_F("DESTROY LINK on port %u\n", (uint32_t)vxi_port);
    destroy_response->rpc_status = rpc::SUCCESS;
    destroy_response->error = rpc::NO_ERROR;
    send_vxi_packet(client, sizeof(destroy_response_packet));
}

void VXI_Server::read()
{
    // This is where we read from the device
    // FIXME: Fill this in
    char readbuffer[] = "DUMMY";
    uint32_t len = strlen(readbuffer);
    LOG_F("READ DATA on port %u; data sent = %s\n", (uint32_t)vxi_port, readbuffer);
    read_response->rpc_status = rpc::SUCCESS;
    read_response->error = rpc::NO_ERROR;
    read_response->reason = rpc::END;
    read_response->data_len = len;
    strcpy(read_response->data, readbuffer);

    send_vxi_packet(client, sizeof(read_response_packet) + len);
}

void VXI_Server::write()
{
    // This is where we write to the device
    uint32_t wlen = write_request->data_len;
    uint32_t len = wlen;
    while (len > 0 && write_request->data[len - 1] == '\n') {
        len--;
    }
    write_request->data[len] = 0;
    LOG_F("WRITE DATA on port %u = \"%s\"\n", (uint32_t)vxi_port, write_request->data);
    /*  Parse and respond to the SCPI command  */
    parse_scpi(write_request->data); // FIXME: Fill this in
    /*  Generate the response  */
    write_response->rpc_status = rpc::SUCCESS;
    write_response->error = rpc::NO_ERROR;
    write_response->size = wlen;
    send_vxi_packet(client, sizeof(write_response_packet));
}

/**
 * @brief This method parses the SCPI commands and issues the appropriate commands to the device.
 * 
 * @param buffer null terminated string to send to the device
 */
void VXI_Server::parse_scpi(char *buffer)
{

}

const char *VXI_Server::get_visa_resource()
{
    static char visa_resource[40];
    sprintf(visa_resource, "TCPIP::%s::INSTR", WiFi.localIP().toString().c_str());
    return visa_resource;
}
