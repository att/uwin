#include	<uwin_share.h>

static const char*	mem[] =
{
	UWIN_SHARE_GLOBAL UWIN_SHARE_DOMAIN UWIN_SHARE,
	UWIN_SHARE_GLOBAL UWIN_SHARE_DOMAIN_OLD UWIN_SHARE_OLD,
};

#define block(s,b)	((char*)(s) + ((s)->block_table + (b) - 1) * (s)->block_size)
#define elementsof(x)	(int)(sizeof(x)/sizeof(x[0]))
#define pointer(s,b)	((char*)(s) + (b))

extern int		printf(const char*, ...);

int main(int argc, char** argv)
{
	Uwin_share_t*	Share;
	HANDLE		hp;
	const char*	name;
	char*		root;
	int		i;
	unsigned int	block_size;
	unsigned int	root_offset;

	for (i = 0; i < elementsof(mem); i++)
		if ((hp = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name = mem[i] + sizeof(UWIN_SHARE_GLOBAL) - 1)) ||
		    (hp = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name = mem[i])))
		{
			root = 0;
			if (Share = (Uwin_share_t*)MapViewOfFileEx(hp, FILE_MAP_READ|FILE_MAP_WRITE, 0, 0, 0, NULL))
			{
				if (Share->magic == UWIN_SHARE_MAGIC && Share->version >= UWIN_SHARE_VERSION_MIN && Share->version <= UWIN_SHARE_VERSION_MAX)
				{
					block_size = Share->block_size;
					root_offset = Share->root_offset;
				}
				else
				{
					block_size = Share->old_block_size;
					root_offset = Share->old_root_offset;
				}
				if (argc > 1)
					printf("mem=%s Share=%p magic=0x%08x version=%u block_size=%d root_offset=%d\n",
						name, Share, Share->magic, Share->version, block_size, root_offset);
				if (block_size >= 512 && !(block_size & (block_size - 1)) && root_offset)
					root = pointer(Share, root_offset);
			}
			if (root && root[1] == ':' && root[2] == '\\')
				printf("UWIN processes are running in '%s'\n", root);
			else
				printf("UWIN processes are running\n");
			return 0;
		}
	printf("There are no UWIN processes running [%d]\n", GetLastError());
	return 1;
}
