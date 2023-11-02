/*===================================================================
Authentication Lab.
Lecture : 02239 - Data Security Fall 23
Author : Joaquim Siqueira Lucena
Last Updated : 2023-10-31

For this project we are using a simple RPC implementation. It should function a bit similar to the Java RMI, I believe.

I decided to do it in C++ because I have no experience using Java, and my group members were not being very cooperative about the project
so I ended up implementing alone. Because of this, I will do my best to explain my code in a way that makes it easy to understand.

As the lab focuses on the authentication and password part, I am not putting focus on input sanitization
and other checks. For example the print server does not check if the print exists, so if I put an unknown name it will crash(probably).
Also because of this, we didnt put too much attention on the client interface, it is just a crude CLI.

Used Libraries:

RPClib
Crypto++


Return values:
0 - success
-1 - Server is not running
-2 - Not authenticated
1 - something went wrong
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
std::vector<std::string> session_tokens; //vector that holds all session tokens in use
std::ofstream outfile;
std::ifstream infile;

std::string myText;
bool running = false;
int jobID = 0;

int salt;

//==========================================================================
/*
*string split(string line)
*
*@brief splits any string
*@detail this function splits strings on whitespaces, and stores them on an STL vector.
*@param A line with strings to split, in our case they come from our password file
*@return vector containing all substrings
*
*/
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

/*
*string SHA256HashString(string msg, string salt)
*
*@brief hashes a message+salt and returns it
*@detail this function hashes a message using the specified salt and the SHA-256 cryptographic hash function
*At the end we encode the digest with Base64 to store it in a file later
*@param msg is our message to encrypt, salt is the salt we want to apply
*@return string containing our hash encoded using Base64
*
*/
std::string SHA256HashString(std::string msg, std::string salt)
{
    std::string digest;
    CryptoPP::SHA256 hash;
    hash.Update((const uint8_t*)msg.data(), msg.size()); //first we hash the password itself
    hash.Update((const uint8_t*)salt.data(), salt.size()); //and then we hash the salt on top of it
    digest.resize(hash.DigestSize());
    hash.Final((uint8_t*)&digest[0]); //save it into our digest

    std::string encoded;
    CryptoPP::StringSource(digest, true, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(encoded))); //encode the digest using Base64 to save it later
    return encoded;
}
/*
*bool find_token(string token)
*
*@brief find token inside session_tokens vector
*@detail this function iterates through the session_tokens vector to find received token exists
*@param token is the token we received from our client
*@return true if found, false if not
*
*/
bool find_token(std::string token) //helper function to find a token inside our session token list
{
    std::cout << token << " ";
    for(auto i : session_tokens)
    {
        if(i == token)
            return true;
    }
    return false;
}
//---------------------------------------------------------



/*
*string print(string filename, string printer, string token)
*
*@brief queues requested file to print on requested printer
*@detail this function first checks if the token the client sent is valid, if yes, a print job of filename
*is queued on printer, adds an ID to the job, and the server sets the printer status to PRINTING
*@param filename is the name of the file we want to print, printer is the name of the printer, token is the session token to verify authentication
*@return 0 if success, -1 if server not running, -2 if not authenticated
*
*/
std::string print(std::string filename, std::string printer, std::string token)
{
    if(find_token(token)) //before doing anything we check if the token sent is the same as the one the server created
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
    return "-2";  //if we are not authenticated               
}

/*
*string queue(string printer, string token)
*
*@brief fetches and sends queue of requested printer to the client
*@detail this function first checks if the token the client sent is valid, if yes, it appends
*all jobs and their IDs inside printer queue and returns them to client
*@param printer is the name of the printer, token is the session token to verify authentication
*@return queue if success, -1 if server not running, -2 if not authenticated
*
*/
std::string queue(std::string printer, std::string token)
{
    if(find_token(token)) //before doing anything we check if the token sent is the same as the one the server created
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

/*
*string topqueue(string printer, string id,string token)
*
*@brief sends job with request id to the top of requested printer queue
*@detail this function first checks if the token the client sent is valid, if yes, it finds
*the job with id inside printer queue, inserts a copy of it on the beginning of the queue vector
*and then removes the original from the queue
*@param printer is the name of the printer, id is the id of the job we want to move,token is the session token to verify authentication
*@return 0 if success, -1 if server not running, -2 if not authenticated, 1 if id not found
*
*/
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
                    queueMap[printer].insert(queueMap[printer].begin(),queueMap[printer][i]); //the copy
                    queueMap[printer].erase(queueMap[printer].begin() + i+1); //removing old one
                    std::cout << "Moved job to the top of queue" << std::endl;
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


/*
*string start(string token)
*
*@brief starts server
*@detail this function first checks if the token the client sent is valid, if yes, it changes running to true
*@param token is the session token to verify authentication
*@return 0 if success, -2 if not authenticated
*
*/
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

/*
*string stop(string token)
*
*@brief starts server
*@detail this function first checks if the token the client sent is valid, if yes, it changes running to false
*@param token is the session token to verify authentication
*@return 0 if success, -2 if not authenticated
*
*/
std::string stop(std::string token)
{   if(find_token(token))
    {
        running = false;
        std::cout << "Stopping Print Server" << std::endl;
        return "0";
    }
    return "-2";
}

/*
*string restart(string token)
*
*@brief starts server
*@detail this function first checks if the token the client sent is valid, if yes, it changes running to true,
*clears the queue for each printer and resets the status for each printer
*@param token is the session token to verify authentication
*@return 0 if success, -2 if not authenticated
*
*/
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

/*
*string status(string printer, string token)
*
*@brief fetches requested printer status and sends it to client
*@detail this function first checks if the token the client sent is valid, if yes,
*it finds the printer status through a dictionary and sends it back to client
*@param printer is the name of the requested printer, token is the session token to verify authentication
*@return status if success, -1 if server not running, -2 if not authenticated
*
*/
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

/*
*string readconfig(string parameter, string token)
*
*@brief fetches requested server parameter and sends it to client
*@detail this function first checks if the token the client sent is valid, if yes,
*it finds the parameter through a dictionary and sends it back to client
*@param parameter is the name of the requested server parameter, token is the session token to verify authentication
*@return parameter if success, -1 if server not running, -2 if not authenticated
*
*/
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

/*
*string setconfig(string parameter, string value, string token)
*
*@brief sets parameter with new value from client
*@detail this function first checks if the token the client sent is valid, if yes,
*it finds the parameter through a dictionary and sets if to the new value
*@param parameter is the name of the requested server parameter, value is the new value,token is the session token to verify authentication
*@return 0 if success, -1 if server not running, -2 if not authenticated
*
*/
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

/*IMPORTANT
*string authenticate(string username, string password)
*
*@brief authenticate client, generate token and send it back
*@detail this function reads information from our password file(format for each user is: "user salt hash", one per line), 
*if it finds the username wanting to authenticate, it calculates the hash of the received password, using the same salt used
*on that user's account creation. If both match the server generates a random number and a new salt. It hashes the token,
*adds it to the session_tokens list and sends it back to the client. The token is encoded using Base64
*@param username is the username of the user that wants to connect, password is the password for that user's account
*@return session token if success, -2 if the authentication failed(e.g. wrong password)
*
*/
std::string authenticate(std::string username, std::string password)
{
    std::string user,hash, given_hash;
    std::cout << "Authentication Requested by " << username << std::endl;
    infile.open("pass"); //this is the file that stores our users credentials
    while (infile.good()) //while we are not at EOF
    {
        //our format is :user salt hash
        //for each user
        infile >> user;
        infile >> salt;
        infile >> hash;
        
        if(user == username) //if we find the user trying to authenticate
        {
            //we hash the typed password with the same hash used when adding the user to the system
            given_hash = SHA256HashString(password,std::to_string(salt));
            given_hash.pop_back(); //this removes a /n that was being added at the end
            if(hash == given_hash) //if they match means the combination user-password is correct
            {
                infile.close();
                int token = random(); //generate a random number
                salt = random(); //generate a random salt
                std::string hashed_token;
                hashed_token = SHA256HashString(std::to_string(token),std::to_string(salt)); //hash both the token number and the new salt
                hashed_token.pop_back(); //remove the /n from the resulting hash
                session_tokens.push_back(hashed_token); //and add it to our session tokens, this hash is going to be our session token from now on
                return hashed_token;
            }
            else
            {
                infile.close();
                return "-2";
            }
        }
   

    }
    infile.close();
    return "-2";
}
/*IMPORTANT
*string remove_token(string token)
*
*@brief remove token from active session tokens list
*@detail this function is called after the users call the exit CLI command, as they are closing the client,  
*the server receives the session token used by them, finds and erases them from the session_tokens vector
*This means that when the user initializes the client program the next time, they will have to authenticate again
*@param token is the session token from the user calling the procedure
*@return 0
*
*/
std::string remove_token(std::string token)
{
    session_tokens.erase(std::remove(session_tokens.begin(), session_tokens.end(), token), session_tokens.end());
    return "0";
}
int main() {
    rpc::server srv(PORT);


    srand(0); //This is just for testing, our random seed is always the same

    ///////////////////////////Population of Data for testing/////////////////
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
    ///////////////////////////Population of Data for testing/////////////////


    ////////Binding of functions that our server provides
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
