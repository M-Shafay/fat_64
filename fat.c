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

char file_name[] = "sys.bin";
FILE*fp_sys=NULL;
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
struct State
{
  int dir_b_number;
  int fat_table_b_number ;
  int fat_table_entries;
  int free_fat_index;
  short Dir_Size ;
  short Dir_free;
  unsigned int System_Size ;
  unsigned short Block_Size ;
  char Name_Length;
  unsigned short Max_File;
  short dir_Count;
};
void CCE(char *Filename,struct State *s);
void CCD(char *Filename,struct State *s);
short find_file(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent);
short find_folder(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent);
bool create_partition_file(char *filename, int size_in_mb);
bool format(char *filename,struct State*s); 
char *Read_Block(int b_number,struct State *s);
bool Write_Block(int b_number, char *Block,struct State *s);
struct DIR_ENTRY Validate_DIR_Entry(char *name, int *size, short *parent,struct State *s);
bool Create_File(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent);
bool Create_Folder(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent);
bool delete_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent);
bool delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent);
bool Dir_Entry_copy(struct DIR_ENTRY *a, struct DIR_ENTRY *b);
int *view_all_childs(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent, short *count);
char *Read(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent, int *size, int *offset);
bool Write(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent, char *insert, int *offset);
bool truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent);
bool Truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, short *index);

bool load_dir_fat(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s);
bool load_variables(struct State*s);
bool Delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, short *index);
void create_file_menu(struct DIR_ENTRY *dir,struct State*s);
void write_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s);
void read_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s);
void create_folder_menu(struct DIR_ENTRY *dir,struct State*s);
void delete_dir_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s);
void truncate_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s);
void delete_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s);
void view_all_childs_menue(struct DIR_ENTRY *dir,struct State*s);
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

bool format(char *filename,struct State*s)
{
  int n_of_blocks = (s->System_Size * 1024 * 1024) / s->Block_Size;
  s->fat_table_entries = n_of_blocks;
  char *Buffer;
  int Offset = 0;
  Buffer = Read_Block(1,s);
  char *nam = (char *)malloc(s->Name_Length + 1);
  memset(nam, 0, s->Name_Length + 1);
  strcpy(nam, "root");
  snprintf(Buffer, s->Block_Size, "%c", 4);
  memcpy(Buffer + 1, nam, s->Name_Length);
  int t = -1;
  int z = -1;
  short p = -1;
  Offset = s->Name_Length + 1;
  memcpy(Buffer + Offset, &t, sizeof(t));
  Offset += sizeof(t);
  memcpy(Buffer + Offset, &z, sizeof(z));
  Offset += sizeof(z);
  memcpy(Buffer + Offset, &p, sizeof(p));
  Offset += sizeof(p);
  free(nam);
  s->dir_b_number = 1;
  if (!Write_Block(1, Buffer,s))
  {
    return false;
  }

  int size_DIR = Offset * s->Dir_Size;

  int b_number = (size_DIR / s->Block_Size);
  int offset = size_DIR % s->Block_Size;
  if (offset > 0)
    b_number++;
  b_number++;
  free(Buffer);

  Buffer = (char *)malloc(s->Block_Size + 1);
  memset(Buffer, 0, s->Block_Size + 1);
  s->fat_table_b_number = b_number;

  offset = 0;

  int remaining = s->Block_Size;
  int Blocks = (n_of_blocks * 5) / s->Block_Size;
  if ((n_of_blocks * 5) % s->Block_Size != 0)
    Blocks++;
  unsigned int total_invalid_blocks = b_number + Blocks; // 1 for bool and 4 for unsigned int


  printf("%d", s->fat_table_entries);
  for (unsigned int i = 0; i <= total_invalid_blocks; i++)
  {
    Buffer[offset++] = 0;
    remaining--;

    if (remaining == 0)
    {
      Write_Block(b_number, Buffer,s);
      remaining = s->Block_Size;
      b_number++;
      free(Buffer);
      Buffer = Read_Block(b_number,s);
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
        Write_Block(b_number, Buffer,s);
        remaining = s->Block_Size;
        b_number++;
        free(Buffer);
        Buffer = Read_Block(b_number,s);
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
      Write_Block(b_number, Buffer,s);
      remaining = s->Block_Size;
      b_number++;
      free(Buffer);
      Buffer = Read_Block(b_number,s);
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
        Write_Block(b_number, Buffer,s);
        remaining = s->Block_Size;
        b_number++;
        free(Buffer);
        Buffer = Read_Block(b_number,s);
        offset = 0;
      }
    }
  }
  Write_Block(b_number, Buffer,s);
  s->free_fat_index = total_invalid_blocks + 1;
  memset(Buffer,0,s->Block_Size);
  memcpy(Buffer,s,sizeof(*s));
  if (!Write_Block(0, Buffer,s))
  {
    printf("couldnt save the variables");
  }
  free(Buffer);
  return true;
  // done
}
short find_free_dir(struct DIR_ENTRY *dir,struct State *s)
{
  for (int i = 1; i < s->Dir_Size; i++)
  {
    if (dir[i].name == NULL || dir[i].name[0] == '\0' || dir[i].name[0] == '$')
      return i;
  }
  return -1;
}
int allocate_new_fat_entry(struct FAT_ENTRY *fat,struct State *s)
{
  if (s->free_fat_index == -1)
  {
    printf("No free FAT entries available.\n");
    return -1;
  }
  int allocated_index = s->free_fat_index;
  s->free_fat_index = fat[s->free_fat_index].next;
  fat[allocated_index].is_free = 0;
  fat[allocated_index].next = -1; // end of linked list
  return allocated_index;
}

int find_free_fat(struct FAT_ENTRY *fat,struct State *s)
{
  if (fat[fat[s->free_fat_index].next].is_free)
  {
    return fat[s->free_fat_index].next;
  }
  return -1;
}
bool Create_File(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent)
{
  int index = find_folder(dir,s, name, parent);
  if (index != -1)
  {
    printf("cannot create file with same name in same context");
    return false;
  }
  int b = 0;
  struct DIR_ENTRY a = Validate_DIR_Entry(name, &b, parent,s);
  if (a.parent == -1)
  {
    free(a.name);
    return false;
  }
  if (s->dir_Count < s->Dir_Size)
  {
    if (s->Dir_free != -1)
    {
      if(dir[s->Dir_free].name)
      {
        free(dir[s->Dir_free].name);
      }
      dir[s->Dir_free] = a;
      short i = s->Dir_free;
      s->Dir_free = find_free_dir(dir,s);
      return true;
    }
  }
  free(a.name);
  return false;
}
bool Delete_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, short *index)
{
  if (*(index) < 0 || *(index) >= s->Dir_Size)
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
  int blocks = size / s->Block_Size;
  if (size % s->Block_Size != 0)
    blocks++;
  int cur = dir[*(index)].Fat_Index;
  int temp = cur;

  for (int i = 0; i < blocks; i++)
  {
    if (cur >= 0 && cur < s->fat_table_entries)
    {
      fat[cur].is_free = true;
      cur = fat[cur].next;
    }
  }
  fat[cur].next = s->free_fat_index;
  s->free_fat_index = temp;
  dir[*(index)].name[0] = '$';
  dir[*(index)].size = 0;
  dir[*(index)].Fat_Index = -1;
  s->dir_Count--;
  return true;
}

bool delete_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent)
{
  short index = find_file(dir,s, name, parent);
  if (index == -1)
    return false;
  if (dir[index].name == NULL)
  {
    return false;
  }
  if (dir[index].name[0] == '$')
    return false;
  truncate_file(dir, fat,s, name, parent);
  dir[index].name[0] = '$';
  return true;
}
bool Delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, short *index)
{
  if (*(index) < 0 || *(index) >= s->Dir_Size)
    return false;
  if (dir[*(index)].name == NULL)
  {
    return false;
  }
  if (dir[*(index)].name[0] == '$')
    return false;

  short count = 0;
  for (int i = 0; i < s->Dir_Size; i++)
  {
    if (dir[i].parent == *index)
      count++;
  }
  short *arr = (short *)malloc(sizeof(short) * count);
  count = 0;
  for (int i = 0; i < s->Dir_Size; i++)
  {
    if (dir[i].parent == *index)
      arr[count++] = i;
  }
  for (int i = 0; i < count; i++)
  {
    if (dir[arr[i]].size == -1)
      Delete_DIR(dir, fat,s, &arr[i]);
    else
      Delete_file(dir, fat,s, &arr[i]);
  }
  free(arr);
  dir[*(index)].name[0] = '$';
  dir[*index].size = 0;
  s->dir_Count--;
  return true;
}
bool delete_DIR(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent) // delete whole folder(all files and folder inside)
{
  int index = find_folder(dir,s, name, parent);
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
  for (int i = 0; i < s->Dir_Size; i++)
  {
    if (dir[i].parent == index)
      count++;
  }
  short *arr = (short *)malloc(sizeof(short) * count);
  count = 0;
  for (int i = 0; i < s->Dir_Size; i++)
  {
    if (dir[i].parent == index)
      arr[count++] = i;
  }
  for (int i = 0; i < count; i++)
  {
    if (dir[arr[i]].size == -1)
      Delete_DIR(dir, fat,s, &arr[i]);
    else
      Delete_file(dir, fat,s, &arr[i]);
  }
  free(arr);
  dir[index].name[0] = '$';
  dir[index].size = 0;
  s->dir_Count--;
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
int *view_all_childs(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent, short *count)
// returns DIR array of all childs + refrence of count
{
  short index = find_folder(dir,s, name, parent);
  int *childs;
  if (index == -1)
    return NULL;
  short Count = 0;
  for (int i = 1; i < s->Dir_Size; i++)
  {
    if (dir[i].parent == index && dir[i].name != NULL && strlen(dir[i].name) > 0)
      Count++;
  }
  childs = malloc(Count * sizeof(int));
  memcpy(count, &Count, sizeof(short));
  Count = 0;
  for (int i = 1; i < s->Dir_Size; i++)
  {
    if (dir[i].parent == index && dir[i].name != NULL && strlen(dir[i].name) > 0)
    {
      childs[Count++] = i;
    }
  }
  return childs;
}
bool Create_Folder(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent)
{
  int index = find_folder(dir,s, name, parent);
  if (index != -1)
  {
    printf("cannot create folder with same name in same context");
    return false;
  }
  int b = -1;
  struct DIR_ENTRY a = Validate_DIR_Entry(name, &b, parent,s);
  if (*parent > s->Dir_Size || *parent < 0)
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
  if (s->dir_Count < s->Dir_Size)
  {
    if (s->Dir_free != -1)
    {
      dir[s->Dir_free] = a;
      short i = s->Dir_free;
      s->Dir_free = find_free_dir(dir,s);
      return true;
    }
  }
  free(a.name);
  return false;
}
short find_file(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent)
{
  for (short i = 1; i < s->Dir_Size; i++)
  {
    if (dir[i].name != NULL && dir[i].name[0] != '$' && (strcmp(dir[i].name, name) == 0) && dir[i].parent == *(parent) && dir[i].size != -1)
      return i;
  }
  return -1;
}
short find_folder(struct DIR_ENTRY *dir,struct State *s, char *name, short *parent)
{
  for (short i = 0; i < s->Dir_Size; i++)
  {
    if (dir[i].name != NULL && dir[i].name[0] != '$' && (strcmp(dir[i].name, name) == 0) && dir[i].parent == *(parent) && dir[i].size == -1)
      return i;
  }
  return -1;
}
bool Truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, short *index)
// doing size 0 and making all the fat table entries free
{
  if (*index < 0 || *index >= s->Dir_Size)
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
  int prev_free_fat_index = s->free_fat_index; // Store the current free FAT index

  while (cur != -1)
  {
    if (cur >= s->fat_table_entries)
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
  s->free_fat_index = prev_free_fat_index;

  // Reset file directory metadata
  dir[*index].Fat_Index = -1;
  dir[*index].size = 0;
  printf("File truncated successfully.\n");
  return true;
}
bool truncate_file(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent)
// doing size 0 and making all the fat table entries free
{
  short index = find_file(dir,s, name, parent);
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
  int prev_free_fat_index = s->free_fat_index; // Store the current free FAT index

  while (cur != -1)
  {
    if (cur >= s->fat_table_entries)
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
  s->free_fat_index = prev_free_fat_index;

  // Reset file directory metadata
  dir[index].Fat_Index = -1;
  dir[index].size = 0;

  printf("File truncated successfully.\n");
  return true;
}

char *Read(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent, int *size, int *offset)
// returning char* array of size if(size+offset<= total file size)
{
  char *Buffer;
  Buffer = (char *)malloc(*size + 1);
  memset(Buffer, 0, *size + 1);
  int index = find_file(dir,s, name, parent);
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
  int n_of_b = *size / s->Block_Size;
  if (*size % s->Block_Size != 0)
    n_of_b++;
  int offset_b = *offset / s->Block_Size;
  int offset_in_b = *offset % s->Block_Size;
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
  char *temp = Read_Block(fat_index,s);
  int cur = s->Block_Size - offset_in_b;
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
    int bytes_to_read = (*size - c > s->Block_Size) ? s->Block_Size : *size - c;
    if (!(temp = Read_Block(fat_index,s)))
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
bool Write(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *name, short *parent, char *insert, int *offset)
{
  int index = find_file(dir,s, name, parent);
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

  if (*offset + size > s->Max_File * s->Block_Size)
  {
    printf("Offset and size exceed maximum file size.\n");
    return false;
  }

  if (dir[index].Fat_Index == -1 || dir[index].Fat_Index == 0)
  {
    int new_index = allocate_new_fat_entry(fat,s);
    if (new_index == -1)
      return false;
    dir[index].Fat_Index = new_index;
  }

  if (*offset > dir[index].size)
  {
    printf("Offset exceeds the current file size.\n");
    return false;
  }

  int n_of_b = size / s->Block_Size;
  if (size % s->Block_Size != 0)
    n_of_b++;
  int offset_b = *offset / s->Block_Size;
  int Offset = *offset % s->Block_Size;
  int fat_index = dir[index].Fat_Index;

  char *Buffer = Read_Block(fat_index,s);
  if (!Buffer)
  {
    printf("Failed to read block %d.\n", fat_index);
    return false;
  }

  for (int i = 0; i < offset_b; i++)
  {
    if (fat_index == -1 || fat[fat_index].next == -1)
    {
      int new_index = allocate_new_fat_entry(fat,s);
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
  int remaining = s->Block_Size - Offset;
  memcpy(Buffer + Offset, insert, (remaining > size) ? size : remaining);
  if (!Write_Block(fat_index, Buffer,s))
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
      int new_index = allocate_new_fat_entry(fat,s);
      if (new_index == -1)
      {
        free(Buffer);
        return false;
      }
      fat[fat_index].next = new_index;
    }
    fat_index = fat[fat_index].next;

    int bytes_to_write = (size - c > s->Block_Size) ? s->Block_Size : size - c;
    memset(Buffer, 0, s->Block_Size);
    memcpy(Buffer, insert + c, bytes_to_write);
    if (!Write_Block(fat_index, Buffer,s))
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

struct DIR_ENTRY Validate_DIR_Entry(char *name, int *size, short *parent,struct State *s)
{
  struct DIR_ENTRY a = {NULL, 0, 0, -1};
  if (name == NULL)
    return a;
  int n = strlen(name);
  if (name[0] == '$' || n > s->Name_Length || ((*size) != -1 && (*size) != 0) || (*parent) < 0 || s->dir_Count == s->Dir_Size)
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
char *Read_Block(int b_number,struct State *s)
{
  if (s->Block_Size <= 0)
  {
    fprintf(stderr,"Block Size not initialized.\n");
    return NULL;
  }

  char *Buf = (char *)malloc(s->Block_Size + 1);
  if (!Buf)
  {
    fprintf(stderr,"\nMemory allocation failed");
    return NULL;
  }

  fseek(fp_sys, b_number * s->Block_Size, SEEK_SET);
  if(fread(Buf, 1, s->Block_Size,fp_sys)<0)
  {
    fprintf(stderr,"couldnt read properly block number: %d",b_number); 
    free(Buf);
    return NULL;
  }
  fseek(fp_sys,0L,SEEK_SET);
  return Buf;
}

bool Write_Block(int b_number, char *Block,struct State *s)
{
  if (!Block)
  {
    fprintf(stderr,"\nBlock data is null.");
    return false;
  }
  if (s->Block_Size <= 0)
  {
    fprintf(stderr, "\nBlock Size not initialized.");
    return false;
  }
  fseek(fp_sys, b_number * s->Block_Size, SEEK_SET);
  if(fwrite(Block , 1, s->Block_Size, fp_sys)<0)
  {  
    fprintf(stderr,"\nError writing on Block:%d",b_number);
    return false;
  }
   
  fseek(fp_sys,0L,SEEK_SET);
  return true;
}
bool commit_changes(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s, char *filename)
{
  char *Buffer = (char *)malloc(s->Block_Size + 1);
  memset(Buffer, 0, s->Block_Size + 1);
  memcpy(Buffer,s,sizeof(*s));
  if (!Write_Block(0, Buffer,s))
  {
    printf("Couldnt commit variables");
    return false;
  }
  free(Buffer);
  int size = 0;
  size = (s->Dir_Size * 11 + s->Dir_Size * s->Name_Length);
  int n_of_b = size / s->Block_Size;
  if (size % s->Block_Size != 0)
    n_of_b++;
  Buffer = (char *)malloc((n_of_b * s->Block_Size) + 1);
  memset(Buffer, 0, (n_of_b * s->Block_Size) + 1);
  int index = 0;
  for (int i = 0; i < s->Dir_Size; i++)
  {
    int Size = strlen(dir[i].name);
    Buffer[index++] = Size;
    int temp = index;
    for (int j = 0; j < Size; j++)
    {
      Buffer[index++] = dir[i].name[j];
    }
    index = temp + s->Name_Length;
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

  char *temp_1 = (char *)malloc(s->Block_Size + 1);
  memset(temp_1, 0, s->Block_Size + 1);
  for (int i = 0; i < n_of_b; i++)
  {
    memcpy(temp_1, Buffer + (i * s->Block_Size), s->Block_Size);
    if (!Write_Block(s->dir_b_number + i, temp_1,s))
    {
      printf("couldnt write %d block", i);
    }
  }
  free(temp_1);
  free(Buffer);
  int fat_size = 5 * s->fat_table_entries;
  n_of_b = fat_size / s->Block_Size;
  if (size % s->Block_Size != 0)
    n_of_b++;
  Buffer = (char *)malloc((s->Block_Size * n_of_b) + 1);
  memset(Buffer, 0, (s->Block_Size * n_of_b) + 1);
  index = 0;
  for (int i = 0; i < s->fat_table_entries; i++)
  {
    char *a = (char *)&fat[i].is_free;
    Buffer[index++] = *(a);
    a = (char *)&fat[i].next;
    for (int j = 0; j < sizeof(int); j++)
    {
      Buffer[index++] = *(a + j);
    }
  }
  char *temp = (char *)malloc(s->Block_Size + 1);
  memset(temp, 0, s->Block_Size + 1);
  for (int i = 0; i < n_of_b; i++)
  {
    memcpy(temp, Buffer + (i * s->Block_Size), s->Block_Size);
    if (!Write_Block(s->fat_table_b_number + i, temp,s))
    {
      printf("couldnt write the fat %d block", i);
    }
  }
  free(temp);
  free(Buffer);
  return true;
}
bool load_dir_fat(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s)
{
  char *Buffer;
  int dir_size = s->Dir_Size * 11 + s->Dir_Size * s->Name_Length;
  int remaining = dir_size;
  Buffer = (char *)malloc(dir_size + 1);
  memset(Buffer, 0, dir_size + 1);
  int offset = 0;
  int n_of_b = dir_size / s->Block_Size;
  if (dir_size % s->Block_Size != 0)
    n_of_b++;
  for (int i = 0; i < n_of_b; i++)
  {
    char *b = Read_Block(i + s->dir_b_number,s);
    for (int j = 0; j < s->Block_Size && remaining > 0; j++)
    {
      Buffer[offset++] = b[j];
      remaining--;
    }
    free(b);
  }
  int index = 0;
  if (offset != dir_size)
    printf("shit");
  printf("\ndir size;%d",s->Dir_Size);
  for (int i = 0; i < s->Dir_Size; i++)
  {
    char Size = Buffer[index++];
    int temp = index;
    dir[i].name = (char *)malloc(Size + 1); // for /0
    for (int j = 0; j < Size; j++)
    {
      dir[i].name[j] = Buffer[index++];
    }
    dir[i].name[Size] = 0;
    index = temp + s->Name_Length;
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
  int fat_size = 5 * s->fat_table_entries;
  remaining = fat_size;
  n_of_b = fat_size / s->Block_Size;
  if (fat_size % s->Block_Size != 0)
    n_of_b++;
  Buffer = (char *)malloc(fat_size);
  for (int i = 0; i < n_of_b; i++)
  {
    char *b = Read_Block(i + s->fat_table_b_number,s);
    for (int j = 0; j < s->Block_Size && remaining > 0; j++)
    {
      Buffer[offset++] = b[j];
      remaining--;
    }
    free(b);
  }
  index = 0;
  for (int i = 0; i < s->fat_table_entries; i++)
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
bool load_variables(struct State *s)
{
  char *Buffer = Read_Block(0,s);
  memcpy(s,Buffer,sizeof(*s));
  free(Buffer);
  return true;
}
int main()
{
  struct DIR_ENTRY *dir;
  struct FAT_ENTRY *fat;
  struct State s={0,0,0,0,0,1,0,0,0,0,1};
  fp_sys=fopen("sys.bin","rb+");
  if(!fp_sys)
  {
    fp_sys=fopen("sys.bin","w");
    if(!fp_sys)
    {
      printf("couldnt create file");
      return 1;
    }
    fclose(fp_sys);
    fp_sys=fopen("sys.bin","rb+");
  }
  int a = 0;
  while (a != 1 && a != 2)
  {
    printf("do you want to create system again and formate? or Load? \n for load enter 1 \nFor new enter 2:");
    scanf("%d", &a);
    getchar();
    if (a == 2)
    {
      printf("\nEnter disk size in MB to be created:");
      scanf("%u", &s.System_Size);
      getchar();
      printf("\nEnter Block size in Bytes (should be >=64) to be created:");
      scanf("%hu", &s.Block_Size);
      getchar();
      if (s.Block_Size < 64)
      {
        printf("\nInvalid Blocksize:");
        a = -1;
        continue;
      }
      printf("\nEnter max total dir entries to be created:");
      scanf("%hi", &s.Dir_Size);
      getchar();
      if (s.Dir_Size <= 0)
      {
        printf("\nInvalid Dirsize:");
        a = -1;
        continue;
      }
      printf("\nEnter max number of blocks a file can have:");
      scanf("%hu", &s.Max_File);
      getchar();
      printf("\nEnter max Name Length a file can have:");
      int temp=0;
      scanf("%d", &temp);
      getchar();
      s.Name_Length=temp;
      if (s.Name_Length <= 0)
        if (s.Dir_Size <= 0)
        {
          printf("\nInvalid NameLength:");
          a = -1;
          continue;
        }
    }
  }
  if (a == 2)
  {
    if (!create_partition_file("sys.bin", s.System_Size))
    {
      printf("\ncouldnt");
    }
    if (!format("sys.bin", &s))
    {
      printf("\nCouldnt format");
    }
    FILE *fp = fopen("Blocksize.txt", "w");
    fprintf(fp, "%hu", s.Block_Size);
    fclose(fp);
  }
  else
  {
    FILE *fp = fopen("Blocksize.txt", "r");
    fscanf(fp, "%hu", &s.Block_Size);
    fclose(fp);
    CCD("sys.bin",&s);
  }
  if (!load_variables(&s))
  {
    printf("couldnt load");
  }
  dir = (struct DIR_ENTRY *)malloc(sizeof(struct DIR_ENTRY) * s.Dir_Size);          // Initialize directory
  fat = (struct FAT_ENTRY *)malloc(sizeof(struct FAT_ENTRY) * s.fat_table_entries); // Initialize FAT table
  if (!load_dir_fat(dir, fat,&s))
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
      create_file_menu(dir,&s);
      break;
    case 2:
      write_file_menu(dir, fat,&s);
      break;
    case 3:
      read_file_menu(dir, fat,&s);
      break;
    case 4:
      create_folder_menu(dir,&s);
      break;
    case 5:
      delete_dir_menu(dir, fat,&s);
      break;
    case 6:
      delete_file_menu(dir, fat,&s);
      break;
    case 7:
      truncate_file_menu(dir, fat,&s);
      break;
    case 8:
      view_all_childs_menue(dir,&s);
      break;
    case 9:
      printf("Exiting...\n");
      CCE("sys.bin",&s);
      printf("\ndir size;%d",s.Dir_Size);
      for (int i = 0; i < s.Dir_Size; i++)
        free(dir[i].name);
      free(dir);
      free(fat);
      fclose(fp_sys);
      return 0;
    default:
      printf("Invalid choice! Please try again.\n");
    }
    if (choice >= 1 && choice <= 8)
    {
      if (!commit_changes(dir, fat,&s, "sys.bin"))
      {
        printf("Couldnt commit! ");
      }
    }
  }
  return 0;
}

// Menu handlers
void create_file_menu(struct DIR_ENTRY *dir,struct State*s)
{
  short parent = 0;
  char *filename = malloc(s->Name_Length + 1);
  printf("\nEnter file name: ");
  if (fgets(filename, s->Name_Length + 1, stdin) == NULL)
  {
    printf("Error reading file name.\n");
    return;
  }
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline character
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (Create_File(dir,s, filename, &parent))
  {
    printf("File created successfully.\n");
  }
  else
  {
    printf("Failed to create file.\n");
  }
  free(filename);
}
void CCE(char *Filename,struct State*s)
{
  char a[64];
  strcpy(a, "key-");
  strcat(a, Filename);
  FILE *fp2 = fopen(a, "w");
  char key = time(0) % 256;
  fputc(key, fp2);
  fclose(fp2);
  int n_of_b=(s->System_Size*1024*1024/s->Block_Size);
  char*Buffer;
  for(int i=0;i<n_of_b;i++)
  {
    Buffer=Read_Block(i,s);
    for (int i = 0; i < s->Block_Size; i++)
    {
      int c = (key + Buffer[i]) % 256;
      Buffer[i] = c;
    }
    if(!Write_Block(i,Buffer,s))
    {
      fprintf(stderr,"Couldnt encode block: %d",i);
      free(Buffer);
      return ;
    }  
   free(Buffer);
  }
}
void CCD(char *filename,struct State*s)
{
  char *buffer;
  char a[64];
  strcpy(a, "key-");
  strcat(a, filename);
  FILE *fp2 = fopen(a, "r");
  char key = fgetc(fp2);
  fclose(fp2);
  buffer=Read_Block(0,s);
  for (int i = 0; i < s->Block_Size; i++)
  {
    int c = buffer[i] - key;
    if (c < 0)
      c += 256;
    c = c % 256;
    buffer[i] = c;
  }
  if(!Write_Block(0,buffer,s))
  {
    fprintf(stderr,"Couldnt encode block: %d",0);
    free(buffer);
    return ;
  }
  free(buffer);
  if(!load_variables(s))
  {
    printf("Couldnot load variables");
    return;
  }
  int n_of_b=(s->System_Size*1024*1024/s->Block_Size);
  for(int i=1;i<n_of_b;i++)
  {
    buffer=Read_Block(i,s);
    for (int i = 0; i < s->Block_Size; i++)
    {
      int c = buffer[i] - key;
      if (c < 0)
        c += 256;
      c = c % 256;
      buffer[i] = c;
    }
    if(!Write_Block(i,buffer,s))
    {
      fprintf(stderr,"Couldnt encode block: %d",i);
      free(buffer);
      return ;
    }
    free(buffer);
  }
}

void write_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s)
{
  short parent = 0;
  char *filename = malloc(s->Name_Length);
  char *content = malloc(s->Block_Size);
  memset(content, 0, s->Block_Size);
  int offset = 0;
  printf("Enter file name: ");
  fgets(filename, s->Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  printf("\nEnter text for file: ");
  fgets(content, s->Block_Size + 1, stdin);
  content[strcspn(content, "\n")] = '\0';
  printf("\nEnter offset to place this array:");
  scanf("%d", &offset);
  getchar();
  // Remove newline
  if (Write(dir, fat,s, filename, &parent, content, &offset))
  {
    printf("\nContent written successfully.");
  }
  else
  {
    printf("\nFailed to write to file.");
  }
  free(filename);
  free(content);
}
void view_all_childs_menue(struct DIR_ENTRY *dir,struct State*s)
{
  short parent = 0;
  char *filename = malloc(s->Name_Length + 1);
  int size, offset = 0;
  printf("Enter file name: ");
  fgets(filename, s->Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  short count = 0;
  int *d = view_all_childs(dir,s, filename, &parent, &count);
  for (int i = 0; i < count; i++)
  {
    printf("\nFilename :%s ,Size: %d, File\'s Fat index: %d,Parent : %hi", dir[d[i]].name, dir[d[i]].size, dir[d[i]].Fat_Index, dir[d[i]].parent);
  }
  free(d);
  free(filename);
}
void read_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s)
{
  short parent = 0;
  char *filename = malloc(s->Name_Length + 1);
  int size, offset = 0;
  printf("Enter file name: ");
  fgets(filename, s->Name_Length + 1, stdin);
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
  char *data = Read(dir, fat,s, filename, &parent, &size, &offset);
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

void create_folder_menu(struct DIR_ENTRY *dir,struct State*s)
{

  short parent = 0;
  char *foldername = malloc(s->Name_Length + 1);
  printf("Enter folder name: ");
  fgets(foldername, s->Name_Length + 1, stdin);
  foldername[strcspn(foldername, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (Create_Folder(dir,s, foldername, &parent))
  {
    printf("Folder created successfully.\n");
  }
  else
  {
    printf("Failed to create folder.\n");
  }
  free(foldername);
}

void delete_dir_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s)
{
  short parent = 0;
  char *foldername = malloc(s->Name_Length + 1);
  printf("Enter directory name: ");
  fgets(foldername, s->Name_Length + 1, stdin);

  foldername[strcspn(foldername, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (delete_DIR(dir, fat,s, foldername, &parent))
  {
    printf("Directory deleted successfully.\n");
  }
  else
  {
    printf("Failed to delete directory.\n");
  }

  free(foldername);
}
void truncate_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s)
{
  short parent = 0;
  char *filename = malloc(s->Name_Length + 1);
  printf("Enter file name: ");
  fgets(filename, s->Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (truncate_file(dir, fat,s, filename, &parent))
  {
    printf("File deleted successfully.\n");
  }
  else
  {
    printf("Failed to delete file.\n");
  }
  free(filename);
}
void delete_file_menu(struct DIR_ENTRY *dir, struct FAT_ENTRY *fat,struct State *s)
{
  short parent = 0;
  char *filename = malloc(s->Name_Length + 1);
  printf("Enter file name: ");
  fgets(filename, s->Name_Length + 1, stdin);
  filename[strcspn(filename, "\n")] = '\0'; // Remove newline
  printf("\nEnter parent");
  scanf("%hi", &parent);
  getchar();
  if (delete_file(dir, fat,s, filename, &parent))
  {
    printf("File deleted successfully.\n");
  }
  else
  {
    printf("Failed to delete file.\n");
  }
  free(filename);
}