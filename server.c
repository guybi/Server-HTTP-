// GUY BITON 305574709

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include "threadpool.h"

// according to assume the max request is 4000

#define SIZE_REQUEST 4000
#define SIZE_MAX_PATH 4000
#define ERROR -1
#define SUCCESS 0
#define TRUE 0
#define FALSE -1
#define NOT_FOUND -2
#define NOT_PERRMISSION -3
#define ERROR_AFTER_PRINT -6

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

int checkPPL(char** argv,int *port,int *pool_size,int *max_number_of_request);
void printInput(char** argv,int argc);
void printUsage();
int check_strtol(char* string);
int check_valid(int ,int ,int);
void clientHandler(void* sockfd);
int send_respone_to_client(int fd_socket,int num_error,char* path);
int print_list_files(int fd,char* path,char* target);
int find_index_html_with_permission(char* path);
int check_if_existence(char* path);// check if file / dir existing
void print_place(int x);
int write_to_socket(int fd_socket,char* target,int size_all);
int open_file(int fd_sock,char* full_path,char* path);
char* get_mime_type(char *name);
int open_index_html(int fd_sock,char* path);
int num_of_chars(int x);
int size_body_files(char* path);
int check_if_permission(char* full_path);
int find_slash_from_p(char* path,int p);
int check_ok_path(char* path);

int num_request = 0;
////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
  struct sockaddr_in srv;	/* used by bind() */
  int fd_socket;
//int new_fd_sock;

  if(argc != 4)
  {
    printUsage();
    return ERROR;
  }

  int port,pool_size,max_number_of_request;
  if (checkPPL(argv,&port,&pool_size,&max_number_of_request) == ERROR)
  {
    printUsage();
    return ERROR;
  }
  if(check_valid(port,pool_size,max_number_of_request) == ERROR)
  {
    printUsage();
    return ERROR;
  }

  // the web server must create a socket of type SOCK_STREAM
  if((fd_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) // 0 _ tcp
  {
	   perror("socket\n");  // for system call perror
	   return ERROR;
  }
  // fd 0 1 2 saved

  bzero((char *) &srv, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_addr.s_addr = htonl(INADDR_ANY); //a client may connect to any of my addresses
  srv.sin_port = htons(port);


  if( bind(fd_socket,(struct sockaddr *) &srv ,sizeof(srv))<0){	//bind the socket to the input <port>
		perror("binding");
	  return ERROR;
	}

  if(listen(fd_socket,5)<0)  // the queue of request for main socket
  {
    perror("listen");
    return ERROR;
  }

  threadpool* threadPool;
  if( (threadPool = create_threadpool(pool_size)) == NULL)//create threadPool for the clients handling
  {
    printf("ERROR TO CREATE THREADPOOL\n");
		return ERROR;
  }

  int client_index;
  int* p = &num_request;
  *p = max_number_of_request;

  struct sigaction sa;
          memset(&sa, 0, sizeof(sa));
          sa.sa_handler = SIG_IGN;
          sa.sa_flags = 0;
          if (sigaction(SIGPIPE, &sa, 0) == -1)
          {
                  perror("sigaction");
                  exit(1);
          }
  for (client_index = 0; client_index < max_number_of_request ;client_index++)
  {
        int* new_fd_sock = (int*)malloc(sizeof(int));
        if(!new_fd_sock)
        {
          perror("malloc");
          return ERROR;
        }
        bzero(new_fd_sock,sizeof(int));
       *new_fd_sock = accept(fd_socket, NULL, NULL);	//accept incoming request for connection
     		if(new_fd_sock < 0)
        {
     			perror("accept");
     			close(fd_socket);
     			destroy_threadpool(threadPool);
          free(new_fd_sock);
	      	return ERROR;
     		}
      	dispatch(threadPool,(dispatch_fn)clientHandler,(void*)new_fd_sock);
  	}
  close(fd_socket);	//close the socket
	destroy_threadpool(threadPool);	//free the threadpool
	return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
void clientHandler(void* fd_so){

  int * temp_s = (int*)fd_so;
  int fd_socket = *temp_s;
  free(temp_s);

  num_request--;

  if(num_request == 0)
  {
    return;
  }

  char* path;
  char* protocol;
  char request[SIZE_REQUEST];
  bzero(request,SIZE_REQUEST);
  int tmpsize = 0;
  int size=0;
do{
    if((tmpsize=read(fd_socket,request+size,SIZE_REQUEST-size)) < 0 )
    {
      perror("read");
      close(fd_socket);
      return;
    }
    size+=tmpsize;
  if( strstr(request,"\r\n") != NULL || size == 0 ){	//if we read \r\n its the end of the request
    break;
  }
}while(1);

  char* test = strstr(request,"\r\n");
  if(test == NULL || test == request)
  {
    if(send_respone_to_client(fd_socket,400,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }
  char temp[test-request +1];
  bzero(temp,test-request +1);
  memcpy(temp, request, test-request);
  temp[test-request]='\0';

  char* method;
  method = strtok(temp," ");
  path = strtok(NULL," ");
  protocol = strtok(NULL," ");
  if(method == NULL || path == NULL || protocol == NULL)
  {
    if(send_respone_to_client(fd_socket,400,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }
  if((strcmp(protocol,"HTTP/1.0")!=0) && (strcmp(protocol,"HTTP/1.1")!=0)) // check the http version
  {
    if(send_respone_to_client(fd_socket,400,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }

  if(strcmp(method,"GET")  != 0)
  {
    if(send_respone_to_client(fd_socket,400,NULL) == ERROR)
    {
      printf("ERROR IN SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }

  // take the currnet dir //
  char server_dir[SIZE_MAX_PATH];
  bzero(server_dir,SIZE_MAX_PATH);

  if(getcwd(server_dir, SIZE_MAX_PATH) == NULL)
  {
    perror("getcwd");
    if(send_respone_to_client(fd_socket,500,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }
  // enter \0 in end of server path
  int size_cur = strlen(server_dir);
  server_dir[size_cur]='\0';

  // full_path is beffer for server_dir + path

  char full_path[SIZE_MAX_PATH];
  bzero(full_path,SIZE_MAX_PATH);

  // check if input is / or " "
  // if yes full
  if(strcmp(path," ") == 0 || strcmp(path,"/") == 0)
  {
    strcat(full_path,server_dir);
    strcat(full_path,"/");
  }
  else
  {
    strcat(full_path,server_dir);
    strcat(full_path,path);
  }
  int check_multy_slash = check_ok_path(full_path);
  if(check_multy_slash == ERROR)
  {
    if(send_respone_to_client(fd_socket,404,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }
  // check if existence method use if
  // access and flag F_OK
  // its mean -- need to check permmision bebore here

  int res_check_if_exist = check_if_existence(full_path);
  // -1 its ERROR 404

  if(res_check_if_exist == ERROR )
  {
    if(send_respone_to_client(fd_socket,404,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }

  struct stat statbuf;
  if(stat(full_path, &statbuf) == ERROR) // get information about file
  {
    if(send_respone_to_client(fd_socket,404,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }

  int res_permission = check_if_permission(full_path);
  if(res_permission == ERROR)
  {
    if(send_respone_to_client(fd_socket,403,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }

  if(res_permission == NOT_FOUND)
  {
    if(send_respone_to_client(fd_socket,404,NULL) == ERROR)
    {
      printf("ERROR TO SEND RESPONE TO CLIENT\n");
    }
    close(fd_socket);
    return;
  }

  // method check dir
  if(S_ISDIR(statbuf.st_mode))
  {
      int last_char = strlen(path);
      if(path[last_char-1] != '/') // its mean -> Directory and not finish in /
      {
        if ( send_respone_to_client(fd_socket,302,path) == ERROR)
        {
          printf("ERROR IN SEND RESPONE TO CLIENT\n");
        }
        close(fd_socket);
        return;
      }
        int res_check_html = find_index_html_with_permission(full_path);
        if(res_check_html == ERROR)
        {
          if(send_respone_to_client(fd_socket,403,NULL) == ERROR)
          {
            printf("ERROR TO SEND RESPONE TO CLIENT\n");
          }
          close(fd_socket);
          return;
        }
        if(res_check_html == NOT_FOUND)
        {

        }
        else if(res_check_html  == SUCCESS)    // its exist!!!
         {
                char path_with_index[SIZE_MAX_PATH];
                bzero(path_with_index,SIZE_MAX_PATH);
                strcat(path_with_index,full_path);
                strcat(path_with_index,"index.html");
                int res_open_html = open_index_html(fd_socket,path_with_index);
                if(res_open_html == ERROR)
                {
                  send_respone_to_client(fd_socket,500,NULL);
                }
              close(fd_socket);
              return;
          }
            if(print_list_files(fd_socket,path,full_path) == ERROR )
             {
               send_respone_to_client(fd_socket,500,NULL);
             }
           close(fd_socket);
           return;
    }
    if(S_ISREG(statbuf.st_mode)) // method check file
    {
        int res_open_file =  open_file(fd_socket, full_path ,path);
        if(res_open_file == ERROR_AFTER_PRINT) // exit after read write - not need retuen any ERROR
        {
          // ERROR AFTER READ OR WRITE - not need to show error
          close(fd_socket);
          return ;
        }
        if(res_open_file == ERROR)
        {
          close(fd_socket);
          return ;
        }
        close(fd_socket);
        return;
      }
       else
          {
               if(send_respone_to_client(fd_socket,403,NULL) == ERROR)
               {
                  printf("ERROR TO SEND RESPONE TO CLIENT\n");
                }
                  close(fd_socket);
                  return;
            }
close(fd_socket);
return;
}
////////////////////////////////////////////////////////////////////////////////
int find_index_html_with_permission(char* path)
{
  int res = check_if_permission(path);
  if(res == ERROR)
  {
    return NOT_PERRMISSION;
  }

  char temp_path[SIZE_MAX_PATH];
  bzero(temp_path,SIZE_MAX_PATH);
  strcat(temp_path,path);

  DIR        *d;
  struct dirent *dir;

  d = opendir(temp_path);
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      if(strcmp(dir->d_name,"index.html")==0)
      {
        strcat(temp_path,"index.html");
        // found HTML - NEED TO CHECK PERRMISSION HTML
        res = check_if_permission(temp_path);
        if(res == ERROR) // PERMISSION
        {
          closedir(d);
          return ERROR;
        }
        else if(res == SUCCESS) // OKAY
        {
          closedir(d);
          return SUCCESS;
        }
        else if(res == NOT_FOUND) // ERROR IN PATH
        {
          closedir(d);
          return NOT_FOUND;
        }
      }
    }
  }
  closedir(d);
  return NOT_FOUND;
}

////////////////////////////////////////////////////////////////////////////////
int print_list_files(int fd,char* path,char* target)
{
  int size_body = size_body_files(target);
	  if(size_body == ERROR)
	  {
	    return ERROR;
	  }

	  time_t now;
	  char time_buffer[128];
	  now=time(NULL);
	  strftime(time_buffer,sizeof(time_buffer),RFC1123FMT, gmtime(&now));

	  DIR        *dip;
	  struct dirent *dit;

	  char currentPath[FILENAME_MAX];


    int size_plus_header = 0;

    size_plus_header+=strlen("<HTML>\n\r");
    size_plus_header+=strlen("<HEAD><TITLE>Index of");
    size_plus_header+=strlen(target);
    size_plus_header+=strlen("</TITLE></HEAD>\r\n\r\n");
    size_plus_header+=strlen("<BODY>\r\n");
    size_plus_header+=strlen("<H4>Index of ");
    size_plus_header+=strlen(path);
    size_plus_header+=strlen("</H4>\r\n\r\n");
    size_plus_header+=strlen("<table CELLSPACING=8>\r\n");
    size_plus_header+=strlen("<tr><th> Name");
    size_plus_header+=strlen("</th><th>");
    size_plus_header+=strlen(" Last Modified ");
    size_plus_header+=strlen("</th><th>");
    size_plus_header+=strlen(" Size");
    size_plus_header+=strlen("</th></tr>\r\n\r\n");
    size_plus_header+=strlen("</table>\r\n\r\n");
    size_plus_header+=strlen("<HR>\r\n\r\n");
    size_plus_header+=strlen("<address> Webserver /1.0 <address/>");
    size_plus_header+=strlen("</BODY></HTML>\r\n\r\n");


    char* body = (char*)malloc(sizeof(char)*(size_body+size_plus_header));
    if(body == NULL)
    {
        perror("malloc");
        send_respone_to_client(fd,500,NULL);
        return ERROR_AFTER_PRINT;
    }

	  bzero(body,sizeof(char)*size_body);
	  strcat(body,"<HTML>\n\r");
	  strcat(body,"<HEAD><TITLE>Index of");
	  strcat(body,target);
	  strcat(body,"</TITLE></HEAD>\r\n\r\n");
	  strcat(body,"<BODY>\r\n");
	  strcat(body,"<H4>Index of ");
	  strcat(body,path);
	  strcat(body,"</H4>\r\n\r\n");
	  strcat(body,"<table CELLSPACING=8>\r\n");
	  strcat(body,"<tr><th> Name");
	  strcat(body,"</th><th>");
	  strcat(body," Last Modified ");
	  strcat(body,"</th><th>");
	  strcat(body," Size");
	  strcat(body,"</th></tr>\r\n\r\n");


    	  char location[SIZE_MAX_PATH];
    	  bzero(location,SIZE_MAX_PATH);
    	  strcat(location,".");
    	  strcat(location,path);

	      if((dip = opendir(location)) == NULL)
	      {
      		perror("opendir");
      		free(body);
      		return ERROR;
	      }

        struct stat statbuf;
        if(stat(target, &statbuf) == ERROR)
        {
          send_respone_to_client(fd,404,NULL);
          perror("stat");
          free(body);
          return ERROR;
        }
	      if((getcwd(currentPath, FILENAME_MAX)) == NULL)
	      {
         perror("getcwd");
         if(send_respone_to_client(fd,500,NULL) == ERROR)
         {
           printf("ERROR TO SEND RESPONE TO CLIENT\n");
         }
	       free(body);
	       return ERROR_AFTER_PRINT;
	      }
	       /*Read all items in directory*/
	       while((dit = readdir(dip)) != NULL)
	       {
               char temp_path[SIZE_MAX_PATH];
               bzero(temp_path,SIZE_MAX_PATH);
               strcat(temp_path,target);
               strcat(temp_path,dit->d_name);
               struct stat statbuf;
               if(stat(temp_path, &statbuf) == ERROR)
               {
                 send_respone_to_client(fd,404,NULL);
                 perror("stat");
                 free(body);
                 return ERROR;
               }
              int size = (int)statbuf.st_size;

      		  strcat(body,"<tr>\r\n");
      		  strcat(body,"<td>");
      		  strcat(body,"<A HREF=");
      		  strcat(body,dit->d_name);
      		//  if(dit->d_type == 4)
      		//    strcat(body,"/");
      		  strcat(body,">");
      		  strcat(body,dit->d_name);
      		  strcat(body,"<A>");
      		  strcat(body,"</td>");
      		  ////////////////////////////////////////////
      		  char buffer_date[4096];
      		  bzero(buffer_date,4096);
      		  strcat(buffer_date,dit->d_name);
      		  stat(buffer_date, &statbuf);
      		  strcat(body,"<td>");
      		  strcat(body,ctime(&statbuf.st_mtime));
      		  strcat(body,"</A>");
      		  strcat(body,"</td>");
      		  strcat(body,"<td>");
      	    size = (int)statbuf.st_size;

      		  if(size != 4096)
            {
      		    sprintf(body,"%s%d",body,size);
            }
      		  else
            {
      		    sprintf(body,"%s%s",body,"");
            }

      		  strcat(body,"</A>");
      		  strcat(body,"</td>");
      		  strcat(body,"\r\n");
      		  strcat(body,"</td>\r\n");
      		  strcat(body,"</tr>\r\n");

      		}
		closedir(dip);
	  strcat(body,"</table>\r\n\r\n");
	  strcat(body,"<HR>\r\n\r\n");
	  strcat(body,"<address> Webserver /1.0 <address/>");
	  strcat(body,"</BODY></HTML>\r\n\r\n");

	  int num_of_digits = 0;
	  int num = 0;
	  int size = 0;
	  for(num = strlen(body) ; num>0 ; num_of_digits++){ num=num/10;}
	  if(num_of_digits == 0){num_of_digits = 1;}

	  size = strlen("HTTP/1.0 200 OK\r\n") +strlen("\r\nServer: webserver/1.0\r\nDate: ")+strlen(time_buffer)+strlen("\r\nContent-Type: text/html\r\nContent-length: ")+num_of_digits+strlen("\r\nConncetion: Close\r\n\r\n")+strlen(body)+1;

	  char message[size+1];
	  bzero(message,size+1);
	  sprintf(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate:  %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConncetion: Close \r\n\r\n%s",time_buffer,(int)strlen(body),body);
    message[size+1]='\0';

	    if( write_to_socket(fd, message,size+1) == ERROR)
	    {
	      free(body);
	      return ERROR_AFTER_PRINT;
	    }
	    free(body);
	  return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int write_to_socket(int fd_socket,char* target,int size_all)
{
  int size = 0;
  int temp_size;
  while(size+2 < size_all)
  {
    if((temp_size = write(fd_socket,target+size,size_all-size)) < 0 )
    {
      perror("write");
      return ERROR;
    }
    size+=temp_size;
  }
  return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int send_respone_to_client(int fd_socket,int num_error,char* path)
{
  char* body;
  char* temp_message;
  time_t now;
  char time_buffer[128];
  bzero(time_buffer,0);

  char* headArray[]=
  {"HTTP/1.0 302 Found","HTTP/1.0 400 Bad Request","HTTP/1.0 403 Forbidden"
  ,"HTTP/1.0 404 Not Found","HTTP/1.0 500 Internal Server Error","HTTP/1.0 501 Not supported"};

  //the body for the Error message

  char* bodyArray[]=
  {"<HTML><HEAD><TITLE>302 Found</TITLE></HEAD><BODY><H4>302 Found</H4>Directories must end with a slash.</BODY></HTML>\r\n\r\n,"
   ,"<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD><BODY><H4>400 Bad request</H4>Bad Request.</BODY></HTML>\r\n\r\n"
   ,"<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD><BODY><H4>403 Forbidden</H4>Access denied.</BODY></HTML>\r\n\r\n"
   ,"<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD><BODY><H4>404 Not Found</H4>File not found.</BODY></HTML>\r\n\r\n"
   ,"<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD><BODY><H4>500 Internal Server Error</H4>Some server side error.</BODY></HTML>\r\n\r\n"
   ,"<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD><BODY><H4>501 Not supported</H4>Method is not supported.</BODY></HTML>\r\n\r\n"};
   int flag =  FALSE;
   switch (num_error)
    {
     case 302:
         temp_message=headArray[0];
         flag = TRUE;
         body=bodyArray[0];
         break;
    case 400:
        temp_message=headArray[1];
        body=bodyArray[1];
        break;
    case 403:
        temp_message=headArray[2];
        body=bodyArray[2];
        break;
    case 404:
        temp_message=headArray[3];
        body=bodyArray[3];
        break;
    case 500:
          temp_message=headArray[4];
          body=bodyArray[4];
          break;
    case 501:
          temp_message=headArray[5];
          body=bodyArray[5];
          break;
        }

   //GET TIME
   now = time(NULL);
   strftime(time_buffer,sizeof(time_buffer),RFC1123FMT, gmtime(&now));

   //GET NUM OF DIGITS
   int num_of_digits = 0;
   int num = 0;
   int size = 0;
   for(num = strlen(body) ; num>0 ; num_of_digits++)
   {
      num=num/10;
   }
   if(num_of_digits == 0)
   {
     num_of_digits = 1;
   }

   if(flag == TRUE) // its mean we enter to 302 - need to add location to header
   {
     size =strlen(temp_message)+strlen("\r\nServer: webserver/1.0\r\nDate: ")+strlen(time_buffer)+strlen("\r\nLocation: \r\n")+strlen(path)+strlen("\r\nContent-Type: text/html\r\nContent-length: ")+num_of_digits+strlen("\r\nConncetion: Close\r\n\r\n")+strlen(body)+2;
     char message[size];
     bzero(message,size);
     sprintf(message,"%s\r\nServer: webserver/1.0\r\nDate:  %s\r\nLocation: %s/\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConncetion: Close \r\n\r\n%s",temp_message,time_buffer,path,(int)strlen(body)+4,body);

//     printf("this is respond:\n%s\n",message);
     if(write_to_socket(fd_socket, message,size) == ERROR)
     {
       return ERROR;
     }
      return SUCCESS;
   }
   else{
     size =strlen(temp_message)+strlen("\r\nServer: webserver/1.0\r\nDate: ")+strlen(time_buffer)+strlen("\r\nContent-Type: text/html\r\nContent-length: ")+num_of_digits+strlen("\r\nConncetion: Close\r\n\r\n")+strlen(body)+1;
     char message[size];
     bzero(message,size);
     sprintf(message,"%s\r\nServer: webserver/1.0\r\nDate:  %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConncetion: Close \r\n\r\n%s",temp_message,time_buffer,(int)strlen(body)+4,body);

//     printf("this is respond:\n%s\n",message);
     if(write_to_socket(fd_socket, message,size) == ERROR)
     {
       return ERROR;
     }
      return SUCCESS;
   }

}

////////////////////////////////////////////////////////////////////////////////
void printInput(char** argv,int argc)
{
  int i = 0;
  while( i < argc)
  {
    printf("%s ",argv[i]);
    i++;
  }
  printf("\n");
}
////////////////////////////////////////////////////////////////////////////////
void printUsage()
{
  printf("Usage: server <port> <pool-size>\n");
  return;
}
////////////////////////////////////////////////////////////////////////////////
int checkPPL(char** argv,int *port,int *pool_size,int *max_number_of_request)
{
  char* temp;
  *port = strtol(argv[1],&temp,0);
  if( check_strtol(temp) == ERROR){ return ERROR ;}
  *pool_size = strtol(argv[2],&temp,0);
  if( check_strtol(temp) == ERROR){ return ERROR ;}
  *max_number_of_request = strtol(argv[3],&temp,0);
  if( check_strtol(temp) == ERROR){ return ERROR ;}
  return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int check_strtol(char* string)
{
  if (*string != '\0'){return ERROR;}
  return SUCCESS;
}
int check_valid(int port,int pool_size,int max)
{
  if(port < 0 || pool_size < 0 || max < 0)
  {
    return ERROR;
  }
  return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int check_if_existence(char* path)
{
  struct stat sb;
  if (stat(path, &sb) == ERROR)
  {
      perror("stat ");
      return ERROR;
  }
  return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int open_file(int fd_sock,char* full_path,char* path)
{

  //sleep(2);
  time_t now;
  char time_buffer[128];

  now=time(NULL);
  strftime(time_buffer,sizeof(time_buffer),RFC1123FMT, gmtime(&now));
  struct stat statbuf;

  if(stat(full_path, &statbuf) == ERROR)
  {
    send_respone_to_client(fd_sock, 404 ,NULL);
    return ERROR_AFTER_PRINT;
  }

  char* last_mod = ctime(&statbuf.st_mtime);
  int  file_size = statbuf.st_size;
  char* file_type = get_mime_type(path);
    int length_type;
    int length_modified;
    int content_length;
  if(file_type!=NULL)
     length_type = strlen(file_type);
  else
    length_type = 0;
    if(last_mod != NULL)
      length_modified = strlen(last_mod);
    else
      length_modified=0;

  content_length = num_of_chars(file_size);

  int size = 0;
  size =  strlen("HTTP/1.0 200 OK\r\n");
  size+= strlen("\r\nServer: webserver/1.0\r\nDate: ");
  size+= length_modified;
  size+=strlen("\r\nContent-Type: ");
  size+=length_type;
  size+=strlen("\r\nContent-length: ");
  size+=content_length;
  size+=strlen("\r\nLast-Modified: ");
  size+=length_modified;
  size+=strlen("\r\nConncetion: Close\r\n\r\n");

  char message[size];
  bzero(message,size);
  sprintf(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate:  %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nLast-midified: %sConncetion: Close \r\n\r\n",time_buffer,file_type,file_size,last_mod);
  size=strlen(message);

  if(write_to_socket(fd_sock,message,size)==ERROR)
  {
    return ERROR_AFTER_PRINT;
  }

  int fp = open(full_path,O_RDONLY);
  if(fp == ERROR)
  {
    send_respone_to_client(fd_sock, 500 ,NULL);
    perror("open");
    return ERROR_AFTER_PRINT;
  }

  int sum=0;
  unsigned char buffer[5096];
  memset(buffer, 0, 5096);
  int readBytes;
  int writeB;
  while((readBytes = read(fp, buffer, 5096 )) > 0)
  {
      if(readBytes <0 )
      {
        perror("read");
        close(fp);
        return ERROR_AFTER_PRINT;
      }
      if((writeB = write(fd_sock, buffer, readBytes)) < 0 )
      {
          close(fp);
          return ERROR_AFTER_PRINT;
      }
        memset(buffer, 0, 5096);
        sum+=writeB;
  }
  close(fp);
  return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int open_index_html(int fd_sock,char* path)
{
  //GET TIME
  time_t now;
  char time_buffer[128];

  now=time(NULL);
  strftime(time_buffer,sizeof(time_buffer),RFC1123FMT, gmtime(&now));

  struct stat statbuf;
  stat(path, &statbuf);
  int size_index = (int)statbuf.st_size;

  int content_length_digits = num_of_chars(size_index);

  if(content_length_digits == ERROR)
  {
    return ERROR;
  }

  int size = 0;
  size =  strlen("HTTP/1.0 200 OK\r\n");
  size+= strlen("Server: webserver/1.0\r\nDate: ");
  size+= strlen(time_buffer);
  size+=strlen("\r\nContent-Type: text/html\r\nContent-length: ");
  size+=content_length_digits;
  size+=strlen("\r\nConncetion: Close\r\n\r\n");
  char message[size+1];
  bzero(message,size+1);


  strcat(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate:  ");
  strcat(message,time_buffer);
  strcat(message,"\r\nContent-Type: text/html\r\nContent-Length: ");
  sprintf(message,"%s%d",message,size_index);
  strcat(message,"\r\nConncetion: Close \r\n\r\n");

  if(write(fd_sock,message,size) == ERROR)
  {
    perror("write\n");
    return ERROR;
  }
  int fp = open(path,O_RDONLY);
  if(fp == ERROR)
  {
    perror("open");
    return ERROR;
  }
  char buffer[225];
  bzero(buffer,225);
  int readBytes;
  while((readBytes = read(fp, buffer, 225 )) > 0)
  {
      if(readBytes <0 )
      {
        close(fp);
        return ERROR;
      }
      int writeB;
      if((writeB = write(fd_sock, buffer, readBytes)) < 0 )
      {
          perror("write");
          close(fp);
          return ERROR;
      }
  }
  close(fp);
  return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
char *get_mime_type(char *name)
{
      char *ext = strrchr(name, '.');
      if (!ext) return NULL;
      if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
      if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
      if (strcmp(ext, ".gif") == 0) return "image/gif";
      if (strcmp(ext, ".png") == 0) return "image/png";
      if (strcmp(ext, ".css") == 0) return "text/css";
      if (strcmp(ext, ".au") == 0) return "audio/basic";
      if (strcmp(ext, ".wav") == 0) return "audio/wav";
      if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
      if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
      if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
      if (strcmp(ext, ".c") == 0) return "text/html";
      if (strcmp(ext, ".divx") == 0)return "video/mpeg";
      if (strcmp(ext, ".mkv") == 0)return "video/mpeg";
      if (strcmp(ext, ".mp4") == 0) return "video/mp4";
      if (strcmp(ext, ".flv") == 0) return "video/mpeg";
      if (strcmp(ext, ".txt") == 0) return "text/css";
      if (strcmp(ext, ".ico") == 0) return "text/css";

return NULL;
}
////////////////////////////////////////////////////////////////////////////////
int num_of_chars(int x)
{
  //GET NUM OF DIGITS
  int num_of_digits = 0;
  int num = x;
  for(num = x ; num > 0 ; num_of_digits++)
  {
     num=num/10;
  }
  if(num_of_digits == 0)
  {
    num_of_digits = 1;
  }
  return num_of_digits;
}
///////////////////////////////////////////////////////////////////////////////////////////////
int size_body_files(char* path)
{
  struct stat statbuf;
  if(stat(path, &statbuf) == ERROR)
   {
    perror("stat");
    return ERROR;
   }
  DIR        *dip;
  struct dirent *dit;

  int size_body=0;
  dip = opendir(path);
  if(dip == NULL)
  {
    closedir(dip);
    return ERROR;
  }
  while((dit = readdir(dip)) != NULL)
  {
    size_body++;
  }
  closedir(dip);
  return size_body*500+1;
}
/////////////////////////////////////////////////////////////////////////////////////
int check_if_permission(char* path)
{  struct stat sb;

  int num_slash = 0;
  int p = find_slash_from_p(path,0);
  int i = 0;
  // sum /
  while(i < strlen(path))
  {
    if(path[i]=='/')
    num_slash++;
    i++;
  }
  i=0;
  // need to stop in last slash beacuse the target path need to be with r permission
  while(i<num_slash-1)
  {
    char temp[p+1];
    bzero(temp,p+1);
    strncpy(temp,path,p+1);
    temp[p+1]='\0';

      if (stat(temp, &sb) == -1) {
          perror("stat");
          return -2;
      }
       int x = (unsigned long) sb.st_mode; // get permission from pat
       unsigned char a=1;
       unsigned char res= a & x;
       if(res != 1)
       {
         return ERROR;
       }
    i++;
    p=find_slash_from_p(path,p+1);
    }
    if (stat(path, &sb) == ERROR)
    {
      perror("stat");
      return NOT_FOUND;
    }
      int x = (unsigned long) sb.st_mode; // get permission from path
      unsigned char a=4;
      unsigned char res= a & x;
      if(res != 4)
      {
        return ERROR;
      }
   return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
int find_slash_from_p(char* path,int p)
{
  if(p > strlen(path))
  {
    return ERROR;
  }
  int loc;
  for(loc = p; loc < strlen(path);loc++)
  {
    if(path[loc]=='/')
    {
      break;
    }
  }
  return loc;
}
//////////////////////////////////////////////////////////////////////////////////////
int check_ok_path(char* path)
{
  int size = strlen(path);
  if(size > 1)
  {
      int i = 1;
      while(i < size-1)
      {
        if(path[i] == '/')
        {
          if(path[i-1]=='/' || path[i+1] == '/')
          {
            return ERROR;
          }
        }
        i++;
      }
  }
  return 0;
}
