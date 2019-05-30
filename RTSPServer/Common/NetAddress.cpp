#include "NetAddress.h"
#include <stdio.h>

AddressString::AddressString(struct sockaddr_in const& addr) 
{
	init(addr.sin_addr.s_addr);
}

AddressString::AddressString(struct in_addr const& addr) 
{
	init(addr.s_addr);
}

AddressString::AddressString(netAddressBits addr) 
{
	init(addr);
}

void AddressString::init(netAddressBits addr) 
{
	fVal = new char[16]; // large enough for "abc.def.ghi.jkl"
	netAddressBits addrNBO = htonl(addr); // make sure we have a value in a known byte order: big endian
	sprintf(fVal, "%u.%u.%u.%u", (addrNBO>>24)&0xFF, (addrNBO>>16)&0xFF, (addrNBO>>8)&0xFF, addrNBO&0xFF);
}

AddressString::~AddressString() 
{
	delete[] fVal;
}
