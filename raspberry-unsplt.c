/*
MIT License

Copyright (c) 2019 Thomas Lovell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/* Whew.  Do I really need that? */

#define  VERS        "1.01"
#define DEBUG 0     /* '1' produces debug info; '0' does not */
#define _LARGEFILE64_SOURCE
#define S 512      /* sector size */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
void hexDump(char *desc, void *addr, int len);

int main(int argc, char **argv) {
int      ifd, ofd;
char     bfn[S], ifn[S+8], ofn[S+5];
int      ret;
ssize_t  sz=0;
int      errsv;
off64_t  off64t, of, sectors;
unsigned short int sig=43605; /* this is 0x55aa */

unsigned int p1_lba;
unsigned int p1_num;
unsigned int p2_lba;
unsigned int p2_num;

union rec_tag {
    struct mbr_tag {
        char zeros[440];
        int  diskid;
        char diskid2[2];
        struct part_tag {
            unsigned char status[1];
            unsigned char chs_first[3];
            unsigned char type[1];
            unsigned char chs_last[3];
            unsigned char lba[4];
            unsigned char num[4];
        } p1, p2, p3, p4;
        unsigned char disksig[2];
    } mbr;
    char    buf[S];
} rec;

//  --------------------------------------  //
//  Use either this internal base filename  //
//  or one from the command line.           //
//  --------------------------------------  //
    if (argv[1] == NULL)
        strncpy(bfn, "/home/pi/Downloads/raspios/2025-12-04-raspios-trixie-arm64.img", sizeof(bfn));
    else
        strncpy(bfn, argv[1], sizeof(bfn)-1);
    printf("basename set to %s\n", bfn);
//  ---------------------------------  //
//  append '.unsplit' to the basename  //
//  ---------------------------------  //
    strcpy(ofn, bfn);
    strcat(ofn, ".unsplit");
    printf("output filename set to %s\n", ofn);
//  --------------------  //
//  then open the output  //
//  --------------------  //
    if ((ofd = open(ofn, O_WRONLY | O_LARGEFILE | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
        errsv = errno;
        printf("The output file '%s' could not be opened/created.\n", ofn);
        printf(" errno is '%i - %s'\n", errsv, strerror(errsv));
        exit(4);
    }
//  -----------------------------  //
//  append '.mbr' to the basename  //
//  -----------------------------  //
    strcpy(ifn, bfn);
    strcat(ifn, ".mbr");
    printf("input filename set to %s\n", ifn);
//  ---------------------  //
//  then open it as input  //
//  ---------------------  //
    if ((ifd = open(ifn, O_RDONLY | O_LARGEFILE)) == -1) {
        errsv = errno;
        printf("The input file '%s' could not be opened.\n", ifn);
        printf(" errno is '%i - %s'\n", errsv, strerror(errsv));
        exit(4);
    }
//  -------------------  //
//  now read in the mbr  //
//  -------------------  //
    if ((sz = read(ifd, (void *) rec.buf, S)) == S) {
        if (DEBUG) {
            hexDump("diskid  ", &rec.mbr.diskid, 4);
            hexDump("disksig ", &rec.mbr.disksig, 2);
            hexDump("0x55aa  ", &sig, 2);
        }
        if (memcmp(&rec.mbr.disksig, &sig, 2) != 0) {
            printf("Input mbr file disk signature, %x%x, not 0x55aa.\n", rec.mbr.disksig[0], rec.mbr.disksig[1]);
            exit(4);
        }
    } else {
        printf("size read, %li, not %i\n", sz, S);
        exit(4);
    }
//  ---------------------------------------------------------------  //
//  save the start and number of sectors of the original partitions  //
//  ---------------------------------------------------------------  //
    memcpy(&p1_lba, &rec.mbr.p1.lba, 4);
    memcpy(&p1_num, &rec.mbr.p1.num, 4);
    memcpy(&p2_lba, &rec.mbr.p2.lba, 4);
    memcpy(&p2_num, &rec.mbr.p2.num, 4);
//  -------------  //
//  write the mbr  //
//  -------------  //
    write(ofd, rec.buf, S);
//  -------------------  //
//  and close the input  //
//  -------------------  //
    close(ifd);
//  --------------------------------------------------------------------  //
//  now position the output file to where the 1st partition should start  //
//  --------------------------------------------------------------------  //
    of = p1_lba*S;
    off64t = lseek64(ofd, of, SEEK_SET);
//  ------------------------------  //
//  how many sectors we're copying  //
//  ------------------------------  //
    sectors = p1_num;
//  ------------------------------------------  //
//  append '.boot' to the basename of the input //
//  ------------------------------------------  //
    strcpy(ifn, bfn);
    strcat(ifn, ".boot");
    printf("input filename set to %s\n", ifn);
//  -------------------------------  //
//  open the input file, i.e. .boot  //
//  -------------------------------  //
    if ((ifd = open(ifn, O_RDONLY | O_LARGEFILE)) == -1) {
        errsv = errno;
        printf("The input file '%s' could not be opened.\n", ifn);
        printf(" errno is '%i - %s'\n", errsv, strerror(errsv));
        exit(4);
    }
//  -------------------------  //
//  and send it to the output  //
//  -------------------------  //
    while (sectors--) {
        if (!(sz = (read(ifd, rec.buf, S)) == S)) {
            printf ("size read, %li, not %i\n", sz, S);
            exit(4);
        }
        write(ofd, rec.buf, S);
    }
//  ---------------------------------------------------------------------  //
//  finally, close the input in anticipation of writing the 2nd partition  //
//  ---------------------------------------------------------------------  //
    close(ifd);
//  --------------------------------------------------------------------  //
//  now position the output file to where the 2nd partition should start  //
//  --------------------------------------------------------------------  //
    of = p2_lba*S;
    off64t = lseek64(ofd, of, SEEK_SET);
//  ------------------------------  //
//  how many sectors we're copying  //
//  ------------------------------  //
    sectors = p2_num;
//  ------------------------------------------  //
//  append '.root' to the basename of the input //
//  ------------------------------------------  //
    strcpy(ifn, bfn);
    strcat(ifn, ".root");
    printf("input filename set to %s\n", ifn);
//  -------------------------------  //
//  open the input file, i.e. .root  //
//  -------------------------------  //
    if ((ifd = open(ifn, O_RDONLY | O_LARGEFILE)) == -1) {
        errsv = errno;
        printf("The input file '%s' could not be opened.\n", ifn);
        printf(" errno is '%i - %s'\n", errsv, strerror(errsv));
        exit(4);
    }
//  -------------------------  //
//  and send it to the output  //
//  -------------------------  //
    while (sectors--) {
        if (!(sz = (read(ifd, rec.buf, S)) == S)) {
            printf ("size read, %li, not %i\n", sz, S);
            exit(4);
        }
        write(ofd, rec.buf, S);
    }
//  ---------------  //
//  close the input  //
//  ---------------  //
    close(ofd);
//  ----------------  //
//  close the output  //
//  ----------------  //
    close(ifd);
//  --------------  //
//  and we're done  //
//  --------------  //
    exit(0);
}

void hexDump(char *desc, void *addr, int len) {
/*
MIT License

Copyright (c) 2019 Thomas Lovell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with lne offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}
