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

//CRYPTO++
#include "cryptopp/sha.h"
#include "cryptopp/filters.h"
#include "cryptopp/base64.h"
#include "cryptopp/cryptlib.h"


#define PORT 55555

std::map<std::string, std::vector<std::string>> queueMap; //a dictionary to hold the queue for each printer
std::map<std::string, std::string> statusMap; //dictionary to hold printer status
std::map<std::string, std::string> configs; //dictionary to hold server settings
//std::map<std::string, std::string> users; //dictionary that holds authenticated sesion tokens
std::vector<std::string> session_tokens;
std::ofstream outfile;
std::ifstream infile;

std::string myText;
bool running = false;
int jobID = 0;

int salt;

//==========================================================================
std::vector<std::string> split(std::string line)
{
    std::vector<std::string> words;
    std::stringstream ss(line);
    std::string word;
    while (std::getline(ss, word, ' '))
    {
        words.push_back(word);
    }
        
    return words;
  
}

std::string SHA256HashString(std::string msg, std::string salt)
{
    std::string digest;
   // CryptoPP::Base64Encoder encoder(new StringSink(digest));


    CryptoPP::SHA256 hash;
    hash.Update((const uint8_t*)msg.data(), msg.size());
    hash.Update((const uint8_t*)salt.data(), salt.size());
    digest.resize(hash.DigestSize());
    hash.Final((uint8_t*)&digest[0]);

   //d std::cout << "Message: " << msg << std::endl;
    std::string encoded;
    CryptoPP::StringSource(digest, true, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(encoded)));
    return encoded;
}

bool find_token(std::string token)
{
    for(auto i : session_tokens)
    {
        if(i == token)
            return true;
    }
    return false;
}
//---------------------------------------------------------



//print method, It prints on the console and adds to the printer queue
std::string print(std::string filename, std::string printer, std::string token)
{
    if(find_token(token))
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
    return "-2";                
}
//sends the queue to the client machine, which prints on the clients CLI.
std::string queue(std::string printer, std::string token)
{
    if(find_token(token))
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
    return "-2";

}
//puts job with the requested ID on top of the selected printer
 std::string topqueue(std::string printer, std::string id, std::string token)
{
    if(find_token(token))
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
    return "-2";

}

std::string start(std::string token)
{
    if(find_token(token))
    {
        running = true;
        std::cout << "Starting Print Server" << std::endl;
        return "0";
    }
    return "-2";

}
std::string stop(std::string token)
{   if(find_token(token))
    {
        running = false;
        std::cout << "Stopping Print Server" << std::endl;
        return "0";
    }
    return "-2";
}

std::string restart(std::string token)
{
    if(find_token(token))
    {
        running = true;
                        
        std::cout << "Restarting Print Server" << std::endl;
        for (auto i = queueMap.begin(); i != queueMap.end(); i++)
            i->second.clear(); //clear job queue for each printer
        for (auto i = statusMap.begin(); i != statusMap.end(); i++)
            i->second = "IDLE"; //clear status queue for each printer
        return "0";
    }
    return "-2";
}
//returns requested config printer status
std::string status(std::string printer, std::string token)
{
    if(find_token(token))
    {
        if(running)
        {
            std::cout << "Status from " << printer << " requested" << std::endl;
            return statusMap[printer];
        }
        return std::to_string(-1);
    }
    return "-2";
}

//returns requested config parameter value
std::string readconfig(std::string parameter, std::string token)
{
    if(find_token(token))
    {
        if(running)
        {
            std::cout << "Config " << parameter << " requested" << std::endl;
            return configs[parameter];
        }
        return std::to_string(-1);
    }
    return "-2";
    
}
//sets config parameter to new value
std::string setconfig(std::string parameter, std::string value, std::string token)
{
    if(find_token(token))
    {
        if(running)
        {
            std::cout << "Setting " << parameter << " from " << configs[parameter] << " to " << value << std::endl;
            configs[parameter] = value;
            return "0";
        }
        return "-1";
    }
    return "-2";
}

std::string authenticate(std::string username, std::string password)
{
    std::string user,hash, given_hash;
    std::cout << "Authentication Requested by " << username << std::endl;
    infile.open("pass");
    while (infile.good()) 
    {
        
        infile >> user;
        infile >> salt;
        infile >> hash;
        
        if(user == username)
        {
            given_hash = SHA256HashString(password,std::to_string(salt));
            given_hash.pop_back();
            if(hash == given_hash)
            {
                infile.close();
                int token = random();
                std::string hashed_token;
                hashed_token = SHA256HashString(std::to_string(token),std::to_string(token));
                hashed_token.pop_back();
                session_tokens.push_back(hashed_token);
                return hashed_token;
            }
            else
            {
                infile.close();
                return "2";
            }
        }
   

    }
    infile.close();
    return "2";
}
std::string remove_token(std::string token)
{
    session_tokens.erase(std::remove(session_tokens.begin(), session_tokens.end(), token), session_tokens.end());
    return "0";
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


    std::string aux;
    salt = random();
    aux = SHA256HashString("admin",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile.open("pass");
    outfile << "admin " << salt  << " " << aux << std::endl;

    salt = random();
    aux = SHA256HashString("12345",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "joca " << salt  << " " << aux << std::endl;

    outfile.close();

    srv.bind("print", &print);
    srv.bind("queue", &queue);
    srv.bind("topqueue", &topqueue);
    srv.bind("start", &start);
    srv.bind("stop", &stop);
    srv.bind("restart", &restart);
    srv.bind("status", &status);
    srv.bind("readconfig", &readconfig);
    srv.bind("setconfig", &setconfig);
    srv.bind("authenticate", &authenticate);
    srv.bind("remove_token", &remove_token);

    srv.run();

    return 0;
}
