#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
// since i am making an argument based envirnment i will make Global variables
// functions headers
// i was using snprintf to store values using formated string but turns out it stores values in characters
// %d in snprintf doesnt always accupies 4 bytes rather it takes bytes = number of characters in the value like -1 takes 2 bytes in that char array
// i wanted -1 to take exactly 4 bytes or so i say sizeof(type) i used memcopy that copes byte by byte

int dir_b_number = 0;
int fat_table_b_number = 0;
int fat_table_entries = 0;
int free_fat_index = 0;
short Dir_Size = 0;
short Dir_free = 1;
int files_b_number = 0;
unsigned int System_Size = 0;
unsigned short Block_Size = 0;
char Name_Length = 0;
unsigned short Max_File = 0;
char file_name[] = "sys.bin";
short dir_Count = 1;

struct DIR_ENTRY
{
  char *name;    // name of file or a folder
  int size;      // Actual size in bytes in case of folder this would be -1
  int Fat_Index; // first block index in FAT table
  short parent;  // parent index in DIR
};
struct FAT_ENTRY
{
  bool is_free; // IS this block occupied or not
  int next;     // next index in FAT table
};

void CCE(char *Filename);
void CCD(char *Filename);
long int get_file_size(FILE *fptr);
char *Read_Block(int b_number);
short find_file(struct DIR_ENTRY *dir, char *name, short *parent);
short find_folder(struct DIR_ENTRY *dir, char *name, short *parent);
bool create_partition_file(char *filename, int size_in_mb);
bool format(char *filename, unsigned int system_size, unsigned short b_size, short dir_entries, unsigned short max_file_size, char name_size);
char *Read_Block(int b_number);
bool Write_Block(int b_number, char *Block);
struct DIR_ENTRY Validate_DIR_Entry(char *name, int *size, short *parent);
bool Create_File(struct DIR_ENTRY *dir, char *name, short *parent);
bool Create_Folder(struct DIR_ENTRY *dir, char *name, short *parent);
bool delete_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent);
bool delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent);
bool Dir_Entry_copy(struct DIR_ENTRY *a, struct DIR_ENTRY *b);
int *view_all_childs(struct DIR_ENTRY *dir, char *name, short *parent, short *count);
char *Read(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent, int *size, int *offset);
bool Write(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent, char *insert, int *offset);
bool truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent);
bool Truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, short *index);

bool load_dir_fat(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat);
bool load_variables();
bool Delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, short *index);
void create_file_menu(struct DIR_ENTRY *dir);
void write_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat);
void read_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat);
void create_folder_menu(struct DIR_ENTRY *dir);
void delete_dir_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat);
void truncate_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat);
void delete_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat);
void view_all_childs_menue(struct DIR_ENTRY *dir);
// creates a file with size=size_in_mb
bool create_partition_file(char *filename, int size_in_mb)
{
  FILE *fp = fopen(filename, "wb");
  if (!fp)
  {
    return false;
  }
  char c = 0;
  for (int i = 0; i < size_in_mb; i++)
  {
    for (int j = 0; j < 1024; j++)
    {
      for (int k = 0; k < 1024; k++)
      {
        fputc(c, fp);
      }
      // either this or i can insert 1024 size array loop times
    }
  }
  fclose(fp);
  return true;
}

// Meta data like variables are in 0 block then DIR starts from 1 previously i implemented Dir right after meta data variables
// but that didnt seem right so i decided DIR should start from new block

bool format(char *filename, unsigned int system_size, unsigned short b_size, short dir_entries, unsigned short max_file_size, char name_size)
{
  System_Size = system_size;
  Block_Size = b_size;
  Max_File = max_file_size;
  Dir_Size = dir_entries;
  Name_Length = name_size;
  int n_of_blocks = (system_size * 1024 * 1024) / Block_Size;
  fat_table_entries = n_of_blocks;
  char *Buffer;
  int Offset = 0;
  Buffer = Read_Block(1);
  char *nam = (char *)malloc(Name_Length + 1);
  memset(nam, 0, Name_Length + 1);
  strcpy(nam, "root");
  snprintf(Buffer, Block_Size, "%c", 4);
  memcpy(Buffer + 1, nam, Name_Length);
  int t = -1;
  int z = -1;
  short p = -1;
  Offset = Name_Length + 1;
  memcpy(Buffer + Offset, &t, sizeof(t));
  Offset += sizeof(t);
  memcpy(Buffer + Offset, &z, sizeof(z));
  Offset += sizeof(z);
  memcpy(Buffer + Offset, &p, sizeof(p));
  Offset += sizeof(p);
  free(nam);
  dir_b_number = 1;
  if (!Write_Block(1, Buffer))
  {
    return false;
  }

  int size_DIR = Offset * Dir_Size;

  int b_number = (size_DIR / Block_Size);
  int offset = size_DIR % Block_Size;
  if (offset > 0)
    b_number++;
  b_number++;
  free(Buffer);

  Buffer = (char *)malloc(Block_Size + 1);
  memset(Buffer, 0, Block_Size + 1);
  fat_table_b_number = b_number;

  offset = 0;

  int remaining = Block_Size;
  int Blocks = (n_of_blocks * 5) / Block_Size;
  if ((n_of_blocks * 5) % Block_Size != 0)
    Blocks++;
  unsigned int total_invalid_blocks = b_number + Blocks; // 1 for bool and 4 for unsigned int

  files_b_number = total_invalid_blocks + 1;

  printf("%d", fat_table_entries);
  for (unsigned int i = 0; i <= total_invalid_blocks; i++)
  {
    Buffer[offset++] = 0;
    remaining--;

    if (remaining == 0)
    {
      Write_Block(b_number, Buffer);
      remaining = Block_Size;
      b_number++;
      free(Buffer);
      Buffer = Read_Block(b_number);
      offset = 0;
    }
    int k = -1;
    char *a = (char *)&k;
    for (int j = 0; j < sizeof(int); j++)
    {
      Buffer[offset++] = *(a + j);
      remaining--;
      if (remaining == 0)
      {
        Write_Block(b_number, Buffer);
        remaining = Block_Size;
        b_number++;
        free(Buffer);
        Buffer = Read_Block(b_number);
        offset = 0;
      }
    }
  }
  for (unsigned int i = total_invalid_blocks + 1; i < n_of_blocks; i++)
  {
    Buffer[offset++] = 1;
    remaining--;

    if (remaining == 0)
    {
      Write_Block(b_number, Buffer);
      remaining = Block_Size;
      b_number++;
      free(Buffer);
      Buffer = Read_Block(b_number);
      offset = 0;
    }
    int k = i + 1;
    if (i == n_of_blocks - 1)
      k = -1;
    char *a = (char *)&k;
    for (int j = 0; j < sizeof(int); j++)
    {
      Buffer[offset++] = *(a + j);
      remaining--;
      if (remaining == 0)
      {
        Write_Block(b_number, Buffer);
        remaining = Block_Size;
        b_number++;
        free(Buffer);
        Buffer = Read_Block(b_number);
        offset = 0;
      }
    }
  }
  Write_Block(b_number, Buffer);
  free_fat_index = total_invalid_blocks + 1;
  memset(Buffer, 0, Block_Size);
  Offset = 0;
  memcpy(Buffer, &System_Size, sizeof(System_Size));
  Offset += sizeof(System_Size);
  memcpy(Buffer + Offset, &Block_Size, sizeof(Block_Size));
  Offset += sizeof(Block_Size);
  memcpy(Buffer + Offset, &Dir_Size, sizeof(Dir_Size));
  Offset += sizeof(Dir_Size);
  memcpy(Buffer + Offset, &Max_File, sizeof(Max_File));
  Offset += sizeof(Max_File);
  memcpy(Buffer + Offset, &Name_Length, sizeof(Name_Length));
  Offset += sizeof(Name_Length);
  memcpy(Buffer + Offset, &free_fat_index, sizeof(free_fat_index));
  Offset += sizeof(free_fat_index);
  memcpy(Buffer + Offset, &dir_Count, sizeof(dir_Count));
  Offset += sizeof(dir_Count);
  memcpy(Buffer + Offset, &Dir_free, sizeof(Dir_free));
  Offset += sizeof(Dir_free);
  memcpy(Buffer + Offset, &dir_b_number, sizeof(dir_b_number));
  Offset += sizeof(dir_b_number);
  memcpy(Buffer + Offset, &fat_table_b_number, sizeof(fat_table_b_number));
  Offset += sizeof(fat_table_b_number);
  memcpy(Buffer + Offset, &fat_table_entries, sizeof(fat_table_entries));
  Offset += sizeof(fat_table_entries);
  if (!Write_Block(0, Buffer))
  {
    printf("couldnt save the variables");
  }
  free(Buffer);
  return true;
  // done
}
short find_free_dir(struct DIR_ENTRY *dir)
{
  for (int i = 1; i < Dir_Size; i++)
  {
    if (dir[i].name == NULL || dir[i].name[0] == '\0' || dir[i].name[0] == '$')
      return i;
  }
  return -1;
}
int allocate_new_fat_entry(struct FAT_ENTRY *fat)
{
  if (free_fat_index == -1)
  {
    printf("No free FAT entries available.\n");
    return -1;
  }
  int allocated_index = free_fat_index;
  free_fat_index = fat[free_fat_index].next;
  fat[allocated_index].is_free = 0;
  fat[allocated_index].next = -1; // end of linked list
  return allocated_index;
}

int find_free_fat(struct FAT_ENTRY *fat)
{
  if (fat[fat[free_fat_index].next].is_free)
  {
    return fat[free_fat_index].next;
  }
  return -1;
}
bool Create_File(struct DIR_ENTRY *dir, char *name, short *parent)
{
  int index = find_folder(dir, name, parent);
  if (index != -1)
  {
    printf("cannot create file with same name in same context");
    return false;
  }
  int b = 0;
  struct DIR_ENTRY a = Validate_DIR_Entry(name, &b, parent);
  if (a.parent == -1)
  {
    free(a.name);
    return false;
  }
  if (dir_Count < Dir_Size)
  {
    if (Dir_free != -1)
    {
      dir[Dir_free] = a;
      short i = Dir_free;
      Dir_free = find_free_dir(dir);
      return true;
    }
  }
  free(a.name);
  return false;
}
bool Delete_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, short *index)
{
  if (*(index) < 0 || *(index) >= Dir_Size)
    return false;
  if (dir[*(index)].name == NULL)
  {
    return false;
  }
  if (dir[*(index)].name[0] == '$')
    return false;
  if (dir[*(index)].size == -1)
    return false; // it is a directory
  int size = dir[*(index)].size;
  int blocks = size / Block_Size;
  if (size % Block_Size != 0)
    blocks++;
  int cur = dir[*(index)].Fat_Index;
  int temp = cur;

  for (int i = 0; i < blocks; i++)
  {
    if (cur >= 0 && cur < fat_table_entries)
    {
      fat[cur].is_free = true;
      cur = fat[cur].next;
    }
  }
  fat[cur].next = free_fat_index;
  free_fat_index = temp;
  dir[*(index)].name[0] = '$';
  dir[*(index)].size = 0;
  dir[*(index)].Fat_Index = -1;
  dir_Count--;
  return true;
}

bool delete_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent)
{
  short index = find_file(dir, name, parent);
  if (index == -1)
    return false;
  if (dir[index].name == NULL)
  {
    return false;
  }
  if (dir[index].name[0] == '$')
    return false;
  truncate_file(dir, fat, name, parent);
  dir[index].name[0] = '$';
  return true;
}
bool Delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, short *index)
{
  if (*(index) < 0 || *(index) >= Dir_Size)
    return false;
  if (dir[*(index)].name == NULL)
  {
    return false;
  }
  if (dir[*(index)].name[0] == '$')
    return false;

  short count = 0;
  for (int i = 0; i < Dir_Size; i++)
  {
    if (dir[i].parent == *index)
      count++;
  }
  short *arr = (short *)malloc(sizeof(short) * count);
  count = 0;
  for (int i = 0; i < Dir_Size; i++)
  {
    if (dir[i].parent == *index)
      arr[count++] = i;
  }
  for (int i = 0; i < count; i++)
  {
    if (dir[arr[i]].size == -1)
      Delete_DIR(dir, fat, &arr[i]);
    else
      Delete_file(dir, fat, &arr[i]);
  }
  free(arr);
  dir[*(index)].name[0] = '$';
  dir[*index].size = 0;
  dir_Count--;
  return true;
}
bool delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent) // delete whole folder(all files and folder inside)
{
  int index = find_folder(dir, name, parent);
  if (index == -1)
  {
    printf("\nfile doesnt exists");
    return false;
  }
  if (index == 0)
  {
    printf("\ncannot delet root dir");
    return false;
  }
  if (dir[index].name == NULL)
  {
    return false;
  }
  if (dir[index].name[0] == '$')
    return false;
  if (dir[index].size != -1)
    return false;
  // delete all childs + their childs
  short count = 0;
  for (int i = 0; i < Dir_Size; i++)
  {
    if (dir[i].parent == index)
      count++;
  }
  short *arr = (short *)malloc(sizeof(short) * count);
  count = 0;
  for (int i = 0; i < Dir_Size; i++)
  {
    if (dir[i].parent == index)
      arr[count++] = i;
  }
  for (int i = 0; i < count; i++)
  {
    if (dir[arr[i]].size == -1)
      Delete_DIR(dir, fat, &arr[i]);
    else
      Delete_file(dir, fat, &arr[i]);
  }
  free(arr);
  dir[index].name[0] = '$';
  dir[index].size = 0;
  dir_Count--;
  return true;
}
bool Dir_Entry_copy(struct DIR_ENTRY *a, struct DIR_ENTRY *b)
{
  if (a->name)
    free(a->name);
  a->name = (char *)malloc(strlen(b->name) + 1);
  if (!a->name)
    return false;
  strcpy(a->name, b->name);
  a->Fat_Index = b->Fat_Index;
  a->parent = b->parent;
  a->size = b->size;
  return true;
}
int *view_all_childs(struct DIR_ENTRY *dir, char *name, short *parent, short *count)
// returns DIR array of all childs + refrence of count
{
  short index = find_folder(dir, name, parent);
  int *childs;
  if (index == -1)
    return NULL;
  short Count = 0;
  for (int i = 1; i < Dir_Size; i++)
  {
    if (dir[i].parent == index && dir[i].name != NULL && strlen(dir[i].name) > 0)
      Count++;
  }
  childs = malloc(Count * sizeof(int));
  memcpy(count, &Count, sizeof(short));
  Count = 0;
  for (int i = 1; i < Dir_Size; i++)
  {
    if (dir[i].parent == index && dir[i].name != NULL && strlen(dir[i].name) > 0)
    {
      childs[Count++] = i;
    }
  }
  return childs;
}
bool Create_Folder(struct DIR_ENTRY *dir, char *name, short *parent)
{
  int index = find_folder(dir, name, parent);
  if (index != -1)
  {
    printf("cannot create folder with same name in same context");
    return false;
  }
  int b = -1;
  struct DIR_ENTRY a = Validate_DIR_Entry(name, &b, parent);
  if (*parent > Dir_Size || *parent < 0)
  {
    free(a.name);
    return false;
  }
  if (dir[*parent].size != -1)
  {
    free(a.name);
    return false;
  }
  if (a.parent == -1)
  {
    free(a.name);
    return false;
  }
  if (dir_Count < Dir_Size)
  {
    if (Dir_free != -1)
    {
      dir[Dir_free] = a;
      short i = Dir_free;
      Dir_free = find_free_dir(dir);
      return true;
    }
  }
  free(a.name);
  return false;
}
short find_file(struct DIR_ENTRY *dir, char *name, short *parent)
{
  for (short i = 1; i < Dir_Size; i++)
  {
    if (dir[i].name != NULL && dir[i].name[0] != '$' && (strcmp(dir[i].name, name) == 0) && dir[i].parent == *(parent) && dir[i].size != -1)
      return i;
  }
  return -1;
}
short find_folder(struct DIR_ENTRY *dir, char *name, short *parent)
{
  for (short i = 0; i < Dir_Size; i++)
  {
    if (dir[i].name != NULL && dir[i].name[0] != '$' && (strcmp(dir[i].name, name) == 0) && dir[i].parent == *(parent) && dir[i].size == -1)
      return i;
  }
  return -1;
}
bool Truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, short *index)
// doing size 0 and making all the fat table entries free
{
  if (*index < 0 || *index >= Dir_Size)
    return false;
  int size = dir[*index].size;
  if (size == 0)
    return true;
  if (dir[*index].Fat_Index == 0 || dir[*index].Fat_Index == -1)
  {
    dir[*(index)].size = 0;
    return true;
  }
  int cur = dir[*index].Fat_Index;
  int prev_free_fat_index = free_fat_index; // Store the current free FAT index

  while (cur != -1)
  {
    if (cur >= fat_table_entries)
    {
      printf("FAT chain is corrupted.\n");
      return false;
    }

    int next = fat[cur].next;            // Save the next block before overwriting
    fat[cur].is_free = true;             // Mark the current block as free
    fat[cur].next = prev_free_fat_index; // Link current block to the free list
    prev_free_fat_index = cur;           // Update the local free index
    cur = next;                          // Move to the next block in the chain
  }

  // After exiting the loop, update the global free list head
  free_fat_index = prev_free_fat_index;

  // Reset file directory metadata
  dir[*index].Fat_Index = -1;
  dir[*index].size = 0;
  free(dir[*index].name);
  printf("File truncated successfully.\n");
  return true;
}
bool truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent)
// doing size 0 and making all the fat table entries free
{
  short index = find_file(dir, name, parent);
  if (index == -1)
    return false;
  int size = dir[index].size;
  if (size == 0)
    return true;
  if (dir[index].Fat_Index == 0 || dir[index].Fat_Index == -1)
  {
    dir[index].size = 0;
    return true;
  }
  int cur = dir[index].Fat_Index;
  int prev_free_fat_index = free_fat_index; // Store the current free FAT index

  while (cur != -1)
  {
    if (cur >= fat_table_entries)
    {
      printf("FAT chain is corrupted.\n");
      return false;
    }

    int next = fat[cur].next;            // Save the next block before overwriting
    fat[cur].is_free = true;             // Mark the current block as free
    fat[cur].next = prev_free_fat_index; // Link current block to the free list
    prev_free_fat_index = cur;           // Update the local free index
    cur = next;                          // Move to the next block in the chain
  }

  // After exiting the loop, update the global free list head
  free_fat_index = prev_free_fat_index;

  // Reset file directory metadata
  free(dir[index].name);
  dir[index].Fat_Index = -1;
  dir[index].size = 0;

  printf("File truncated successfully.\n");
  return true;
}

char *Read(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent, int *size, int *offset)
// returning char* array of size if(size+offset<= total file size)
{
  char *Buffer;
  Buffer = (char *)malloc(*size + 1);
  memset(Buffer, 0, *size + 1);
  int index = find_file(dir, name, parent);
  if (index == -1)
  {
    printf("\nfile doesnt exists");
    return Buffer;
  }
  if (dir[index].size < (*size + *offset))
  {
    printf("Your data read size+ offset is greater than total fie size");
    return Buffer;
  }
  int n_of_b = *size / Block_Size;
  if (*size % Block_Size != 0)
    n_of_b++;
  int offset_b = *offset / Block_Size;
  int offset_in_b = *offset % Block_Size;
  int fat_index = dir[index].Fat_Index;
  int remaining = *size;
  if (fat_index == -1)
    return Buffer; // it is a folder read (not a file read whihch is false)
  for (int i = 0; i < offset_b; i++)
  {
    if (fat[fat_index].next == -1)
    {
      printf("\nFAT chain ended ,corupted fat.");
      free(Buffer);
      return NULL;
    }
    fat_index = fat[fat_index].next;
  }
  char *temp = Read_Block(fat_index);
  int cur = Block_Size - offset_in_b;
  int c = 0;
  int value = *size % cur;
  if (value == 0)
    value = cur;
  memcpy(Buffer, temp + offset_in_b, value);
  free(temp);
  remaining -= value;
  c = value;
  fat_index = fat[fat_index].next;
  for (int i = 1; i < n_of_b && c < *size && fat_index != -1; i++)
  {
    int bytes_to_read = (*size - c > Block_Size) ? Block_Size : *size - c;
    if (!(temp = Read_Block(fat_index)))
    {
      printf("Could not write to block %d.\n", fat_index);
      free(Buffer);
      return false;
    }
    memcpy(Buffer + c, temp, bytes_to_read);
    c += bytes_to_read;
    fat_index = fat[fat_index].next;
    remaining -= bytes_to_read;
    free(temp);
  }
  return Buffer;
}
bool Write(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *name, short *parent, char *insert, int *offset)
{
  int index = find_file(dir, name, parent);
  if (index == -1 || insert == NULL || dir[index].size == -1)
  {
    printf("Invalid file.\n");
    return false;
  }

  int size = strlen(insert);
  if (size == 0)
  {
    printf("No data to write.\n");
    return true;
  }

  if (*offset + size > Max_File * Block_Size)
  {
    printf("Offset and size exceed maximum file size.\n");
    return false;
  }

  if (dir[index].Fat_Index == -1 || dir[index].Fat_Index == 0)
  {
    int new_index = allocate_new_fat_entry(fat);
    if (new_index == -1)
      return false;
    dir[index].Fat_Index = new_index;
  }

  if (*offset > dir[index].size)
  {
    printf("Offset exceeds the current file size.\n");
    return false;
  }

  int n_of_b = size / Block_Size;
  if (size % Block_Size != 0)
    n_of_b++;
  int offset_b = *offset / Block_Size;
  int Offset = *offset % Block_Size;
  int fat_index = dir[index].Fat_Index;

  char *Buffer = Read_Block(fat_index);
  if (!Buffer)
  {
    printf("Failed to read block %d.\n", fat_index);
    return false;
  }

  for (int i = 0; i < offset_b; i++)
  {
    if (fat_index == -1 || fat[fat_index].next == -1)
    {
      int new_index = allocate_new_fat_entry(fat);
      if (new_index == -1)
      {
        free(Buffer);
        return false;
      }
      fat[fat_index].next = new_index;
    }
    fat_index = fat[fat_index].next;
  }

  int c = 0;
  int remaining = Block_Size - Offset;
  memcpy(Buffer + Offset, insert, (remaining > size) ? size : remaining);
  if (!Write_Block(fat_index, Buffer))
  {
    printf("Could not write to block %d.\n", fat_index);
    free(Buffer);
    return false;
  }
  c += remaining;
  dir[index].size += (remaining > size) ? size : remaining;

  for (int i = 1; i < n_of_b && c < size; i++)
  {
    if (fat_index == -1 || fat[fat_index].next == -1)
    {
      int new_index = allocate_new_fat_entry(fat);
      if (new_index == -1)
      {
        free(Buffer);
        return false;
      }
      fat[fat_index].next = new_index;
    }
    fat_index = fat[fat_index].next;

    int bytes_to_write = (size - c > Block_Size) ? Block_Size : size - c;
    memset(Buffer, 0, Block_Size);
    memcpy(Buffer, insert + c, bytes_to_write);
    if (!Write_Block(fat_index, Buffer))
    {
      printf("Could not write to block %d.\n", fat_index);
      free(Buffer);
      return false;
    }
    c += bytes_to_write;
    dir[index].size += bytes_to_write;
  }
  printf("\nCurrent filesize is :%d",dir[index].size);
  free(Buffer);
  return true;
}

struct DIR_ENTRY Validate_DIR_Entry(char *name, int *size, short *parent)
{
  struct DIR_ENTRY a = {NULL, 0, 0, -1};
  if (name == NULL)
    return a;
  int n = strlen(name);
  if (name[0] == '$' || n > Name_Length || ((*size) != -1 && (*size) != 0) || (*parent) < 0 || dir_Count == Dir_Size)
  {
    return a;
  }
  a.name = (char *)malloc(n + 1);
  strcpy(a.name, name);
  a.parent = (*parent);
  a.size = (*size);
  a.Fat_Index = -1;
  return a;
}
char *Read_Block(int b_number)
{
  if (!file_name || Block_Size <= 0)
  {
    fprintf(stderr,"\nFile name or Block Size not initialized.\n");
    return NULL;
  }
  FILE *fp = fopen(file_name, "rb");
  if (!fp)
  {
    fprintf(stderr,"\nError opening file for reading");
    return NULL;
  }

  char *Buf = (char *)malloc(Block_Size + 1);
  if (!Buf)
  {
    fclose(fp);
    fprintf(stderr,"\nMemory allocation failed");
    return NULL;
  }

  fseek(fp, b_number * Block_Size, SEEK_SET);
  if(fread(Buf, 1, Block_Size, fp)<0)
  {
    fprintf(stderr,"couldnt read properly block number: %d",b_number); 
    free(Buf);
    fclose(fp);
    return NULL;
  }
  rewind(fp);
  fclose(fp);
  return Buf;
}

bool Write_Block(int b_number, char *Block)
{
  if (!Block)
  {
    fprintf(stderr,"\nBlock data is null.");
    return false;
  }
  if (!file_name || Block_Size <= 0)
  {
    fprintf(stderr, "\nFile name or Block Size not initialized.");
    return false;
  }
  FILE *fp = fopen(file_name, "rb+");
  if (!fp)
  {
    fprintf(stderr,"\nError opening file for writing");
    return false;
  }

  fseek(fp, b_number * Block_Size, SEEK_SET);
  if(fwrite(Block , 1, Block_Size, fp)<0)
  {  
    fprintf(stderr,"\nError writing on Block:%d",b_number);
    return false;
  }
   
  rewind(fp);
  fclose(fp);
  return true;
}
bool commit_changes(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat, char *filename)
{

  char *Buffer = (char *)malloc(Block_Size + 1);
  memset(Buffer, 0, Block_Size + 1);
  int Offset = 0;
  memcpy(Buffer, &System_Size, sizeof(System_Size));
  Offset += sizeof(System_Size);
  memcpy(Buffer + Offset, &Block_Size, sizeof(Block_Size));
  Offset += sizeof(Block_Size);
  memcpy(Buffer + Offset, &Dir_Size, sizeof(Dir_Size));
  Offset += sizeof(Dir_Size);
  memcpy(Buffer + Offset, &Max_File, sizeof(Max_File));
  Offset += sizeof(Max_File);
  memcpy(Buffer + Offset, &Name_Length, sizeof(Name_Length));
  Offset += sizeof(Name_Length);
  memcpy(Buffer + Offset, &free_fat_index, sizeof(free_fat_index));
  Offset += sizeof(free_fat_index);
  memcpy(Buffer + Offset, &dir_Count, sizeof(dir_Count));
  Offset += sizeof(dir_Count);
  memcpy(Buffer + Offset, &Dir_free, sizeof(Dir_free));
  Offset += sizeof(Dir_free);
  memcpy(Buffer + Offset, &dir_b_number, sizeof(dir_b_number));
  Offset += sizeof(dir_b_number);
  memcpy(Buffer + Offset, &fat_table_b_number, sizeof(fat_table_b_number));
  Offset += sizeof(fat_table_b_number);
  memcpy(Buffer + Offset, &fat_table_entries, sizeof(fat_table_entries));
  Offset += sizeof(fat_table_entries);

  if (!Write_Block(0, Buffer))
  {
    printf("Couldnt commit variables");
    return false;
  }
  free(Buffer);
  int size = 0;
  size = (Dir_Size * 11 + Dir_Size * Name_Length);
  int n_of_b = size / Block_Size;
  if (size % Block_Size != 0)
    n_of_b++;
  Buffer = (char *)malloc((n_of_b * Block_Size) + 1);
  memset(Buffer, 0, (n_of_b * Block_Size) + 1);
  int index = 0;
  for (int i = 0; i < Dir_Size; i++)
  {
    int Size = strlen(dir[i].name);
    Buffer[index++] = Size;
    int temp = index;
    for (int j = 0; j < Size; j++)
    {
      Buffer[index++] = dir[i].name[j];
    }
    index = temp + Name_Length;
    char *a = (char *)&dir[i].size;
    for (int j = 0; j < sizeof(int); j++)
    {
      Buffer[index++] = *(a + j);
    }
    a = (char *)&dir[i].Fat_Index;
    for (int j = 0; j < sizeof(int); j++)
    {
      Buffer[index++] = *(a + j);
    }
    a = (char *)&dir[i].parent;
    for (int j = 0; j < sizeof(short); j++)
    {
      Buffer[index++] = *(a + j);
    }
  }
  // now all the dir data is loaded onto a 1 d array now writeing chunk by chunk on file

  char *temp_1 = (char *)malloc(Block_Size + 1);
  memset(temp_1, 0, Block_Size + 1);
  for (int i = 0; i < n_of_b; i++)
  {
    memcpy(temp_1, Buffer + (i * Block_Size), Block_Size);
    if (!Write_Block(dir_b_number + i, temp_1))
    {
      printf("couldnt write %d block", i);
    }
  }
  free(temp_1);
  free(Buffer);
  int fat_size = 5 * fat_table_entries;
  n_of_b = fat_size / Block_Size;
  if (size % Block_Size != 0)
    n_of_b++;
  Buffer = (char *)malloc((Block_Size * n_of_b) + 1);
  memset(Buffer, 0, (Block_Size * n_of_b) + 1);
  index = 0;
  for (int i = 0; i < fat_table_entries; i++)
  {
    char *a = (char *)&fat[i].is_free;
    Buffer[index++] = *(a);
    a = (char *)&fat[i].next;
    for (int j = 0; j < sizeof(int); j++)
    {
      Buffer[index++] = *(a + j);
    }
  }
  char *temp = (char *)malloc(Block_Size + 1);
  memset(temp, 0, Block_Size + 1);
  for (int i = 0; i < n_of_b; i++)
  {
    memcpy(temp, Buffer + (i * Block_Size), Block_Size);
    if (!Write_Block(fat_table_b_number + i, temp))
    {
      printf("couldnt write the fat %d block", i);
    }
  }
  free(temp);
  free(Buffer);
  return true;
}
bool load_dir_fat(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat)
{
  char *Buffer;
  int dir_size = Dir_Size * 11 + Dir_Size * Name_Length;
  int remaining = dir_size;
  Buffer = (char *)malloc(dir_size + 1);
  memset(Buffer, 0, dir_size + 1);
  int offset = 0;
  int n_of_b = dir_size / Block_Size;
  if (dir_size % Block_Size != 0)
    n_of_b++;
  for (int i = 0; i < n_of_b; i++)
  {
    char *b = Read_Block(i + dir_b_number);
    for (int j = 0; j < Block_Size && remaining > 0; j++)
    {
      Buffer[offset++] = b[j];
      remaining--;
    }
    free(b);
  }
  int index = 0;
  if (offset != dir_size)
    printf("shit");
  printf("\ndir size;%d",Dir_Size);
  for (int i = 0; i < Dir_Size; i++)
  {
    char Size = Buffer[index++];
    int temp = index;
    dir[i].name = (char *)malloc(Size + 1); // for /0
    for (int j = 0; j < Size; j++)
    {
      dir[i].name[j] = Buffer[index++];
    }
    dir[i].name[Size] = 0;
    index = temp + Name_Length;
    char *a = (char *)&dir[i].size;
    for (int j = 0; j < sizeof(int); j++)
    {
      *(a + j) = Buffer[index++];
    }
    a = (char *)&dir[i].Fat_Index;
    for (int j = 0; j < sizeof(int); j++)
    {
      *(a + j) = Buffer[index++];
    }
    a = (char *)&dir[i].parent;
    for (int j = 0; j < sizeof(short); j++)
    {
      *(a + j) = Buffer[index++];
    }
  }
  free(Buffer);
  // Now loading fat
  offset = 0;
  int fat_size = 5 * fat_table_entries;
  remaining = fat_size;
  n_of_b = fat_size / Block_Size;
  if (fat_size % Block_Size != 0)
    n_of_b++;
  Buffer = (char *)malloc(fat_size);
  for (int i = 0; i < n_of_b; i++)
  {
    char *b = Read_Block(i + fat_table_b_number);
    for (int j = 0; j < Block_Size && remaining > 0; j++)
    {
      Buffer[offset++] = b[j];
      remaining--;
    }
    free(b);
  }
  index = 0;
  for (int i = 0; i < fat_table_entries; i++)
  {
    char *a = (char *)&fat[i].is_free;
    *(a) = Buffer[index++];
    a = (char *)&fat[i].next;
    for (int j = 0; j < sizeof(int); j++)
    {
      *(a + j) = Buffer[index++];
    }
    // printf("\n%d%u",fat[i].is_free,fat[i].next);
  }
  free(Buffer);
  return true;
}

// Global variables
bool load_variables()
{
  char *Buffer = Read_Block(0);
  int Offset = 0;
  memcpy(&System_Size, Buffer + Offset, sizeof(System_Size));
  Offset += sizeof(System_Size);
  memcpy(&Block_Size, Buffer + Offset, sizeof(Block_Size));
  Offset += sizeof(Block_Size);
  memcpy(&Dir_Size, Buffer + Offset, sizeof(Dir_Size));
  Offset += sizeof(Dir_Size);
  memcpy(&Max_File, Buffer + Offset, sizeof(Max_File));
  Offset += sizeof(Max_File);
  memcpy(&Name_Length, Buffer + Offset, sizeof(Name_Length));
  Offset += sizeof(Name_Length);
  memcpy(&free_fat_index, Buffer + Offset, sizeof(free_fat_index));
  Offset += sizeof(free_fat_index);
  memcpy(&dir_Count, Buffer + Offset, sizeof(dir_Count));
  Offset += sizeof(dir_Count);
  memcpy(&Dir_free, Buffer + Offset, sizeof(Dir_free));
  Offset += sizeof(Dir_free);
  memcpy(&dir_b_number, Buffer + Offset, sizeof(dir_b_number));
  Offset += sizeof(dir_b_number);
  memcpy(&fat_table_b_number, Buffer + Offset, sizeof(fat_table_b_number));
  Offset += sizeof(fat_table_b_number);
  memcpy(&fat_table_entries, Buffer + Offset, sizeof(fat_table_entries));
  Offset += sizeof(fat_table_entries);
  free(Buffer);
  return true;
}
int main()
{
  struct DIR_ENTRY *dir;
  struct FAT_ENTRY *fat;
  int a = 0;
  while (a != 1 && a != 2)
  {
    printf("do you want to create system again and formate? or Load? \n for load enter 1 \nFor new enter 2:");
    scanf("%d", &a);
    getchar();
    if (a == 2)
    {
      printf("\nEnter disk size in MB to be created:");
      scanf("%u", &System_Size);
      getchar();
      printf("\nEnter Block size in Bytes (should be >=64) to be created:");
      scanf("%hu", &Block_Size);
      getchar();
      if (Block_Size < 64)
      {
        printf("\nInvalid Blocksize:");
        a = -1;
        continue;
      }
      printf("\nEnter max total dir entries to be created:");
      scanf("%hi", &Dir_Size);
      getchar();
      if (Dir_Size <= 0)
      {
        printf("\nInvalid Dirsize:");
        a = -1;
        continue;
      }
      printf("\nEnter max number of blocks a file can have:");
      scanf("%hu", &Max_File);
      getchar();
      printf("\nEnter max Name Length a file can have:");
      scanf("%c", &Name_Length);
      getchar();
      if (Name_Length <= 0)
        if (Dir_Size <= 0)
        {
          printf("\nInvalid NameLength:");
          a = -1;
          continue;
        }
    }
  }
  if (a == 2)
  {
    if (!create_partition_file("sys.bin", System_Size))
    {
      printf("\ncouldnt");
    }
    if (!format("sys.bin", System_Size, Block_Size, Dir_Size, Max_File, Name_Length))
    {
      printf("\nCouldnt format");
    }
    FILE *fp = fopen("Blocksize.txt", "w");
    fprintf(fp, "%hu", Block_Size);
    fclose(fp);
  }
  else
  {
    FILE *fp = fopen("Blocksize.txt", "r");
    fscanf(fp, "%hu", &Block_Size);
    fclose(fp);
    CCD("sys.bin");
  }
  if (!load_variables())
  {
    printf("couldnt load");
  }
  dir = (struct DIR_ENTRY *)malloc(sizeof(struct DIR_ENTRY) * Dir_Size);          // Initialize directory
  fat = (struct FAT_ENTRY *)malloc(sizeof(struct FAT_ENTRY) * fat_table_entries); // Initialize FAT table
  if (!load_dir_fat(dir, fat))
  {
    printf("couldnt load");
  }
  int choice = 0;
  while (1)
  {
    printf("\n===== FAT System Menu =====\n");
    printf("1. Create File\n");
    printf("2. Write to File\n");
    printf("3. Read File\n");
    printf("4. Create Directory\n");
    printf("5. Delete Directory\n");
    printf("6. Delete File\n");
    printf("7. Truncate file:\n");
    printf("8. View all childs of a Directory:\n");
    printf("9. Exit\n");
    printf("Enter your choice: ");

    if (scanf("%d", &choice) != 1)
    {
      printf("Invalid input! Please enter a number.\n");
      getchar(); // Clear the input buffer
      continue;
    }
    getchar(); // Consume newline character

    switch (choice)
    {
    case 1:
      create_file_menu(dir);
      break;
    case 2:
      write_file_menu(dir, fat);
      break;
    case 3:
      read_file_menu(dir, fat);
      break;
    case 4:
      create_folder_menu(dir);
      break;
    case 5:
      delete_dir_menu(dir, fat);
      break;
    case 6:
      delete_file_menu(dir, fat);
      break;
    case 7:
      truncate_file_menu(dir, fat);
      break;
    case 8:
      view_all_childs_menue(dir);
      break;
    case 9:
      printf("Exiting...\n");
      CCE("sys.bin");
      printf("\ndir size;%d",Dir_Size);
      for (int i = 0; i < Dir_Size; i++)
        free(dir[i].name);
      free(dir);
      free(fat);
      return 0;
    default:
      printf("Invalid choice! Please try again.\n");
    }
    if (choice >= 1 && choice <= 8)
    {
      if (!commit_changes(dir, fat, "sys.bin"))
      {
        printf("Couldnt commit! ");
      }
    }
  }
  return 0;
}

// Menu handlers
void create_file_menu(struct DIR_ENTRY *dir)
{

  short parent = 0;
  char *filename = malloc(Name_Length + 1);
  printf("\nEnter file name: ");
  if (fgets(filename, Name_Length + 1, stdin) == NULL)
  {
    printf("Error reading file name.\n");
    return;
  }
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline character
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (Create_File(dir, filename, &parent))
  {
    printf("File created successfully.\n");
  }
  else
  {
    printf("Failed to create file.\n");
  }
  free(filename);
}
long int get_file_size(FILE *fptr)
{

  fseek(fptr, 0L, SEEK_END);
  long int size = ftell(fptr);
  fseek(fptr, 0L, SEEK_SET);
  return size;
}
void CCE(char *Filename)
{

  char a[64];
  strcpy(a, "key-");
  strcat(a, Filename);
  FILE *fp2 = fopen(a, "w");
  char key = time(0) % 256;
  fputc(key, fp2);
  fclose(fp2);
  int n_of_b=(System_Size*1024*1024/Block_Size);
  char*Buffer;
  for(int i=0;i<n_of_b;i++)
  {
    Buffer=Read_Block(i);
    for (int i = 0; i < Block_Size; i++)
    {
      int c = (key + Buffer[i]) % 256;
      Buffer[i] = c;
    }
    if(!Write_Block(i,Buffer))
    {
      fprintf(stderr,"Couldnt encode block: %d",i);
      free(Buffer);
      return ;
    }  
   free(Buffer);
  }
}
void CCD(char *filename)
{
  char *buffer;
  char a[64];
  strcpy(a, "key-");
  strcat(a, filename);
  FILE *fp2 = fopen(a, "r");
  char key = fgetc(fp2);
  fclose(fp2);
  int n_of_b=(System_Size*1024*1024/Block_Size);
  for(int i=0;i<n_of_b;i++)
  {
    buffer=Read_Block(i);
    for (int i = 0; i < Block_Size; i++)
    {
      int c = buffer[i] - key;
      if (c < 0)
        c += 256;
      c = c % 256;
      buffer[i] = c;
      if(!Write_Block(i,buffer))
      {
        fprintf(stderr,"Couldnt encode block: %d",i);
        free(buffer);
        return ;
      }
      free(buffer);
    }
  }
}

void write_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat)
{

  short parent = 0;
  char *filename = malloc(Name_Length);
  char *content = malloc(Block_Size);
  memset(content, 0, Block_Size);
  int offset = 0;
  printf("Enter file name: ");
  fgets(filename, Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  printf("\nEnter text for file: ");
  fgets(content, Block_Size + 1, stdin);
  content[strcspn(content, "\n")] = '\0';
  printf("Enter offset to place this array:");
  scanf("%d", &offset);
  getchar();
  // Remove newline
  if (Write(dir, fat, filename, &parent, content, &offset))
  {
    printf("Content written successfully.\n");
    
  }
  else
  {
    printf("Failed to write to file.\n");
  }
  free(filename);
  free(content);
}
void view_all_childs_menue(struct DIR_ENTRY *dir)
{
  short parent = 0;
  char *filename = malloc(Name_Length + 1);
  int size, offset = 0;
  printf("Enter file name: ");
  fgets(filename, Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  short count = 0;
  int *d = view_all_childs(dir, filename, &parent, &count);
  for (int i = 0; i < count; i++)
  {
    printf("\nFilename :%s ,Size: %d, File\'s Fat index: %d,Parent : %hi", dir[d[i]].name, dir[d[i]].size, dir[d[i]].Fat_Index, dir[d[i]].parent);
  }
  free(d);
  free(filename);
}
void read_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat)
{
  short parent = 0;
  char *filename = malloc(Name_Length + 1);
  int size, offset = 0;
  printf("Enter file name: ");
  fgets(filename, Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  printf("\nEnter size to read:");
  scanf("%d", &size);
  getchar();
  printf("\nEnter offset to read:");
  scanf("%d", &offset);
  getchar();
  char *data = Read(dir, fat, filename, &parent, &size, &offset);
  printf("\n");
  if (data)
  {

    printf("Data read size:%ld \nData :", strlen(data));
    for (int i = 0; i < size; i++)
    {
      printf("%c", data[i]);
    }
    printf("\n");
    free(data); // Read allocates memory
  }
  else
  {
    printf("Failed to read file.\n");
  }
  free(filename);
}

void create_folder_menu(struct DIR_ENTRY *dir)
{

  short parent = 0;
  char *foldername = malloc(Name_Length + 1);
  printf("Enter folder name: ");
  fgets(foldername, Name_Length + 1, stdin);
  foldername[strcspn(foldername, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (Create_Folder(dir, foldername, &parent))
  {
    printf("Folder created successfully.\n");
  }
  else
  {
    printf("Failed to create folder.\n");
  }
  free(foldername);
}

void delete_dir_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat)
{
  short parent = 0;
  char *foldername = malloc(Name_Length + 1);
  printf("Enter directory name: ");
  fgets(foldername, Name_Length + 1, stdin);

  foldername[strcspn(foldername, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (delete_DIR(dir, fat, foldername, &parent))
  {
    printf("Directory deleted successfully.\n");
  }
  else
  {
    printf("Failed to delete directory.\n");
  }

  free(foldername);
}
void truncate_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat)
{
  short parent = 0;
  char *filename = malloc(Name_Length + 1);
  printf("Enter file name: ");
  fgets(filename, Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (truncate_file(dir, fat, filename, &parent))
  {
    printf("File deleted successfully.\n");
  }
  else
  {
    printf("Failed to delete file.\n");
  }
  free(filename);
}
void delete_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat)
{
  short parent = 0;
  char *filename = malloc(Name_Length + 1);
  printf("Enter file name: ");
  fgets(filename, Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (delete_file(dir, fat, filename, &parent))
  {
    printf("File deleted successfully.\n");
  }
  else
  {
    printf("Failed to delete file.\n");
  }
  free(filename);
}