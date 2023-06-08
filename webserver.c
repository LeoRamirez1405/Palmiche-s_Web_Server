#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/sendfile.h>
#include<errno.h>
#include <sys/mman.h>
#include <time.h>

#include<stdbool.h>

// DIR dirent opendir readdir
#include <dirent.h>
// #include <sys/types.h>
//#include <sys/socket.h>

#define MAXLINE 20000
#define MAXBUF 20000
#define LISTENQ 1024
#define RIO_BUFSIZE 8192



char * home_path;

typedef struct {
    int rio_fd;/* Descriptor for this internal buf */
    int rio_cnt;/* Unread bytes in internal buf */
    char *rio_bufnub;/* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE];/* Internal buffer */
    } rio_t;

typedef struct sockaddr SA;

void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufnub = rp->rio_buf;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;  
    while (rp->rio_cnt <= 0) 
    {
        /* Refill if buf is empty */
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf,sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) 
        {
            if (errno != EINTR)/* Interrupted by sig handler return */
            return -1;
        }
        else if (rp->rio_cnt == 0)/* EOF */
            return 0;
        else
            rp->rio_bufnub = rp->rio_buf;/* Reset buffer ptr */
    }
            
    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt=n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufnub, cnt);
    rp->rio_bufnub += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;
    
    for (n = 1; n < maxlen; n++) 
    {
        rc = rio_read(rp, &c, 1);
        if (rc == 1) 
        {
            *bufp++ = c;
            if (c == '\n')
            break;
        } 
        else if (rc == 0) 
        {
            if (n == 1)
                return 0;/* EOF, no data read */
            else
                break;/* EOF, some data was read */
        } 
        else
            return -1;/* Error */
    }
    *bufp = 0;
    return n;
}

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;
    
    while (nleft > 0) {
        if ((nread = rio_read(rp, bufp, nleft)) < 0) {
            if (errno == EINTR)/* Interrupted by sig handler return */
            nread = 0;/* Call read() again */
            else
            return -1;/* errno set by read() */
            }
            else if (nread == 0)
            break;/* EOF */
            nleft -= nread;
            bufp += nread;
            }
            return (n - nleft);/* Return >= 0 */
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{ 
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;
    while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
    if (errno == EINTR) /* Interrupted by sig handler return */
    nwritten = 0; /* and call write() again */
    else
    return -1; /* errno set by write() */
    }
    nleft -= nwritten;
    bufp += nwritten;
 }
 return n;
}

int open_fdSocketServer(int port)
{
    int fdSocketServer, optval=1;
    
    struct sockaddr_in serveraddr;
    
    /* Create a socket descriptor */
    if ((fdSocketServer = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    
    /* Eliminates "Address already in use" error from bind */
    if (setsockopt(fdSocketServer, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int)) < 0)
        return -1;
    
    /* fdSocketServer will be an end point for all requests to port16on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(fdSocketServer, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    
    /* Make it a listening socket ready to accept connection requests */
    if (listen(fdSocketServer, LISTENQ) < 0)
        return -1;
    return fdSocketServer;
}

void create_html_code(char * filename, char * output)
{
    char * temp = (char *)calloc(sizeof(char), 20000);
    struct stat sbuf;
    DIR * dirp = opendir(filename);
    if (dirp == NULL)
    {
        printf("Error: Cannot open dir\n");
        exit(2);
    }

    strcpy(output, "<html><h1>🐴 Palmiche's Server </h1>"); // Encabezado
    strcat(output, "<head>El Cuartel 🛖: "); // Encabezado
    strcat(output, filename); // Ruta completa (absolute path) del directorio o archivo por el que se esta buscando
    strcat(output, "</head><body><table><tr><th>Name</th><th>Size</th><th>Date</th></tr>"); // Comenzando a construir la tabla con nombre, tamanno y fecha

    // Comenzando a agregar cada directorio y archivo
    struct dirent *direntp;

    while ((direntp = readdir(dirp)) != NULL) 
    {
        if(strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0)
            continue;
        strcpy(temp, filename);
        strcat(temp, "/");
        strcat(temp, direntp->d_name);

        if (stat(temp, &sbuf) < 0)
            continue;

        strcat(output, "<tr><td><a href=\"");
        strcat(output, filename);


        //int year,month,day,hour,minute,second;
        struct stat time;
        //char mtime[100];
        
        char*actual_filename=calloc(500,sizeof(char));
        strcpy(actual_filename,filename);
        strcat(actual_filename,"/");
        strcat(actual_filename,direntp->d_name);
        stat(actual_filename, &time);
        char* temp_time=calloc(100,sizeof(char));
        strcpy(temp_time, ctime(&time.st_mtime));

        sprintf(output, "%s/%s\">%s</a></td><td>%ld</td><td>%s</td></tr>", output, direntp->d_name, direntp->d_name, sbuf.st_size,temp_time);

        free(temp_time);
    }
    
    strcat(output, "</table></body></html>"); // Copiando el contenido del final
    
    /* Cerramos el directorio */
    closedir(dirp);
    // free(dirp);
    // free(direntp);
    free(temp);

}

// Replaces a substring in a string
// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with) 
{
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) 
    {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) 
    {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    char * temp_uri = str_replace(uri, strdup("%20"), strdup(" "));
    if(temp_uri == NULL)
    {
        temp_uri = uri;
    }
    if (!strstr(temp_uri, "cgi-bin")) 
    {   
        /* Static content */
        strcpy(cgiargs, "");
        strcpy(filename, temp_uri); 
        if (uri[strlen(temp_uri)-1] == '/')
        {
            if(strlen(temp_uri)<=1)
            {
                strcpy(filename, home_path);
            }
        }
        free(temp_uri);
        return 1;
    }
    else 
    {
        /* Dynamic content */
        ptr = index(temp_uri, '?');
        if (ptr) 
        {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
        {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, temp_uri);
        free(temp_uri);
        return 0;
    }
}

/* get_filetype - derive file type from file name*/
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".zip"))
    strcpy(filetype, "application/zip");  
    else if (strstr(filename, ".pdf"))
    strcpy(filetype, "application/pdf");     
    
    else
    strcpy(filetype, "text/plain");
}

void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg)
{  
    char *buf=(char *)calloc(sizeof(char), MAXLINE);//[MAXLINE], 
    char* body=(char *)calloc(sizeof(char), MAXBUF);//[MAXLINE], 
 /* Build the HTTP response body */
 sprintf(body, "<html><title>Palmiche's WebServer Error :( </title>");
 sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
 sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
 sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
 sprintf(body, "%s<hr><em>Palmiche's WebServer</em>\r\n", body);

 /* Print the HTTP response */
 sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
 rio_writen(fd, buf, strlen(buf));
sprintf(buf, "Content-type: text/html; charset=utf-8\r\n");
rio_writen(fd, buf, strlen(buf));
 sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
    free(buf);
    free(body);
}

void serve_static(int fd, char *filename, int filesize, bool is_directory, struct stat sbuf)
{
    int srcfd;
    char * filetype=(char *)calloc(sizeof(char), 1000);//[MAXLINE], 
    char * buf=(char *)calloc(sizeof(char), 20000);//[MAXBUF];
    
    if(is_directory){ // If is a directory (not a file)
         
            
        char * output = (char *)calloc(sizeof(char), 200000);//[20000]; 
        create_html_code(filename, output);
        filesize = strlen(output);

        sprintf(buf, "HTTP/1.0 \r\n");
        sprintf(buf, "%sServer: Palmiche's WebServer\r\n", buf);
        sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
        sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, "text/html; charset=utf-8");
        
        rio_writen(fd, buf, strlen(buf));
        rio_writen(fd, output, filesize);
        free(output);
    }


    /* Send response headers to client */
    
    
    /* Send response body to client */
    else
    { // If is a file

        get_filetype(filename, filetype);
        sprintf(buf, "HTTP/1.0  \r\n");
        sprintf(buf, "%sServer: Palmiche's WebServer\r\n", buf);
        sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
        sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
        rio_writen(fd, buf, strlen(buf));

        srcfd = open(filename, O_RDONLY, 0);
        if(srcfd < 0)
            clienterror(fd, filename, "404", "Not found","Couldn't find this file");
        else
            sendfile(fd, srcfd, NULL, sbuf.st_blksize);

        close(srcfd);
    }
    free(filetype);
    free(buf);
}
    
void read_requestLines(rio_t *rp)
{  
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) 
    {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };
    pid_t pid;
    char*  var1 = "MAXLINE = 20000";
    char*  var2 = "MAXBUF=20000";
    char*  var3 = "LISTENQ=1024";
    char*  var4 = "RIO_BUFSIZE=8192";
    char *const environ[] = {var1,var2,var3,var4,NULL};

     /* Send response headers to client */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Palmiche's WebServer\r\n");
    rio_writen(fd, buf, strlen(buf));
     /* Fork a child process to run the CGI script */
    if ((pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }
    else if (pid == 0) { /* Child process */
        /* Set environment variables for the CGI script */
        setenv("QUERY_STRING", cgiargs, 1);
        dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
        execve(filename, emptylist, environ); /* Run CGI script */
        exit(0);
    }
    else { /* Parent process */
        waitpid(pid, NULL, 0); /* Wait for child to exit */
    }
}  
void requestHandler(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    
    /* Read request line and headers */
    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) 
    {
        clienterror(fd, method, "501", "Not Implemented","Palmiche's WebServer does not implement this method");
        return;
    }
        read_requestLines(&rio);
        
        /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) 
    {
        clienterror(fd, filename, "404", "Not found","Palmiche's WebServer couldn't find this file");
        return;
    }
            
    if (is_static) 
    {
        /* Serve static content */
        bool is_directory =  (S_ISDIR(sbuf.st_mode));
        serve_static(fd, filename, sbuf.st_size, is_directory, sbuf);
    }
    else 
    {
        /* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden", "Palmiche's WebServer couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd,filename,cgiargs);
        return;
    }
}

 int main(int argc, char **argv)
 {
    int fdSocketServer, fdSocketClient, port, clientlen;
    struct sockaddr_in clientaddr;

    home_path = strdup("/home");

    /* Check command line args */
    if (argc <= 1) {
        fprintf(stderr, "usage: %s <port> <folder>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    if(argc == 3){
        home_path = argv[2];
    }

    fdSocketServer = open_fdSocketServer(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        fdSocketClient = accept(fdSocketServer, (SA *)&clientaddr, &clientlen);
        requestHandler(fdSocketClient);
        close(fdSocketClient);
    }
}
