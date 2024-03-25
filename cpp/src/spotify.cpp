//
// Created by hendrik on 3/24/24.
//

#include "spotify.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <string>


#include "utils/shared.h"


std::string execute_process(const char* cmd, const std::vector<std::string>& args) {
    std::stringstream fullCmd;
    fullCmd << cmd;

    for (const auto& arg : args) {
        fullCmd << " " << arg;
    }

    const auto out_file = string(std::tmpnam(nullptr));

    // Redirect output of the command appropriately
    std::ostringstream ss;
    ss << fullCmd.str() << " >" << out_file;
    cout << out_file << "\n";
    const auto finalCmd = ss.str();

    // Call the command
    const auto status = std::system(finalCmd.c_str());

    if(status != 0) {
        cout << "Status " << status << " \n";
        throw std::runtime_error("Command failed");
    }

    // Read the output from the files and remove them
    std::stringstream out_stream;

    std::ifstream file( out_file );
    if ( file )
    {
        cout << "Out stream \n";
        out_stream << file.rdbuf();
        file.close();

        // operations on the buffer...
    }

    std::remove(out_file.c_str());

    return out_stream.str();
}

void Spotify::initialize() {
    auto spotify = config->get_spotify();
    auto any_empty = spotify.access_token->empty() || spotify.refresh_token->empty() || spotify.expires_at == 0;

    if(!any_empty) {
        // Check for refresh
    }

    auto curr = std::filesystem::current_path() / "spotify/authorize.js";


    printf("Path %s \n", curr.string().c_str());
    printf("Authorize at: http://10.6.0.23:8888/login \n");
    auto out = execute_process("node", { curr.string(), "8888"} );

    printf("Out %s \n", out.c_str());
}


Spotify::Spotify() = default;
