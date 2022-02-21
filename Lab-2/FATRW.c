/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 2: FATRW.c
 * This program implements a file allocation table (FAT) on a disk emulator called jdisk. It allows a user
 * to import and export files from a jdisk. Jdisk and its associated functions are defined in jdisk.h and
 * implemented in jdisk.c by Dr. James Plank (jplank@utk.edu).
 * 10/11/2021 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jdisk.h"

unsigned short Read_Link(int link, void *jdisk, unsigned char **FAT);
void Write_Link(int link, unsigned short val, int *written, unsigned char **FAT);
void Flush_Links(void *jdisk, int *written, unsigned char **FAT);

int main(int argc, char **argv)
{	
	char *usage;
	void *jdisk;
	FILE *fin, *fout;
	unsigned char **FAT;
	unsigned char buf[JDISK_SECTOR_SIZE];
	int *written;
	int i, sectors, D, S, file_size, file_sectors, free_blocks, can_import, export_sector;
	unsigned short next_link, free_link, file_bytes;

	usage = "usage: FATRW <diskfile> <import> <input-file>\n       FATRW <diskfile> <export> <starting-block> <output-file>\n";

	if(argc != 4 && argc != 5)
	{
		fprintf(stderr, "%s", usage);
		return -1;
	}
	
	jdisk = jdisk_attach(argv[1]);

	//calculate the number of FAT sectors and data sectors on the jdisk
	sectors = jdisk_size(jdisk) / JDISK_SECTOR_SIZE;
	if(sectors % 513 == 0)
		S = sectors / 513;
	else
		S = sectors / 513 + 1;
	D = sectors - S;

	//create the FAT and initialize all of the sectors to NULL
	FAT = (unsigned char**)malloc(S * sizeof(unsigned char*));

	for(i = 0; i < S; i++)
		FAT[i] = NULL;
	
	//importing
	if(argc == 4)
	{
		if(strcmp(argv[2], "import") != 0)
		{
			fprintf(stderr, "%s", usage);
			return -1;
		}
		
		fin = fopen(argv[3], "rb");
		
		if(fin < 0)
		{
			fprintf(stderr, "File %s could not be opened for reading.\n", argv[3]);
			return -1;
		}

		//get the size of the file to be imported and make sure it isn't bigger than the disk's data sectors
		fseek(fin, 0, SEEK_END);
		file_size = ftell(fin);
		fseek(fin, 0, SEEK_SET);

		if(file_size > D * JDISK_SECTOR_SIZE)
		{
			fprintf(stderr, "%s is too big for the disk (%d vs %d)\n", argv[3], file_size, D * JDISK_SECTOR_SIZE);
			return -1;
		}
		
		//calculate the number of free sectors required to store the file
		if(file_size % JDISK_SECTOR_SIZE == 0)
			file_sectors = file_size / JDISK_SECTOR_SIZE;
		else
			file_sectors = file_size / JDISK_SECTOR_SIZE + 1;

		//check the free list to see if the file can be imported, stop when enough sectors are found
		free_blocks = 0;
		next_link = Read_Link(0, jdisk, FAT);
		can_import = 0;

		while(next_link != 0)
		{
			free_blocks++;
			if(free_blocks == file_sectors)
			{
				can_import = 1;
				break;
			}
			next_link = Read_Link(next_link, jdisk, FAT);
		}
		
		//enough free sectors were found
		if(can_import)
		{
			//at most two links need to be written at the end:
			//1. The free list head link in sector (0) is always updated
			//2. The imported file's tail link will be updated to indicate that sector is the end of the file
			//written[0] contains the current number of links to updated, always <= 2
			written = (int*)malloc(3 * sizeof(int));
			written[0] = 0;

			free_blocks = 0;
			next_link = Read_Link(0, jdisk, FAT);

			//starting at the head of the free list, write the file to each sector as you go
			while(next_link != 0)
			{
				free_blocks++;
				if(free_blocks == 1)
					printf("New file starts at sector %d\n", S + next_link - 1);
				
				//do everything to clean up the last sector in the file
				if(free_blocks == file_sectors)
				{	
					//fix the free list
					free_link = Read_Link(next_link, jdisk, FAT);
					Write_Link(0, free_link, written, FAT);
				
					fread(buf, 1, file_size, fin);

					//file's last link should be 0 if it takes up the whole sector
					if(file_size == JDISK_SECTOR_SIZE)
						Write_Link(next_link, 0, written, FAT);	
					else
					{
						//last byte in this sector should be 0xff if the last block of the file takes up exactly JDISK_SECTOR_SIZE - 1 bytes
						if(file_size == JDISK_SECTOR_SIZE - 1)
							buf[JDISK_SECTOR_SIZE - 1] = 0xff;
						//otherwise the last two bytes of the sector are what remains of file_size
						else
							memcpy(buf + JDISK_SECTOR_SIZE - 2, &file_size, 2);
						
						//file's last link is equal to its entry number (link value) to indicate it is the last sector of the file, but doesn't take up the whole sector
						Write_Link(next_link, next_link, written, FAT);
					}
					//write the final block of the file
					jdisk_write(jdisk, S + next_link - 1, buf); 
					break;
				}
				//write this block of the file to this sector, decrement file_size by JDISK_SECTOR_SIZE to eventually get number of bytes in final sector
				fread(buf, 1, JDISK_SECTOR_SIZE, fin);
				file_size -= JDISK_SECTOR_SIZE;
				jdisk_write(jdisk, S + next_link - 1, buf);
				next_link = Read_Link(next_link, jdisk, FAT);
			}
			//update the FAT
			Flush_Links(jdisk, written, FAT);
			printf("Reads: %d\nWrites: %d\n", jdisk_reads(jdisk), jdisk_writes(jdisk));
			free(written);
			fclose(fin);
		}
		else
		{
			fprintf(stderr, "Not enough free sectors (%d) for %s, which needs %d.\n", free_blocks, argv[3], file_sectors);
			return -1;
		}

	}
	//exporting
	else if(argc == 5)
	{
		if(strcmp(argv[2], "export") != 0)
		{
			fprintf(stderr, "%s", usage);
			return -1;
		}
		
		fout = fopen(argv[4], "wb");

		if(fout < 0)
		{
			fprintf(stderr, "File %s could not be opened for writing.\n", argv[4]);
			return -1;
		}
		
		export_sector = atoi(argv[3]);

		//follow the file's links until the last sector is outputted to the output file
		while(1)
		{
			jdisk_read(jdisk, export_sector, buf);

			next_link = Read_Link(export_sector - S + 1, jdisk, FAT);
		
			if(next_link == 0)
			{
				//the final sector belongs entirely to the file, print it and end
				fwrite(buf, 1, JDISK_SECTOR_SIZE, fout);
				break;
			}
			else if(next_link == export_sector - S + 1)
			{
				//the final sector does not belong entirely to the file
				
				//the final sector up until the last byte belongs to the file, print it and end
				if(buf[1023] == 0xff)
					fwrite(buf, 1, 1023, fout);
				//the number of bytes in the final sector belonging to the file are determined by the last two bytes, print it and end
				else
				{
					memcpy(&file_bytes, buf + JDISK_SECTOR_SIZE - 2, 2);
					fwrite(buf, 1, file_bytes, fout);
				}
				break;
			}
			else
			{
				//not at the final sector, print this sector and continue
				fwrite(buf, 1, JDISK_SECTOR_SIZE, fout);
				export_sector = S + next_link - 1;
			}
		}
		printf("Reads: %d\nWrites: %d\n", jdisk_reads(jdisk), jdisk_writes(jdisk));
		fclose(fout);
	}

	//free memory
	
	jdisk_unattach(jdisk);

	for(i = 0; i < S; i++)
		if(FAT[i] != NULL)
			free(FAT[i]);
	
	free(FAT);
	
	return 0;
}

//reads a link from the jdisk and returns its value. If that sector has been read before, read it from the local FAT rather than the disk
unsigned short Read_Link(int link, void *jdisk, unsigned char **FAT)
{
	int total_offset, sector_offset;
	unsigned short next_link;
	
	//actual byte location on disk is the link value * 2 (each link is two bytes)
	total_offset = link * 2;
	
	//if this sector hasn't been read, read it from the disk
	if(FAT[total_offset / JDISK_SECTOR_SIZE] == NULL)
	{
		FAT[total_offset / JDISK_SECTOR_SIZE] = (unsigned char*)malloc(JDISK_SECTOR_SIZE * sizeof(unsigned char));
		jdisk_read(jdisk, total_offset / JDISK_SECTOR_SIZE, FAT[total_offset / JDISK_SECTOR_SIZE]);
	}

	//the byte location in the sector is relative just to the JDISK_SECTOR_SIZE bytes in the sector
	sector_offset = total_offset - (total_offset / JDISK_SECTOR_SIZE) * JDISK_SECTOR_SIZE;
	
	//copy the two bytes of the link into an unsigned short and return it
	memcpy(&next_link, FAT[total_offset / JDISK_SECTOR_SIZE] + sector_offset, 2);

	return next_link;
}

//write a new value to a link. Update the written pointer to indicate what sector was modified
void Write_Link(int link, unsigned short val, int *written, unsigned char **FAT)
{
	int total_offset, sector_offset;

	//actual byte location on the disk is the link value * 2 (each link is two bytes)
	total_offset = link * 2;

	//the byte location in the sector is relative just to the JDISK_SECTOR_SIZE bytes in the sector
	sector_offset = total_offset - (total_offset / JDISK_SECTOR_SIZE) * JDISK_SECTOR_SIZE;

	//written[0] indicates how many sectors have been modified so far. Put this new modification in the next open index in written
	if(written[0] == 0)
	{
		written[1] = total_offset / JDISK_SECTOR_SIZE;
		written[0] = 1;
	}
	//if the file's final link is in the same sector as the head of the free list (0), just write to sector (0) once
	else if(written[0] == 1 && total_offset / JDISK_SECTOR_SIZE != written[1])
	{
		written[2] = total_offset / JDISK_SECTOR_SIZE;
		written[0] = 2;
	}

	//copy the new value val over the old link value in the FAT
	memcpy(FAT[total_offset / JDISK_SECTOR_SIZE] + sector_offset, &val, 2);
}

//write a FAT sector to the jdisk only if it has been modified
void Flush_Links(void *jdisk, int *written, unsigned char **FAT)
{
	if(written[0] == 1 || written[0] == 2)
		jdisk_write(jdisk, written[1], FAT[written[1]]);
	if(written[0] == 2)
		jdisk_write(jdisk, written[2], FAT[written[2]]);
}
