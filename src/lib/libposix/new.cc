extern "C" {
	extern void *malloc(unsigned int);
	void free(void*);
}

void *operator new( size_t size)
{
	return(malloc((unsigned int)size));
}

void __cdecl operator delete(void *p) throw()
{
	free(p);
}
