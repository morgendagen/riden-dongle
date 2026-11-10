#pragma once
struct HostMDNS {
    bool isRunning() const { return false; }
    int addService(const char *, const char *, const char *, int) { return 0; }
    void addServiceTxt(int, const char *, int) {}
};
inline HostMDNS MDNS;
