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


// Dynamic Array to store inode types
short *inode_type;
// Dynamic array to keep track of blocks used
uint *blocks_used;
// Dynamic array to keep track of inodes visited
uint *inode_visited;
// Dynamic array to keep tack of how many times a directory was seen 
uint *dir_visited;
// Number of inodes in the file system
uint ninodes; 


//Function to recursive travere through the file system if a directory is found
void recursive_dir(ushort inum, char* addr, struct dinode *dip, uint num_blocks)
{
     int temp, i, j;
     //Variable to store directory entries
     struct dirent *temp_de;
     //Cast to directory entry structure the address of the directory
     temp_de = (struct dirent *) (addr + (dip[inum].addrs[0])*BLOCK_SIZE);
     //Find the number of entries 
     temp = dip[inum].size/sizeof(struct dirent);
          
     //Iterate through all the entries
     for(j = 0; j < temp; j++, temp_de++)
     {
       //Check if the first entry is "." (itself)
       if(strcmp(temp_de->name, ".") != 0 && j == 0)
       {
         fprintf(stderr, "ERROR: directory not properly formatted.\n");
         exit(1);
       }
       //Check if the second entry is ".." (parent)
       if(strcmp(temp_de->name, "..") != 0 && j == 1)
       {
         fprintf(stderr, "ERROR: directory not properly formatted.\n");
         exit(1);
       }
       
       //If directory is not parent or itself, count it as seen
       if(j >= 2)
       {
         dir_visited[temp_de->inum]++;
       }
       
       //Increment inode visited count
       inode_visited[temp_de->inum]++;
       
       //Set the inode type for the inode being traversed in the array keeping track of inode type
       inode_type[temp_de->inum] = dip[temp_de->inum].type;
       
       //Iterate through the direct address blocks
       for(i = 0; i < NDIRECT; i++)
       {
         //Check if the direct address is valid
         if(dip[temp_de->inum].addrs[i] > num_blocks)
         {
             fprintf(stderr, "ERROR: bad direct address in inode.\n");
             exit(1);
         }
         
         //check if the block addressed has already been used even if the inode is being visited the first time 
         if(inode_visited[temp_de->inum] == 1 && blocks_used[dip[temp_de->inum].addrs[i]] == 1 && dip[temp_de->inum].addrs[i] != 0 )
         {
             fprintf(stderr, "ERROR: direct address used more than once.\n");
             exit(1);
         }   
         
         //Update the block addressed in the array of blocks used
         blocks_used[dip[temp_de->inum].addrs[i]] = 1;
         
         //Set a pointer to point to the address of bitmap block
         char *bitmap = addr + (BBLOCK(dip[temp_de->inum].addrs[i], ninodes)) * BSIZE;
         
         //byte offset that contains the bit for the block
         int block_byte = dip[temp_de->inum].addrs[i]/8;
         //bit offset within the byte block
         int block_bit = dip[temp_de->inum].addrs[i]%8;
         
         //Bit wise operation to check if the bitmap bit is 0
         if (!(bitmap[block_byte] & (0x1 << block_bit))) 
         {
                fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                exit(1);
         }
         
       }
       //check if thr indirect address pointer block is allocated
       if(dip[temp_de->inum].addrs[NDIRECT] != 0)
       {
         //If allocated mark the block as used
         blocks_used[dip[temp_de->inum].addrs[NDIRECT]] = 1;  
         //Set a pointer to the indirect address pointer block to iterate through the entries
         uint *bptr = (uint*) (addr + dip[temp_de->inum].addrs[NDIRECT] * BSIZE);
         
         //Iterate through the indirect addresses
         for(i = 0; i < NINDIRECT; i++)
         {
           //Check if the indirect address is valid
           if(bptr[i] > num_blocks)
           {
             fprintf(stderr, "ERROR: bad indirect address in inode.\n");
             exit(1);
           }
           //check if the block addressed has already been used even if the inode is being visited the first time 
           if(inode_visited[temp_de->inum] == 1 && blocks_used[bptr[i]] == 1 && bptr[i] != 0 )
           {
               fprintf(stderr, "ERROR: indirect address used more than once.\n");
               exit(1);
           }
           //mark the block addressed to be used
           blocks_used[bptr[i]] = 1;
           //Get the bitmap block with the block addresses. 
           char *bitmap = addr + (BBLOCK(bptr[i], ninodes)) * BSIZE;
           
           //byte offset that contains the bit for the block
           int block_byte = bptr[i]/8;
           //bit offset within the byte block
           int block_bit = bptr[i]%8;
           
           //Bit wise operation to check if the bitmap bit is 0
           if (!(bitmap[block_byte] & (0x1 << block_bit))) 
           {
                  fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                  exit(1);
           }
         }
       }
       
       //If the inode is a directory and not itself or parent, traverse through the directory
       if(dip[temp_de->inum].type == 1 && j >= 2)
       {
         recursive_dir(temp_de->inum, addr, dip, num_blocks);
       }
     }
}

//Function to find a given inode in the directory
int findDir(int inumtofind, int ninodes, char *addr)
{
    struct dinode *inode;
    struct dirent *dir;
     
    //Variable to keep track of how many times an inode is seen 
    int count = 0;
    
    //Counter variables for loops
    int j,k, l;
    
    //Iterate through all the inodes
    for(j = 0; j < ninodes; j++)
    {
        //if the inode is the inode being searched and inode is not the root directory, skip 
        if(j == inumtofind && j!= 1)
        {
            continue;
        }
        
        //retrieve the inode into a structure from the address
        inode = (struct dinode *) (addr + 2*BLOCK_SIZE + (j * sizeof(struct dinode)));
        
        //If the inode is a directory skip 
        if(inode->type != 1)
        {
          continue;
        }
        
        //Iterate through all the direct addresses blocks
        for(k = 0; k < NDIRECT; k++)
        {
            //If not allocated, skip
            if(inode->addrs[k] == 0)
            {
                continue;
            }
            
            //Get the starting directory entry for the block addressed
            dir = (struct dirent *) (addr + inode->addrs[k]*BLOCK_SIZE);
            
            //Iterate through all the directory entries
            for(l = 0; l < BSIZE / sizeof(struct dirent); l++, dir++)
            {
                //If the inode found in the directory is the inode searched for increment count
                if(dir->inum == inumtofind)
                {
                    count++;
                }

            }

        }

        uint *bn; 
       
        //Check if the indirect block is allocated
        if(inode->addrs[NDIRECT] != 0)
        {
           //Get starting indirect block addressed
           bn = (uint*) (addr + inode->addrs[NDIRECT] * BSIZE);
           //Iterate through all the indirectly addressed blocks
           for(k = 0; k < NINDIRECT; k++)
           {
             //If not allocated, skip
             if(bn[k] == 0)
             {
               continue;
             }
            
             //Get the starting directory entry for the block addressed
             dir = (struct dirent *) (addr + bn[k]*BLOCK_SIZE);
           
             //Iterate through all the directory entries
             for(l = 0; l < BSIZE / sizeof(struct dirent); l++, dir++)
             {
               //If the inode found in the directory is the inode searched for increment count
               if(dir->inum == inumtofind)
               {
                 count++;
               }

             }
            
           }
        }
    }
    
    //If the inode was found return the count
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

  //Check if the valid arguments were passed
  if(argc < 2){
    fprintf(stderr, "Usage: fcheck <file_system_image>\n");
    exit(1);
  }

  //Open the file system image
  fsfd = open(argv[1], O_RDONLY);

  //If file did not open or does not exist
  if(fsfd < 0){
    fprintf(stderr, "image not found.\n");
    exit(1);
  }
  
  //Get fstat of the image file
  if (fstat(fsfd, &image) != 0)
  {
      perror("fstat() error");
      exit(1);
  }
  
  //Map and get the address of file system image
  addr = mmap(NULL, image.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
  
  //Check if mapping was successful
  if (addr == MAP_FAILED){
	perror("mmap failed");
	exit(1);
  }
  
  //read the super block 
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);
  
  //Store the number of inodes in a global variable
  ninodes = sb->ninodes; 
  
  //Allocate all the arrays
  inode_type = malloc(sb->ninodes * sizeof(short)); 
  blocks_used = malloc(sb->size * sizeof(uint));
  inode_visited = malloc(sb->ninodes * sizeof(uint)); 
  dir_visited = malloc(sb->ninodes * sizeof(uint)); 
  
  int inode_num;
  //Initalize all the arrays
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
  
  // read the inodes 
  dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE); 

  //Set root inode's type
  inode_type[1] = dip[ROOTINO].type;
  
  // get the address of root dir 
  de = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE);

  //Check if root directory exists
  if(de == NULL)
  {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  
  //Check if the root directory points to the correct inode
  if(de->inum != 1)
  {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  de++;
  
  //Check if the root directory points to the correct inode
  if(de->inum != 1)
  {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  de--;
 
  //Call function to traverse through the file system  
  recursive_dir(de->inum, addr, dip, sb->nblocks);
  
  // Iterate through the array of inode types
  for(inode_num = 1; inode_num < sb->ninodes; inode_num++)
  {
    //Check if the inode type is valid
    if(inode_type[inode_num] < 0 || inode_type[inode_num] > 3)
    {
      fprintf(stderr, "ERROR: bad inode.\n");
      exit(1);
    }
  }
  
  //Ignore bit map for file system blocks and iterate through the data blocks
  for(k = BBLOCK(sb->size, sb->ninodes) + 1; k < BBLOCK(sb->size, sb->ninodes) + sb->nblocks + 1; k++)
  {
     //Get the bitmap block for the data block. 
     char *bitmap = addr + (BBLOCK(k, sb->ninodes)) * BSIZE;
     
     //byte offset that contains the bit for the block
     int block_byte = k / 8;
     //bit offset within the byte block
     int block_bit = k % 8;
     
     //Bit wise operation to check if the bitmap bit is 1
     if ((bitmap[block_byte] & (0x1 << block_bit))) {
          
          //Check if the block was used
          if(blocks_used[k] != 1) 
          {    
              fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
              exit(1);
          }
      }
    
  }

  //Iterate through all the inodes
  for(inode_num = 2; inode_num < sb->ninodes; inode_num++)  
  {
    //Get the count for how many times the inode is seen in the directory
    int inode_ref = findDir(inode_num, sb->ninodes, addr);
    
    //Check if inode is in use, it is found in the directory
    if(dip[inode_num].type != 0 && inode_ref == 0)
    {
        fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
        exit(1);
    }
    
    //Check if the inode is not in use but is found in the directory
    if(dip[inode_num].type == 0 && inode_ref > 0)
    {
        fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
        exit(1);
    }
    
    //Check if the reference count matches number of times the inode is seen in the directory
    if(dip[inode_num].nlink != inode_ref && dip[inode_num].type != 1)
    {
        fprintf(stderr, "ERROR: bad reference count for file.\n");
        exit(1);
    }

    //Check if a directory only has one link other than itself
    if(dip[inode_num].nlink != 1 && dip[inode_num].type == 1)
    {
        fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
        exit(1);
    }
    
    //Check if the directory is found in more places through the file system
    if(dip[inode_num].nlink == 1 && dip[inode_num].type == 1 && dir_visited[inode_num]  > 1)
    {
        fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
        exit(1);
    }
    
  }
  exit(0);

}

