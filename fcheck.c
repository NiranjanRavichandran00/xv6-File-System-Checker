#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types.h"
#include "fs.h"

#define BLOCK_SIZE (BSIZE)


short *inode_type;
uint *blocks_used;
uint *inode_visited;
uint *dir_visited;
uint ninodes; 

void recursive_dir(ushort inum, char* addr, struct dinode *dip, uint num_blocks)
{
     int temp, i, j;
     struct dirent *temp_de;
     temp_de = (struct dirent *) (addr + (dip[inum].addrs[0])*BLOCK_SIZE);
     temp = dip[inum].size/sizeof(struct dirent);
     
     
     
     for(j = 0; j < temp; j++, temp_de++)
     {
       
       //printf(" inum %d, name %s \n", temp_de->inum, temp_de->name);
       if(strcmp(temp_de->name, ".") != 0 && j == 0)
       {
         fprintf(stderr, "ERROR: directory not properly formatted.\n");
         exit(1);
       }
       
       if(strcmp(temp_de->name, "..") != 0 && j == 1)
       {
         fprintf(stderr, "ERROR: directory not properly formatted.\n");
         exit(1);
       }

        if(j >= 2)
        {
            dir_visited[temp_de->inum]++;

        }
       
       
       inode_visited[temp_de->inum]++;
       
       
       /*if(temp_de->inum == 0)
       {
         break;
       }
       */
       //printf(" inum %d, name %s ", temp_de->inum, temp_de->name);
	     //printf("inode  size %d links %d type %d \n", dip[temp_de->inum].size, dip[temp_de->inum].nlink, dip[temp_de->inum].type);
       inode_type[temp_de->inum] = dip[temp_de->inum].type;
       
       //Check 2 
       for(i = 0; i < NDIRECT; i++)
       {
         //printf("Block num: %d\n", dip[temp_de->inum].addrs[i]);
         if(dip[temp_de->inum].addrs[i] > num_blocks)
         {
             fprintf(stderr, "ERROR: bad direct address in inode.\n");
             exit(1);
         }
         
         if(inode_visited[temp_de->inum] == 1 && blocks_used[dip[temp_de->inum].addrs[i]] == 1 && dip[temp_de->inum].addrs[i] != 0 )
         {
             fprintf(stderr, "ERROR: direct address used more than once.\n");
             exit(1);
         }   
         //printf("Blocks Direct: %d \n", dip[temp_de->inum].addrs[i]);
         blocks_used[dip[temp_de->inum].addrs[i]] = 1;
         //Bitmap check
         char *bitmap = addr + (BBLOCK(dip[temp_de->inum].addrs[i], ninodes)) * BSIZE;
         
         int block_byte = dip[temp_de->inum].addrs[i]/8;
         int block_bit = dip[temp_de->inum].addrs[i]%8;
         
         if (!(bitmap[block_byte] & (0x1 << block_bit))) 
         {
                fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                exit(1);
         }
         //printf("block num: %d bit pos: %d \n", block_num,  (0x1 << (block_num % 8)));
       }
       
       if(dip[temp_de->inum].addrs[NDIRECT] != 0)
       {
         blocks_used[dip[temp_de->inum].addrs[NDIRECT]] = 1;  
         uint *bptr = (uint*) (addr + dip[temp_de->inum].addrs[NDIRECT] * BSIZE);
         
         for(i = 0; i < NINDIRECT; i++)
         {
           if(bptr[i] > num_blocks)
           {
             fprintf(stderr, "ERROR: bad indirect address in inode.\n");
             exit(1);
           }
           if(inode_visited[temp_de->inum] == 1 && blocks_used[bptr[i]] == 1 && bptr[i] != 0 )
           {
               fprintf(stderr, "ERROR: indirect address used more than once.\n");
               exit(1);
           }
           //printf("Blocks In: %d \n", bptr[i]);
           blocks_used[bptr[i]] = 1;
           char *bitmap = addr + (BBLOCK(bptr[i], ninodes)) * BSIZE;
           
           //Byte is 8 bits
           int block_byte = bptr[i]/8;
           int block_bit = bptr[i]%8;
           
           if (!(bitmap[block_byte] & (0x1 << block_bit))) 
           {
                  fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                  exit(1);
           }
         }
       }
       
       if(dip[temp_de->inum].type == 1 && j >= 2)
       {
         recursive_dir(temp_de->inum, addr, dip, num_blocks);
       }
     }
}

int findDir(int inumtofind, int ninodes, char *addr)
{
    struct dinode *inode;
    struct dirent *dir;

    int count = 0;

    int j,k, l;
    for(j = 0; j < ninodes; j++)
    {
        if(j == inumtofind && j!= 1)
        {
            continue;
        }
        inode = (struct dinode *) (addr + 2*BLOCK_SIZE + (j * sizeof(struct dinode)));
        
        if(inode->type != 1)
        {
          continue;
        }
        
        for(k = 0; k < NDIRECT; k++)
        {
            if(inode->addrs[k] == 0)
            {
                continue;
            }
            
            dir = (struct dirent *) (addr + inode->addrs[k]*BLOCK_SIZE);
            for(l = 0; l < BSIZE / sizeof(struct dirent); l++, dir++)
            {
                if(dir->inum == inumtofind)
                {
                    count++;
                }

            }

        }

       uint *bn; 
       if(inode->addrs[NDIRECT] != 0)
       {
        bn = (uint*) (addr + inode->addrs[NDIRECT] * BSIZE);
        for(k = 0; k < NINDIRECT; k++)
        {
            if(bn[k] == 0)
            {
                continue;
            }
            
            dir = (struct dirent *) (addr + bn[k]*BLOCK_SIZE);
            for(l = 0; l < BSIZE / sizeof(struct dirent); l++, dir++)
            {
                if(dir->inum == inumtofind)
                {
                    count++;
                }

            }
            //printf("Type test: %d \n", inode->type);
        }
      }
    }
    
    if(count > 0)
    {
        return count;   
    }
    return 0;

}

int
main(int argc, char *argv[])
{
  int fsfd;
  char *addr;
  struct dinode *dip;
  struct superblock *sb;
  struct dirent *de;
  struct stat image;

  if(argc < 2){
    fprintf(stderr, "Usage: fcheck <file_system_image>\n");
    exit(1);
  }


  fsfd = open(argv[1], O_RDONLY);

  if(fsfd < 0){
    fprintf(stderr, "image not found.\n");
    exit(1);
  }
  
  if (fstat(fsfd, &image) != 0)
  {
      perror("fstat() error");
      exit(1);
  }
  
  //printf("Size: %d \n\n\n", image.st_size);
  
  /* Dont hard code the size of file. Use fstat to get the size */
  addr = mmap(NULL, image.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
  if (addr == MAP_FAILED){
	perror("mmap failed");
	exit(1);
  }
  /* read the super block */
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);
  //printf("fs size %d, no. of blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);
  ninodes = sb->ninodes; 
  
  //printf("Size superblock : %d \n", sb->size);
  inode_type = malloc(sb->ninodes * sizeof(short)); 
  blocks_used = malloc(sb->size * sizeof(uint));
  inode_visited = malloc(sb->ninodes * sizeof(uint)); 
  dir_visited = malloc(sb->ninodes * sizeof(uint)); 
  
  int inode_num;
  for(inode_num = 0; inode_num < sb->ninodes; inode_num++)
  {
    inode_type[inode_num] = 0;
    inode_visited[inode_num] = 0;
    dir_visited[inode_num] = 0;
  }
  
  int k;
  for(k = 0; k < sb->size; k++)
  {
      blocks_used[k] = 0;
  }

  
  /* read the inodes */
  dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE); 
  //printf("begin addr %p, begin inode %p , offset %d \n", addr, dip, (char *)dip -addr);

  // read root inode
  //printf("Root inode  size %d links %d type %d \n", dip[ROOTINO].size, dip[ROOTINO].nlink, dip[ROOTINO].type);

  inode_type[1] = dip[ROOTINO].type;
  // get the address of root dir 
  de = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE);

  // print the entries in the first block of root dir 
  
  //Check 3
  if(de == NULL)
  {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  
  if(de->inum != 1)
  {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  de++;
  if(de->inum != 1)
  {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  de--;
  //n = dip[ROOTINO].size/sizeof(struct dirent);
  
  //printf("No of entries : %d \n", n);
  
  recursive_dir(de->inum, addr, dip, sb->nblocks);
  
  
  
  // Check 1
  for(inode_num = 1; inode_num < sb->ninodes; inode_num++)
  {
    if(inode_type[inode_num] < 0 || inode_type[inode_num] > 3)
    {
      fprintf(stderr, "ERROR: bad inode.\n");
      exit(1);
    }
  }
  
  //Ignore bit map for file system blocks
  for(k = BBLOCK(sb->size, sb->ninodes) + 1; k < BBLOCK(sb->size, sb->ninodes) + sb->nblocks + 1; k++)
  {
  
     char *bitmap = addr + (BBLOCK(k, sb->ninodes)) * BSIZE;
     
     int block_byte = k / 8;
     int block_bit = k % 8;
     
      if ((bitmap[block_byte] & (0x1 << block_bit))) {
      
          //printf("Block: %d \n", k);
          
          if(blocks_used[k] != 1) {
              //printf("Block failed: %d \n", k);
              fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
              exit(1);
          }
      }
    
  }

  for(inode_num = 2; inode_num < sb->ninodes; inode_num++)  
  {
     if(dip[inode_num].type != 0 && findDir(inode_num, sb->ninodes, addr) == 0)
     {
        fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
        exit(1);
     }

    if(dip[inode_num].type == 0 && findDir(inode_num, sb->ninodes, addr) > 0)
    {
        fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
        exit(1);
    }
    
    if(dip[inode_num].nlink != findDir(inode_num, sb->ninodes, addr) && dip[inode_num].type != 1)
    {
        fprintf(stderr, "ERROR: bad reference count for file.\n");
        exit(1);
    }

    if(dip[inode_num].nlink != 1 && dip[inode_num].type == 1)
    {
        //printf("Inode num: %d Inode links: %d Links found: %d Inode visited: %d\n", inode_num, dip[inode_num].nlink, findDir(inode_num, sb->ninodes, addr), inode_visited[inode_num]);
        fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
        exit(1);
    }
    
    if(dip[inode_num].nlink == 1 && dip[inode_num].type == 1 && dir_visited[inode_num]  > 1)
    {
        //printf("Inode num: %d Inode links: %d Links found: %d Inode visited: %d\n", inode_num, dip[inode_num].nlink, findDir(inode_num, sb->ninodes, addr), inode_visited[inode_num]);
        fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
        exit(1);
    }
    
    
  }
  exit(0);

}

