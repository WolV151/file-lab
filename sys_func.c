/*
    Name: Marin Donchev
    SID: R00192936
    Group: SDH4-A
*/

// system functions for the filesystem

#include "filesystem.h"

int bytes_to_int(char *byte_sequence) // function from here https://stackoverflow.com/questions/12240299/convert-bytes-to-int-uint-in-c
{
    int my_int = byte_sequence[0] + (byte_sequence[1] << 8) + (byte_sequence[2] << 16) + (byte_sequence[3] << 24);

    return my_int;
}

void get_param(char *line, char **dest, int param_no, int offset) // gets parameters from cat and mv
{
    int i = offset;
    int j = 0;
    int s = 0;
    char params[param_no][10]; // file names can be 10 chars
    char temp[10];             // construct strings

    for (s; s < param_no; s++)
    {
        j = 0;
        clear_buffer(temp, 10);
        for (i; i < (param_no * 10); i++)
        {
            temp[j] = line[i];
            if (temp[j] == 32 || line[i] == 0)
            {
                temp[j] = '\0';
                kstrcpy(params[s], temp);
                i++;
                break;
            }
            j++;
        }
    }
    kmemcpy(dest, params, (param_no * 10 * sizeof(char)));
    clear_buffer(line, 100);
}

void clear_buffer(char *buf, int size) // function call instead of repeating this for loop
{
    for (int i = 0; i < size; i++)
    {
        buf[i] = '\0';
    }
}

int find_file_by_name(char *file) // a function to get the file by name for cat and mv
{
    for (int i = 0; i < master.no; i++)
    {
        if (!kstrcmp(file, master.files[i].name))
            return i;
    }
    return -1;
}

void master_clear() // clear master structure
{
    int i;
    for (i = 0; i < master.no; i++)
    {
        clear_buffer(master.files[i].name, 10);
        clear_buffer(master.files[i].sectors, 10);
        master.files[i].size = 0;
    }
}

void master_init() // initialize the master struct, e.g. stores all files with their attributes
{
    int i;
    unsigned int sector = 0; // 0th sector contains file metadata
    char buf[512];
    unsigned char size_bits[4];
    unsigned int flag = 0;    // read mode: name, sectors, size
    unsigned int file_no = 0; // file number (0-9)
    unsigned int fsector = 0; // currently selected sector (0-9)
    unsigned int counter = 0; // mark every 10 bytes for name and sectors and 4 for size
    master.no = 0;            // the counter would need to be changed if we change the sizes in the master struct

    clear_buffer(buf, 512);

    getSector(sector, buf);

    // skip header (4 bytes)
    for (i = 4; i < 512; i++)
    {
        switch (flag)
        {
        case 0: // read name
            if (buf[i] != 0)
                master.files[file_no].name[counter] = buf[i];

            if (counter == 9)
            {
                flag = 1;    // switch to sectors read mode
                counter = 0; // reset counter
                continue;
            }
            counter++;
            break;
        case 1: // read sectors
            if (buf[i] != 0)
                master.files[file_no].sectors[counter] = buf[i];

            if (counter == 9)
            {
                flag = 2;    // switch to size read mode
                counter = 0; // reset counter
                continue;
            }
            counter++;
            break;
        case 2: // read size
            if (buf[i] != 0)
                size_bits[counter] = buf[i];

            if (counter == 3)
            {
                flag = 0;    // switch to name read mode
                counter = 0; // reset counter

                if ((size_bits[0] + size_bits[1] + size_bits[2] + size_bits[3]) != 0)
                {
                    master.files[file_no].size = bytes_to_int(size_bits);
                    clear_buffer(size_bits, 4);
                }

                if (buf[i + 1] != 0)
                {
                    file_no++; // move onto the next file if any left
                }
                continue;
            }
            counter++;
            break;

        default:
            break;
        }
    }
    master.no = file_no + 1;
    clear_buffer(buf, 512);
}

void ls()
{
    int i;

    for (i = 0; i < master.no; i++)
    {
        uprintf("%s - %dbytes\n", master.files[i].name, master.files[i].size);
    }
}

void cat(char *file)
{
    int file_number = find_file_by_name(file); // find file
    unsigned int sector;

    char buf[512];
    int i; // goes through sectors (1-10)
    int j; // goes through the sector (0-511)

    if (file_number == -1)
        uprintf("No such file.\n"); // if no file found function returns -1
    else
    {
        for (i = 0; i < MAX_SECTORS; i++) // go through sectors (1-10)
        {
            sector = master.files[file_number].sectors[i]; // grab the sector with index from the file

            if (!sector) // if no sector left, don't bother continuing
                break;

            clear_buffer(buf, 512);
            getSector(sector, buf);
            for (j = 0; j < 512; j++)
            {
                if (buf[j] != 0)
                {
                    uprintf("%c", buf[j]);
                }
            }
        }
    }
    clear_buffer(buf, 512);
}

void mv(char *file, char *new_name)
{
    unsigned int sector = 0; // sector 0 metadata
    char buf[512];           // store sector

    unsigned int file_number = find_file_by_name(file); // find file index

    if (new_name[9] != 0)  // some checks
    {
        uprintf("\nProvided new name is too long. File name size must be 9 characters.\n");
        return -1;
    }

    if (file_number == -1)
    {
        uprintf("\nNo such file found.\n");
        return -1;
    }
    else if (find_file_by_name(new_name) >= 0)
    {
        uprintf("\nNew file name already exists. Please choose a different name.\n");
        return -1;
    }

    unsigned int write_index = (sizeof(struct sdc_File) * file_number) + 4; // determine the start position to write

    clear_buffer(buf, 512); // clear buffer

    getSector(sector, buf); // move sector data into buffer

    unsigned int i = write_index;
    unsigned int j = 0;

    for (i; i < (10 + write_index); i) // write the new file name into buffer
        for (j; j < 10; j)
            buf[i++] = new_name[j++];

    putSector(sector, buf); // and copy it into sector
    master_clear();         // clear master and re-initialize it
    master_init();
    clear_buffer(buf, 512);
}

/*
COPY FUNCTION SUGGESTION:

1. find the file to copy by name.
2. based on this name, find which file it is in the "master" struct and its sectors and size.
3. check if the master has reached its max file limit, if it is - return, else continue.
4. check if the master has reached its max sectors, if it has - return, else continue.
5. check if there are enough free sectors in the mastser for the file to be copied, if not return.
6. in sector 0 calculate the offset (similarly to mv) to place the copied file name, sectors and size
7. find the first free sector
    * I believe this can be done by going through all the sectors (except 0) and checking the first
    byte and if it is zero, this means that the sector is free and it would be safe to
    assume, in this case, the next sectors would be free too.

    Considering the check done in 4 in a similar fashion to the description above, enough sectors should
    already be available.

8. have a for loop to copy all the sectors in a buffer, one by one, and then write them to the new sectors (which
   should already be specified from sector 0).

* There should be an additional check to rename the file if no name is specified to avoid having files
  with the same name, e.g. test -> test1. This is assuming file extentions do not matter.
*/