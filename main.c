/**
 * @file main.c
 * @author iesimple (https://github.com/iesimple)
 * @brief
 * @version 0.1
 * @date 2022-06-15
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <termio.h>
#include <regex.h>
#include "base64.h"

#define MAX_LEN_BUF 1024
#define MAX_LEN_CMD 30

typedef enum
{
    false,
    true
} bool;

// 接收服务器返回的缓冲区
char rcvbuf[MAX_LEN_BUF];
size_t n;

// 因为某些命令的返回过长（比如RETR LIST），可能会超过MAX_LEN_BUF
// 这里用flag标志这些命令，在client读取的时候特殊处理，具体可以见client_run函数
bool flag = false;

bool cmdParse(const char *line);
int getch(void);
int connectTopop(const char *name);
void get_sendbuf(char *sendbuf);
void client_run(int sockfd);
bool cmdParse(const char *cmd);

/**
 * @brief linux下实现的类似windows下的getch，读取字符不回显，而且不等待'\n'
 *
 * @return int 读取到的字符
 */
int getch(void)
{
    struct termios tm, tm_old;
    int fd = 0, ch;
    if (tcgetattr(fd, &tm) < 0) //保存现在的终端设置
        return -1;
    tm_old = tm;
    cfmakeraw(&tm);                      //更改终端设置为原始模式，该模式下所有的输入数据以字节为单位被处理
    if (tcsetattr(fd, TCSANOW, &tm) < 0) //设置上更改之后的设置
        return -1;
    ch = getchar();
    if (tcsetattr(fd, TCSANOW, &tm_old) < 0) //更改设置为最初的样子
        return -1;
    return ch;
}

/**
 * @brief 使用tcp连接指定的域名服务器
 *
 * @param name 域名
 * @return int 描述符
 */
int connectTopop(const char *name)
{
    int sockfd;
    int len, result;
    struct sockaddr_in address;
    struct hostent *host;
    char str[32];
    // socket(domain, type, protocol)
    // domain - 协议类型 ( AF_INET - IPv4 AF_INET6 - IPv6 )
    // type - SOCK_STREAM SOCK_DGRAM SOCK_RAW
    // protocol - 只有type = SOCK_RAW 不是0（有待考证）
    host = gethostbyname(name);
    if (host == NULL)
    {
        // herror("gethostname():");
        printf("%s\n", hstrerror(h_errno));
        exit(-1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
    // memmove(&address.sin_addr.s_addr, *host->h_addr_list, sizeof(&address.sin_addr.s_addr));
    // address.sin_addr.s_addr = inet_addr("220.181.12.110");
    address.sin_port = htons(110);
    // 连接成功返回0，失败返回-1
    result = connect(sockfd, (struct sockaddr *)&address, sizeof(address));
    if (result == -1)
    {
        perror("connect():");
        exit(-1);
    }
    printf("host official name: %s\n", host->h_name);
    printf("address: %s\n", inet_ntop(host->h_addrtype, host->h_addr_list[0], str, sizeof(str)));
    n = read(sockfd, rcvbuf, MAX_LEN_BUF);
    rcvbuf[n] = 0;
    printf("%s", rcvbuf);
    return sockfd;
}

/**
 * @brief 得到发送给服务器的字符串，字符串最后一定是"\r\n"
 *
 * @param sendbuf 待发送的字符串
 */
void get_sendbuf(char *sendbuf)
{
    int i = 0;
    // 读取输入的第一部分，USER PASS LIST STAT ...
    while ((sendbuf[i] = getch()) != ' ' && sendbuf[i] != '\r')
    {
        printf("%c", sendbuf[i]);
        i++;
    }
    printf("%c", sendbuf[i]); // 手动打印最后一个字符

    // 读取第二部分，有些命令是没有的，比如QUIT
    if (sendbuf[i] != '\r') // 这条命令后面还有参数，比如USER PASS后面需要有其他输入
    {
        i++;
        if (strcmp(sendbuf, "PASS ") == 0) // 即将输入密码，读取后以*代替输出，隐藏密码
        {
            while ((sendbuf[i] = getch()) != '\r')
            {
                printf("*");
                i++;
            }
            printf("\n"); // 手动输出换行，因为getch读取到的"enter"只有'\r'，所以补一个'\n'，下同
        }
        else
        {
            while ((sendbuf[i] = getchar()) != '\n') // getchar读取到的换行符为'\n'
                i++;
            sendbuf[i] = '\r'; // 手动修正最后一个字符
        }
    }
    else
        printf("\n");
    sendbuf[i + 1] = '\n'; // 手动补一个'\n'到字符串最后，因为服务器要求的输入一定是"\r\n"结尾
}

void fileParse(FILE *fin)
{
    FILE *fp = fopen("../result/out.txt", "w");
    regmatch_t pmatch;
    regex_t reg1, reg2;
    const char *pattern1 = "base64";
    const char *pattern2 = "base64.*--";
    int status;
    regcomp(&reg1, pattern1, REG_NEWLINE);
    regcomp(&reg2, pattern2, REG_NEWLINE);
    int offset = 0;
    char buf[1024];
    char code[10000] = {0};
    int found = 0;
    do
    {
        fgets(buf, 1024, fin);
        status = regexec(&reg1, buf, 1, &pmatch, 0);
        if (status != REG_NOMATCH)
            found = !found;
        if (found)
        {
            strcat(code + strlen(code), buf);
            code[strlen(code) - 2] = 0;
        }
    } while (!feof(fin));
    status = regexec(&reg2, code, 1, &pmatch, 0);
    if (status != REG_NOMATCH)
    {
        int end = pmatch.rm_eo;
        while (code[--end] == '-')
            ;
        code[end + 1] = 0;
        printf("content:\n%s\n", base64_decode(code + pmatch.rm_so + 6));
        fputs(base64_decode(code + pmatch.rm_so + 6), fp);
    }
    fclose(fp);
    regfree(&reg1);
    regfree(&reg2);
}

/**
 * @brief 运行pop3客户端
 *
 * @param sockfd 与服务器建立连接之后的套接字描述符
 */
void client_run(int sockfd)
{
    char cmd[MAX_LEN_CMD];
    char file_name[30] = "../emails/email-x.txt";
    char file_id = '0';
    while (1)
    {
        fflush(stdin);
        fflush(stdout);
        flag = 0;
        memset(cmd, 0, sizeof(cmd));
        get_sendbuf(cmd);
        if (!cmdParse(cmd))
        {
            printf("Error, illegal commad!\n");
            continue;
        }
        write(sockfd, cmd, strlen(cmd));
        if (flag)
        {
            file_name[16] = file_id;
            file_id = file_id++;
            if (file_id > 57)
                file_id = '0';
            FILE *fp = fopen(file_name, "w+");
            while ((n = read(sockfd, rcvbuf, MAX_LEN_BUF)) > 0)
            {
                rcvbuf[n] = 0;
                // printf("%s", rcvbuf);
                if (!(strcmp(cmd, "LIST\r\n") == 0))
                    fputs(rcvbuf, fp);
                else
                    printf("%s", rcvbuf);
                // printf("----------%c----------%d---------%d\n", rcvbuf[n - 3], rcvbuf[n - 2], rcvbuf[n - 1]);
                // 结束符为".\r\n"
                if (rcvbuf[n - 1] == '\n' && rcvbuf[n - 2] == '\r' && rcvbuf[n - 3] == '.')
                    break;
            }

            if (!(strcmp(cmd, "LIST\r\n") == 0))
            {
                printf("+OK\nwhole content has been written in %s\n", file_name);
                fseek(fp, 0, SEEK_SET);
                fileParse(fp);
            }
            fclose(fp);
        }
        else
        {
            n = read(sockfd, rcvbuf, MAX_LEN_BUF);
            rcvbuf[n] = 0;
            printf("%s", rcvbuf);
        }
        if (strcmp(cmd, "QUIT\r\n") == 0)
            break;
    }
}

/**
 * @brief 解析cmd，判断cmd是否合法
 *
 * @param cmd 命令
 * @return true 合法
 * @return false 不合法
 */
bool cmdParse(const char *cmd)
{
    char *token = NULL;
    char command[MAX_LEN_CMD] = {0};
    strcpy(command, cmd);
    // printf("%s|\n", command);
    token = strtok(command, " ");
    token = strtok(command, "\r");
    // printf("command is %s|\n", token);
    if (strcmp(token, "USER") && strcmp(token, "PASS") && strcmp(token, "STAT") && strcmp(token, "LIST") && strcmp(token, "RETR") && strcmp(token, "DELE") && strcmp(token, "RSET") && strcmp(token, "TOP") && strcmp(token, "QUIT"))
        return false;

    if (strcmp(token, "LIST") == 0 || strcmp(token, "RETR") == 0 || strcmp(token, "TOP") == 0)
        flag = true;
    return true;
}



int main()
{
    int sockfd;
    sockfd = connectTopop("pop.qq.com");
    client_run(sockfd);
    close(sockfd);
    return 0;
}