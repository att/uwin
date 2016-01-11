#include "includes.h"
#include "ssh.h"
#include "log.h"

#if TRACE_ADDRINFO

#undef	getaddrinfo

int _trace_getaddrinfo(char* addr, char* port, struct addrinfo* hints, struct addrinfo** aip, char* file, int line)
{
	struct addrinfo*	p;
	int			r;

	if (r = getaddrinfo(addr, port, hints, aip))
		logit("%s:%d:%u getaddrinfo [%s] addr=%s port=%s family=%d type=%d flags=%x", file, line, getpid(), gai_strerror(r), addr, port, hints ? hints->ai_family : 0, hints ? hints->ai_socktype : 0, hints ? hints->ai_flags : 0);
	else
	{
		p = *aip;
		logit("%s:%d:%u getaddrinfo ai=%p addr=%s port=%s family=%d type=%d flags=%x", file, line, getpid(), p, addr, port, hints ? hints->ai_family : 0, hints ? hints->ai_socktype : 0, hints ? hints->ai_flags : 0);
		while (p)
		{
			logit("  ai=%p family=%d type=%d prot=%d flags=%x len=%d addr=%p name=%s", p, p->ai_family, p->ai_socktype, p->ai_protocol, p->ai_flags, p->ai_addrlen, p->ai_addr, p->ai_canonname);
			p = p->ai_next;
		}
	}
	return r;
}

#undef	freeaddrinfo

void _trace_freeaddrinfo(struct addrinfo* ai, char* file, int line)
{
	struct addrinfo*	p;

	p = ai;
	logit("%s:%d:%u freeaddrinfo ai=%p", getpid(), file, line, p);
	while (p)
	{
		logit("  ai=%p family=%d type=%d prot=%d flags=%x len=%d addr=%p name=%s", p, p->ai_family, p->ai_socktype, p->ai_protocol, p->ai_flags, p->ai_addrlen, p->ai_addr, p->ai_canonname);
		p = p->ai_next;
	}
	freeaddrinfo(ai);
}

#else

int	_STUB_trace_addrinfo;

#endif
