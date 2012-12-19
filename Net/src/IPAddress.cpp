//
// IPAddress.cpp
//
// $Id: //poco/1.4/Net/src/IPAddress.cpp#5 $
//
// Library: Net
// Package: NetCore
// Module:  IPAddress
//
// Copyright (c) 2005-2011, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/RefCountedObject.h"
#include "Poco/NumberFormatter.h"
#include "Poco/BinaryReader.h"
#include "Poco/BinaryWriter.h"
#include "Poco/String.h"
#include "Poco/Types.h"


using Poco::RefCountedObject;
using Poco::NumberFormatter;
using Poco::BinaryReader;
using Poco::BinaryWriter;
using Poco::toLower;
using Poco::UInt8;
using Poco::UInt16;
using Poco::UInt32;


namespace {

template <typename T>
unsigned maskBits(T val, unsigned size)
	/// Returns the length of the mask (number of bits set in val).
	/// The val should be either all zeros or two contiguos areas of 1s and 0s. 
	/// The algorithm ignores invalid non-contiguous series of 1s and treats val 
	/// as if all bits between MSb and last non-zero bit are set to 1.
{
	unsigned count = 0;
	if (val)
	{
		val = (val ^ (val - 1)) >> 1;
		for (count = 0; val; ++count) val >>= 1;
	}
	else count = size;
	return size - count;
}

} // namespace


namespace Poco {
namespace Net {


//
// IPAddressImpl
//


class IPAddressImpl: public RefCountedObject
{
public:
	virtual std::string toString() const = 0;
	virtual poco_socklen_t length() const = 0;
	virtual const void* addr() const = 0;
	virtual IPAddress::Family family() const = 0;
	virtual int af() const = 0;
	virtual Poco::UInt32 scope() const = 0;
	virtual bool isWildcard() const	= 0;
	virtual bool isBroadcast() const = 0;
	virtual bool isLoopback() const = 0;
	virtual bool isMulticast() const = 0;
	virtual bool isLinkLocal() const = 0;
	virtual bool isSiteLocal() const = 0;
	virtual bool isIPv4Mapped() const = 0;
	virtual bool isIPv4Compatible() const = 0;
	virtual bool isWellKnownMC() const = 0;
	virtual bool isNodeLocalMC() const = 0;
	virtual bool isLinkLocalMC() const = 0;
	virtual bool isSiteLocalMC() const = 0;
	virtual bool isOrgLocalMC() const = 0;
	virtual bool isGlobalMC() const = 0;
	virtual void mask(const IPAddressImpl* pMask, const IPAddressImpl* pSet) = 0;
	virtual unsigned prefixLength() const = 0;

	virtual IPAddressImpl* clone() const = 0;

protected:
	IPAddressImpl()
	{
	}
	
	virtual ~IPAddressImpl()
	{
	}

private:
	IPAddressImpl(const IPAddressImpl&);
	IPAddressImpl& operator = (const IPAddressImpl&);
};


class IPv4AddressImpl: public IPAddressImpl
{
public:
	IPv4AddressImpl()
	{
		std::memset(&_addr, 0, sizeof(_addr));
	}
	
	IPv4AddressImpl(const void* addr)
	{
		std::memcpy(&_addr, addr, sizeof(_addr));
	}
	
	IPv4AddressImpl(unsigned prefix)
	{
		UInt32 addr = (prefix == 32) ? 0xffffffff : ~(0xffffffff >> prefix);
		_addr.s_addr = htonl(addr);
	}

	std::string toString() const
	{
		const UInt8* bytes = reinterpret_cast<const UInt8*>(&_addr);
		std::string result;
		result.reserve(16);
		NumberFormatter::append(result, bytes[0]);
		result.append(".");
		NumberFormatter::append(result, bytes[1]);
		result.append(".");
		NumberFormatter::append(result, bytes[2]);
		result.append(".");
		NumberFormatter::append(result, bytes[3]);
		return result;
	}

	poco_socklen_t length() const
	{
		return sizeof(_addr);
	}
	
	const void* addr() const
	{
		return &_addr;
	}
	
	IPAddress::Family family() const
	{
		return IPAddress::IPv4;
	}
	
	int af() const
	{
		return AF_INET;
	}

	unsigned prefixLength() const
	{
		return maskBits(ntohl(_addr.s_addr), 32);
	}
	
	Poco::UInt32 scope() const
	{
		return 0;
	}
	
	bool isWildcard() const
	{
		return _addr.s_addr == INADDR_ANY;
	}
	
	bool isBroadcast() const
	{
		return _addr.s_addr == INADDR_NONE;
	}
	
	bool isLoopback() const
	{
		return (ntohl(_addr.s_addr) & 0xFF000000) == 0x7F000000; // 127.0.0.1 to 127.255.255.255
	}
	
	bool isMulticast() const
	{
		return (ntohl(_addr.s_addr) & 0xF0000000) == 0xE0000000; // 224.0.0.0/24 to 239.0.0.0/24
	}
		
	bool isLinkLocal() const
	{
		return (ntohl(_addr.s_addr) & 0xFFFF0000) == 0xA9FE0000; // 169.254.0.0/16
	}
	
	bool isSiteLocal() const
	{
		UInt32 addr = ntohl(_addr.s_addr);
		return (addr & 0xFF000000) == 0x0A000000 ||     // 10.0.0.0/24
			(addr & 0xFFFF0000) == 0xC0A80000 ||        // 192.68.0.0/16
			(addr >= 0xAC100000 && addr <= 0xAC1FFFFF); // 172.16.0.0 to 172.31.255.255
	}
	
	bool isIPv4Compatible() const
	{
		return true;
	}

	bool isIPv4Mapped() const
	{
		return true;
	}

	bool isWellKnownMC() const
	{
		return (ntohl(_addr.s_addr) & 0xFFFFFF00) == 0xE0000000; // 224.0.0.0/8
	}
	
	bool isNodeLocalMC() const
	{
		return false;
	}
	
	bool isLinkLocalMC() const
	{
		return (ntohl(_addr.s_addr) & 0xFF000000) == 0xE0000000; // 244.0.0.0/24
	}
	
	bool isSiteLocalMC() const
	{
		return (ntohl(_addr.s_addr) & 0xFFFF0000) == 0xEFFF0000; // 239.255.0.0/16
	}
	
	bool isOrgLocalMC() const
	{
		return (ntohl(_addr.s_addr) & 0xFFFF0000) == 0xEFC00000; // 239.192.0.0/16
	}
	
	bool isGlobalMC() const
	{
		UInt32 addr = ntohl(_addr.s_addr);
		return addr >= 0xE0000100 && addr <= 0xEE000000; // 224.0.1.0 to 238.255.255.255
	}

	static IPv4AddressImpl* parse(const std::string& addr)
	{
		if (addr.empty()) return 0;		
#if defined(_WIN32) 
		struct in_addr ia;
		ia.s_addr = inet_addr(addr.c_str());
		if (ia.s_addr == INADDR_NONE && addr != "255.255.255.255")
			return 0;
		else
			return new IPv4AddressImpl(&ia);
#else
#if __GNUC__ < 3 || defined(POCO_VXWORKS)
		struct in_addr ia;
		ia.s_addr = inet_addr(const_cast<char*>(addr.c_str()));
		if (ia.s_addr == INADDR_NONE && addr != "255.255.255.255")
			return 0;
		else
			return new IPv4AddressImpl(&ia);
#else
		struct in_addr ia;
		if (inet_aton(addr.c_str(), &ia))
			return new IPv4AddressImpl(&ia);
		else
			return 0;
#endif
#endif
	}
	
	void mask(const IPAddressImpl* pMask, const IPAddressImpl* pSet)
	{
		poco_assert (pMask->af() == AF_INET && pSet->af() == AF_INET);
		
		_addr.s_addr &= static_cast<const IPv4AddressImpl*>(pMask)->_addr.s_addr;
		_addr.s_addr |= static_cast<const IPv4AddressImpl*>(pSet)->_addr.s_addr & ~static_cast<const IPv4AddressImpl*>(pMask)->_addr.s_addr;
	}
	
	IPAddressImpl* clone() const
	{
		return new IPv4AddressImpl(&_addr);
	}

	IPv4AddressImpl operator & (const IPv4AddressImpl& addr) const
	{
		IPv4AddressImpl result(&_addr);
		result._addr.s_addr &= addr._addr.s_addr;
		return result;
	}

	IPv4AddressImpl operator | (const IPv4AddressImpl& addr) const
	{
		IPv4AddressImpl result(&_addr);
		result._addr.s_addr |= addr._addr.s_addr;
		return result;
	}

	IPv4AddressImpl operator ^ (const IPv4AddressImpl& addr) const
	{
		IPv4AddressImpl result(&_addr);
		result._addr.s_addr ^= addr._addr.s_addr;
		return result;
	}

	IPv4AddressImpl operator ~ () const
	{
		IPv4AddressImpl result(&_addr);
		result._addr.s_addr ^= 0xffffffff;
		return result;
	}

private:	
	IPv4AddressImpl(const IPv4AddressImpl& addr)
	{
		std::memcpy(&_addr, &addr._addr, sizeof(_addr));
	}

	IPv4AddressImpl& operator = (const IPv4AddressImpl&);

	struct in_addr _addr;
};


#if defined(POCO_HAVE_IPv6)


class IPv6AddressImpl: public IPAddressImpl
{
public:
	IPv6AddressImpl():
		_scope(0)
	{
		std::memset(&_addr, 0, sizeof(_addr));
	}

	IPv6AddressImpl(const void* addr):
		_scope(0)
	{
		std::memcpy(&_addr, addr, sizeof(_addr));
	}

	IPv6AddressImpl(const void* addr, Poco::UInt32 scope):
		_scope(scope)
	{
		std::memcpy(&_addr, addr, sizeof(_addr));
	}


	IPv6AddressImpl(unsigned prefix):
		_scope(0)
	{
		unsigned i = 0;
#ifdef POCO_OS_FAMILY_WINDOWS
		for (; prefix >= 16; ++i, prefix -= 16) {
			_addr.s6_addr16[i] = 0xffff;
		}
		if (prefix > 0)
			_addr.s6_addr16[i++] = htons(~(0xffff >> prefix));
		while (i < 8)
			_addr.s6_addr16[i++] = 0;
#else
		for (; prefix >= 32; ++i, prefix -= 32) {
			_addr.s6_addr32[i] = 0xffffffff;
		}
		if (prefix > 0)
			_addr.s6_addr32[i++] = htonl(~(0xffffffffU >> prefix));
		while (i < 4)
			_addr.s6_addr32[i++] = 0;
#endif
	}

	std::string toString() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		if (isIPv4Compatible() || isIPv4Mapped())
		{
			std::string result;
			result.reserve(24);
			if (words[5] == 0)
				result.append("::");
			else
				result.append("::ffff:");
			const UInt8* bytes = reinterpret_cast<const UInt8*>(&_addr);
			NumberFormatter::append(result, bytes[12]);
			result.append(".");
			NumberFormatter::append(result, bytes[13]);
			result.append(".");
			NumberFormatter::append(result, bytes[14]);
			result.append(".");
			NumberFormatter::append(result, bytes[15]);
			return result;
		}
		else
		{
			std::string result;
			result.reserve(64);
			bool zeroSequence = false;
			int i = 0;
			while (i < 8)
			{
				if (!zeroSequence && words[i] == 0)
				{
					int zi = i;
					while (zi < 8 && words[zi] == 0) ++zi;
					if (zi > i + 1)
					{
						i = zi;
						result.append(":");
						zeroSequence = true;
					}
				}
				if (i > 0) result.append(":");
				if (i < 8) NumberFormatter::appendHex(result, ntohs(words[i++]));
			}
			if (_scope > 0)
			{
				result.append("%");
#if defined(_WIN32)
				NumberFormatter::append(result, _scope);
#else
				char buffer[IFNAMSIZ];
				if (if_indextoname(_scope, buffer))
				{
					result.append(buffer);
				}
				else
				{
					NumberFormatter::append(result, _scope);
				}
#endif
			}
			return toLower(result);
		}
	}
	
	poco_socklen_t length() const
	{
		return sizeof(_addr);
	}

	const void* addr() const
	{
		return &_addr;
	}

	IPAddress::Family family() const
	{
		return IPAddress::IPv6;
	}

	int af() const
	{
		return AF_INET6;
	}
	
	unsigned prefixLength() const
	{
		unsigned bits = 0;
		unsigned bitPos = 128;
#if defined(POCO_OS_FAMILY_UNIX)
		for (int i = 3; i >= 0; --i)
		{
			unsigned addr = ntohl(_addr.s6_addr32[i]);
			if ((bits = maskBits(addr, 32))) return (bitPos - (32 - bits));
			bitPos -= 32;
		}
		return 0;
#elif defined(POCO_OS_FAMILY_WINDOWS)
		for (int i = 7; i >= 0; --i)
		{
			unsigned short addr = ntohs(_addr.s6_addr16[i]);
			if ((bits = maskBits(addr, 16))) return (bitPos - (16 - bits));
			bitPos -= 16;
		}
		return 0;
#else
#warning prefixLength() not implemented
		throw NotImplementedException("prefixLength() not implemented");
#endif
	}
	Poco::UInt32 scope() const
	{
		return _scope;
	}

	bool isWildcard() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 && 
			words[4] == 0 && words[5] == 0 && words[6] == 0 && words[7] == 0;
	}
	
	bool isBroadcast() const
	{
		return false;
	}
	
	bool isLoopback() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 && 
			words[4] == 0 && words[5] == 0 && words[6] == 0 && ntohs(words[7]) == 0x0001;
	}
	
	bool isMulticast() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFE0) == 0xFF00;
	}
		
	bool isLinkLocal() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFE0) == 0xFE80;
	}
	
	bool isSiteLocal() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return ((ntohs(words[0]) & 0xFFE0) == 0xFEC0) || ((ntohs(words[0]) & 0xFF00) == 0xFC00);
	}
	
	bool isIPv4Compatible() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 && words[4] == 0 && words[5] == 0;
	}

	bool isIPv4Mapped() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 && words[4] == 0 && ntohs(words[5]) == 0xFFFF;
	}

	bool isWellKnownMC() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFF0) == 0xFF00;
	}
	
	bool isNodeLocalMC() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFEF) == 0xFF01;
	}
	
	bool isLinkLocalMC() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFEF) == 0xFF02;
	}
	
	bool isSiteLocalMC() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFEF) == 0xFF05;
	}
	
	bool isOrgLocalMC() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFEF) == 0xFF08;
	}
	
	bool isGlobalMC() const
	{
		const UInt16* words = reinterpret_cast<const UInt16*>(&_addr);
		return (ntohs(words[0]) & 0xFFEF) == 0xFF0F;
	}

	static IPv6AddressImpl* parse(const std::string& addr)
	{
		if (addr.empty()) return 0;
#if defined(_WIN32)
		struct addrinfo* pAI;
		struct addrinfo hints;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_NUMERICHOST;
		int rc = getaddrinfo(addr.c_str(), NULL, &hints, &pAI);
		if (rc == 0)
		{
			IPv6AddressImpl* pResult = new IPv6AddressImpl(&reinterpret_cast<struct sockaddr_in6*>(pAI->ai_addr)->sin6_addr, static_cast<int>(reinterpret_cast<struct sockaddr_in6*>(pAI->ai_addr)->sin6_scope_id));
			freeaddrinfo(pAI);
			return pResult;
		}
		else return 0;
#else
		struct in6_addr ia;
		std::string::size_type pos = addr.find('%');
		if (std::string::npos != pos)
		{
			std::string::size_type start = ('[' == addr[0]) ? 1 : 0;
			std::string unscopedAddr(addr, start, pos - start);
			std::string scope(addr, pos + 1, addr.size() - start - pos);
			Poco::UInt32 scopeId(0);
			if (!(scopeId = if_nametoindex(scope.c_str())))
				return 0;
			if (inet_pton(AF_INET6, unscopedAddr.c_str(), &ia) == 1)
				return new IPv6AddressImpl(&ia, scopeId);
			else
				return 0;
		}
		else
		{
			if (inet_pton(AF_INET6, addr.c_str(), &ia) == 1)
				return new IPv6AddressImpl(&ia);
			else
				return 0;
		}
#endif
	}
	
	void mask(const IPAddressImpl* pMask, const IPAddressImpl* pSet)
	{
		throw Poco::NotImplementedException("mask() is only supported for IPv4 addresses");
	}

	IPAddressImpl* clone() const
	{
		return new IPv6AddressImpl(&_addr, _scope);
	}

	IPv6AddressImpl operator & (const IPv6AddressImpl& addr) const
	{
		IPv6AddressImpl result(&_addr);
#ifdef POCO_OS_FAMILY_WINDOWS
		result._addr.s6_addr16[0] &= addr._addr.s6_addr16[0];
		result._addr.s6_addr16[1] &= addr._addr.s6_addr16[1];
		result._addr.s6_addr16[2] &= addr._addr.s6_addr16[2];
		result._addr.s6_addr16[3] &= addr._addr.s6_addr16[3];
		result._addr.s6_addr16[4] &= addr._addr.s6_addr16[4];
		result._addr.s6_addr16[5] &= addr._addr.s6_addr16[5];
		result._addr.s6_addr16[6] &= addr._addr.s6_addr16[6];
		result._addr.s6_addr16[7] &= addr._addr.s6_addr16[7];
#else
		result._addr.s6_addr32[0] &= addr._addr.s6_addr32[0];
		result._addr.s6_addr32[1] &= addr._addr.s6_addr32[1];
		result._addr.s6_addr32[2] &= addr._addr.s6_addr32[2];
		result._addr.s6_addr32[3] &= addr._addr.s6_addr32[3];
#endif
		return result;
	}

	IPv6AddressImpl operator | (const IPv6AddressImpl& addr) const
	{
		IPv6AddressImpl result(&_addr);
#ifdef POCO_OS_FAMILY_WINDOWS
		result._addr.s6_addr16[0] |= addr._addr.s6_addr16[0];
		result._addr.s6_addr16[1] |= addr._addr.s6_addr16[1];
		result._addr.s6_addr16[2] |= addr._addr.s6_addr16[2];
		result._addr.s6_addr16[3] |= addr._addr.s6_addr16[3];
		result._addr.s6_addr16[4] |= addr._addr.s6_addr16[4];
		result._addr.s6_addr16[5] |= addr._addr.s6_addr16[5];
		result._addr.s6_addr16[6] |= addr._addr.s6_addr16[6];
		result._addr.s6_addr16[7] |= addr._addr.s6_addr16[7];
#else
		result._addr.s6_addr32[0] |= addr._addr.s6_addr32[0];
		result._addr.s6_addr32[1] |= addr._addr.s6_addr32[1];
		result._addr.s6_addr32[2] |= addr._addr.s6_addr32[2];
		result._addr.s6_addr32[3] |= addr._addr.s6_addr32[3];
#endif
		return result;
	}

	IPv6AddressImpl operator ^ (const IPv6AddressImpl& addr) const
	{
		IPv6AddressImpl result(&_addr);
#ifdef POCO_OS_FAMILY_WINDOWS
		result._addr.s6_addr16[0] ^= addr._addr.s6_addr16[0];
		result._addr.s6_addr16[1] ^= addr._addr.s6_addr16[1];
		result._addr.s6_addr16[2] ^= addr._addr.s6_addr16[2];
		result._addr.s6_addr16[3] ^= addr._addr.s6_addr16[3];
		result._addr.s6_addr16[4] ^= addr._addr.s6_addr16[4];
		result._addr.s6_addr16[5] ^= addr._addr.s6_addr16[5];
		result._addr.s6_addr16[6] ^= addr._addr.s6_addr16[6];
		result._addr.s6_addr16[7] ^= addr._addr.s6_addr16[7];
#else
		result._addr.s6_addr32[0] ^= addr._addr.s6_addr32[0];
		result._addr.s6_addr32[1] ^= addr._addr.s6_addr32[1];
		result._addr.s6_addr32[2] ^= addr._addr.s6_addr32[2];
		result._addr.s6_addr32[3] ^= addr._addr.s6_addr32[3];
#endif
		return result;
	}

	IPv6AddressImpl operator ~ () const
	{
		IPv6AddressImpl result(&_addr);
#ifdef POCO_OS_FAMILY_WINDOWS
		result._addr.s6_addr16[0] ^= 0xffff;
		result._addr.s6_addr16[1] ^= 0xffff;
		result._addr.s6_addr16[2] ^= 0xffff;
		result._addr.s6_addr16[3] ^= 0xffff;
		result._addr.s6_addr16[4] ^= 0xffff;
		result._addr.s6_addr16[5] ^= 0xffff;
		result._addr.s6_addr16[6] ^= 0xffff;
		result._addr.s6_addr16[7] ^= 0xffff;
#else
		result._addr.s6_addr32[0] ^= 0xffffffff;
		result._addr.s6_addr32[1] ^= 0xffffffff;
		result._addr.s6_addr32[2] ^= 0xffffffff;
		result._addr.s6_addr32[3] ^= 0xffffffff;
#endif
		return result;
	}

private:
	IPv6AddressImpl(const IPv6AddressImpl& addr): _scope(0)
	{
		std::memcpy((void*) &_addr, (void*) &addr._addr, sizeof(_addr));
	}

	IPv6AddressImpl& operator = (const IPv6AddressImpl&);

	struct in6_addr _addr;	
	Poco::UInt32    _scope;
};


#endif // POCO_HAVE_IPv6


//
// IPAddress
//


IPAddress::IPAddress(): _pImpl(new IPv4AddressImpl)
{
}


IPAddress::IPAddress(const IPAddress& addr): _pImpl(addr._pImpl)
{
	_pImpl->duplicate();
}


IPAddress::IPAddress(Family family): _pImpl(0)
{
	if (family == IPv4)
		_pImpl = new IPv4AddressImpl();
#if defined(POCO_HAVE_IPv6)
	else if (family == IPv6)
		_pImpl = new IPv6AddressImpl();
#endif
	else
		throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}


IPAddress::IPAddress(const std::string& addr)
{
	_pImpl = IPv4AddressImpl::parse(addr);
#if defined(POCO_HAVE_IPv6)
	if (!_pImpl)
		_pImpl = IPv6AddressImpl::parse(addr);
#endif
	if (!_pImpl) throw InvalidAddressException(addr);
}


IPAddress::IPAddress(const std::string& addr, Family family): _pImpl(0)
{
	if (family == IPv4)
		_pImpl = IPv4AddressImpl::parse(addr);
#if defined(POCO_HAVE_IPv6)
	else if (family == IPv6)
		_pImpl = IPv6AddressImpl::parse(addr);
#endif
	else throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
	if (!_pImpl) throw InvalidAddressException(addr);
}


IPAddress::IPAddress(const void* addr, poco_socklen_t length)
{
	if (length == sizeof(struct in_addr))
		_pImpl = new IPv4AddressImpl(addr);
#if defined(POCO_HAVE_IPv6)
	else if (length == sizeof(struct in6_addr))
		_pImpl = new IPv6AddressImpl(addr);
#endif
	else throw Poco::InvalidArgumentException("Invalid address length passed to IPAddress()");
}


IPAddress::IPAddress(const void* addr, poco_socklen_t length, Poco::UInt32 scope)
{
	if (length == sizeof(struct in_addr))
		_pImpl = new IPv4AddressImpl(addr);
#if defined(POCO_HAVE_IPv6)
	else if (length == sizeof(struct in6_addr))
		_pImpl = new IPv6AddressImpl(addr, scope);
#endif
	else throw Poco::InvalidArgumentException("Invalid address length passed to IPAddress()");
}


IPAddress::IPAddress(unsigned prefix, Family family): _pImpl(0)
{
	if (family == IPv4)
	{
		if (prefix <= 32) {
			_pImpl = new IPv4AddressImpl(prefix);
		}
	}
#if defined(POCO_HAVE_IPv6)
	else if (family == IPv6)
	{
		if (prefix <= 128)
		{
			_pImpl = new IPv6AddressImpl(prefix);
		}
	}
#endif
	else throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
	if (!_pImpl) throw Poco::InvalidArgumentException("Invalid prefix length passed to IPAddress()");
}


#if defined(_WIN32)
IPAddress::IPAddress(const SOCKET_ADDRESS& socket_address)
{
	ADDRESS_FAMILY family = socket_address.lpSockaddr->sa_family;
	if (family == AF_INET)
		_pImpl = new IPv4AddressImpl(&reinterpret_cast<const struct sockaddr_in*>(socket_address.lpSockaddr)->sin_addr);
#if defined(POCO_HAVE_IPv6)
	else if (family == AF_INET6)
		_pImpl = new IPv6AddressImpl(&reinterpret_cast<const struct sockaddr_in6*>(socket_address.lpSockaddr)->sin6_addr, reinterpret_cast<const struct sockaddr_in6*>(socket_address.lpSockaddr)->sin6_scope_id);
#endif
	else throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}
#endif


IPAddress::IPAddress(const struct sockaddr& sockaddr)
{
	unsigned short family = sockaddr.sa_family;
	if (family == AF_INET)
		_pImpl = new IPv4AddressImpl(&reinterpret_cast<const struct sockaddr_in*>(&sockaddr)->sin_addr);
#if defined(POCO_HAVE_IPv6)
	else if (family == AF_INET6)
		_pImpl = new IPv6AddressImpl(&reinterpret_cast<const struct sockaddr_in6*>(&sockaddr)->sin6_addr, reinterpret_cast<const struct sockaddr_in6*>(&sockaddr)->sin6_scope_id);
#endif
	else throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}


IPAddress::~IPAddress()
{
	_pImpl->release();
}


IPAddress& IPAddress::operator = (const IPAddress& addr)
{
	if (&addr != this)
	{
		_pImpl->release();
		_pImpl = addr._pImpl;
		_pImpl->duplicate();
	}
	return *this;
}


void IPAddress::swap(IPAddress& address)
{
	std::swap(_pImpl, address._pImpl);
}

	
IPAddress::Family IPAddress::family() const
{
	return _pImpl->family();
}


Poco::UInt32 IPAddress::scope() const
{
	return _pImpl->scope();
}

	
std::string IPAddress::toString() const
{
	return _pImpl->toString();
}


bool IPAddress::isWildcard() const
{
	return _pImpl->isWildcard();
}
	
bool IPAddress::isBroadcast() const
{
	return _pImpl->isBroadcast();
}


bool IPAddress::isLoopback() const
{
	return _pImpl->isLoopback();
}


bool IPAddress::isMulticast() const
{
	return _pImpl->isMulticast();
}

	
bool IPAddress::isUnicast() const
{
	return !isWildcard() && !isBroadcast() && !isMulticast();
}

	
bool IPAddress::isLinkLocal() const
{
	return _pImpl->isLinkLocal();
}


bool IPAddress::isSiteLocal() const
{
	return _pImpl->isSiteLocal();
}


bool IPAddress::isIPv4Compatible() const
{
	return _pImpl->isIPv4Compatible();
}


bool IPAddress::isIPv4Mapped() const
{
	return _pImpl->isIPv4Mapped();
}


bool IPAddress::isWellKnownMC() const
{
	return _pImpl->isWellKnownMC();
}


bool IPAddress::isNodeLocalMC() const
{
	return _pImpl->isNodeLocalMC();
}


bool IPAddress::isLinkLocalMC() const
{
	return _pImpl->isLinkLocalMC();
}


bool IPAddress::isSiteLocalMC() const
{
	return _pImpl->isSiteLocalMC();
}


bool IPAddress::isOrgLocalMC() const
{
	return _pImpl->isOrgLocalMC();
}


bool IPAddress::isGlobalMC() const
{
	return _pImpl->isGlobalMC();
}


bool IPAddress::operator == (const IPAddress& a) const
{
	poco_socklen_t l1 = length();
	poco_socklen_t l2 = a.length();
	if (l1 == l2)
		return std::memcmp(addr(), a.addr(), l1) == 0;
	else
		return false;
}


bool IPAddress::operator != (const IPAddress& a) const
{
	poco_socklen_t l1 = length();
	poco_socklen_t l2 = a.length();
	if (l1 == l2)
		return std::memcmp(addr(), a.addr(), l1) != 0;
	else
		return true;
}


bool IPAddress::operator < (const IPAddress& a) const
{
	poco_socklen_t l1 = length();
	poco_socklen_t l2 = a.length();
	if (l1 == l2)
		return std::memcmp(addr(), a.addr(), l1) < 0;
	else
		return l1 < l2;
}


bool IPAddress::operator <= (const IPAddress& a) const
{
	poco_socklen_t l1 = length();
	poco_socklen_t l2 = a.length();
	if (l1 == l2)
		return std::memcmp(addr(), a.addr(), l1) <= 0;
	else
		return l1 < l2;
}


bool IPAddress::operator > (const IPAddress& a) const
{
	poco_socklen_t l1 = length();
	poco_socklen_t l2 = a.length();
	if (l1 == l2)
		return std::memcmp(addr(), a.addr(), l1) > 0;
	else
		return l1 > l2;
}


bool IPAddress::operator >= (const IPAddress& a) const
{
	poco_socklen_t l1 = length();
	poco_socklen_t l2 = a.length();
	if (l1 == l2)
		return std::memcmp(addr(), a.addr(), l1) >= 0;
	else
		return l1 > l2;
}


IPAddress IPAddress::operator & (const IPAddress& other) const
{
	if (family() == other.family())
	{
		if (family() == IPv4)
		{
			IPv4AddressImpl t(_pImpl->addr());
			IPv4AddressImpl o(other._pImpl->addr());
			return IPAddress((t & o).addr(), sizeof(struct in_addr));
		}
#if defined(POCO_HAVE_IPv6)
		else if (family() == IPv6)
		{
			IPv6AddressImpl t(_pImpl->addr());
			IPv6AddressImpl o(other._pImpl->addr());
			return IPAddress((t & o).addr(), sizeof(struct in6_addr));
		}
#endif
		else
			throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
	}
	else
		throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}


IPAddress IPAddress::operator | (const IPAddress& other) const
{
	if (family() == other.family())
	{
		if (family() == IPv4)
		{
			IPv4AddressImpl t(_pImpl->addr());
			IPv4AddressImpl o(other._pImpl->addr());
			return IPAddress((t | o).addr(), sizeof(struct in_addr));
		}
#if defined(POCO_HAVE_IPv6)
		else if (family() == IPv6)
		{
			IPv6AddressImpl t(_pImpl->addr());
			IPv6AddressImpl o(other._pImpl->addr());
			return IPAddress((t | o).addr(), sizeof(struct in6_addr));
		}
#endif
		else
			throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
	}
	else
		throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}


IPAddress IPAddress::operator ^ (const IPAddress& other) const
{
	if (family() == other.family())
	{
		if (family() == IPv4)
		{
			IPv4AddressImpl t(_pImpl->addr());
			IPv4AddressImpl o(other._pImpl->addr());
			return IPAddress((t ^ o).addr(), sizeof(struct in_addr));
		}
#if defined(POCO_HAVE_IPv6)
		else if (family() == IPv6)
		{
			IPv6AddressImpl t(_pImpl->addr());
			IPv6AddressImpl o(other._pImpl->addr());
			return IPAddress((t ^ o).addr(), sizeof(struct in6_addr));
		}
#endif
		else
			throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
	}
	else
		throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}


IPAddress IPAddress::operator ~ () const
{
	if (family() == IPv4)
	{
		IPv4AddressImpl self(this->_pImpl->addr());
		return IPAddress((~self).addr(), sizeof(struct in_addr));
	}
#if defined(POCO_HAVE_IPv6)
	else if (family() == IPv6)
	{
		IPv6AddressImpl self(this->_pImpl->addr());
		return IPAddress((~self).addr(), sizeof(struct in6_addr));
	}
#endif
	else
		throw Poco::InvalidArgumentException("Invalid or unsupported address family passed to IPAddress()");
}


poco_socklen_t IPAddress::length() const
{
	return _pImpl->length();
}

	
const void* IPAddress::addr() const
{
	return _pImpl->addr();
}


int IPAddress::af() const
{
	return _pImpl->af();
}


unsigned IPAddress::prefixLength() const
{
	return _pImpl->prefixLength();
}

void IPAddress::init(IPAddressImpl* pImpl)
{
	_pImpl->release();
	_pImpl = pImpl;
}


IPAddress IPAddress::parse(const std::string& addr)
{
	return IPAddress(addr);
}


bool IPAddress::tryParse(const std::string& addr, IPAddress& result)
{
	IPAddressImpl* pImpl = IPv4AddressImpl::parse(addr);
#if defined(POCO_HAVE_IPv6)
	if (!pImpl) pImpl = IPv6AddressImpl::parse(addr);
#endif
	if (pImpl)
	{
		result.init(pImpl);
		return true;
	}
	else return false;
}


void IPAddress::mask(const IPAddress& mask)
{
	IPAddressImpl* pClone = _pImpl->clone();
	_pImpl->release();
	_pImpl = pClone;
	IPAddress null;
	_pImpl->mask(mask._pImpl, null._pImpl);
}


void IPAddress::mask(const IPAddress& mask, const IPAddress& set)
{
	IPAddressImpl* pClone = _pImpl->clone();
	_pImpl->release();
	_pImpl = pClone;
	_pImpl->mask(mask._pImpl, set._pImpl);
}


IPAddress IPAddress::wildcard(Family family)
{
	return IPAddress(family);
}


IPAddress IPAddress::broadcast()
{
	struct in_addr ia;
	ia.s_addr = INADDR_NONE;
	return IPAddress(&ia, sizeof(ia));
}


BinaryWriter& operator << (BinaryWriter& writer, const IPAddress& value)
{
	writer.stream().write((const char*) value.addr(), value.length());
	return writer;
}

BinaryReader& operator >> (BinaryReader& reader, IPAddress& value)
{
	char buf[sizeof(struct in6_addr)];
	reader.stream().read(buf, value.length());
	value = IPAddress(buf, value.length());
	return reader;
}


} } // namespace Poco::Net
