#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define le16_to_cpu(x) ( (((x)>>8)&0xff) | (((x)<<8)&0xff00) )
#define le32_to_cpu(x) (\
	(((x)>>24)&0xff)\
	|\
	(((x)>>8)&0xff00)\
	|\
	(((x)<<8)&0xff0000)\
	|\
	(((x)<<24)&0xff000000)\
)

#define	BS_SIZE				36

#define	FAT12				1
#define FAT16				2
#define	FAT32				3

#define DIRECTORY	0x10

typedef struct _FAT_BS
{
	/* Boot Sector */
	char    jmp_boot[3]; // Jump instruction to boot code, most common eb3c90 
	char    OEM_name[8]; // An 8 byte string, typically used to indicate what OS formatted the volume. Microsoft operating systems don’t pay any attention to this field
	/* BIOS Parameter Block */
  	uint16_t	BytsPerSec; // Count of bytes per sector. Possible values 512, 1024, 2048 and 4096
	uint8_t     SecPerClus; // Number of sectors per allocation unit, must be a power of 2 between 1-12
	uint16_t 	RsvdSecCnt; // Number of reserved sectors in the reserved region of the volume. This is 1 for FAT12 and FAT16. For FAT32 this is most commonly 32. 
	uint8_t		NumFATs;    // Count of FAT data structures on the volume. Should always be set to 2
	uint16_t	RootEnt;    // For FAT12/FAT16 this field contains the count of 32. For FAT32 this is 0, it's set later in the FAT32 extended fields. 
	uint16_t	TotSec16;   // old 16bit value of total sectors, for FAT12/16 this field is used, for FAT32 the sector count is set in the extended field. 
	uint8_t		Media;      // Type of media, 0xF8 non-removable media, 0xF0 removable media
	uint16_t 	FATSz16;	// count of sectors occupied by one FAT. Used by FAT12/FAT16, 0 for FAT32
	uint16_t	SecPerTrk;	// Sectors per track for interrupt 0x13, only relevant if the media have a geometry. 
	uint16_t	NumHeads;	// Number of heads for interrupt 0x13, only relevant if the media have a ge	ometry.
	uint32_t	HiddSec;	// Hidden sectors preceding the partiation that contains this FAT volume.
	uint32_t 	TotSec32; 	// Total sectors for FAT32, 0 for FAT12/FAT16

} FAT_BS;

FAT_BS initFAT_BS(char buf[])
{
	FAT_BS f;
	memcpy(&f.jmp_boot, &(buf[0]), 3);
	memcpy(&f.OEM_name, &(buf[3]), 8);
	memcpy(&f.BytsPerSec, &(buf[11]), 2);
	memcpy(&f.SecPerClus, &(buf[13]), 1);
	memcpy(&f.RsvdSecCnt, &(buf[14]), 2);
	memcpy(&f.NumFATs, &(buf[16]), 1);
	memcpy(&f.RootEnt, &(buf[17]), 2);
	memcpy(&f.TotSec16, &(buf[19]), 2);
	memcpy(&f.Media, &(buf[21]), 1);
	memcpy(&f.FATSz16, &(buf[22]), 2);
	memcpy(&f.SecPerTrk, &(buf[24]), 2);
	memcpy(&f.NumHeads, &(buf[26]), 2);
	memcpy(&f.HiddSec, &(buf[28]), 4);
	memcpy(&f.TotSec32,&(buf[32]), 4);

	return f;
}

void printFAT_BS(FAT_BS f)
{
	printf("JMP_BOOT=0x%x %x %x\n", f.jmp_boot[0], f.jmp_boot[1], f.jmp_boot[2]);
	printf("%.*s\n", 8, f.OEM_name);
	printf("BytsPerSec=%d\n", f.BytsPerSec);
	printf("SecPerClus=%d\n", f.SecPerClus);
	printf("RsvdSecCnt=%d\n", f.RsvdSecCnt);
	printf("NumFATs=%d\n", f.NumFATs);
	printf("RootEnt=%d\n", f.RootEnt);
	printf("TotSec=%d\n", f.TotSec16);
	printf("Media=%d\n", f.Media);
	printf("FATSz16=%d\n", f.FATSz16);
	printf("SecPerTrk=%d\n", f.SecPerTrk);
	printf("NumHeads=%d\n", f.NumHeads);
	printf("HiddSec=%d\n", f.HiddSec);
	printf("TotSec32=%d\n", f.TotSec32);
}

/* getFATType - the FAT type is determind by the count of clusters on the volume */
int getFATType(FAT_BS f, uint32_t FATSz32)
{
	uint32_t RootDirSectors = (((f.RootEnt) * 32) + ((f.BytsPerSec) - 1)) / (f.BytsPerSec);
	uint32_t FATSz, TotSec, DataSec, CountOfClusters;
	
	if((f.FATSz16) != 0)
		FATSz = (f.FATSz16);
	else
		FATSz = (FATSz32);
	if((f.TotSec16) != 0)
		TotSec = (f.TotSec16);
	else
		TotSec = (f.TotSec32);

	DataSec = TotSec - ((f.RsvdSecCnt) + (f.NumFATs * FATSz) + RootDirSectors);
	CountOfClusters = DataSec / f.SecPerClus;
	printf("CountOfClusters=%d\n", CountOfClusters);

	if(CountOfClusters < 4085)
		return FAT12;
	else if(CountOfClusters < 66525)
		return FAT16;
	else
		return FAT32;

	return 0;
}

__inline uint32_t firstCluster(FAT_BS f)
{
	return (f.RsvdSecCnt + (f.NumFATs * f.FATSz16)) * f.BytsPerSec; 
}


// root_dir_sectors = ((fat_boot->root_entry_count * 32) + (fat_boot->bytes_per_sector - 1)) / fat_boot->bytes_per_sector;
__inline uint32_t rootDirSector(FAT_BS f)
{
	return ((f.RootEnt * 32) + (f.BytsPerSec -1)) / f.BytsPerSec;
}

// first_data_sector = fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size) + root_dir_sectors;

__inline uint32_t firstDataSector(FAT_BS f)
{
	return f.RsvdSecCnt + (f.NumFATs * f.FATSz16) + rootDirSector(f);
}

__inline uint32_t getCluster(FAT_BS f, uint32_t clusterNum)
{
	if(clusterNum < 3)
		return f.RsvdSecCnt + (f.NumFATs * f.FATSz16);
	else
		return ((clusterNum - 2 ) * f.SecPerClus) + firstDataSector(f);
}


// first_sector_of_cluster = ((cluster - 2) * fat_boot->sectors_per_cluster) + first_data_sector;
__inline uint32_t getClusterOffset(FAT_BS f, uint32_t clusterNum)
{
	if(clusterNum < 3)
		return firstCluster(f);
	else
		return ((((clusterNum - 2 ) * f.SecPerClus) + firstDataSector(f)) * f.BytsPerSec);

}

void listDir(FAT_BS f, FILE *fptr, uint32_t cluster)
{
	char buf[512];
	char filename[11]; 
	uint32_t i=0;
	
	fseek(fptr, (getClusterOffset(f, cluster)), SEEK_SET);
	fread(buf, 512, 1, fptr);
	while(1)
	{
		memcpy(&filename, &(buf[i]), 11);
		if(filename[0] == 0)
			break;
		else
		{
			printf("--");
			printf("%.*s	", 11, filename);
			memcpy(&cluster, &(buf[i+26]), 2);
			printf("cluster=%d	", cluster);
			printf("0x%x	", getClusterOffset(f, cluster));
			printf("%d\n", getCluster(f, cluster));
			i+=32;
		}
	}
}

void listDirs(FAT_BS f, int type, FILE *fptr)
{
	char buf[512]; 
	char filename[11];
	uint32_t i=0;
	if(type == FAT32)
	{
	}
	else // FAT12 or FAT16
	{
		uint32_t cluster_offset = firstCluster(f);
		uint16_t cluster=0;
		printf("%d\n", cluster_offset);
		fseek(fptr, cluster_offset, SEEK_SET);
		fread(buf, 512, 1, fptr);
		/*memcpy(&filename, &(buf[0]), 11);
		printf("%.*s\n", 11, filename);
		memcpy(&filename, &(buf[32]), 11);
		printf("%.*s\n", 11, filename);*/
		while(1)
		{
			memcpy(&filename, &(buf[i]), 11);
			if(filename[0] == 0)
				break;
			else
			{
				printf("%.*s	", 11, filename);
				memcpy(&cluster, &(buf[i+26]), 2);
				printf("cluster=%d	", cluster);
				printf("0x%x	", getClusterOffset(f, cluster));
				printf("%d\n", getCluster(f, cluster));
				if(buf[i+11] == DIRECTORY)
				{
					listDir(f, fptr, cluster);
					printf("got dir!\n");
				}
				i+=32;
			}
		}
		
	}
}

int main(int argc, char *argv[])
{
	if(argc == 1)
	{
		printf("Error: no input file\n");
		return -1;
	}
	FILE *fptr = fopen(argv[1], "r"); ;
	char buf[512];
	uint32_t FATSz32=0; 
	fread(buf, 512, 1, fptr);

	FAT_BS fatBS = initFAT_BS(buf);
	if(fatBS.FATSz16 == 0)
	{
		memcpy(&FATSz32, &(buf[36]), 4);
	}

	printFAT_BS(fatBS);
	printf("FATSz32=%d\n", FATSz32);
		
	switch(getFATType(fatBS, FATSz32)){
		case FAT12:
			printf("FAT12\n");
			break;
		case FAT16:
			printf("FAT16\n");
			break;
		case FAT32:
			printf("FAT32\n");
			break;
	}

	listDirs(fatBS, FAT16, fptr);

	return 0;
}
