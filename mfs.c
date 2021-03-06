/*

  Eric Zelaya 
  1000861438
  Section 003

*/
//FAT32 file system (file allocation table)
//how files are stored

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

// string file = dir[i].DIR_NAME;
// file = file.substr(0,11);
// cout << file << "\n";
//printf("--%s--\n", token[1]);
//printf("name %s\n", name);

#define WHITESPACE " \t\n" // We want to split our command line up into tokens \
                           // so we need to define what delimits our tokens.   \
                           // In this case  white space                        \
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5 // Mav shell only supports five arguments
#define BPB_BytsPerSec_Offset 11
#define BPB_BytsPerSec_Size 2

#define BPB_SecPerClus_Offset 13
#define BPB_SecPerClus_Size 1

#define BPB_RsvdSecCnt_Offset 14
#define BPB_RsvdSecCnt_Size 2

#define BPB_NumFATS_Offset 16
#define BPB_NumFATS_Size 2

#define BPB_RootEntCnt_Offset 17
#define BPB_RootEntCnt_Size 2

#define BPB_FATSz32_Offset 36
#define BPB_FATSz32_Size 4

#define BS_VolLab_Offset 36
#define BS_VolLabb 71
#define BS_VolLab_Size 11

FILE *fp;
int file_open = 0;

//fat32 layout, storing vars
int16_t BPB_BytsPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATS;
int16_t BPB_RootEntCnt;
char BS_VolLab[11];
int32_t BPB_FATSz32;
int32_t BPB_RootClus;

int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;

char dirVolName[11];
int did_not_find = 0;

int rootDir = 0;
int curDir = 0;
int i, inside = 0, ad = 0, j; //inside is if u inside the dir, if its found, if not the input dir dont exist
//ad for the current directory being used (ad for address when passed to LBAToOffset)

struct __attribute__((__packed__)) DirectoryEntry
{
  char DIR_NAME[11];
  uint8_t DIR_Attr;
  u_int8_t Unused1[8];
  u_int16_t DIR_FirstClusterHigh;
  u_int8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];

//Finds the starting address for a block of data given the sector number corresponding to that data block
int LBAToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec);
}
//Given a logical block address, look up into the first FAT and there is no futher block then return -1
int16_t NextLB(uint32_t sector)
{
  uint32_t FATAdderess = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp, FATAdderess, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

int main()
{

  char *cmd_str = (char *)malloc(MAX_COMMAND_SIZE);
  int root_clus_Address;
  int get = 0;

  while (1)
  {
    // Print out the msh prompt
    printf("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(cmd_str, MAX_COMMAND_SIZE, stdin))
      ;

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str = strdup(cmd_str);

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your shell functionality

    // int token_index  = 0;
    // for( token_index = 0; token_index < token_count; token_index ++ )
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index] );
    // }
    //opens the file system if the input is open
    if (token[0] == NULL) //continue if press enter
    {
      continue;
    }
    if (strcasecmp(token[0], "quit") == 0 || strcasecmp(token[0], "exit") == 0)
    {
      exit(0);
    }
    if (strcasecmp(token[0], "open") == 0) //opening file fat32.img
    {
      if (token[1] == NULL)
      {
        printf("Error: File system image not found.\n");
        continue;
      }
      if (file_open == 1)
      {
        printf("Error: File system image already open.\n");
        continue;
      }

      fp = fopen(token[1], "r");
      if (fp == NULL)
      {
        printf("Error: File system image not found.\n");
      }
      else
      {
        //fread paramters are:
        //1: where is my dest
        //2: size of item i want to read
        //3: num of items i want to read
        file_open = 1;

        fseek(fp, 11, SEEK_SET);
        fread(&BPB_BytsPerSec, 2, 1, fp); //bytes per sector

        fseek(fp, 13, SEEK_SET);
        fread(&BPB_SecPerClus, 1, 1, fp); //sector per cluster

        fseek(fp, 14, SEEK_SET);
        fread(&BPB_RsvdSecCnt, 1, 2, fp); //reserved sector

        fseek(fp, 16, SEEK_SET);
        fread(&BPB_NumFATS, 1, 1, fp); //num of FATS

        fseek(fp, 36, SEEK_SET);
        fread(&BPB_FATSz32, 1, 4, fp);

        //BPB_RootEntCnt
        fseek(fp, BPB_RootEntCnt_Offset, SEEK_SET);
        fread(&BPB_RootEntCnt, BPB_RootEntCnt_Size, 1, fp);

        //BS_VolLab
        fseek(fp, BS_VolLab_Offset, SEEK_SET);
        fread(&BS_VolLab, BS_VolLab_Size, 1, fp);

        fseek(fp, BS_VolLabb, SEEK_SET);
        fread(&dirVolName, 1, 11, fp);

        //update the root offset
        root_clus_Address = (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
        //printf("offset is not %d\n", root_clus_Address);

        //calculating address of root directory
        rootDir = (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
        //printf("%x\n", root_clus_Address);
        curDir = rootDir;
        //allocating 32 bytes of struct
        fseek(fp, rootDir, SEEK_SET);

        int i;
        for (i = 0; i < 16; i++)
        {
          fread(&dir[i], sizeof(struct DirectoryEntry), 1, fp);
        }

        // for(i=0; i < 16; i++)
        // {
        //   printf("%s\n", dir[i].DIR_NAME);
        // }
      }
    }

    if (strcasecmp(token[0], "close") == 0) //if closing file
    {
      if (file_open == 0) //if file already close
      {
        printf("Error: File system not open.\n");
      }
      else
      {
        file_open = 0; //close the file
        fclose(fp);
        fp = NULL;
      }
    }
    if (strcasecmp(token[0], "info") == 0) //printing info
    {
      printf("\n  BPB_BytsPerSec: %d %x\n", BPB_BytsPerSec, BPB_BytsPerSec);
      printf("  BPB_SecPerClus: %d %x\n", BPB_SecPerClus, BPB_SecPerClus);
      printf("  BPB_RsvdSecCnt: %d %x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
      printf("     BPB_NumFATS: %d %x\n", BPB_NumFATS, BPB_NumFATS);
      printf("     BPB_FATSz32: %d %x\n\n", BPB_FATSz32, BPB_FATSz32);
    }

    //stat should give you info about the file
    //what attribute value it is
    //what the size is
    //and what the starting cluster is
    //refer to prof video at 9:00
    if (strcasecmp(token[0], "stat") == 0) //needs to print for a specefic given file
    {
      //if it didnt find file give right error
      int did_not_find = 0;
      if (file_open == 0) //if file already close
      {
        printf("Error: File system not open.\n");
      }
      //if user only gives stat and no filename/folder skip
      if (token[1] != NULL)
      {

        //printf("inside stat\n");
        //lets see if the user has used a . so we know whether to look for a file or folder
        //flag is found a period in user input
        int period;
        int flag_for_period_found = 0;
        for (period = 0; period < strlen(token[1]); period++)
        {
          if (token[1][period] == '.') //if we find a period, Lets excute a file** else ** a folder
          {
            flag_for_period_found = 1;
          }
        }
        if (flag_for_period_found == 1) //Look for file type
        {
          int i;
          for (i = 0; i < 16; i++)
          {
            char name[12]; //adding a null terminate to end of file names
            memcpy(name, dir[i].DIR_NAME, 11);
            name[11] = '\0';

            //add chars to new char
            //char by char
            char new_name[12];
            int j;
            int g;
            for (j = 0, g = 0; j < 8; j++)
            {
              if (name[j] != ' ')
              {
                new_name[g] = name[j];
                g++;
                new_name[g] = '.';
              }
            }
            g++;
            for (j = 8; j < 11; j++)
            {
              new_name[g] = name[j];
              g++;
              new_name[g] = '\0';
            }

            if (strcasecmp(token[1], new_name) == 0)
            {
              {
                printf("\n\t%s\n", new_name);
                printf("\tAttribute is: %d\n", dir[i].DIR_Attr);
                printf("\tFile size is: %d\n", dir[i].DIR_FileSize);
                printf("\tStarting Cluster Number is: %d\n\n", dir[i].DIR_FirstClusterLow);
                did_not_find = 1;
              }
            }
          }
        }
        else //Look for folder
        {
          //this is not done
          //this will have to change with more if statements
          int i;
          for (i = 0; i < 16; i++)
          {
            char name[12]; //adding a null terminate to end of file names
            memcpy(name, dir[i].DIR_NAME, 11);
            name[11] = '\0';

            //were gonna make a new char and give it the new name
            //this will be the name we will compare to show stats
            char new_name[12];

            int j;
            int g = 0;
            for (j = 0; j < 11; j++)
            {
              if (name[j] != ' ')
              {
                new_name[g] = name[j];
                g++;
                new_name[g] = '\0';
              }
            }

            if (strcasecmp(token[1], new_name) == 0)
            {
              {
                printf("\n\t%s\n", new_name);
                printf("\tAttribute is: %d\n", dir[i].DIR_Attr);
                printf("\tFile size is: %d\n", dir[i].DIR_FileSize);
                printf("\tStarting Cluster Number is: %d\n\n", dir[i].DIR_FirstClusterLow);
                did_not_find = 1;
              }
            }
          }
        }
        if (did_not_find == 0)
        {
          printf("Error: File not found\n");
        }
      }
      else
      {
        printf("Error: Not a valid command\n");
      }
    }
    if (strcasecmp(token[0], "get") == 0)
    {
      if (token[1] != NULL)
      {

        int i;
        //this is the same as stat to parse and compare strings
        char new_name[12];
        int did_not_find = 0;

        for (i = 0; i < 16; i++)
        {
          char name[12]; //adding a null terminate to end of file names
          memcpy(name, dir[i].DIR_NAME, 11);
          name[11] = '\0';

          int j;
          int g;
          for (j = 0, g = 0; j < 8; j++)
          {
            if (name[j] != ' ')
            {
              new_name[g] = name[j];
              g++;
              new_name[g] = '.';
            }
          }
          g++;
          for (j = 8; j < 11; j++)
          {
            new_name[g] = name[j];
            new_name[g] = tolower(new_name[g]);
            g++;
            new_name[g] = '\0';
          }

          if (strcasecmp(token[1], new_name) == 0)
          {
            did_not_find = 1;
            //open a new file in current directory and give the same name as file you want to copy
            FILE *newfp = fopen(new_name, "wb");

            int file_size = dir[i].DIR_FileSize;
            int LowClusterNumber = dir[i].DIR_FirstClusterLow;
            int offset = LBAToOffset(LowClusterNumber);
            //make malloc to get empty chars and enough space
            unsigned char *get_chars = (unsigned char *)malloc(file_size);

            while (file_size > 512)
            {
              fseek(fp, offset, SEEK_SET);
              fread(get_chars, 1, 512, fp);
              file_size = file_size - 512;

              //find the new logical block
              LowClusterNumber = NextLB(LowClusterNumber);

              if (LowClusterNumber == -1)
              {
                break;
              }

              offset = LBAToOffset(LowClusterNumber);
              fwrite(get_chars, 1, 512, newfp);
            }
            if (file_size > 0)
            {
              fseek(fp, offset, SEEK_SET);
              fread(get_chars, 1, file_size, fp);
              fwrite(get_chars, 1, file_size, newfp);
            }
            fclose(newfp);
            free(get_chars);
          }
        }
        if (did_not_find == 0)
        {
          printf("Error: File not found\n");
        }
      }
      else
      {
        printf("Error: Not a valid command\n");
      }
    }

    if (strcasecmp(token[0], "ls") == 0)
    {
      char name[12];
      if (file_open == 0) //if file already close
      {
        printf("Error: File system not open.\n");
      }
      if (fp != NULL)
      {

        for (i = 0; i < 16; i++)
        {
          if ((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20) && dir[i].DIR_NAME[0] != -27) //bascially print the acutal files not the junk with it (the deleted files or nulls)
          {
            //adding a null terminate to end of file names
            memcpy(name, dir[i].DIR_NAME, 11);
            name[11] = '\0';
            printf("%.11s\n", name);
          }
        }
      }
      if (token[1] != NULL)
      {
        //printf("ls ..\n");
        if (strcasecmp(token[1], "..") == 0)
        {
          int l;
          char name[12];
          for (l = 0; l < 16; l++)
          {
            //adding a null terminate to end of file names
            memcpy(name, dir[l].DIR_NAME, 11);
            name[11] = '\0';

            int q;
            for (q = 0; q < 12; q++) //filling empty spaces with null to cmp the folder name
            {
              if (name[q] == ' ')
              {
                name[q] = '\0';
              }
            }
            if (strcasecmp(token[1], name) == 0)
            {
              if (dir[l].DIR_FirstClusterLow == 0)
              {
                get = LBAToOffset(2);
              }
              else
              {
                ad = dir[l].DIR_FirstClusterLow;
                get = LBAToOffset(ad);
              }
              fseek(fp, get, SEEK_SET);
              int w;
              for (w = 0; w < 16; w++)
              {
                fread(&dir[w], 1, 32, fp);
              }
              printf("Previous files: ");
              for (i = 0; i < 16; i++)
              {
                if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
                {
                  memcpy(name, dir[i].DIR_NAME, 11);
                  name[11] = '\0';
                  if (dir[i].DIR_Attr == 0x10)
                  {
                    if (name[0] != -27)
                    {
                      int u;
                      for (u = 0; u < 12; u++)
                      {
                        if (name[u] == ' ')
                        {
                          name[u] = '\0';
                        }
                      }
                      printf("%s ", name);
                    }
                  }
                }
              }
              printf("\n");
            }
          }
        }
      }
    }
    //fseek to BS_volab inside info calculations and print volume name
    if (strcasecmp(token[0], "volume") == 0)
    {
      if (file_open == 0)
      {
        printf("Error: File system is not open.\n");
        continue;
      }
      if (dirVolName == NULL) //if not found, creats warning but should works fine
      {
        printf("Error: Volume not found.\n");
      }
      else
      {
        printf("Volume Name :'%s'\n", dirVolName);
      }
    }

    if (strcasecmp(token[0], "cd") == 0)
    {
      if (file_open == 0) //if file already close
      {
        printf("Error: File system not open.\n");
      }
      else if (token[1] == NULL) //initially typing cd, goes back to root
      {
        int ad = LBAToOffset(2);
        fseek(fp, ad, SEEK_SET);
        int j;
        for (j = 0; j < 16; j++)
        {
          fread(&dir[j], 1, 32, fp);
        }
      }
      else
      {
        if (token[1] == NULL) //typing cd while being in the directories
        {
          int ad = LBAToOffset(2);
          fseek(fp, ad, SEEK_SET);
          int j;
          for (j = 0; j < 16; j++)
          {
            fread(&dir[j], 1, 32, fp);
          }
        }
        else //cd ing into the files
        {
          char *paths = token[1]; //if input is a path, determine # of files in path
          int n;
          int length = 1;
          int size = strlen(token[1]);
          for (n = 0; n < size; n++)
          {
            if (paths[n] == '/')
            {
              length++;
            }
          }
          int start = 0;      //start from 1st file name, then 2nd and so on and this need to be init out of the loop!!!
          while (length >= 1) //will excecute cd for 1 or more files
          {
            //if there is a path of files, then enter into the file one at a time
            //first store the first file input name and fseek there and then the next
            int go; //iterate through each of the file input(paths)
            char input_file[12];
            memset(&input_file, 0, 12);

            int f = strlen(token[1]); //length of while path

            int in = 0; //input file storer iterator

            int temp = length;
            for (go = start; go < f; go++)
            {
              if (token[1][go] == '/')
              {
                start = go + 1; //next time loop execute it will the get the position of the 2nd file name
                //printf("%d\n", go);
                break;
              }
              else
              {
                input_file[in] = token[1][go];
                in++;
              }
            }
            //printf("%s\n", input_file);
            int i, k;
            for (i = 0; i < 16; i++)
            {
              char name[12]; //adding a null terminate to end of file names
              memcpy(name, dir[i].DIR_NAME, 11);
              name[11] = '\0';

              int q;
              for (q = 0; q < 12; q++) //filling empty spaces with null to cmp the folder name
              {
                if (name[q] == ' ')
                {
                  name[q] = '\0';
                }
              }
              if (strcasecmp(name, input_file) == 0) //checking if input is directory
              {
                inside = 1;                            //if not, no such dir exist
                if (strcasecmp(input_file, "..") == 0) //go back a dir
                {
                  //printf("-in dot dot-\n");
                  if (dir[i].DIR_FirstClusterLow == 0)
                  {
                    get = LBAToOffset(2);
                  }
                  else
                  {
                    ad = dir[i].DIR_FirstClusterLow;
                    get = LBAToOffset(ad);
                  }
                  fseek(fp, get, SEEK_SET);
                  int k;
                  for (k = 0; k < 16; k++)
                  {
                    fread(&dir[k], 1, 32, fp);
                  }
                }
                else //go inside a dir
                {
                  ad = dir[i].DIR_FirstClusterLow;
                  get = LBAToOffset(dir[i].DIR_FirstClusterLow);
                  fseek(fp, get, SEEK_SET);

                  for (k = 0; k < 16; k++)
                  {
                    fread(&dir[k], 1, 32, fp);
                  }
                }
              }
            }
            if (inside == 0)
            {
              printf("%s is not a directory \n", token[1]);
            }
            length--;
          }
        }
      }
    }
    free(working_root);
  }

  return 0;
}
