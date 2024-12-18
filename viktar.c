#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <md5.h>

#include "viktar.h"

static int is_verbose = 0;
#define FLAGS 301
//#define BUFFER 2000
#define BUFFER 500

void extract_archive(const char * archive_name, char * argv[], int argc);
void extract_files(int iarch, viktar_header_t * header);

void create_archive(const char * archive_name, char * argv[], int argc);
void display_short_toc(const char * archive_name);
void display_long_toc(const char * archive_name);
void validate_archive(const char * archive_name);

int main(int argc, char * argv[])
{
   int opt = 0;
   char * archive_name = NULL;
   
   viktar_action_t flag = ACTION_NONE; 
  
   while ((opt = getopt(argc, argv, OPTIONS)) != -1)
   { 
       switch (opt)
       {
           case 'x':
               flag = ACTION_EXTRACT; 
               break;

           case 'c':
               flag = ACTION_CREATE; 
               break;
     
           case 't':
               flag = ACTION_TOC_SHORT;
               break;

           case 'T':
               flag = ACTION_TOC_LONG;
               break;

           case 'f':
               archive_name = optarg;

               break;

           case 'V':
               flag = ACTION_VALIDATE;
               break;

           case 'h':
               fprintf(stderr, "help text"
                      "\n\t./viktar"
                      "\n\tOptions: xctTf:Vhv"
                      "\n\t\t-x\t\textract file/files from archive"
		      "\n\t\t-c\t\tcreate an archive file"
                      "\n\t\t-t\t\tdisplay a short table of contents of the archive file"
                      "\n\t\t-T\t\tdisplay a long table of contents of the archive file"
                      "\n\t\tOnly one of xctTV can be specified"
                      "\n\t\t-f filename\tuse filename as the archive file"
                      "\n\t\t-V\t\tvalidate the MD5 values in the viktar file"
                      "\n\t\t-v\t\tgive verbose diagnostic messages"
                      "\n\t\t-h\t\tdisplay this AMAZING help message\n"); 

               exit(EXIT_SUCCESS); 
               break;

           case 'v':
               is_verbose = 1;
               break;

           default:
               fprintf(stderr, "oopsie - unrecognized command line option \"%s\"\n", argv[optind]);
 
               exit(EXIT_FAILURE);

               break;
       }
   }

   if (is_verbose)
       fprintf(stderr, "verbose level: %d\n", is_verbose);

   if (flag == ACTION_NONE) 
   {
       fprintf(stderr, "no action supplied\nexiting without doing ANYTHING...\n");
       exit(EXIT_FAILURE);
   }

   switch (flag)
   {
       case ACTION_CREATE:
           create_archive(archive_name, argv, argc);
           break;

      case ACTION_EXTRACT:
           extract_archive(archive_name, argv, argc);
           break;

       case ACTION_TOC_SHORT:
           display_short_toc(archive_name);
           break;

       case ACTION_TOC_LONG:
           display_long_toc(archive_name);
           break;

       case ACTION_VALIDATE:
           validate_archive(archive_name);

       default:
           break;
   }
 
   return EXIT_SUCCESS;
}

void create_archive(const char * archive_name, char * argv[], int argc)
{
   int oarch = STDOUT_FILENO;
   struct stat md;
   int fd = 0;
   viktar_header_t header;
   viktar_footer_t footer;
   unsigned char md5_header_buf[sizeof(viktar_header_t)];
   unsigned char md5_data_buf[sizeof(viktar_footer_t)];
   MD5_CTX context;
   size_t bytes_read = 0;

   if (!archive_name)
   {
       for (int i = optind; i < argc; ++i)
           printf("%s ", argv[i]);
   }

   if (archive_name)
      oarch = open(archive_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

   fchmod(oarch, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

   if (oarch < 0) 
   {
       fprintf(stderr, "failed to open output archive file \"%s\"", archive_name ? archive_name : "stdin");
       fprintf(stderr, "\nexiting...\n");
       exit(EXIT_FAILURE);
   }

   //Writes the VIKTAR_FILE header line into the file
   write(oarch, VIKTAR_TAG, strlen(VIKTAR_TAG));

   if (is_verbose)
   {
       printf("creating archive file: \"%s\"", archive_name);
      
       for (int i = optind; i < argc; ++i)
           printf("\nadding file: %s to archive: %s", argv[i], archive_name);
   }

   for (int i = optind; i < argc; ++i)
   { 
       fd = open(argv[i], O_RDONLY);

       if (fd < 0) {
           perror("Error opening input file");
           close(oarch);
           exit(EXIT_FAILURE);
       }

       //fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

       if (fstat(fd, &md) < 0)
       {
           perror("Error getting file status");
           close(fd);
           exit(EXIT_FAILURE);
       }

       //Fill in data members for viktar header. 
       //memset(&header, 0, sizeof(viktar_header_t));
       strncpy(header.viktar_name, argv[i], VIKTAR_MAX_FILE_NAME_LEN);
       
       header.st_size = md.st_size;
       header.st_mode = md.st_mode;
       header.st_uid = md.st_uid;
       header.st_gid = md.st_gid;
       header.st_atim = md.st_atim;
       header.st_mtim = md.st_mtim;

       write(oarch, &header, sizeof(viktar_header_t));   
    
       memcpy(md5_header_buf, &header, sizeof(viktar_header_t));

       //Resets context for calculating the md5 sum of the header
       //section for the current file.
       MD5Init(&context);   
       MD5Update(&context, md5_header_buf, sizeof(viktar_header_t));    
       MD5Final(footer.md5sum_header, &context);

       //Resets context for calculating the md5 sum of the data
       //section for the current file.
       MD5Init(&context);      

       //Reads data from the current file.
       while ((bytes_read = read(fd, md5_data_buf, sizeof(viktar_footer_t))) > 0)
       {
           write(oarch, md5_data_buf, bytes_read); 
           MD5Update(&context, md5_data_buf, bytes_read);
       }
             
       MD5Final(footer.md5sum_data, &context);

       write(oarch, &footer, sizeof(viktar_footer_t));
       close(fd);
 
       //./viktar -x -c -x -T -t file1 -c file2 -f archive.viktar file3
       //./viktar -cf archive.viktar file1 file2 file3
   }

   if (oarch > -1)
       close(oarch);
}

void display_short_toc(const char * archive_name)
{
   int iarch = STDIN_FILENO;
   char buf[100] = {'\0'};
   viktar_header_t header;

   if (archive_name)
       iarch = open(archive_name, O_RDONLY);

   //printf("reading archive file: %s\n", archive_name);

   if (iarch < 0)
   {
       fprintf(stderr, "failed to open input archive file %s"
                       "\nexiting...\n", archive_name);             
       exit(EXIT_FAILURE);
   }

   if (archive_name)
       fprintf(stderr, "reading archive file: \"%s\"", archive_name);
    
    else
       fprintf(stderr, "reading archive from stdin");

   read(iarch, buf, strlen(VIKTAR_TAG));


   if (strncmp(buf, VIKTAR_TAG, strlen(VIKTAR_TAG)) != 0)
   { 
       fprintf(stderr, "\nnot a viktar file: \"%s\"\n", archive_name ? archive_name : "stdin");
       exit(EXIT_FAILURE);
   } 

   printf("Contents of viktar file: \"%s\"\n", archive_name != NULL ? archive_name : "stdin"); 

   while (read(iarch, &header, sizeof(viktar_header_t)) > 0)
   {
       //Print archive member name
       memset(buf, 0, 100);
       strncpy(buf, header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN);

       printf("\tfile name: %s\n", buf);
       lseek(iarch, header.st_size + sizeof(viktar_footer_t), SEEK_CUR);
   }

   if (iarch > -1)
       close(iarch);
}

void display_long_toc(const char * archive_name)
{
   int iarch = STDIN_FILENO;

   //Variable store data from the viktar header.
   char buf[100] = {'\0'}; 
   viktar_header_t header;   
   struct passwd * pwd = NULL;
   struct group *grp = NULL;
   struct tm * tm;
   time_t mod_time;
   time_t acc_time;
   char datestring[256];   

   //Variable store data from the viktar footer.
   viktar_footer_t footer;   

   if (archive_name)
       iarch = open(archive_name, O_RDONLY);

   //printf("reading archive file: %s\n", archive_name);

   if (iarch < 0)
   {
       fprintf(stderr, "failed to open input archive file %s"
                       "\nexiting...\n", archive_name);             
       exit(EXIT_FAILURE);
   }

   if (archive_name)
       fprintf(stderr, "reading archive file: \"%s\"", archive_name);
    
    else
       fprintf(stderr, "reading archive from stdin");

   read(iarch, buf, strlen(VIKTAR_TAG));

   if (strncmp(buf, VIKTAR_TAG, strlen(VIKTAR_TAG)) != 0)
   { 
       fprintf(stderr, "\nnot a viktar file: %s\n", archive_name ? archive_name : "stdin");
       exit(EXIT_FAILURE);
   } 

   printf("Contents of viktar file: \"%s\"", archive_name != NULL ? archive_name : "stdin"); 

   while (read(iarch, &header, sizeof(viktar_header_t)) > 0)
   {
       //Prints archive member name
       memset(buf, 0, 100);
       strncpy(buf, header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN);

       printf("\n\tfile name: %s", buf);
       //printf("\n\t\tmode: %3o", mode);
 
       printf("\n\t\tmode:           ");     
 
       printf((S_ISDIR(header.st_mode)) ? "d" : "-");

       //Owner permissions
       printf((header.st_mode & S_IRUSR) ? "r" : "-");
       printf((header.st_mode & S_IWUSR) ? "w" : "-");
       printf((header.st_mode & S_IXUSR) ? "x" : "-");

       //Group permissions
       printf((header.st_mode & S_IRGRP) ? "r" : "-");
       printf((header.st_mode & S_IWGRP) ? "w" : "-");
       printf((header.st_mode & S_IXGRP) ? "x" : "-");

       //Other permissions
       printf((header.st_mode & S_IROTH) ? "r" : "-");
       printf((header.st_mode & S_IWOTH) ? "w" : "-");
       printf((header.st_mode & S_IXOTH) ? "x" : "-");

       //Prints out owner's name if it is found using getpwuid().
       if ((pwd = getpwuid(header.st_uid)) != NULL)
           printf("\n\t\tuser:           %s", pwd->pw_name);
 
       //Prints out the group name if it is found using getgrgid().
       if ((grp = getgrgid(header.st_gid)) != NULL)
           printf("\n\t\tgroup:          %s", grp->gr_name);

       printf("\n\t\tsize:           %ld", header.st_size);
     
       //Gets the time of last modification. 
       //std_mtim is of type struct time_spec. 
       //tv_sec is a data member of type time_t of struct time_spec
       mod_time = header.st_mtim.tv_sec;

       tm = localtime(&mod_time);

       //Prints out time of last modification.  
       strftime(datestring, 256, "%Y-%m-%d %H:%M:%S %Z", tm);
       printf("\n\t\tmtime:          %s", datestring);
 
       //Gets the time of last access.  
       //std_atim is of type struct time_spec. 
       //tv_sec is a data member of type time_t of struct time_spec
       acc_time = header.st_atim.tv_sec;
       tm = localtime(&acc_time);
 
       //Prints out time of last access.
       strftime(datestring, 256, "%Y-%m-%d %H:%M:%S %Z", tm);
       printf("\n\t\tatime:          %s", datestring);

       lseek(iarch, header.st_size, SEEK_CUR);

       //Reads in the data in the footer of the current file.
       read(iarch, &footer, sizeof(viktar_footer_t));
     
       printf("\n\t\tmd5 sum header: ");

       //Prints md5 sum header.
       for(int j = 0; j < MD5_DIGEST_LENGTH; ++j)
           printf("%02x", footer.md5sum_header[j]);

       printf("\n\t\tmd5 sum data:   ");

       //Prints the md5 sum daa.
       for(int j = 0; j < MD5_DIGEST_LENGTH; ++j)
           printf("%02x", footer.md5sum_data[j]);
    }
         
    printf("\n");

    if (iarch > -1)
        close(iarch);
}

void validate_archive(const char * archive_name)
{
   char buf1[BUFFER] = {'\0'}; 
   unsigned char buf2[BUFFER] = {'\0'};
   int iarch = STDIN_FILENO;
   ssize_t bytes_read = 0;
   MD5_CTX context;

   uint8_t md5sum_header[MD5_DIGEST_LENGTH];
   uint8_t md5sum_data[MD5_DIGEST_LENGTH];

   viktar_header_t header;

   //Variable store data from the viktar footer.
   viktar_footer_t footer;   

   int md5header_no_match = 0;
   int md5data_no_match = 0;

   if (archive_name)
       iarch = open(archive_name, O_RDONLY);

   if (iarch < 0)
   {
       fprintf(stderr, "failed to open input archive file %s"
                       "\nexiting...\n", archive_name);             
       exit(EXIT_FAILURE);
   }

   if (archive_name)
       fprintf(stderr, "reading archive file: \"%s\"", archive_name);
    
    else
       fprintf(stderr, "reading archive from stdin");


   read(iarch, buf1, strlen(VIKTAR_TAG));

   if (strncmp(buf1, VIKTAR_TAG, strlen(VIKTAR_TAG)) != 0)
   { 
       fprintf(stderr, "\nnot a viktar file: \"%s\"\n", archive_name ? archive_name : "stdin");
       exit(EXIT_FAILURE);
   } 

   for(int i = 1; ((bytes_read = read(iarch, &header, sizeof(viktar_header_t))) > 0); ++i) 
   { 
       //Moves the file pointer up to the start of the header for the current 
       //member file.
       //lseek(iarch, -bytes_read, SEEK_CUR);

       //bytes_read = read(iarch, buf2, sizeof(viktar_header_t));
       memcpy(buf2, &header, sizeof(viktar_header_t));
 
       //Resets context for calculating the md5 sum of the metadata
       //section for the current file.
       MD5Init(&context);      
       MD5Update(&context, buf2, sizeof(viktar_header_t));    
       MD5Final(md5sum_header, &context);

       printf("Validation for data member %d:", i);
 
       bytes_read = read(iarch, buf2, header.st_size);
 
       //Resets context for calculating the md5 sum of the metadata
       //section for the current file.
       MD5Init(&context);      
       MD5Update(&context, buf2, bytes_read);    
       MD5Final(md5sum_data, &context);

       //Reads in the data in the footer of the current file.
       read(iarch, &footer, sizeof(viktar_footer_t));
    
       if (!(md5header_no_match = memcmp(md5sum_header, footer.md5sum_header, MD5_DIGEST_LENGTH)))
           printf("\n\tHeader MD5 does match:");
  
       else
           printf("\n\t*** Header MD5 does not match:");

       printf("\n\t\tfound:   ");

       for(int j = 0; j < MD5_DIGEST_LENGTH; ++j) 
           printf("%02x", md5sum_header[j]);
       
       printf("\n\t\tin file: ");

       //Prints the md5 sum of the header section for the current file.
       for(int j = 0; j < MD5_DIGEST_LENGTH; ++j)
           printf("%02x", footer.md5sum_header[j]);
    
       //Prints the md5 sum of the header section in the footer.
       if (!(md5data_no_match = memcmp(md5sum_data, footer.md5sum_data, MD5_DIGEST_LENGTH)))
           printf("\n\tData MD5 does match:");
  
       else
           printf("\n\t*** Data MD5 does not match:");

       printf("\n\t\tfound:   ");

       //Prints the md5 sum of the data section for the current file.
       for(int j = 0; j < MD5_DIGEST_LENGTH; ++j) 
           printf("%02x", md5sum_data[j]);
       
       printf("\n\t\tin file: ");

       //Prints the md5 sum of the data section in the footer.
       for(int j = 0; j < MD5_DIGEST_LENGTH; ++j)
           printf("%02x", footer.md5sum_data[j]);

       if (!md5header_no_match && !md5data_no_match)
           printf("\n\tValidation success:        %s for member %d\n", archive_name ? archive_name : "stdin", i);

       else
           printf("\n\t*** Validation failure:        %s for member %d\n", archive_name ? archive_name : "stdin", i);
   }

   if (iarch > -1)
       close(iarch);
}

void extract_archive(const char * archive_name, char * argv[], int argc)
{
   char buf[BUFFER] = {'\0'}; 
   int iarch = STDIN_FILENO;
   viktar_header_t header;
   ssize_t bytes_read = 0;
   int found = 0;

   if (archive_name)
       iarch = open(archive_name, O_RDONLY);

   if (iarch < 0)
   {
       fprintf(stderr, "failed to open input archive file %s"
                       "\nexiting...\n", archive_name);             
       exit(EXIT_FAILURE);
   }

   if (!archive_name)
       fprintf(stderr, "reading archive from stdin");

   read(iarch, buf, strlen(VIKTAR_TAG));

   if (strncmp(buf, VIKTAR_TAG, strlen(VIKTAR_TAG)) != 0)
   {
       fprintf(stderr, "\nnot a viktar file: \"%s\"\n", archive_name ? archive_name : "stdin");
       exit(EXIT_FAILURE);
   } 

   if (optind < argc)
   {
   
       for (int i = optind; i < argc; ++i)
       {
      
           while (!found && (bytes_read = read(iarch, &header, sizeof(viktar_header_t))) > 0)
           {
               if (strcmp(header.viktar_name, argv[i]) == 0)
                   found = 1;
             
               else 
                   lseek(iarch, header.st_size + sizeof(viktar_footer_t), SEEK_CUR);
           }

           if (found)       
           {
               extract_files(iarch, &header);
               lseek(iarch, 0, SEEK_SET);
               found = 0;
           }
       }

   }

   else
       while ((bytes_read = read(iarch, &header, sizeof(viktar_header_t))) > 0)
           extract_files(iarch, &header);

   close(iarch);
      
   //umask(old_mode);
}


//void extract_files(const char * archive_name, int iarch, viktar_header_t * md, ssize_t bytes_read, int member_pos)
void extract_files(int iarch, viktar_header_t * md)
{
   unsigned char * buf = NULL;
   viktar_header_t header = *md;
   struct timespec times[2];
   int oarch = STDOUT_FILENO;
   ssize_t bytes_read = 0;
   //ssize_t total_bytes = 0;
   //char datestring[256];   
   //struct passwd * pwd = NULL;
   //struct group *grp = NULL;


   oarch = open(header.viktar_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
   fchmod(oarch, header.st_mode);  
   times[0] = header.st_atim;
   times[1] = header.st_mtim;

   bytes_read = 1;

   buf = (unsigned char *) malloc(header.st_size * sizeof(unsigned char));

   memset(buf, 0, header.st_size);

   bytes_read = read(iarch, buf, header.st_size);
   write(oarch, buf, bytes_read);

   if (buf)
   {
       free(buf);
       buf = NULL;
   }

   lseek(iarch, sizeof(viktar_footer_t), SEEK_CUR);

   //Changes the access and modification times of the file pointed by oarch.
   futimens(oarch, times);
}

