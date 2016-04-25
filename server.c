#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

struct fdnode
{
	int err_out;
	int validate;	
	int p_in_fd;
	int p_out_fd;
	int outputfd;
	int errorfd;
	struct fdnode *next;
};

struct fdnode* get_fdnode(int a,int b,int c,int v);
struct fdnode* get_nth_node(struct fdnode *head,int n);
void handle_child(int sig);

int main(void)
{
	struct fdnode *queue=NULL , *free_temp ,*nth_node;
	struct sockaddr_in serv_addr;
	int listenfd , connfd,i=0,j=0;
	int pipefd[2];
	int pipe_num=0;
	int ex_num=0;
	int child_return;
	int redir_flag=0;
	char sendbuff[1024],recebuff[11000];
	char **command_s;
	char newpath[50];
	char *env_got;
	char *end;
	char end_line[]="end of line";

	signal(SIGCHLD,handle_child);

	if((listenfd = socket(AF_INET , SOCK_STREAM , 0)) < 0)
	{
		printf("socket() call error\n");
		return 0;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(5002);

	if(bind(listenfd , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0)
	{
		printf("bind() call error\n");
		return 0;
	}
	if(listen(listenfd,1) < 0)
	{
		printf("listen() call error\n");
		return 0;
	}
	printf("listen socket fd is %d\n",listenfd);
	command_s = (char **)malloc(10000*sizeof(char *));

	chdir("./ras");
	setenv("PATH","bin:.",1);

	while(1)//keep listening
	{
		if((connfd = accept(listenfd,(struct sockaddr*)NULL,NULL)) < 0)
		{
			printf("connect() call error\n");
			return 0;
		}
		//now connect to a client,need a damn child
		if(fork() == 0)//fork a child that connect with client
		{
			//init a fdnode for this client, call get_fdnode()
			close(2);
			dup(connfd);
			close(1);
			dup(connfd);
			close(listenfd);
			queue=get_fdnode(-1,0,1,0);//-1 is a special usage

			bzero(sendbuff,sizeof(sendbuff));
			bzero(recebuff,sizeof(recebuff));
			strcpy(sendbuff,"****************************************\n** Welcome to the information server. **\n****************************************\n");
			write(connfd,sendbuff,strlen(sendbuff));

			while(1)//keep prompt until "exit"
			{
				bzero(sendbuff,sizeof(sendbuff));
				bzero(recebuff,sizeof(recebuff));
	
				write(connfd,"% ",2);
				read(connfd,recebuff,sizeof(recebuff));

				end = strstr(recebuff,"\r");
				*end = '\0';
				
				if(strrchr(recebuff,'/') != NULL)
				{
					printf("/ is not acceptable !\n");
					continue;
				}

				command_s[0]=strtok(recebuff," ");

				i=1;
				while(1)
				{
					command_s[i]=strtok(NULL," ");
					if(command_s[i]==NULL)
					{
						command_s[i+1]=end_line;
						break;
					}
					else
						i++;
				}
				if(strcmp(command_s[0],"exit") == 0)
					break;
				else if(strcmp(command_s[0],"setenv") == 0)
				{
					if(setenv(command_s[1],command_s[2],1) < 0)
					{
						printf("setenv error\n");
						continue;
					}
				}
				else if(strcmp(command_s[0],"printenv") == 0)
				{
					if(command_s[1] == NULL)
					{
						printf("printenv error\n");
						continue;
					}
					env_got = getenv(command_s[1]);
					if(env_got == NULL)
					{
						printf("env does not exist\n");
						continue;
					}
					strcpy(sendbuff,command_s[1]);
					strcat(sendbuff,"=");
					strcat(sendbuff,env_got);
					strcat(sendbuff,"\n");
					if(write(1,sendbuff,strlen(sendbuff)) < 0)
					{
						printf("print to client error\n");
						continue;
					}
				}
				else
				{
					while(command_s[0] != end_line)
					{
						j=0;
						pipe_num=0;
						redir_flag=0;
						ex_num=0;
						while(command_s[j] != NULL)
						{
							if(strrchr(command_s[j],'!') != NULL)// | sysmbol detected
							{
								if(strlen(command_s[j]) == 1)
									pipe_num = 1;
								else
									pipe_num = atoi(command_s[j]+1);
								queue->validate = 1;
								nth_node = get_nth_node(queue,pipe_num);//trace n nodes
								if(nth_node -> validate == 1)//if node validate(exist a pipe to nth command),use this pipe 
								{
									//printf("validate\n");
									queue->errorfd = nth_node->p_in_fd;
								}
								else//create a pipe to nth command
								{
									//printf("create pipe\n");
									nth_node->validate = 1;
									pipe(pipefd);
									nth_node->p_in_fd = pipefd[1];
									nth_node->p_out_fd = pipefd[0];
									nth_node->outputfd = 1;
									queue->errorfd = pipefd[1];//change current fdnode
									//printf("%d %d\n",pipefd[1],pipefd[0]);
								}
								command_s[j] = NULL;//flowing are important steps!!!!
								if(command_s[j+1] == NULL)//means command NULL NULL end_line
									command_s[j+1] = end_line;//change to command NULL end_line end_line
								break;
							}
							if(strrchr(command_s[j],'>') != NULL)
							{
								redir_flag=1;
								command_s[j] = NULL;//replace > By NULL
								j++;//(get file name)
								queue->validate = 1;
								queue->outputfd = open(command_s[j],O_CREAT|O_WRONLY|O_TRUNC,00644);
								if(command_s[j+1] == NULL)//important!! means command >(NULL) file(NULL) NULL 
									command_s[j] = end_line;//change to command >(NULL) file(end_line) NULL
								break;
							}
							/*
							if(strrchr(command_s[j],'!') != NULL)
							{	
								printf("! detected\n");
								if(strlen(command_s[j]) == 1)
									ex_num = 1;
								else
									ex_num = atoi(command_s[j]+1);
								printf("%d\n",ex_num);
								queue->validate = 1;
								queue->err_out = 1;
								nth_node = get_nth_node(queue,pipe_num);//trace n nodes
								if(nth_node -> validate == 1)//if node validate(exist a pipe to nth command),use this pipe 
								{
									//queue->errorfd = nth_node->p_in_fd;
									printf("validate create pipe\n");
									nth_node->validate = 1;
									pipe(pipefd);
									nth_node->p_in_fd = pipefd[1];
									nth_node->p_out_fd = pipefd[0];
									nth_node->outputfd = 1;
									queue->errorfd = pipefd[1];//change current fdnode
									printf("%d %d\n",pipefd[1],pipefd[0]);
								}
								else//create a pipe to nth command
								{
									printf("create pipe\n");
									nth_node->validate = 1;
									pipe(pipefd);
									nth_node->p_in_fd = pipefd[1];
									nth_node->p_out_fd = pipefd[0];
									nth_node->outputfd = 1;
									queue->errorfd = pipefd[1];//change current fdnode
									printf("%d %d\n",pipefd[1],pipefd[0]);
								}
								//command_s[j] = NULL;//flowing are important steps!!!!
								//if(command_s[j+1] == NULL)//means command NULL NULL end_line
							//		command_s[j+1] = end_line;
								break;
							}
							*/
							j++;
						}
						if(fork() == 0)//fork a child that execute the command
						{
							if(queue->validate == 1)//change fd base on a validate fdnode
							{
								if(queue->p_in_fd != (-1))
								{//close pipe's input ,ready to execute
									close(queue->p_in_fd);
								}
								if(queue->p_out_fd != 0)
								{
									//printf("change input fd = %d\n",queue->p_out_fd);
									close(0);
									dup(queue->p_out_fd);
									close(queue->p_out_fd);
								}
								if(queue->errorfd != 2)
								{
									//printf("change error fd = %d\n",queue->errorfd);
									close(2);
									dup(queue->errorfd);
									close(queue->errorfd);
								}
								/*
								if(queue->err_out != 0)
								{
									printf("%s change error out %d to %d\n",command_s[0],2,queue->errorfd);
									close(2);
									dup(queue->errorfd);
									close(queue->errorfd);
								}*/
							}
							//printf("execute %s",command_s[0]);
							execvp(command_s[0],command_s);
							strcpy(sendbuff,"Unknown command: [");
							strcat(sendbuff,command_s[0]);
							strcat(sendbuff,"].\n");
							write(connfd,sendbuff,strlen(sendbuff));
							exit(-1);
						}
						else
						{
							if(queue->validate == 1)
							{
								if(queue->p_in_fd != (-1))//close pipe's input,leave it for child
									close(queue->p_in_fd);
								if(redir_flag != 0)//if output is a file , close it!!!!
									close(queue->outputfd);
							}
							wait(&child_return);
							
							if(WIFEXITED(child_return) && WEXITSTATUS(child_return) == 255)//child_return > 0
							{
								command_s[0] = end_line;
								queue->outputfd = 1;
							}
							else
							{
								//printf("%s success\n",command_s[0]);
								if(queue->validate == 1)//if child success,means now parent can close the pipe
								{
									if(queue->p_out_fd != 0)
										close(queue->p_out_fd);
									//if(queue->err_out != 0)
									//	close(queue->errorfd);
								}
								command_s = &(command_s[j+1]);
								free_temp = queue;
								if(queue->next == NULL)
									queue->next = get_fdnode(-1,0,1,0);//-1 is a special usage
								queue = queue->next;
								free(free_temp);
							}
						}
					}
				}
			}//end of while(1) for prompt
			close(connfd);//end session with this user
			exit(0);
		}//if fork == 0
		else//listen side
		{
			close(connfd);//leave it for child
		}
	}//while(1) keep listening
	close(listenfd);
	printf("End of my NP server\n");
	return 0;
}

struct fdnode* get_fdnode(int a,int b,int c,int v)
{
	struct fdnode* rc= (struct fdnode*)malloc(sizeof(struct fdnode));
	rc->validate = v;
	rc->p_in_fd=a;
	rc->p_out_fd=b;
	rc->outputfd=c;
	rc->err_out=0;
	rc->errorfd=2;
	rc->next=NULL;
	return rc;
}

struct fdnode* get_nth_node(struct fdnode *head,int n)
{
	//call by value?
	int s;
	struct fdnode *temp = head;
	for(s=0;s<n;s++)
	{
		if(temp->next == NULL)
			temp->next = get_fdnode(-1,0,1,0);//-1 is a special usage
		temp = temp ->next;
	}
	return temp;
}

void handle_child(int sig)
{
	int rv,pid;
	pid = wait(&rv);
	//printf("child %d termincated\n",pid);
	return;
}
