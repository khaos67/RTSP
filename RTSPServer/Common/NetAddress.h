#ifndef __NET_ADDRESS_H__
#define __NET_ADDRESS_H__

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

typedef unsigned int netAddressBits;

bool IsMulticastAddress(netAddressBits address);

// A mechanism for displaying an IPv4 address in ASCII.  This is intended to replace "inet_ntoa()", which is not thread-safe.
class AddressString 
{
public:
	AddressString(struct sockaddr_in const& addr);
	AddressString(struct in_addr const& addr);
	AddressString(netAddressBits addr); // "addr" is assumed to be in host byte order here

	virtual ~AddressString();

	char const* val() const { return fVal; }

private:
	void init(netAddressBits addr); // used to implement each of the constructors

private:
	char* fVal; // The result ASCII string: allocated by the constructor; deleted by the destructor
};

#endif
