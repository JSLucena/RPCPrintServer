#include <cstdlib>
#include <iostream>
#include <vector>
#include<string>



#include "rpc/client.h"
#include "rpc/rpc_error.h"
#define PORT 55555

void handle_return(std::string ret)
{
    if(ret == "-1")
        std::cout << "Server is NOT running, execute the start command first" << std::endl;
    else if(ret == "0")
        std::cout << "Operation Successful" << std::endl;
    else if(ret == "1")
        std::cout << "Oops something went wrong" << std::endl;
    else if(ret == "2")
        std::cout << "Wrong username-password combination" << std::endl;
    else if(ret == "-2")
        std::cout << "Authentication failed, please try again" << std::endl;
    else
        std::cout << ret << std::endl;
}



int main() {
    rpc::client client("localhost", PORT);//create client and connect it to the server
    std::vector<std::string> queue = {""};
    std::string command = "";
    std::string result, username, password;
    std::string token;

    std::cout << "enter username: ";
    std::cin >> username;
    std::cout << "enter password: ";
    std::cin >> password;

    //here we call the authentication function, the return is a token, or an error code if something went wrong
    result = client.call("authenticate",username,password).as<std::string>();

    token = result;
    while(command != "exit")
    {

        if(result == "-2") //if the authentication failed we try again
        {
            std::cout << "enter username: ";
            std::cin >> username;
            std::cout << "enter password: ";
            std::cin >> password;
            result = client.call("authenticate",username,password).as<std::string>();
            token = result;
        }

        std::cout << "$>";
        std::cin >> command; //here we get the command and depending on it we invoke different remote procedures

        if(command == "print")
        {
            std::string printer, filename;

            std::cin >> filename;
            std::cin >> printer;
            std::cin.clear(); //clear garbage from standard input
            
            //this function should be similar to calling server.print(filename,printer) on RMI
            result = client.call("print", filename, printer,token ).as<std::string>(); 
        }
        else if(command == "queue")
        {
           std::string printer;
           std::cin >> printer;
           std::cin.clear(); //clear garbage from standard input

           //this function should be similar to calling server.queue(printer) on RMI
           result = client.call("queue",printer,token ).as<std::string>();
        }
        else if(command == "topqueue")
        {
            std::string printer;
            std::string id;
            std::cin >> printer;
            std::cin >> id;
            //this function should be similar to calling server.topqueue(printer,id) on RMI
            result = client.call("topqueue",printer,id,token ).as<std::string>();
        }

        else if(command == "start")
        {
            //this function should be similar to calling server.start() on RMI
            result = client.call("start",token).as<std::string>();
        }

        else if(command == "stop")
        {
            //this function should be similar to calling server.stop() on RMI
            result = client.call("stop",token).as<std::string>();
        }

        else if(command == "restart")
        {
            //this function should be similar to calling server.restart() on RMI
            result = client.call("restart",token).as<std::string>();
        }

        else if(command == "status")
        {
            std::string printer;
            std::cin >> printer;
            //this function should be similar to calling server.status(printer) on RMI
            result = client.call("status",printer,token ).as<std::string>();
        }

        else if(command == "readconfig" )
        {
            std::string parameter;
            std::cin >> parameter;
            //this function should be similar to calling server.readconfig(parameter) on RMI
            result = client.call("readconfig",parameter,token ).as<std::string>();
        }

        else if(command == "setconfig")
        {
            std::string parameter;
            std::string value;
            std::cin >> parameter;
            std::cin >> value;
            //this function should be similar to calling server.setconfig(parameter, value) on RMI
            result = client.call("setconfig",parameter, value,token ).as<std::string>();
        }
        else
            std::cout << "unknown command" << std::endl;
        handle_return(result); //this function handles the return values from all procedures
        
    }
    //at the end of the session we call the remove_token procedure to remove our session token from the server list
    client.call("remove_token", token); 
    return 0;
}
