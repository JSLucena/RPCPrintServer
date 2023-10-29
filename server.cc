/*===================================================================
Authentication Lab.
Lecture : 02239 - Data Security Fall 23
Author : Joaquim Siqueira Lucena
Last Updated : 2023-10-26

For this project we are using a simple RPC implementation. It should function a bit similar to the Java RMI, I believe.

I decided to do it in C++ because I have no experience using Java, and my group members were not being very cooperative about the project
so I ended up implementing alone. Because of this, I will do my best to explain my code in a way that makes it easy to understand.

As the lab focuses on the authentication and password part, I am not putting focus on input sanitization
and other checks. For example the print server does not check if the print exists, so if I put an unknown name it will crash(probably).
Also because of this, we didnt put too much attention on the client interface, it is just a crude CLI.

===================================================================
*/


#include <iostream>
#include <cstdlib>
#include <vector>
#include <fstream>

#include "rpc/server.h"
#include "rpc/this_handler.h"



#define PORT 55555

std::vector<std::string> split(std::string line)
{
    std::vector<std::string> words;
    std::stringstream ss(line);
    std::string word;
    while (std::getline(ss, word, ' '))
        words.push_back(word);

    return words;
  
}
//---------------------------------------------------------

std::map<std::string, std::vector<std::string>> queueMap; //a dictionary to hold the queue for each printer
std::map<std::string, std::string> statusMap; //dictionary to hold printer status
std::map<std::string, std::string> configs; //dictionary to hold server settings
std::map<std::string, std::string[2]> users;
std::ofstream outfile;
std::ifstream infile;

std::string myText;
bool running = false;
int jobID = 0;

int salt;

//print method, It prints on the console and adds to the printer queue
std::string print(std::string filename, std::string printer)
{
    if(running)
    {
        std::cout << "Printing " << filename << " on printer " << printer << std::endl;
        std::string formatting = std::to_string(jobID++) + " " + filename + "\n"; //this is just formatting the string
        queueMap[printer].push_back(formatting); //we store it in a vector
        statusMap[printer] = "PRINTING"; //and set the printer to printing status
        return "0"; 
    }
    return "-1"; //if the server is not running we return an error code
                    
}
//sends the queue to the client machine, which prints on the clients CLI.
std::string queue(std::string printer)
{
    if(running)
    {
        std::cout << "Fetching queue " << printer <<std::endl;
        std::string buffer = "";
        for(int i = 0; i < queueMap[printer].size();i++)
            buffer+= queueMap[printer][i]; //just contatenate all jobs
        return buffer;
    }
    std::string buffer = "-1"; 
    return buffer;

}
//puts job with the requested ID on top of the selected printer
 std::string topqueue(std::string printer, std::string id)
{
    if(running)
    {
        for(int i = 0; i < queueMap[printer].size();i++)
        {
            //to change priority we find the job we need, insert a copy of it on the beginning and delete the old one
            if(queueMap[printer][i].find(id) != -1)
            {
                queueMap[printer].insert(queueMap[printer].begin(),queueMap[printer][i]);
                queueMap[printer].erase(queueMap[printer].begin() + i+1);
                std::cout << "Moved job to the order of queue" << std::endl;
                return "0";
            }
            }
        std::cout << "ID not found for topqueue" << std::endl;   
        return "1";
    }
    return "-1";

}

std::string start()
{
    running = true;
    std::cout << "Starting Print Server" << std::endl;
    return "0";

}
std::string stop()
{
    running = false;
    std::cout << "Stopping Print Server" << std::endl;
    return "0";
}

std::string restart()
{
    running = true;
                   
    std::cout << "Restarting Print Server" << std::endl;
        for (auto i = queueMap.begin(); i != queueMap.end(); i++)
        i->second.clear(); //clear job queue for each printer
    return "0";
}
//returns requested config printer status
std::string status(std::string printer)
{
    if(running)
    {
        std::cout << "Status from " << printer << " requested" << std::endl;
        return statusMap[printer];
    }
    return std::to_string(-1);
}

//returns requested config parameter value
std::string readconfig(std::string parameter)
{
    if(running)
    {
        std::cout << "Config " << parameter << " requested" << std::endl;
        return configs[parameter];
    }
    return std::to_string(-1);
}
//sets config parameter to new value
std::string setconfig(std::string parameter, std::string value)
{
    if(running)
    {
        std::cout << "Setting " << parameter << " from " << configs[parameter] << " to " << value << std::endl;
        configs[parameter] = value;
        return "0";
    }
    return "-1";
}

int main() {
    rpc::server srv(PORT);


    srand(0);
    queueMap["p1"];
    queueMap["p2"];
    queueMap["p3"];

    statusMap["p1"] = "IDLE";
    statusMap["p2"] = "IDLE";
    statusMap["p3"] = "IDLE";
    
    configs["size"] = "A4";
    configs["quality"] ="medium";
    configs["color"] = "uncolored";
    configs["orientation"] = "portrait";

    srv.bind("print", &print);
    srv.bind("queue", &queue);
    srv.bind("topqueue", &topqueue);
    srv.bind("start", &start);
    srv.bind("stop", &stop);
    srv.bind("restart", &restart);
    srv.bind("status", &status);
    srv.bind("readconfig", &readconfig);
    srv.bind("setconfig", &setconfig);

    srv.run();

    return 0;
}
