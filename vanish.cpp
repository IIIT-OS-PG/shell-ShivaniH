/**
 * Author: Shivani Hanji
 * Roll number: 2019201016
 * 
 */
#include <stdio.h>
#include <iostream>
#include <limits>
#include <string.h>
#include <string>
#include <algorithm>
#include <map>

/*-------------------------------------
| POSIX header files              |
-------------------------------------*/
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <pwd.h>

//Refer: https://support.sas.com/documentation/onlinedoc/sasc/doc750/html/lr2/z2101586.htm

/*-------------------------------------
| Vani's header files              |
-------------------------------------*/
#include "vanish.hpp"
#include "utilities.hpp"
#include "background.hpp"
#include "redirection.hpp"
#include "piping.hpp"

using namespace std;

/*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       ENVIRONMENT SETUP        |
----------------------------------------------------------------------------------------------------------------------------------------*/
void envSetup(string &prompt, map<string, vector<string>> &mapRef, map<string, string> &mulMedRef)
{
    FILE *vanishrc = fopen(".vanishrc", "r+");
    vector<string> envVars;
    char line[500];
    while (1) 
    {
        if (fgets(line, 500, vanishrc) == NULL) break;
        envVars.push_back(line);
    }

    string varName, value;
    uid_t currentUser;
    currentUser = getuid();
    struct passwd *ptr = getpwuid(currentUser);
   
    for(int i = 0; i < envVars.size(); ++i)
    {
        string::size_type eqPos = envVars[i].find('=');
        varName = envVars[i].substr(0, eqPos-1);
        value = envVars[i].substr(eqPos+2);     //These magic numbers are cause of the spaces
        value.erase(remove(value.begin(), value.end(), '\n'), value.end());
        
        if( varName == "PS1")
        {
            prompt += value;
            //cout<<"value of prompt is "<<value<<" which is = "<<prompt<<"\n";        
        }
        if (varName == "USER")
        {
            if(ptr != NULL) value = ptr->pw_name;
            prompt = value + "@";
        }
        if(varName == "HOSTNAME")
        {
            char hostname[1024];
            gethostname(hostname, 1024);
            value = hostname;
            prompt += value;
        }
        if( varName == "HOME")
        {
            //cout<<"here?\n";
            if(ptr != NULL) value = ptr->pw_dir;
        }
        int x;
        int &refx = x;
        vector<string> alAndName = splitInput(x, (char*)varName.c_str());
        if( alAndName[0] == "alias" )
        {
            int numFToks = 0;
            int &numFToksRef = numFToks;
            vector<string> fakenomToks = splitInput(numFToksRef, (char*)value.c_str());
            mapRef.insert(make_pair(alAndName[1], fakenomToks));
            
            /*cout<<"Printing map\n";
            auto it = mapRef.begin();
            for(int i = 0; i < fakenomToks.size(); ++i)
            {
                cout<<it->second[i];
            }
            */
        }
        if( varName == "MEDIA" )
        {
            value.erase(remove(value.begin(), value.end(), '['), value.end());
            value.erase(remove(value.begin(), value.end(), ']'), value.end());
            value.erase(remove(value.begin(), value.end(), ','), value.end());
            int numMeds = 0;
            int &numMedsRef = numMeds;
            vector<string> oneMedia = splitInput(numMedsRef, (char*)value.c_str());
            mulMedRef.insert(make_pair(oneMedia[2],oneMedia[1]));   //{file extension: apppath}
        }
        //cout<<"varname is "<<varName<<" and its value is"<<value<<"\n";
        if(alAndName[0] != "alias" && varName != "MEDIA") setenv(varName.c_str(), value.c_str(), 1);
        //cout<<"Fine till here\n";
    }
    prompt += " ";
    fclose(vanishrc);

}

/*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       MAIN        |
----------------------------------------------------------------------------------------------------------------------------------------*/

int main()
{
    //BUNCH OF VARIABLE DECLARATIONS

    string vaniPrompt;

    int shmFlags = IPC_CREAT | SHM_PERM;

    vector<backgroundProc> bg;

    string &promptRef = vaniPrompt;

    map<string, vector<string>> akas;
    map<string, vector<string>> &akaRef = akas;

    map<string, string> mulMed;
    map<string, string> &mulMedRef = mulMed;

    map<string, string> alarmies;
    map<string, string> &alarmRef = alarmies;

    string oldPWD = "";  

    envSetup(promptRef, akaRef, mulMedRef);

    int childStatus = 0;
    int &childRef = childStatus;

    bool rootMode = false;

    FILE *hist = fopen("history.txt","r+");
    int histBuffSize = atoi(getenv("HISTSIZE"))+1;
    //cout<<"histBuff limit is "<<histBuffSize<<"\n";
    int numHistLines;
    char *numHistLinesStr = (char*)malloc(sizeof(char)*histBuffSize);
    fgets(numHistLinesStr, histBuffSize, hist);  
    numHistLines = atoi(numHistLinesStr);
    //cout<<"num history lines is "<<numHistLines<<"\n";

    /*
    struct termios term;

    tcgetattr (STDIN_FILENO, & term);
    term.c_lflag &= ~(ICANON);
    tcsetattr (STDIN_FILENO, TCSAFLUSH, & term);
    */

    //cout<<"pid of vanish = "<<getpid()<<" and group id of vanish = "<<getpgid(getpid());
    while(true)
    {
        char *command = (char*)malloc(sizeof(char)*(INPUT_MAX));

        int *fd;    //For piping in bg to fg

        int sharedMemSegID = shmget(SHM_KEY, MAXY_TOKENS*MAX_TOKEN_SIZE, shmFlags);
        if(sharedMemSegID < 0)
        {
            cout<<"Could not create shared memory segment!\n";
            exit(EXIT_FAILURE);
        }

        //cout<<"The shmid parent has is "<<sharedMemSegID<<"\n";

        char **commandWords;
        commandWords = (char**)shmat(sharedMemSegID, NULL, 0);   //NULL -> The system will automatically look for an address to attach the segment

        if(commandWords == (char**)-1)
        {
            cout<<"Could not attach shared memory segment!\n";
            exit(EXIT_FAILURE);
        }

        /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       ACCEPTING USER INPUT        |
----------------------------------------------------------------------------------------------------------------------------------------*/
        cout<<vaniPrompt;
        
        int index = 0;
        char input; 
        //cin>>input;
        input = cin.get();
        //int testVar = 7;
        
        while(input != '\n' && input != '\t')
        {
            command[index] = input;
            ++index;
            //cin>>input;
            input = cin.get();
            //std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            //cout<<"Just stuck here\n";
            //cout<<"Came back here\n";
        }
        command[index] = '\0';
        //cout<<"Really took the input\n";

        //fputs(command, hist);
        if(numHistLines >= histBuffSize-1)
        {
            fclose(hist);
            hist = fopen("history.txt", "w");
            char *tempPointer = (char*)malloc(sizeof(char)*histBuffSize);
            fgets(tempPointer, histBuffSize, hist);
        }
        string cppcommand = command;
        //fprintf(hist, "%s\n",cppcommand.c_str());
        fputs(cppcommand.c_str(), hist);
        //cout<<"going to write "<<cppcommand.c_str()<<"\n";
        fputs("\n",hist);
        //cout<<"ok till here -- fprintf\n";
        ++numHistLines;

        int numTokens = 0;
        int &ref = numTokens;
        //char **commandWords = splitInput(ref, command);
        vector<string> toks;
        toks = splitInput(ref, command);

        if( strncmp("su",command,2) == 0 || strncmp("su root",command,7) == 0 || strncmp("sudo su",command,7) == 0 )
        {
            rootMode = true;
            setenv("PS1","#",1);
        }

        if(checkThisInTokens("open"))
        {
            string::size_type dotPos = toks[1].find('.');
            string extension = toks[1].substr(dotPos);
            //cout<<"The extension is "<<extension<<"\n";
            if(mulMed.find(extension) != mulMed.end())
            {
                toks[0] = mulMed[extension];
            }
            //cout<<"Opening "<<toks[1]<<" using "<<mulMed[extension]<<"\n";
        }

        if(checkThisInTokens("$$"))
        {
            for(int i = 0; i < numTokens; ++i)
            {
                if(toks[i] == "$$")
                {
                    int procId = getpid();
                    toks[i] = to_string(procId); 
                }
            }
        }

        if(checkThisInTokens("$?"))
        {
            for(int i = 0; i < numTokens; ++i)
            {
                if(toks[i] == "$?")
                {
                    toks[i] = to_string(childStatus);
                }
            }
        }

        for(int i = 0; i < numTokens; ++i)
        {
            if(toks[i][0]=='$')
            {
                string envy = toks[i].substr(1);
                toks[i] = getenv(envy.c_str());
            }
        }

        if(checkThisInTokens("~"))
        {
            for(int i = 0; i < numTokens; ++i)
            {
                if(toks[i] == "~")
                {
                    string homeDir = getenv("HOME");
                    toks[i] = homeDir; 
                }
            }
        }

        for(int i = 0; i < numTokens; ++i)
        {
            if (akas.find(toks[i]) != akas.end())
            {
                //cout<<"This worked for alias = "<<toks[i]<<"\n";
                vector<string> valsToPut = akas[toks[i]];
                //cout<<"vals to put size = "<<valsToPut.size()<<"\n";
                for(int j = 0; j < valsToPut.size(); ++j)
                {
                    toks.insert(toks.begin()+i+j, valsToPut[j]);
                    if(j > 0) ++numTokens;
                }

                /*cout<<"updated tokens = \n";
                for(int j = 0; j < numTokens; ++j)
                {
                    cout<<toks[j]<<"\n";
                }
                */
            }
        }

        if(checkThisInTokens("alias"))
        {
            vector<string> pseuNoms;
            string pseudonym = toks[1];
            string realComm;
            for(int k = 3; k < numTokens; ++k) realComm = realComm + toks[k] + " ";  //has single quotes, get rid of them
            //cout<<"realComm is "<<realComm<<"\n";
            realComm.erase(remove(realComm.begin(), realComm.end(), '\''), realComm.end());
            int numPseus;
            int &numPseusRef = numPseus;
            pseuNoms = splitInput(numPseusRef, (char*)realComm.c_str());
            akas.insert(make_pair(pseudonym, pseuNoms));
        }
        else if(checkThisInTokens("&"))
        {
            /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       BACKGROUND PROCESS        |
----------------------------------------------------------------------------------------------------------------------------------------*/

            for(int i = 0; i < numTokens; ++i)
            {
                if(toks[i] != "&") commandWords[i] = (char*)toks[i].c_str();
            }
            
            commandWords[numTokens-1] = NULL;

            fd = (int*)malloc(sizeof(int)*2);

            backgroundDealer(bg, fd);

            int shMSID = shmget((key_t)1098, sizeof(int), SHM_PERM);
            if(shMSID < 0)
            {
                cout<<"Could not fetch shared memory segment!\n";
                exit(EXIT_FAILURE);
            }

            int *saveSTDOUT = (int*)shmat(shMSID, NULL, 0);

            if(saveSTDOUT == (int*)-1)
            {
                cout<<"Could not attach shared memory segment!\n";
                exit(EXIT_FAILURE);
            }

            bg[bg.size()-1].setSTDOUT(*saveSTDOUT);
        
            //cout<<"I'm done here\n";
        }
        else if(checkThisInTokens("fg"))
        {
            /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       BACK TO FOREGROUND        |
----------------------------------------------------------------------------------------------------------------------------------------*/
            
            signal(SIGTTOU, SIG_IGN);
            //int childStatus;
            pid_t deadOrAlive;
            int i;
            for(i = 0; i < bg.size(); ++i)
            {
                deadOrAlive = bg[i].getBgId();
                cout<<"Checking process with pid = "<<deadOrAlive<<"\n";
                if( waitpid(deadOrAlive, &childStatus, WNOHANG) == 0 )    //this one's still running!
                {              
                    cout<<"I'm alive!\n";
                    break;
                }
            }
            if(i == bg.size())
            {
                cout<<"There is no process currently running in the background\n";
            }
            else {
                if(tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
                {
                    cout<<"Could not change background process' group to foreground group\n";
                }

                /**
                 * BAD! ATTEMPT WITH PIPE
                 * 
                if(dup2(STDOUT_FILENO, fd[0]) == -1)
                {
                    cout<<"Could not resume printing background process' output\n";
                }
                */

                /**
                 * ATTEMPT WITH fd in proc folder --- THIS seems to be the most likely to work!
                 * 
                 * 
                string filePath = "/proc/"+to_string(deadOrAlive)+"/fd/1";
                int outFD = open(filePath.c_str(), O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
                cout<<"outFD is "<<outFD<<"\n";
                if(outFD == -1)
                {
                    cout<<"Couldn't get access to background process' output\n";
                }

                char *buff;
                while(read(outFD, buff, 1) != 0)
                {
                    write(STDOUT_FILENO, buff, 1);
                }
                */
                            
                cout<<"STDOUT of background process is "<<bg[i].getSTDOUT()<<"\n";

                if(dup2(bg[i].getSTDOUT(), STDOUT_FILENO) == -1)
                {
                    cout<<"Could not resume printing background process' output\n";
                }
                
            }
        }
        else if(checkThisInTokens("cd"))
        {
            //cout<<"Ok till here\n";
            //cout<<"path is "<<toks[1]<<"\n";
            if(toks[1] == "-")
            {
                if(oldPWD == "")
                {
                    cout<<"You have not changed to any directory before this\n";
                }
                else {
                    toks[1] = oldPWD;
                }
            }
            if (chdir(toks[1].c_str()) != 0) 
            {
                cout<<"Please specify a valid path for cd\n";
            }
            else {
                oldPWD = toks[1];
            }
        }
        else if(checkThisInTokens("exit"))
        {
            if(rootMode)
            {
                rootMode = false;
                setenv("PS1","$",1);
            }
            fclose(hist);
            FILE *histUpdate = fopen("history.txt", "w");
            numHistLinesStr = (char*)to_string(numHistLines).c_str();
            fputs(numHistLinesStr, histUpdate);
            fclose(histUpdate);

            return 0;
            //cout<<"ok till here\n";
        }
        else if(checkThisInTokens(">"))
        {
            /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       OUTPUT REDIRECTION        |
----------------------------------------------------------------------------------------------------------------------------------------*/
            int i;
            for(i = 0; i < numTokens && toks[i] != ">"; ++i)
            {
                commandWords[i] = (char*)toks[i].c_str();
            }

            commandWords[i] = NULL;
            outputRedirection(toks, i, ">", "r+", childRef);
        }
        else if(checkThisInTokens(">>"))
        {
            /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       OUTPUT REDIRECTION APPEND        |
----------------------------------------------------------------------------------------------------------------------------------------*/
            int i;
            for(i = 0; i < numTokens && toks[i] != ">>"; ++i)
            {
                commandWords[i] = (char*)toks[i].c_str();
            }

            commandWords[i] = NULL;
            outputRedirection(toks, i, ">>", "a", childRef);
        }
        else if(checkThisInTokens("|"))
        {
            /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |       PIPES       |
----------------------------------------------------------------------------------------------------------------------------------------*/
            int i, j, k;
            //int pipeFD[2];
            //pipe(pipeFD);
            //cout<<"pipefd[0] = "<<pipeFD[0]<<" "<<pipeFD[1]<<"\n";
            //int (&piperef)[2] = pipeFD;
            bool first = true;
            bool last = false;
            
            int numCommands = 1;
            for(i = 0; i < numTokens; ++i)
            {
                if(toks[i] == "|")
                {
                    ++numCommands;
                }
            }   

            int pipeFD[numCommands][2]; //read from current, write to next

            //test with: ls | grep c | grep v | wc
            
            int (*pipePtr)[2] = pipeFD;
            for(i = 0; i < numCommands; ++i)
            {
                pipe(pipeFD[i]);
                //cout<<"pipe[0] =  "<<pipeFD[i][0]<<" pipe[1] = "<<pipeFD[i][1]<<"\n";
            }

            //int (&piperef)[][2] = pipeFD;
            

            for(i = 0, j = 0, k = 0; i < numTokens; ++i)
            {
                if(toks[i] != "|" )
                {
                    commandWords[k] = (char*)toks[i].c_str();
                    //cout<<"Stored "<<commandWords[k]<<" now\n";
                    ++k;
                }
                else
                {
                    commandWords[k] = NULL;
                    /*
                    cout<<"position k = "<<k<<" was set to NULL\n";
                    cout<<"There's a pipe now\n";
                    */
                    
                    k = 0;
                    piping(j, numCommands, pipePtr ,first, last, childRef);
                    ++j;
                    //cout<<"I'm gonna call piping for "<<commandWords[0]<<" now\n";
                    first = false;
                }
            }
            //for the final command
            last = true;
            commandWords[k] = NULL;
            k = 0;
            //cout<<"I'm gonna call piping for "<<commandWords[0]<<" now\n";
            piping(j, numCommands, pipePtr ,first, last, childRef);
            //cout<<"I'm out, bye bye\n";
        }
        else {
            /*----------------------------------------------------------------------------------------------------------------------------------------
                                            |      NORMAL CASE        |
----------------------------------------------------------------------------------------------------------------------------------------*/

            for(int i = 0; i < numTokens; ++i)
            {
                //memcpy(commandWords[i],(char*)toks[i].c_str(), toks[i].length());
                commandWords[i] = (char*)toks[i].c_str();
            }
            
            //cout<<"number of tokens is "<<numTokens<<"\n";
            commandWords[numTokens] = NULL;
            ++numTokens;

            pid_t childPid = fork();
            if(childPid > 0)
            {
                // I am the parent

                //cout<<"Waiting for my child. . . \n";
                //pid_t waitRet = wait(childStatus);
                pid_t waitRet = waitpid(childPid, &childStatus, 0);
                
                /*
                cout<<"waitRet is "<<waitRet<<"\n";
                cout<<"childpid is "<<childPid<<"\n";
                */

                if(waitRet != childPid)
                {
                    //cout<<"Could not execute command: "<<commandWords[0]<<endl;
                    cout<<"Could not execute command\n";
                }
            }
            else if(childPid == 0)
            {
                //I am the child
                
                int shmid = shmget(SHM_KEY, MAXY_TOKENS*MAX_TOKEN_SIZE, SHM_PERM);
                if(shmid < 0)
                {
                    cout<<"Could not get shared memory segment!\n";
                    exit(EXIT_FAILURE);
                }

                char **commandWordZ = (char**)shmat(shmid, NULL, 0);

                if(commandWordZ == (char**)-1)
                {
                    cout<<"Could not attach shared memory segment!\n";
                    exit(EXIT_FAILURE);
                }

                //cout<<"I can read testVar! : "<<testVar<<"\n";
                //cout<<"I am running "<<commandWordZ[0]<<"\n";
                //int exitStat = execvp(commandWords[0],commandWords);
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
        }

        free(command);
        fflush(stdout);
    }
    fclose(hist);
    FILE *histUpdate = fopen("history.txt", "w");
    numHistLinesStr = (char*)to_string(numHistLines).c_str();
    fputs(numHistLinesStr, histUpdate);
    fclose(histUpdate);

    return 0;
}