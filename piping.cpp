#include <stdio.h>
#include <iostream>
#include <limits>
#include <string.h>
#include <string>

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <vector>

#include "piping.hpp"

using namespace std;
//int numCommands;

void piping(int currentPipe, int numCommands, int (*pipePtr)[2], bool first, bool last, int &childStatus)
{
    int pipeFD[numCommands][2];
       
    pipeFD[currentPipe][0] = pipePtr[currentPipe][0];
    pipeFD[currentPipe+1][1] = pipePtr[currentPipe+1][1];

    //cout<<"in the parent piping, read end = "<<pipeFD[currentPipe][0]<<" and write end = "<<pipeFD[currentPipe+1][1]<<"\n";
    
    pid_t childPid = fork();
    if(childPid > 0)
    {
        // I am the parent

        //int childStatus;
        //pid_t waitRet = wait(childStatus);
        //cout<<"I'm now waiting for child "<<childPid<<"\n";
        if(first) close(pipeFD[currentPipe][0]);
        //if(first) close(pipeFD[currentPipe][1]);
        if(currentPipe+1 < numCommands) close(pipeFD[currentPipe+1][1]);    //close write end of next

        pid_t waitRet = waitpid(childPid, &childStatus, WUNTRACED);
        //cout<<"I'm done waiting for child "<<waitRet<<" its exit status is "<<childStatus<<"\n";
        /*
        cout<<"waitRet is "<<waitRet<<"\n";
        cout<<"childpid is "<<childPid<<"\n";
        */

        /*
        if(waitRet != childPid)
        {
            //cout<<"Could not execute command: "<<commandWords[0]<<endl;
            cout<<"Could not execute command\n";
        }
        */

        //cout<<"child's execution done\n";
    }
    else if(childPid == 0)
    {
        //I am the child

        //cout<<"in the child piping, read end = "<<pipeFD[0]<<" and write end = "<<pipeFD[1]<<"\n";

        if(!first)
        {
            if(dup2(pipeFD[currentPipe][0], STDIN_FILENO) == -1)  //read from pipe
            {
                cout<<"first dup2 is the problem\n";
            }
        }
       
        if(!last)
        {
            if(dup2(pipeFD[currentPipe+1][1], STDOUT_FILENO) == -1) //write to pipe
            {
                cout<<"second dup2 is the problem\n";
            }
        }

        //close(pipeFD[currentPipe][0]);
        if(first) close(pipeFD[currentPipe][0]);
        if(currentPipe+1 < numCommands) close(pipeFD[currentPipe+1][1]); 

        int shmid = shmget(SHM_KEY, MAXY_TOKENS*MAX_TOKEN_SIZE, SHM_PERM);
        if(shmid < 0)
        {
            cout<<"Could not get shared memory segment!\n";
            exit(EXIT_FAILURE);
        }
        //cout<<"The shmid child has is "<<shmid<<"\n";

        char **commandWordZ = (char**)shmat(shmid, NULL, 0);

        if(commandWordZ == (char**)-1)
        {
            cout<<"Could not attach shared memory segment!\n";
            exit(EXIT_FAILURE);
        }

        //cout<<"The command being processed now from child "<<getpid()<<" is "<<commandWordZ[0]<<"\n";
        /*
        if(commandWordZ[1] == NULL)
        {
            cout<<"commandWordZ[1] is NULL\n";
        }
        */
        int exitStat = execvp(commandWordZ[0],commandWordZ);
        //cout<<"exitstat from child is "<<exitStat<<"\n";

        if(exitStat == -1)
        {
            exit(-1);
        }
        else {
            exit(0);
        }      
    }
    else {
        cout<<"fork error!\n";
    }
}