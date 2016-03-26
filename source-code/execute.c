#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include "global.h"
#define DEBUG
int goon = 0, ingnore = 0;       //ÓÃÓÚÉèÖÃsignalÐÅºÅÁ¿
char *envPath[10], cmdBuff[40];  //Íâ²¿ÃüÁîµÄ´æ·ÅÂ·¾¶¼°¶ÁÈ¡Íâ²¿ÃüÁîµÄ»º³å¿Õ¼ä
History history;                 //ÀúÊ·ÃüÁî
Job *head = NULL;                //×÷ÒµÍ·Ö¸Õë
pid_t fgPid;                     //µ±Ç°Ç°Ì¨×÷ÒµµÄ½ø³ÌºÅ

/*******************************************************
                  ¹¤¾ßÒÔ¼°¸¨Öú·½·¨
********************************************************/
/*ÅÐ¶ÏÃüÁîÊÇ·ñ´æÔÚ*/
int exists(char *cmdFile){
    int i = 0;
    if((cmdFile[0] == '/' || cmdFile[0] == '.') && access(cmdFile, F_OK) == 0){ //ÃüÁîÔÚµ±Ç°Ä¿Â¼
        strcpy(cmdBuff, cmdFile);
        return 1;
    }else{  //²éÕÒysh.confÎÄ¼þÖÐÖ¸¶¨µÄÄ¿Â¼£¬È·¶¨ÃüÁîÊÇ·ñ´æÔÚ
        while(envPath[i] != NULL){ //²éÕÒÂ·¾¶ÒÑÔÚ³õÊ¼»¯Ê±ÉèÖÃÔÚenvPath[i]ÖÐ
            strcpy(cmdBuff, envPath[i]);
            strcat(cmdBuff, cmdFile);
            
            if(access(cmdBuff, F_OK) == 0){ //ÃüÁîÎÄ¼þ±»ÕÒµ½
                return 1;
            }
            
            i++;
        }
    }
    
    return 0; 
}

/*½«×Ö·û´®×ª»»ÎªÕûÐÍµÄPid*/
int str2Pid(char *str, int start, int end){
    int i, j;
    char chs[20];
    
    for(i = start, j= 0; i < end; i++, j++){
        if(str[i] < '0' || str[i] > '9'){
            return -1;
        }else{
            chs[j] = str[i];
        }
    }
    chs[j] = '\0';
    
    return atoi(chs);
}

/*µ÷Õû²¿·ÖÍâ²¿ÃüÁîµÄ¸ñÊ½*/
void justArgs(char *str){
    int i, j, len;
    len = strlen(str);
    
    for(i = 0, j = -1; i < len; i++){
        if(str[i] == '/'){
            j = i;
        }
    }

    if(j != -1){ //ÕÒµ½·ûºÅ'/'
        for(i = 0, j++; j < len; i++, j++){
            str[i] = str[j];
        }
        str[i] = '\0';
    }
}

/*ÉèÖÃgoon*/
void setGoon(){
    goon = 1;
}

/*ÊÍ·Å»·¾³±äÁ¿¿Õ¼ä*/
void release(){
    int i;
    for(i = 0; strlen(envPath[i]) > 0; i++){
        free(envPath[i]);
    }
}

/*******************************************************
                  ÐÅºÅÒÔ¼°jobsÏà¹Ø
********************************************************/
/*Ìí¼ÓÐÂµÄ×÷Òµ*/
Job* addJob(pid_t pid){
    Job *now = NULL, *last = NULL, *job = (Job*)malloc(sizeof(Job));
    
	//³õÊ¼»¯ÐÂµÄjob
    job->pid = pid;
    strcpy(job->cmd, inputBuff);
    strcpy(job->state, RUNNING);
    job->next = NULL;
    
    if(head == NULL){ //ÈôÊÇµÚÒ»¸öjob£¬ÔòÉèÖÃÎªÍ·Ö¸Õë
        head = job;
    }else{ //·ñÔò£¬¸ù¾Ýpid½«ÐÂµÄjob²åÈëµ½Á´±íµÄºÏÊÊÎ»ÖÃ
		now = head;
		while(now != NULL && now->pid < pid){
			last = now;
			now = now->next;
		}
        last->next = job;
        job->next = now;
    }
    
    return job;
}

/*ÒÆ³ýÒ»¸ö×÷Òµ*/
void rmJob(int sig, siginfo_t *sip, void* noused){
    pid_t pid;
    Job *now = NULL, *last = NULL;
    
    if(ingnore == 1){
        ingnore = 0;
        return;
    }
    if(getpid() == fgPid)
        fgPid = 0;
    
    pid = sip->si_pid;

    now = head;
	while(now != NULL && now->pid < pid){
		last = now;
		now = now->next;
	}
    
    if(now == NULL){ //×÷Òµ²»´æÔÚ£¬Ôò²»½øÐÐ´¦ÀíÖ±½Ó·µ»Ø
        return;
    }
    
	//¿ªÊ¼ÒÆ³ý¸Ã×÷Òµ
    if(now == head){
        head = now->next;
    }else{
        last->next = now->next;
    }
    
    free(now);
}

/*×éºÏ¼üÃüÁîctrl+z*/
void ctrl_Z(){
    Job *now = NULL;
    
    if(fgPid == 0){ //Ç°Ì¨Ã»ÓÐ×÷ÒµÔòÖ±½Ó·µ»Ø
        return;
    }
    
    //SIGCHLDÐÅºÅ²úÉú×Ôctrl+z
    ingnore = 1;
    
	now = head;
    //printf("%d\n",head);
	while(now != NULL && now->pid != fgPid)
		{
            //printf("%d",now->pid);
            now = now->next;
        }
    
    if(now == NULL){ //Î´ÕÒµ½Ç°Ì¨×÷Òµ£¬Ôò¸ù¾ÝfgPidÌí¼ÓÇ°Ì¨×÷Òµ
        now = addJob(fgPid);
    }
    
	//ÐÞ¸ÄÇ°Ì¨×÷ÒµµÄ×´Ì¬¼°ÏàÓ¦µÄÃüÁî¸ñÊ½£¬²¢´òÓ¡ÌáÊ¾ÐÅÏ¢
    strcpy(now->state, STOPPED); 
    now->cmd[strlen(now->cmd)] = '&';
    now->cmd[strlen(now->cmd)+1] = '\0';
    printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);
    
	//·¢ËÍSIGSTOPÐÅºÅ¸øÕýÔÚÇ°Ì¨ÔË×÷µÄ¹¤×÷£¬½«ÆäÍ£Ö¹
    kill(fgPid, SIGSTOP);
    fgPid = 0;
}
void ctrl_C(){
    if(fgPid != 0)
        kill(fgPid,SIGINT);
}

/*fgÃüÁî*/
void fg_exec(int pid){    
    Job *now = NULL; 
    Job *job = NULL;
    Job *last = NULL;
    job = (Job*)malloc(sizeof(Job));
	int i,flag=0;
    printf("%d\n",head);
    //SIGCHLDÐÅºÅ²úÉú×Ô´Ëº¯Êý
    ingnore = 1;
    
	//¸ù¾Ýpid²éÕÒ×÷Òµ
    now = head;
	while(now != NULL && now->pid != pid)
		{
            last = now;
            now = now->next;
        }
    if(head == now)
    {
        flag = 1;
    }
    if(now == NULL){ //Î´ÕÒµ½×÷Òµ
        printf("pidÎª7%d µÄ×÷Òµ²»´æÔÚ£¡\n", pid);
        return;
    }
    
    //¼ÇÂ¼Ç°Ì¨×÷ÒµµÄpid£¬ÐÞ¸Ä¶ÔÓ¦×÷Òµ×´Ì¬
    fgPid = now->pid;
    strcpy(now->state, RUNNING);
    
    //signal(SIGTSTP, ctrl_Z); //ÉèÖÃsignalÐÅºÅ£¬ÎªÏÂÒ»´Î°´ÏÂ×éºÏ¼üCtrl+Z×ö×¼±¸
    i = strlen(now->cmd) - 1;
    while(i >= 0 && now->cmd[i] != '&')
		i--;
    now->cmd[i] = '\0';

    job->pid =  now -> pid;
    strcpy(job->cmd, now -> cmd);
    strcpy(job->state, now ->state);
    job->next = now -> next;

    printf("%s\n", now->cmd);
    tcsetpgrp(STDIN_FILENO,pid);
    kill(now->pid, SIGCONT); 
    sleep(1);
    int status = 0;
    fgPid = pid;
    ////printf("111111\n");
    waitpid(pid, &status,WUNTRACED);
    tcsetpgrp(STDIN_FILENO,getpid());
    ///printf("2222222");
    //printf("%d!!!%d$$$",head,now);
    if(status == 5247)
        {
            now = job;
            if(flag  == 1)
            {
                head = now;
            }
            else
                last ->next  = now;
            ctrl_Z();
        }
    else if(status == 2)
        ctrl_C();

}

/*bgÃüÁî*/
void bg_exec(int pid){
    Job *now = NULL;
    
    //SIGCHLDÐÅºÅ²úÉú×Ô´Ëº¯Êý
    ingnore = 1;
    
	//¸ù¾Ýpid²éÕÒ×÷Òµ
	now = head;
    while(now != NULL && now->pid != pid)
		now = now->next;
    
    if(now == NULL){ //Î´ÕÒµ½×÷Òµ
        printf("pidÎª7%d µÄ×÷Òµ²»´æÔÚ£¡\n", pid);
        return;
    }
    
    strcpy(now->state, RUNNING); //ÐÞ¸Ä¶ÔÏó×÷ÒµµÄ×´Ì¬
    printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);
    
    kill(now->pid, SIGCONT); //Ïò¶ÔÏó×÷Òµ·¢ËÍSIGCONTÐÅºÅ£¬Ê¹ÆäÔËÐÐ
}

/*******************************************************
                    ÃüÁîÀúÊ·¼ÇÂ¼
********************************************************/
void addHistory(char *cmd){
    if(history.end == -1){ //µÚÒ»´ÎÊ¹ÓÃhistoryÃüÁî
        history.end = 0;
        strcpy(history.cmds[history.end], cmd);
        return;
	}
    
    history.end = (history.end + 1)%HISTORY_LEN; //endÇ°ÒÆÒ»Î»
    strcpy(history.cmds[history.end], cmd); //½«ÃüÁî¿½±´µ½endÖ¸ÏòµÄÊý×éÖÐ
    
    if(history.end == history.start){ //endºÍstartÖ¸ÏòÍ¬Ò»Î»ÖÃ
        history.start = (history.start + 1)%HISTORY_LEN; //startÇ°ÒÆÒ»Î»
    }
}

/*******************************************************
                     ³õÊ¼»¯»·¾³
********************************************************/
/*Í¨¹ýÂ·¾¶ÎÄ¼þ»ñÈ¡»·¾³Â·¾¶*/
void getEnvPath(int len, char *buf){
    int i, j, last = 0, pathIndex = 0, temp;
    char path[40];
    
    for(i = 0, j = 0; i < len; i++){
        if(buf[i] == ':'){ //½«ÒÔÃ°ºÅ(:)·Ö¸ôµÄ²éÕÒÂ·¾¶·Ö±ðÉèÖÃµ½envPath[]ÖÐ
            if(path[j-1] != '/'){
                path[j++] = '/';
            }
            path[j] = '\0';
            j = 0;
            
            temp = strlen(path);
            envPath[pathIndex] = (char*)malloc(sizeof(char) * (temp + 1));
            strcpy(envPath[pathIndex], path);
            
            pathIndex++;
        }else{
            path[j++] = buf[i];
        }
    }
    
    envPath[pathIndex] = NULL;
}

/*³õÊ¼»¯²Ù×÷*/
void init(){
    int fd, n, len;
    char c, buf[80];

	//´ò¿ª²éÕÒÂ·¾¶ÎÄ¼þysh.conf
    if((fd = open("ysh.conf", O_RDONLY, 660)) == -1){
        perror("init environment failed\n");
        exit(1);
    }
    
	//³õÊ¼»¯historyÁ´±í
    history.end = -1;
    history.start = 0;
    
    len = 0;
	//½«Â·¾¶ÎÄ¼þÄÚÈÝÒÀ´Î¶ÁÈëµ½buf[]ÖÐ
    while(read(fd, &c, 1) != 0){ 
        buf[len++] = c;
    }
    buf[len] = '\0';

    //½«»·¾³Â·¾¶´æÈëenvPath[]
    getEnvPath(len, buf); 
    
    //×¢²áÐÅºÅ
    struct sigaction action;
    action.sa_sigaction = rmJob;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
    signal(SIGTSTP, ctrl_Z);
    signal(SIGINT,ctrl_C);
}

/*******************************************************
                      ÃüÁî½âÎö
********************************************************/
SimpleCmd* handleSimpleCmdStr(int begin, int end){
    int i, j, k;
    int fileFinished; //¼ÇÂ¼ÃüÁîÊÇ·ñ½âÎöÍê±Ï
    char c, buff[10][40], inputFile[30], outputFile[30], *temp = NULL;
    SimpleCmd *cmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));
    
	//Ä¬ÈÏÎª·ÇºóÌ¨ÃüÁî£¬ÊäÈëÊä³öÖØ¶¨ÏòÎªnull
    cmd->isBack = 0;
    cmd->input = cmd->output = NULL;
    
    //³õÊ¼»¯ÏàÓ¦±äÁ¿
    for(i = begin; i<10; i++){
        buff[i][0] = '\0';
    }
    inputFile[0] = '\0';
    outputFile[0] = '\0';
    
    i = begin;
	//Ìø¹ý¿Õ¸ñµÈÎÞÓÃÐÅÏ¢
    while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
        i++;
    }
    
    k = 0;
    j = 0;
    fileFinished = 0;
    temp = buff[k]; //ÒÔÏÂÍ¨¹ýtempÖ¸ÕëµÄÒÆ¶¯ÊµÏÖ¶Ôbuff[i]µÄË³´Î¸³Öµ¹ý³Ì
    while(i < end){
		/*¸ù¾ÝÃüÁî×Ö·ûµÄ²»Í¬Çé¿ö½øÐÐ²»Í¬µÄ´¦Àí*/
        switch(inputBuff[i]){ 
            case ' ':
            case '\t': //ÃüÁîÃû¼°²ÎÊýµÄ½áÊø±êÖ¾
                temp[j] = '\0';
                j = 0;
                if(!fileFinished){
                    k++;
                    temp = buff[k];
                }
                break;


            case '|': //ÊäÈëÖØ¶¨Ïò±êÖ¾
                if(j != 0){
            //´ËÅÐ¶ÏÎª·ÀÖ¹ÃüÁîÖ±½Ó°¤×Å<·ûºÅµ¼ÖÂÅÐ¶ÏÎªÍ¬Ò»¸ö²ÎÊý£¬Èç¹ûls<sth
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                cmd->pipe=1;
                fileFinished = 1;
                i++;
                cmd->index=i;
                break;

            case '<': //ÊäÈëÖØ¶¨Ïò±êÖ¾
                if(j != 0){
		    //´ËÅÐ¶ÏÎª·ÀÖ¹ÃüÁîÖ¾±½Ó°¤×Å<·ûºÅµ¼ÖÂÅÐ¶ÏÎªÍ¬Ò»¸ö²ÎÊý£¬Èç¹ûls<sth
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                temp = inputFile;
                fileFinished = 1;
                i++;
                break;
                
            case '>': //Êä³öÖØ¶¨Ïò±êÖ¾
                if(j != 0){
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                temp = outputFile;
                fileFinished = 1;
                i++;
                break;
                
            case '&': //ºóÌ¨ÔËÐÐ±êÖ¾
                if(j != 0){
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                cmd->isBack = 1;
                fileFinished = 1;
                i++;
                break;
                
            default: //Ä¬ÈÏÔò¶ÁÈëµ½tempÖ¸¶¨µÄ¿Õ¼ä
                temp[j++] = inputBuff[i++];
                continue;
		}
        
		//Ìø¹ý¿Õ¸ñµÈÎÞÓÃÐÅÏ¢
        while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
            i++;
        }
        if(cmd->pipe == 1)
            break;
	}
    
    if(inputBuff[end-1] != ' ' && inputBuff[end-1] != '\t' && inputBuff[end-1] != '&'){
        temp[j] = '\0';
        if(!fileFinished){
            k++;
        }
    }
    
	//ÒÀ´ÎÎªÃüÁîÃû¼°Æä¸÷¸ö²ÎÊý¸³Öµ
    cmd->args = (char**)malloc(sizeof(char*) * (k + 1));
    cmd->args[k] = NULL;
    for(i = 0; i<k; i++){
        j = strlen(buff[i]);
        cmd->args[i] = (char*)malloc(sizeof(char) * (j + 1));   
        strcpy(cmd->args[i], buff[i]);
    }
    
	//Èç¹ûÓÐÊäÈëÖØ¶¨ÏòÎÄ¼þ£¬ÔòÎªÃüÁîµÄÊäÈëÖØ¶¨Ïò±äÁ¿¸³Öµ
    if(strlen(inputFile) != 0){
        j = strlen(inputFile);
        cmd->input = (char*)malloc(sizeof(char) * (j + 1));
        strcpy(cmd->input, inputFile);
    }

    //Èç¹ûÓÐÊä³öÖØ¶¨ÏòÎÄ¼þ£¬ÔòÎªÃüÁîµÄÊä³öÖØ¶¨Ïò±äÁ¿¸³Öµ
    if(strlen(outputFile) != 0){
        j = strlen(outputFile);
        cmd->output = (char*)malloc(sizeof(char) * (j + 1));   
        strcpy(cmd->output, outputFile);
    }
    #ifdef DEBUG
    printf("****\n");
    printf("isBack: %d\n",cmd->isBack);
    	for(i = 0; cmd->args[i] != NULL; i++){
    		printf("args[%d]: %s\n",i,cmd->args[i]);
	}
    printf("input: %s\n",cmd->input);
    printf("output: %s\n",cmd->output);
    printf("****\n");
    #endif
    return cmd;
}

/*******************************************************
                      ÃüÁîÖ´ÐÐ
********************************************************/
/*Ö´ÐÐÍâ²¿ÃüÁî*/
void execOuterCmd(SimpleCmd *cmd){
    pid_t pid;
    int pipeIn, pipeOut;
    
    if(exists(cmd->args[0])){ //ÃüÁî´æÔÚ

        if((pid = fork()) < 0){
            perror("fork failed");
            return;
        }
        
        if(pid == 0){ //×Ó½ø³Ì
            setpgid(0,0);
            if(cmd->input != NULL){ //´æÔÚÊäÈëÖØ¶¨Ïò
                if((pipeIn = open(cmd->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
                    printf("²»ÄÜ´ò¿ªÎÄ¼þ %s£¡\n", cmd->input);
                    return;
                }
                if(dup2(pipeIn, 0) == -1){
                    printf("ÖØ¶¨Ïò±ê×¼ÊäÈë´íÎó£¡\n");
                    return;
                }
            }
            
            if(cmd->output != NULL){ //´æÔÚÊä³öÖØ¶¨Ïò
                if((pipeOut = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
                    printf("²»ÄÜ´ò¿ªÎÄ¼þ %s£¡\n", cmd->output);
                    return ;
                }
                if(dup2(pipeOut, 1) == -1){
                    printf("ÖØ¶¨Ïò±ê×¼Êä³ö´íÎó£¡\n");
                    return;
                }
            }
            
            if(cmd->isBack){ //ÈôÊÇºóÌ¨ÔËÐÐÃüÁî£¬µÈ´ý¸¸½ø³ÌÔö¼Ó×÷Òµ
                signal(SIGUSR1, setGoon); //ÊÕµ½ÐÅºÅ£¬setGoonº¯Êý½«goonÖÃ1£¬ÒÔÌø³öÏÂÃæµÄÑ­»·
                while(goon == 0) ; //µÈ´ý¸¸½ø³ÌSIGUSR1ÐÅºÅ£¬±íÊ¾×÷ÒµÒÑ¼Óµ½Á´±íÖÐ
                goon = 0; //ÖÃ0£¬ÎªÏÂÒ»ÃüÁî×ö×¼±¸
                
                printf("[%d]\t%s\t\t%s\n", getpid(), RUNNING, inputBuff);
                kill(getppid(), SIGUSR1);
            }
            
            justArgs(cmd->args[0]);
            if(execv(cmdBuff, cmd->args) < 0){ //Ö´ÐÐÃüÁî
                printf("execv failed!\n");
                return;
            }
        }
		else{ //¸¸½ø³Ì
            setpgid(pid,0);
            if(cmd ->isBack){ //ºóÌ¨ÃüÁî             
                fgPid = 0; //pidÖÃ0£¬ÎªÏÂÒ»ÃüÁî×ö×¼±¸
                addJob(pid); //Ôö¼ÓÐÂµÄ×÷Òµ
                sleep(1);
                kill(pid, SIGUSR1); //×Ó½ø³Ì·¢ÐÅºÅ£¬±íÊ¾×÷ÒµÒÑ¼ÓÈë
                signal(SIGTTOU, SIG_IGN);
                //µÈ´ý×Ó½ø³ÌÊä³ö
                signal(SIGUSR1, setGoon);
                while(goon == 0) ;
                goon = 0;
            }else{ //·ÇºóÌ¨ÃüÁî
                int status =1;
                //signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                tcsetpgrp(STDIN_FILENO,pid) ;
                fgPid = pid;
                waitpid(pid, &status,WUNTRACED);
                if(status == 5247)
                    ctrl_Z();
                else if(status == 2)
                    ctrl_C();
                tcsetpgrp(STDIN_FILENO,getpid()) ;
            }
		}
    }else{ //ÃüÁî²»´æÔÚ
        printf("ÕÒ²»µ½ÃüÁî 15%s\n", inputBuff);
    }
}
void execPipe(SimpleCmd *cmd1,SimpleCmd *cmd2)
{
    pid_t main,pid;
    int pipe_fd[2];
    int pipeIn,pipeOut;
    char cmdBuff2[40],cmdBuff1[40];
    if(exists(cmd1->args[0]))
    {
        strcpy(cmdBuff1,cmdBuff);
        if(exists(cmd2->args[0]))
        {
    strcpy(cmdBuff2,cmdBuff);

    if((main=fork())<0)
    {
        perror("Fork failed");
        exit(errno);
    }
    if(pipe(pipe_fd)<0)
    {
        perror("pipe failed");
        exit(errno);
    }
    if(main)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        waitpid(main,NULL,0);
        waitpid(pid,NULL,0);
    }
    else
    {
        if((pid=fork())<0)
        {
            perror("pipe failed");
            exit(errno);
        }
        if(pid)
        {
            close(pipe_fd[0]);
            dup2(pipe_fd[1],1);
            close(pipe_fd[1]);
            if(cmd1->input != NULL)
            { 
                if((pipeIn = open(cmd1->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
                    printf("can't open the file %s£¡\n", cmd1->input);
                    return;
                }
                if(dup2(pipeIn, 0) == -1){
                    printf("Redirecting standard input error£¡\n");
                    return;
                }
            }            
            justArgs(cmd1->args[0]);
            if(execv(cmdBuff1, cmd1->args) < 0){ //Ö´ÐÐÃüÁî
                printf("execv failed!\n");
                return;
            }
        }
        if(!pid)
        {
            close(pipe_fd[1]);
            dup2(pipe_fd[0],0);
            close(pipe_fd[0]);
            if(cmd2->output != NULL)
            { 
                if((pipeOut = open(cmd2->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
                    printf("can't open the file %s£¡\n", cmd2->output);
                    return ;
                }
                if(dup2(pipeOut, 1) == -1){
                    printf("Redirecting standard input error£¡\n");
                    return;
                }
            }
            justArgs(cmd2->args[0]);
            if(execv(cmdBuff2, cmd2->args) < 0){ //Ö´ÐÐÃüÁî
                printf("execv failed!\n");
                return;
            }
        }
    }
    }
    else
    {
        printf("can't find the command 15%s\n", inputBuff);
    }

    }
    else
    { 
        printf("can't find the command 15%s\n", inputBuff);
    }
}

/*Ö´ÐÐÃüÁî*/
void execSimpleCmd(SimpleCmd *cmd){
    int i, pid;
    char *temp;
    Job *now = NULL;
    
    if(strcmp(cmd->args[0], "exit") == 0) { //exitÃüÁî
        exit(0);
    } else if (strcmp(cmd->args[0], "history") == 0) { //historyÃüÁî
        if(history.end == -1){
            printf("ÉÐÎ´Ö´ÐÐÈÎºÎÃüÁî\n");
            return;
        }
        i = history.start;
        do {
            printf("%s\n", history.cmds[i]);
            i = (i + 1)%HISTORY_LEN;
        } while(i != (history.end + 1)%HISTORY_LEN);
    } else if (strcmp(cmd->args[0], "jobs") == 0) { //jobsÃüÁî
        if(head == NULL){
            printf("ÉÐÎÞÈÎºÎ×÷Òµ\n");
        } else {
            printf("index\tpid\tstate\t\tcommand\n");
            for(i = 1, now = head; now != NULL; now = now->next, i++){
                printf("%d\t%d\t%s\t\t%s\n", i, now->pid, now->state, now->cmd);
            }
        }
    } else if (strcmp(cmd->args[0], "cd") == 0) { //cdÃüÁî
        temp = cmd->args[1];
        if(temp != NULL){
            if(chdir(temp) < 0){
                printf("cd; %s ´íÎóµÄÎÄ¼þÃû»òÎÄ¼þ¼ÐÃû£¡\n", temp);
            }
        }
    } else if (strcmp(cmd->args[0], "fg") == 0) { //fgÃüÁî
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            if(pid != -1){
                printf("%d @@\n",head);
                fg_exec(pid);
            }
        }else{
            printf("fg; ²ÎÊý²»ºÏ·¨£¬ÕýÈ·¸ñÊ½Îª£ºfg %<int>\n");
        }
    } else if (strcmp(cmd->args[0], "bg") == 0) { //bgÃüÁî
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            
            if(pid != -1){
                bg_exec(pid);
            }
        }
		else{
            printf("bg; ²ÎÊý²»ºÏ·¨£¬ÕýÈ·¸ñÊ½Îª£ºbg %<int>\n");
        }
    } 
    else if (strcmp(cmd->args[0], "echo") == 0){
            int j = 1;
            printf("@@");
            while(cmd->args[j] != NULL){
                printf("%s ",cmd->args[j]);
                j++;
            }
            printf("\n");
    }
    else if(strcmp(cmd->args[0], "pwd") == 0){
            printf("@@%s\n", get_current_dir_name());
    }
    else if(strcmp(cmd->args[0], "mkdir") == 0){
            if(cmd->args[1] != NULL){
                if(mkdir(cmd->args[1],S_IRWXO|S_IRWXG|S_IRWXU )<0)
                {
                    printf("create file %s failure!\n",cmd->args[1]);
                    //exit(EXIT_FAILURE);
                }   
                else
                {
                     printf("create file %s success!\n",cmd->args[1]);
                }
            }
            else{
                printf("there is no file name\n");
            }
    }
    else{ //Íâ²¿ÃüÁî
        execOuterCmd(cmd);
    }
    
    //ÊÍ·Å½á¹¹Ìå¿Õ¼ä
    for(i = 0; cmd->args[i] != NULL; i++){
        free(cmd->args[i]);
        free(cmd->input);
        free(cmd->output);
        //free(cmd);
    }
    free(cmd);
}

/*******************************************************
                     ÃüÁîÖ´ÐÐ½Ó¿Ú
********************************************************/
void execute(){
    SimpleCmd *cmd = handleSimpleCmdStr(0, strlen(inputBuff));
    if(cmd->pipe)
    {
        SimpleCmd *cmd2=handleSimpleCmdStr(cmd->index, strlen(inputBuff));
        execPipe(cmd,cmd2);
    }
    else
        execSimpleCmd(cmd);
}
