/*===================================================================
Authentication Lab.
Lecture : 02239 - Data Security Fall 23
Author : Joaquim Siqueira Lucena
Last Updated : 2023-11-15

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
-3 - No permission to execute requested service
1 - something went wrong

Access Control Assignment:

For the ACL version the permission list file has a simple format:
user permissions
user2 permissions
....
Permission codes are specified below:
p - print
q - queue
t - topqueue
r - restart
s - start/stop
c - setconfig
v - readconfig
x - getstatus
- - empty

For access control we load the file when the server starts or restarts, and the control is done by checking the token, to see to whom it pertains, 
and searching the users permissions on the file.
===================================================================
*/


#include <iostream>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <chrono>
#include <ctime>

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
//std::vector<std::string> session_tokens; //vector that holds all session tokens in use
std::map<std::string, std::string> session_tokens; //map that holds tokens as keys, usernames as data
std::map<std::string, std::string> acl; //hashmaps to store users-permissions
std::ofstream outfile;
std::ifstream infile;

std::string myText;
bool running = false;
int jobID = 0;

int salt;

//==========================================================================
/*
*time_t get_timestamp()
*
*@brief get current time
*@detail this function gets the current server time to use it as a timestamp in logs.
*@param 
*@return time_t type
*
*/
std::time_t get_timestamp()
{
    const auto p1 = std::chrono::system_clock::now();
    return std::chrono::system_clock::to_time_t(p1);
}
/*
*string printable_timestamp()
*
*@brief makes time_t printable
*@detail this function summons get_timestamp() and transforms its output into a string for it to be printable.
*@param 
*@return string with timestamp
*
*/
std::string printable_timestamp()
{
    std::time_t t = get_timestamp();
    std::string s = std::ctime(&t);
    s.pop_back();
    return s;
}

/*IMPORTANT
*void read_acl()
*
*@brief reads the permission list file
*@detail this function first reads the permissionlist.acl file and
*It stores each pair in a hashmap where the key is the user, and the element are the permissions.
*@param 
*@return
*
*/
void read_acl()
{
    std::string user, permissions;
    infile.open("permissionlist.acl"); //this is the file that stores our users credentials
    while (infile.good()) //while we are not at EOF
    {
        infile >> user;
        infile >> permissions;
        acl[user] = permissions;
    }
}
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


/*IMPORTANT
*Bool access_control(string token, char operation)
*
*@brief returns true if user has access to service, false if not
*@detail This function uses the token-user hashmap to find the user inside the users-permissions hashmap using the current session-token
*After this, it check is the user has permission to access the services by looking for the requested character inside the permissions string.
*@param token is the session token, operation represents our requested operation following the defined convention
*@return true if access granted, false if access denied
*
*/
bool access_control(std::string token, char operation)
{

    if(acl[session_tokens[token]].find(operation) == -1)
    {
        return false;
    }
    return true;

}
/*
*bool verify_permission(string token)
*
*@brief find token inside session_tokens vector and later checks ACL permissions
*@detail this function iterates through the session_tokens vector to find received token exists,
*and if it exists we check is the user has access to that operation
*@param token is the token we received from our client
*@return true if found and user has access to that method, false if not
*
*/
bool verify_permission(std::string token, char operation) //helper function to find a token inside our session token list
{
    
    if (session_tokens.find(token) == session_tokens.end()) 
    {
        // not found
        return false;
    } else 
    {
        // found
        
        std::cout << printable_timestamp() << " "<<session_tokens[token] << " ";
        return access_control(token, operation);
    }
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
    std::string x = "print";
    if(verify_permission(token, 'p')) //before doing anything we check if the token sent is the same as the one the server created
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
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 'p'))
        return "-3";
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
    std::string x = "queue";
    if(verify_permission(token, 'q')) //before doing anything we check if the token sent is the same as the one the server created
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
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 'q'))
        return "-3";
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
    std::string x = "topqueue";
    if(verify_permission(token, 't'))
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
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 't'))
        return "-3";
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
    std::string x = "start";
    read_acl(); //we read the acl file when the server starts
    if(verify_permission(token, 's'))
    {
        running = true;
        
        std::cout << "Starting Print Server" << std::endl;
        return "0";
    }
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 's'))
        return "-3";
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
{
    std::string x = "stop"; 
    if(verify_permission(token, 's'))
    {
        running = false;
        std::cout << "Stopping Print Server" << std::endl;
        return "0";
    }
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 's'))
        return "-3";
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
    std::string x = "restart";
    if(verify_permission(token, 'r'))
    {
        running = true;
        read_acl(); //reread acl file        
        std::cout << "Restarting Print Server" << std::endl;
        for (auto i = queueMap.begin(); i != queueMap.end(); i++)
            i->second.clear(); //clear job queue for each printer
        for (auto i = statusMap.begin(); i != statusMap.end(); i++)
            i->second = "IDLE"; //clear status queue for each printer
        return "0";
    }
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 'r'))
        return "-3";
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
    std::string x = "status";
    if(verify_permission(token, 'x'))
    {
        if(running)
        {
            std::cout << "Status from " << printer << " requested" << std::endl;
            return statusMap[printer];
        }
        return std::to_string(-1);
    }
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 'x'))
        return "-3";
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
    std::string x = "readconfig";
    if(verify_permission(token, 'v'))
    {
        if(running)
        {
            std::cout << "Config " << parameter << " requested" << std::endl;
            return configs[parameter];
        }
        return std::to_string(-1);
    }
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 'v'))
        return "-3";
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
    std::string x = "setconfig";
    if(verify_permission(token, 'c'))
    {
        if(running)
        {
            std::cout << "Setting " << parameter << " from " << configs[parameter] << " to " << value << std::endl;
            configs[parameter] = value;
            return "0";
        }
        return "-1";
    }
    std::cout << "Unauthorized access on " << x << std::endl;
    if(!access_control(token, 'c'))
        return "-3";
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

    std::cout << printable_timestamp() << " " << "Authentication Requested by " << username << std::endl;
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
                session_tokens[hashed_token] = username;
               // session_tokens.push_back(hashed_token); //and add it to our session tokens, this hash is going to be our session token from now on
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
    std::cout << printable_timestamp() << " " << "Session closed by " << session_tokens[token] << std::endl;
    //session_tokens.erase(std::remove(session_tokens.begin(), session_tokens.end(), token), session_tokens.end());
    session_tokens.erase(token);
    return "0";
}
int main() {
    rpc::server srv(PORT);


    srand(0); //This is just for testing, our random seed is always the same for the results to be the same every time

    ///////////////////////////Population of Data for testing/////////////////
    queueMap["printer1"];
    queueMap["printer2"];
    queueMap["printer3"];

    statusMap["p1"] = "IDLE";
    statusMap["p2"] = "IDLE";
    statusMap["p3"] = "IDLE";
    
    configs["size"] = "A4";
    configs["quality"] ="medium";
    configs["color"] = "uncolored";
    configs["orientation"] = "portrait";

/*
    std::string aux;
    salt = random();
    aux = SHA256HashString("admin",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile.open("pass");
    outfile << "alice " << salt  << " " << aux << std::endl;

    salt = random();
    aux = SHA256HashString("12345",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "bob " << salt  << " " << aux << std::endl;

    salt = random();
    aux = SHA256HashString("cecilia99",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "cecilia " << salt  << " " << aux << std::endl;

    salt = random();
    aux = SHA256HashString("abcde",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "david " << salt  << " " << aux << std::endl;
 

    salt = random();
    aux = SHA256HashString("abcde",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "erica " << salt  << " " << aux << std::endl;


    salt = random();
    aux = SHA256HashString("freddy1337",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "fred " << salt  << " " << aux << std::endl;

    salt = random();
    aux = SHA256HashString("sionmain!",std::to_string(salt));
    aux.pop_back(); //remove \n
    outfile << "george " << salt  << " " << aux << std::endl;
    outfile.close();
    */
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
