/*
 * vPICdisasm - PIC program disassembler.
 * Version 1.2 - July 2010.
 * Written by Vanya A. Sergeev - <vsergeev@gmail.com>
 *
 * Copyright (C) 2007 Vanya A. Sergeev
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
 *
 * ui.c - Main user interface to PIC disassembler. Takes care of program arguments
 *  setting disassembly formatting options, and program file type recognition. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file.h"
#include "errorcodes.h"

/* Flags for some long options that don't have a short option equivilant */
static int no_addresses = 0;				/* Flag for --no-addresses */
static int literal_base = 0;				/* Value of the literal base (hex, bin, dec) */
static int literal_ascii_comment = 0;			/* Flag for --literal-ascii */
static int no_destination_comments = 0;			/* Flag for --no-destination-comments */

static struct option long_options[] = {
	{"address-label", required_argument, NULL, 'l'},
	{"arch", required_argument, NULL, 'a'},
	{"file-type", required_argument, NULL, 't'},
	{"no-addresses", no_argument, &no_addresses, 1},
	{"literal-bin", no_argument, &literal_base, FORMAT_OPTION_LITERAL_BIN},
	{"literal-dec", no_argument, &literal_base, FORMAT_OPTION_LITERAL_DEC},
	{"literal-hex", no_argument, &literal_base, FORMAT_OPTION_LITERAL_HEX},
	{"literal-ascii", no_argument, &literal_ascii_comment, 1},
	{"no-destination-comments", no_argument, &no_destination_comments, 1},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static void printUsage(FILE *stream, const char *programName) {
	fprintf(stream, "Usage: %s <option(s)> <file>\n", programName);
	fprintf(stream, " Disassembles PIC program file <file>.\n");
	fprintf(stream, " Written by Vanya A. Sergeev - <vsergeev@gmail.com>.\n\n");
	fprintf(stream, " Additional Options:\n\
  -a, --arch <architecture>	Specify the 8-bit PIC architecture to use\n\
				during disassembly.\n\
  -t, --file-type <type>	Specify the file type of the object file.\n\
  -l, --address-label <prefix> 	Create ghetto address labels with \n\
				the specified label prefix.\n\
  --no-addresses		Do not display the address alongside\n\
				disassembly.\n\
  --no-destination-comments	Do not display the destination address\n\
				comments of relative branch instructions.\n\
  --literal-hex			Represent literals in hexadecimal (default)\n\
  --literal-bin			Represent literals in binary\n\
  --literal-dec			Represent literals in decimal\n\
  --literal-ascii		Show ASCII value of literal operands in a\n\
				comment\n\
  -h, --help			Display this usage/help.\n\
  -v, --version			Display the program's version.\n\n");
	fprintf(stream, "Supported 8-bit PIC Architectures:\n\
  Baseline 			baseline\n\
  Mid-Range			midrange (default)\n\
  Enhanced Mid-Range		enhanced\n\n");
	fprintf(stream, "Supported file types:\n\
  Intel HEX 			ihex\n\
  Auto-recognized with .hex, .ihex, and .ihx file extensions.\n\n\
  Motorola S-Record 		srecord\n\
  Auto-recognized with .srec and .sre file extensions.\n\n");
}

static void printVersion(FILE *stream) {
	fprintf(stream, "vPICdisasm version 1.2 - 07/28/2010.\n");
	fprintf(stream, "Written by Vanya Sergeev - <vsergeev@gmail.com>\n");
}
	
int main (int argc, const char *argv[]) {
	int optc;
	FILE *fileIn, *fileOut;
	char arch[9], fileType[8], *fileExtension;
	int archSelect;
	int (*disassembleFile)(FILE *, FILE *, formattingOptions, int);
	formattingOptions fOptions;

	/* Recent flag options */
	fOptions.options = 0;
	/* Set default address field width for this version. */
	fOptions.addressFieldWidth = 3;
	/* Just print to stdout for this version */
	fileOut = stdout;

	arch[0] = '\0';
	fileType[0] = '\0';	
	while (1) {
		optc = getopt_long(argc, (char * const *)argv, "l:t:a:hv", long_options, NULL);
		if (optc == -1)
			break;
		switch (optc) {
			/* Long option */
			case 0:
				break;
			case 'l':
				fOptions.options |= FORMAT_OPTION_ADDRESS_LABEL;
				strncpy(fOptions.addressLabelPrefix, optarg, sizeof(fOptions.addressLabelPrefix));
				break;
			case 't':
				strncpy(fileType, optarg, sizeof(fileType));
				break;
			case 'a':
				strncpy(arch, optarg, sizeof(arch));
				break;
			case 'h':
				printUsage(stderr, argv[0]);
				exit(EXIT_SUCCESS);	
			case 'v':
				printVersion(stderr);
				exit(EXIT_SUCCESS);
			default:
				printUsage(stdout, argv[0]);
				exit(EXIT_SUCCESS);
		}
	}

	if (!no_addresses)
		fOptions.options |= FORMAT_OPTION_ADDRESS;
	if (!no_destination_comments)
		fOptions.options |= FORMAT_OPTION_DESTINATION_ADDRESS_COMMENT;	

	if (literal_base == FORMAT_OPTION_LITERAL_DEC)
		fOptions.options |= FORMAT_OPTION_LITERAL_DEC;
	else if (literal_base == FORMAT_OPTION_LITERAL_BIN)
		fOptions.options |= FORMAT_OPTION_LITERAL_BIN;
	else
		fOptions.options |= FORMAT_OPTION_LITERAL_HEX;

	if (literal_ascii_comment)
		fOptions.options |= FORMAT_OPTION_LITERAL_ASCII_COMMENT;

	if (optind == argc) {
		fprintf(stderr, "Error: No program file specified!\n\n");
		printUsage(stderr, argv[0]);
		exit(EXIT_FAILURE);
	}

	/* If no file type was specified, try to auto-recognize the file type by extension. */
	if (fileType[0] == '\0') {
		fileExtension = rindex(argv[optind], '.');
		if (fileExtension == NULL) {
			fprintf(stderr, "Unable to auto-recognize file type by extension.\n");
			fprintf(stderr, "Please specify file type with -t,--file-type option.\n");
			exit(EXIT_FAILURE);
		}
		/* To skip the dot */
		fileExtension++;
		if (strcasecmp(fileExtension, "ihx") == 0)
			strcpy(fileType, "ihex");
		else if (strcasecmp(fileExtension, "hex") == 0)
			strcpy(fileType, "ihex");
		else if (strcasecmp(fileExtension, "ihex") == 0)
			strcpy(fileType, "ihex");
		else if (strcasecmp(fileExtension, "srec") == 0)
			strcpy(fileType, "srecord");
		else if (strcasecmp(fileExtension, "sre") == 0)
			strcpy(fileType, "srecord");
		else {
			fprintf(stderr, "Unable to auto-recognize file type by extension.\n");
			fprintf(stderr, "Please specify file type with -t,--file-type option.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* If no architecture was specified, use midrange by default */
	if (arch[0] == '\0') {
		archSelect = PIC_MIDRANGE;
	} else {
		if (strcasecmp(arch, "baseline") == 0)
			archSelect = PIC_BASELINE;
		else if (strcasecmp(arch, "midrange") == 0)
			archSelect = PIC_MIDRANGE;
		else if (strcasecmp(arch, "enhanced") == 0)
			archSelect = PIC_MIDRANGE_ENHANCED;
		else {
			fprintf(stderr, "Unknown 8-bit PIC architecture %s.\n", arch);
			fprintf(stderr, "See program help/usage for supported PIC architectures.\n");
			exit(EXIT_FAILURE);
		}
	}
	
	/* I check the fileType string here and set the disassembleFile
	 * pointer so I don't have to do any file type error checking after
	 * I've opened the file, this means cleaner error handling. */
	if (strcasecmp(fileType, "ihex") == 0)
		disassembleFile = disassembleIHexFile;
	else if (strcasecmp(fileType, "srecord") == 0)
		disassembleFile = disassembleSRecordFile;
	else {
		fprintf(stderr, "Unknown file type %s.\n", fileType);
		fprintf(stderr, "See program help/usage for supported file types.\n");
		exit(EXIT_FAILURE);
	}
	
	fileIn = fopen(argv[optind], "r");
	if (fileIn == NULL) {
		perror("Error: Cannot open program file for disassembly");
		exit(EXIT_FAILURE);
	}

	disassembleFile(fileOut, fileIn, fOptions, archSelect);
	
	fclose(fileIn);

	exit(EXIT_SUCCESS);
}