#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>
#include <string>
#include <sstream>
using namespace std;

using namespace std::chrono_literals;
string root_file = "../directory_big50000_depth5";
// string root_file = "../directory_big50000_depth4";
// string root_file = "../directory_big50000_depth3";
// string root_file = "../directory_big50000";
// string root_file = "../directory_big5000";
// string root_file = "../directory_big";
// string root_file = "../directory_big50";
using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

time_t file_time_to_ttime(const filesystem::file_time_type& ftime){
	auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(ftime-filesystem::file_time_type::clock::now()+chrono::system_clock::now());
	return chrono::system_clock::to_time_t(sctp);
}
bool isHiddenFile(const filesystem::path &filepath){
	// Warning this part has low portability, skip is to skip ..
	int skip = 0; // skip ..
	for(auto& component : filepath){
		if(component.filename().string()[0] == '.' && skip >= 1){
			return true;
		}
		skip++;
	}
	return false;
}
void sendFile(int &socket,const string &filepath){
	ifstream file(filepath,ios::binary);
	
	// file name
	string fullpath = filesystem::relative(filepath, root_file).string();
    int fullpathsz = fullpath.size();
	
	send(socket,&fullpathsz,sizeof(fullpathsz),0);
	send(socket,fullpath.c_str(),fullpathsz,0);
	//file sz
	file.seekg(0,ios::end);
	int filesz = file.tellg();
	file.seekg(0,ios::beg);
	send(socket,&filesz,sizeof(filesz),0);
	
	// file content
	char buffer[1024];
	while(file.read(buffer,sizeof(buffer))){
		send(socket,buffer,file.gcount(),0);
	}
	send(socket,buffer,file.gcount(),0);
	return;
}

void FileSearching(int &socket,time_t &timestamp){
	for (const auto& dirEntry : recursive_directory_iterator(root_file)){
		if(filesystem::is_regular_file(dirEntry)){
			if(isHiddenFile(dirEntry.path())) continue;
			auto const modified_file_time = filesystem::last_write_time(dirEntry);
			time_t  modified_ctime = file_time_to_ttime(modified_file_time);
			if(modified_ctime <= timestamp){
				sendFile(socket,dirEntry.path().string());
				// Wait for "READY" signal from the server
				char ready[6];
				recv(socket, ready, 5, 0);
				ready[5] = '\0';
				if (strcmp(ready, "READY") != 0) {
					cerr << "Failed to receive 'READY' from the server." << endl;
					return;
				}
			}
		}
	}
	return;
}
int main(int argc,char *argv[]){
	int clientSocket = socket(AF_INET,SOCK_STREAM,0);
	if(clientSocket == -1){
		cout<<"Fail Creating Client Socket"<<endl;
	}
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));
	string str = argv[1];
	time_t t = stoll(str);
	FileSearching(clientSocket,t);

	close(clientSocket);

	return 0;
}
