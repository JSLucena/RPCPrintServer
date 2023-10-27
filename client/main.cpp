
/*===================================================================
Authentication Lab.
Lecture : 02239 - Data Security Fall 23
Author : Joaquim Siqueira Lucena
Last Updated : 2023-10-26

For this project we are using a simple RPC implementation. It should function a bit similar to the Java RMI, I believe.

I decided to do it in C++ because I have no experience using Java, and my group members were not being very cooperative about the project
so I ended up implementing alone. Because of this, I will do my best to explain my code in a way that makes it easy to understand.

===================================================================
*/



// STD
#include <cstdlib>
#include <iostream>
#include <vector>
// NANORPC
#include <nanorpc/https/easy.h>

// THIS
#include "common/ssl_context.h"

int main()
{
    std::vector<std::string> queue = {""};
    std::string command = "";
    std::string result;
    try
    {
        //We are using SSL to create a secure channel with the server. The certificate generation is done using OpenSSL,
        // and the channel is created using the boost::asio SSL implementation
        auto context = prepare_ssl_context("cert.pem", "key.pem", "dh.pem"); 

        //This function connects to our server through HTTP/HTTPS. I belive it should be similar to the naming.lookup from RMI
        auto client = nanorpc::https::easy::make_client(std::move(context), "localhost", "55555", 8, "/api/");

    //our client has a CLI interface
    while(command != "exit")
    {

        std::cin >> command; //here we get the command and depending on it we invoke different remote procedures

        if(command == "print")
        {
            std::string printer, filename;

            std::cin >> filename;
            std::cin >> printer;
            std::cin.clear(); //clear garbage from standard input


            //this function should be similar to calling server.print(filename,printer) on RMI
            result = client.call("print", filename, printer ).as<std::string>(); 
            if(result == "-1")
                std::cout << "Server is NOT running, execute the start command first" << std::endl;
            else
                std::cout << "Added file " << filename << " to " << printer << " queue"<< std::endl;
            
        }
        else if(command == "queue")
        {
           std::string printer;
           std::cin >> printer;
           std::cin.clear(); //clear garbage from standard input


           //this function should be similar to calling server.queue()
           result = client.call("queue",printer ).as<std::string>();
           if(result == "-1")
                std::cout << "Server is NOT running, execute the start command first" << std::endl;
            else
                std::cout << result << std::endl;
        }
        else if(command == "topqueue")
        {
            std::string printer;
            std::string id;
            std::cin >> printer;
            std::cin >> id;
            result = client.call("topqueue",printer,id ).as<std::string>();

            if(result == "0")
                std::cout << "Changed job " << id << " succesfully"<< std::endl;
            else if(result == "-1")
                std::cout << "Server is NOT running, execute the start command first" << std::endl;
            else
                 std::cout << "job " << id << " does not exist"<< std::endl;
        }

        else if(command == "start")
        {
            result = client.call("start").as<std::string>();
            std::cout << "Started server" << std::endl;
        }

        else if(command == "stop")
        {
            result = client.call("stop").as<std::string>();
            std::cout << "Stopped server" << std::endl;
        }

        else if(command == "restart")
        {
            result = client.call("restart").as<std::string>();
            std::cout << "Restarted server" << std::endl;
        }

        else if(command == "status")
        {
            std::string printer;
            std::cin >> printer;
            result = client.call("status",printer ).as<std::string>();
            if(result == "-1")
                std::cout << "Server is NOT running, execute the start command first" << std::endl;
            else
                std::cout << "printer " << printer << ":" << result << std::endl;
        }

        else if(command == "readconfig" )
        {
            std::string parameter;
            std::cin >> parameter;
            result = client.call("readconfig",parameter ).as<std::string>();
            if(result == "-1")
                std::cout << "Server is NOT running, execute the start command first" << std::endl;
            else
                std::cout << result << std::endl;
        }

        else if(command == "setconfig")
        {
            std::string parameter;
            std::string value;
            std::cin >> parameter;
            std::cin >> value;
            result = client.call("readconfig",parameter, value ).as<std::string>();
            if(result == "-1")
                std::cout << "Server is NOT running, execute the start command first" << std::endl;
            else
                std::cout << "Parameter set succesfully" << std::endl;
        }
        else
            std::cout << "unknown command" << std::endl;
    }


    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << nanorpc::core::exception::to_string(e) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
