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
#include <string.h>
#include <sys/time.h>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <atomic>
using namespace std;

string receivefolder = "received_files";
using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
// words count for each thread
unordered_map<thread::id,unordered_map<string,long long>> wordscount;
// words count for no thread approach
unordered_map<string,long long> wordscount_nothread;

class ThreadPool{
	private:
		vector<thread> threads;
		queue<function<void()>> job_queue;
		mutex job_queue_mutex;
		condition_variable cv_;
		bool terminate = false;

	public:
		ThreadPool(size_t numberofthread){
			threads.reserve(numberofthread);
			for (size_t i = 0; i < numberofthread; i++) { 
            threads.emplace_back([this] { 
                while (true) { 
                    function<void()> job; 
                    // The reason for putting the below code 
                    // here is to unlock the queue before 
                    // executing the task so that other 
                    // threads can perform enqueue jobs 
                    { 
                        // Locking the queue so that data 
                        // can be shared safely 
                        unique_lock<mutex> lock( 
                            job_queue_mutex); 
  
                        // Waiting until there is a job to 
                        // execute or the pool is stopped 
                        cv_.wait(lock, [this] { 
                            return !job_queue.empty() || terminate; 
                        }); 
  
                        // exit the thread in case the pool 
                        // is stopped and there are no jobs 
                        if (terminate && job_queue.empty()) { 
                            return; 
                        } 
  
                        // Get the next task from the queue 
                        job = move(job_queue.front()); 
                        job_queue.pop(); 
                    } 
  
                    job(); 
                } 
            }); 
        	}
		}
		// Destructor to stop the thread pool 
		~ThreadPool() 
		{ 
			{ 
				// Lock the queue to update the stop flag safely 
				unique_lock<mutex> lock(job_queue_mutex); 
				terminate = true; 
			} 
	
			// Notify all threads 
			cv_.notify_all(); 
	
			// Joining all worker threads to ensure they have 
			// completed their tasks 
			for (auto& thread : threads){ 
				thread.join(); 
			} 
		} 
	
		// Enqueue task for execution by the thread pool 
		void enqueue(function<void()> job) 
		{ 
			{ 
				unique_lock<std::mutex> lock(job_queue_mutex); 
				job_queue.emplace(move(job)); 
			} 
			cv_.notify_one(); 
		} 	
};
void receivefile(int socket){
	int file_receive = 0;
	while(true){
		// get file path
		int filepathsz;
		int filesleft = recv(socket,&filepathsz,sizeof(filepathsz),0);

		if (filesleft <= 0) {
			cout<<"Total File Received: "<<file_receive<<endl;
            cout << "No more files to receive." << endl;
            break;
        }
		char filepath[filepathsz+1];
		recv(socket,filepath,filepathsz,0);
		filepath[filepathsz]='\0';
		//create dir
		filesystem::path fullpath = filesystem::path("received_files")/filesystem::path(filepath);
		filesystem::create_directories(fullpath.parent_path());
		// filesz
		int filesz;
		recv(socket,&filesz,sizeof(filesz),0);
		// file content
		ofstream file(fullpath,ios::binary);
		char buffer[1024];
		int totalbyte = 0;

		while(totalbyte < filesz){
			int byte = recv(socket,buffer,sizeof(buffer),0);
			file.write(buffer,byte);
			totalbyte+=byte;
		}
		cout<<fullpath.string()<<" Received"<<endl;
		string ready = "READY";
        send(socket, ready.c_str(), ready.size(), 0);
		file_receive++;
	}
	return;
}
// unordered_map<thread::id,int> threadtoidx;
// atomic<int> t = -1;
// vector<pair<int,double>> d;  // task,time
void FileReading_mul(string &filename){
	ifstream file(filename);
	string line;
	string str;
	thread::id curr_thread_idx = this_thread::get_id();
	// if(!threadtoidx.count(curr_thread_idx)){
	// 	t++;
	// 	threadtoidx[curr_thread_idx] = t;
	// 	d.push_back(pair<int,double>{0,0});
	// }
	// double time_used;
	// timeval start, end;
	// gettimeofday(&start,NULL);
	while(getline(file,line)){
		stringstream ss(line);
		while(getline(ss,str,' ')){
			wordscount[curr_thread_idx][str]++;
		}
	}
	// gettimeofday(&end,NULL);
	// // Cal total time
	// time_used = (end.tv_sec -start.tv_sec)*1e6;
	// time_used = (time_used + (end.tv_usec -start.tv_usec))*1e-6;
	// d[threadtoidx[curr_thread_idx]].first++;
	// d[threadtoidx[curr_thread_idx]].second+=time_used;
	return;
}
void FileReading_single(string &filename){
	ifstream file(filename);
	string line;
	string str;
	thread::id curr_thread_idx = this_thread::get_id();
	while(getline(file,line)){
		stringstream ss(line);
		while(getline(ss,str,' ')){
			wordscount_nothread[str]++;
		}
	}
	return;
}
void SearchAllFIles_thread(ThreadPool &pool){
	for (const auto& dirEntry : recursive_directory_iterator(receivefolder)){
		if(filesystem::is_regular_file(dirEntry)){
			string dirEntry_str = dirEntry.path().string();
			pool.enqueue(bind(FileReading_mul,dirEntry_str));
		}
	}

	return;
}
void SearchAllFIles(){
	for (const auto& dirEntry : recursive_directory_iterator(receivefolder)){
		if(filesystem::is_regular_file(dirEntry)){
			string dirEntry_str = dirEntry.path().string();
			FileReading_single(dirEntry_str);
		}
	}
	return;
}
void deleteallfiles(const filesystem::path& dir){
	for(const auto &entry:filesystem::directory_iterator(dir)){
		filesystem::remove_all(entry);
	}
}
pair<double,double> HashTable_approach(){
	timeval start, end;

	// single thread
	double time_used;
	gettimeofday(&start,NULL);
	// search and read all files
	SearchAllFIles();
	gettimeofday(&end,NULL);
	// Cal total time
	time_used = (end.tv_sec -start.tv_sec)*1e6;
	time_used = (time_used + (end.tv_usec -start.tv_usec))*1e-6;


	// 8 threads
	double time_used_thread;
	ThreadPool pool(8);
	gettimeofday(&start,NULL);
	// search and read all files
	SearchAllFIles_thread(pool);
	gettimeofday(&end,NULL);
	// Cal total time
	time_used_thread = (end.tv_sec -start.tv_sec)*1e6;
	time_used_thread = (time_used_thread + (end.tv_usec -start.tv_usec))*1e-6;
	
	return {time_used,time_used_thread};
}
int main(int argc,char *argv[]){
	// Clear the receiving file
	filesystem::path receivefolder_path(receivefolder);
	deleteallfiles(receivefolder_path);
	// Create server socket
	int serverSocket = socket(AF_INET,SOCK_STREAM,0);
	if(serverSocket == -1){
		cout<<"Fail Creating Server Socket"<<endl;
	}
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	int check_bind = bind(serverSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));
	if(check_bind == -1){
		cout<<"Fail Binding Server Socket"<<endl;
	}
	listen(serverSocket,1);
	
	int clientSocket = accept(serverSocket,nullptr,nullptr);
	// receive files
	receivefile(clientSocket);
	close(serverSocket);

	const int test_number = 1;

	// This part is for testing and analyzing
	// double avg_time = 0,avg_time_thread= 0;
	// int diff = 0;
	// for(int i=0;i<test_number;i++){
	// 	wordscount_nothread.clear();
	// 	wordscount.clear();
	// 	pair<double,double> time = HashTable_approach();
	// 	avg_time+=time.first;
	// 	avg_time_thread+=time.second;
		 
	// 	// merge counts from threads
	// 	unordered_map<string,int> total_merge_thread;
	// 	for(auto &v:wordscount){
	// 		for(auto &p:v.second){
	// 			total_merge_thread[p.first]+=p.second;
	// 		}
	// 	}
	// 	// output 
	// 	for(auto&v:total_merge_thread){
	// 		// cout<<v.first<<": "<<v.second<<endl;
	// 		diff+=(abs(v.second-wordscount_nothread[v.first]));
	// 	}
	// }
	// avg_time/=test_number;
	// avg_time_thread/=test_number;
	// cout<<"Hashtable method: Avg Time Without Thread: "<<avg_time<<" seconds"<<endl;
	// cout<<"Hashtable method: Avg Time With Thread: "<<avg_time_thread<<" seconds"<<endl;
	// cout<<"Diff: "<<diff<<endl;
	////////////////////////////////////////////////////////////////////
	HashTable_approach();
	unordered_map<string,int> total_merge_thread;
	for(auto &v:wordscount){
		for(auto &p:v.second){
			total_merge_thread[p.first]+=p.second;
		}
	}
	//output 
	for(auto&v:total_merge_thread){
		cout<<v.first<<": "<<v.second<<endl;
	}

	// thread loading
	// for(int i=0;i<8;i++){
	// 	cout<<"Thread "<<i<<": "<<d[i].first<<" in "<<d[i].second<<" Avg: "<<
	// 	d[i].second/d[i].first<<" Load: "<<((double)d[i].first)/62*100<<"%"<<endl;
	// }
	return 0;
}
