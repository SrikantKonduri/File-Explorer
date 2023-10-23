#include <iostream>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <filesystem>
#include <string.h>
#include <bits/stdc++.h>

// #define clear_screen() cout<<"\033[2J\033[1;1H"
#define clear_screen() cout<<"\033[H\033[J";

using namespace std;

struct termios orig_termios;

struct file_attributes{
    string permissions,file_name,modified_date,user_name,group_name;
    long size;
};

vector<file_attributes> files;
stack<string> recent_stack,right_stack;
int cursor_idx = 0;
string pwd = get_current_dir_name();
string mode = "NORMAL MODE";
char mode_bit = 'N';
bool to_rename = false;
string new_file_name = "";
string user_input = "";
bool input_processed = false;
// recent_stack.push(pwd);

void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

string getPermission(struct stat file_meta){
    string permission = "";
    char ch = (S_ISDIR(file_meta.st_mode))?('d'):('-');
    permission.push_back(ch);
	ch = (file_meta.st_mode & S_IRUSR) ? ('r') : ('-');
    permission.push_back(ch);
	ch = (file_meta.st_mode & S_IWUSR) ? ('w') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IXUSR) ? ('x') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IRGRP) ? ('r') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IWGRP) ? ('w') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IXGRP) ? ('x') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IROTH) ? ('r') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IWOTH) ? ('w') : ('-');
	permission.push_back(ch);
    ch = (file_meta.st_mode & S_IXOTH) ? ('x') : ('-');
    permission.push_back(ch);
    return permission;
}

void accumlateFiles(){
    
    DIR *dir = opendir(pwd.c_str());
    struct dirent *ptr = readdir(dir);
    while(ptr != NULL){
        struct stat file_meta;
        string temp = pwd + '/' + ptr->d_name;
        stat(temp.c_str(),&file_meta);
        struct group *grp = getgrgid(file_meta.st_gid);
		struct passwd *usr = getpwuid(file_meta.st_uid);
        string name = ptr->d_name;

        files.push_back(file_attributes());
        files[files.size()-1].file_name = name;
        files[files.size()-1].permissions = getPermission(file_meta);
        files[files.size()-1].user_name = usr->pw_name;
        files[files.size()-1].group_name = grp->gr_name;
        files[files.size()-1].size = file_meta.st_size;

        char lm_time[50];
        strcpy(lm_time,ctime(&file_meta.st_mtime));
        time_t lm_time_t = file_meta.st_mtime;
        struct tm local_time;
        localtime_r(&lm_time_t, &local_time);
        strftime(lm_time, sizeof lm_time, "%b %d %H:%M", &local_time);
        files[files.size()-1].modified_date = string(lm_time);
        ptr = readdir(dir);
    }
}

void printFiles(){
    clear_screen();
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int i=0;
    int rows = w.ws_row;
    int cols = w.ws_col;
    for(i=0;i<files.size();i++){
        struct file_attributes file = files[i];
        if(i == cursor_idx && mode_bit == 'N'){
            cout << ">\t";
        }
        else{
            cout << " \t";
        }
        cout << setw(10) << file.permissions;
        cout << setw(10) << file.user_name;
        cout << setw(10) << file.group_name;
        cout << setw(15) << file.size;
        cout << setw(15) << file.modified_date;
        cout << "\t" << file.file_name << endl;
    }
    for(;i<rows-1;i++){
        cout << endl;
    }
    if(mode_bit == 'N'){
        cout << mode << ": " << pwd << "$";
    }
    else{
        cout << mode << ": " << pwd << "$ " << user_input << flush;
    }
}

void enterEvent(){
    struct file_attributes file = files[cursor_idx];

    if(file.permissions[0] == 'd'){
        cursor_idx = 0;
        // cout << file.file_name << "\n";
        if(file.file_name == ".."){
            int pos = pwd.find_last_of('/');
            if(pos != 0){
                pwd = pwd.substr(0,pos);
                recent_stack.push(pwd);
                files.clear();
                accumlateFiles();
            }
            // else{ // for this case pushing into stack is pending
            //     pwd = "/";
            //     files.clear();
            //     accumlateFiles();
            // }
        }
        else if(file.file_name != "."){
            pwd.push_back('/');
            pwd.append(file.file_name);
            recent_stack.push(pwd);
            files.clear();
            accumlateFiles();
        }
    }
    else{
        pid_t process_id = fork();
        string file_path = pwd + "/" + file.file_name;
		if (process_id == 0)
		{
			execl("/usr/bin/xdg-open", "xdg-open", file_path.c_str(), NULL);
			exit(1);
		}
    }
}

void homeEvent(){
    cursor_idx = 0;
    pwd = "/home/";
    pwd.append(getlogin());
    recent_stack.push(pwd);
    files.clear();
    accumlateFiles();
}

void leftEvent(){
    if(recent_stack.size() > 1){
        right_stack.push(recent_stack.top());
        recent_stack.pop();
        pwd = recent_stack.top();
        files.clear();
        cursor_idx = 0;
        accumlateFiles();
    }
}

void backspaceEvent(){
    int pos = pwd.find_last_of('/');
    if(pos != 0){
        pwd = pwd.substr(0,pos);
        recent_stack.push(pwd);
        files.clear();
        cursor_idx = 0;
        accumlateFiles();
    }
    // else{ // for this case pushing into stack is pending
    //     pwd = "/";
    //     files.clear();
    //     accumlateFiles();
    // }
}

void rightEvent(){
    if(!right_stack.empty()){
        pwd = right_stack.top();
        right_stack.pop();
        files.clear();
        cursor_idx = 0;
        recent_stack.push(pwd);
        accumlateFiles();
    }
}

void normalMode(){
    recent_stack.push(pwd);
    accumlateFiles();
    char ch = ' ';
    while(true){
        printFiles();
        cin.get(ch);
        if(ch == 'q'){
            mode_bit = 'E';
            break;
        }
        switch(ch){
            case 'A':              // UP
                if(cursor_idx > 0){
                    cursor_idx--;
                }
                break;
            case 'B':              // DOWN
                if(cursor_idx < files.size() - 1){
                    cursor_idx++;
                }
                break;
            case 'D':             // LEFT
                leftEvent();
                break;
            case 'C':             // RIGHT
                rightEvent();
                break;
            case '\n':            // ENTER
                enterEvent();
                break;
            case 127:             // BACKSPACE or DELETE
                backspaceEvent();
                break;
            case 'h':             // h
                homeEvent();
                break;
            case ':':
                mode = "COMMAND MODE";
                mode_bit = 'C';
                break;
        }
        if(mode == "COMMAND MODE"){
            break;
        }
    }
}

void clearStack(){
    while(!recent_stack.empty()){
        recent_stack.pop();
    }
    while(!right_stack.empty()){
        right_stack.pop();
    }
}

vector<string> split(string str,char delimiter){
    int n = str.length();
    vector<string> result;
    string temp = "";
    for(int i=0;i<n;i++){
        char ch = str[i];
        if(ch != delimiter){
            temp.append(1,ch);
        }
        else{
            result.push_back(temp);
            temp = "";
        }
    }
    if(temp != ""){
        result.push_back(temp);
    }
    return result;
}

mode_t get_mode(string src)
{
    struct stat inode;
    stat(src.c_str(), &inode);
    mode_t m=0;

    m = m | ((inode.st_mode & S_IROTH)?0004:0);
    m = m | ((inode.st_mode & S_IWOTH)?0002:0);
    m = m | ((inode.st_mode & S_IXOTH)?0001:0);

    m = m | ((inode.st_mode & S_IRUSR)?0400:0);
    m = m | ((inode.st_mode & S_IWUSR)?0200:0);
    m = m | ((inode.st_mode & S_IXUSR)?0100:0);

    m = m | ((inode.st_mode & S_IRGRP)?0040:0);
    m = m | ((inode.st_mode & S_IWGRP)?0020:0);
    m = m | ((inode.st_mode & S_IXGRP)?0010:0);
    

    return m;
}

bool searchFile(string directory,string file_name){
    DIR *dir = opendir(directory.c_str());
    if(dir == NULL){
        cout << "No such Directory Exists!" << endl;
    }
    struct dirent *ptr = readdir(dir);
    while(ptr != NULL){
        struct stat file_meta;
        string entry = ptr->d_name;
        string src = directory + "/" + entry;
        stat(src.c_str(),&file_meta);
        if(entry != "." && entry != ".."){
            if(S_ISDIR(file_meta.st_mode) == 0){
                if(entry == file_name){
                    return true;
                }
            }
            else{
                bool result = searchFile(src,file_name);
                if(result == true){
                    return true;
                }
            }
        }
        ptr = readdir(dir);
    }
    return false;

}

bool searchInPWD(string file_name){
    string directory = pwd;
    DIR *dir = opendir(directory.c_str());
    if(dir == NULL){
        cout << "No such Directory Exists!" << endl;
    }
    struct dirent *ptr = readdir(dir);
    while(ptr != NULL){
        struct stat file_meta;
        string entry = ptr->d_name;
        if(entry == file_name){
            return true;
        }
        ptr = readdir(dir);
    }
    return false;
}

void copyFile(string src,string dest){
    string temp = pwd;
    pwd = dest;
    bool status = searchInPWD(src.substr(src.find_last_of('/')+1));
    pwd = temp;
    if(status){
        cout << endl << "Cannot copy!" << endl;
        string str;
        getline(cin,str);
        return;
    }
    else{
        if(to_rename == false){
            dest += src.substr(src.find_last_of('/'));
        }
        else{
            dest = dest + "/" + new_file_name;
        }

        std::ifstream  src_file(src.c_str(), std::ios::binary);
        std::ofstream  dst_file(dest.c_str(),   std::ios::binary);
        dst_file << src_file.rdbuf();
        src_file.close();
        dst_file.close();
    }

}


string getAbsolutePath(string relative_path){
    string result = pwd;
    if(relative_path[0] == '~'){
        result = "/home/";
        result.append(getlogin());
        relative_path = relative_path.substr(1,relative_path.length()); 
        result.append(relative_path);
    }
    else if(relative_path[0] == '/'){
        result = relative_path;
    }
    else{
        string local_pwd = "";
        for(int i=0;i<relative_path.length();i++){
            char ch = relative_path[i];
            if(ch != '/'){
                local_pwd.append(1,ch);
            }
            else{
                if(local_pwd == ".."){
                    int pos = result.find_last_of('/');
                    if(pos != 0){
                        result = result.substr(0,pos);
                    }
                }
                else if(local_pwd != "." && local_pwd != ""){
                    result.append("/");
                    result.append(local_pwd);
                }
                local_pwd = "";
            }
        }
        if(local_pwd != ""){
            if(local_pwd == ".."){
                int pos = result.find_last_of('/');
                if(pos != 0){
                    result = result.substr(0,pos);
                }
            }
            else if(local_pwd != "."){
                result.append("/");
                result.append(local_pwd);
            }
        }
    }
    return result;
}

void createFile(string path){
    int fd;
    string temp = pwd;
    int pos = path.find_last_of('/');
    pwd = path.substr(0,pos);
    bool status = searchInPWD(path.substr(pos+1));
    pwd = temp;
    if(status){
        cout << endl << "Cannot create file with this name!" << endl;
        string str;
        getline(cin,str);
    }
    else{
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        fd = creat(path.c_str(), mode);
    }
}

bool searchDirNameInDir(string directory,string target_directory){
    DIR *dir = opendir(directory.c_str());
    if(dir == NULL){
        cout << "No such Directory Exists!" << endl;
        return true;
    }
    struct dirent *ptr = readdir(dir);
    while(ptr != NULL){
        struct stat file_meta;
        string entry = ptr->d_name;
        string path = directory + "/" + ptr->d_name;
        stat(path.c_str(),&file_meta);
        if(S_ISDIR(file_meta.st_mode) != 0 && entry == target_directory){
            return true;
        }
        ptr = readdir(dir);
    }
    return false;
}

void createDirectory(string path){
    mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

string getDirectoryName(string dir_name){
    int pos = dir_name.find_last_of('/');
    return dir_name.substr(pos+1,dir_name.length());
}

void copyDirectory(string src_dir,string dtn_dir){
    DIR *dir = opendir(src_dir.c_str());
    if(dir == NULL){
        cout << "No such directory exists! " << endl;
        return;
    }
    struct dirent *ptr = readdir(dir);
    string directory_name = getDirectoryName(src_dir);
    dtn_dir = dtn_dir + "/" + directory_name;
    createDirectory(dtn_dir);
    while(ptr != NULL){
        struct stat file_meta;
        string src = src_dir + "/" + ptr->d_name;
        string dtn = dtn_dir + "/" + ptr->d_name;
        stat(src.c_str(),&file_meta);
        string temp = ptr->d_name;
        if(temp == "." || temp == ".."){
            // Do Nothing
        }
        else if(S_ISDIR(file_meta.st_mode) != 0){
            copyDirectory(src,dtn_dir);
        }
        else{
            copyFile(src,dtn_dir);
        }
        ptr = readdir(dir);
    }
}


bool searchDirectory(string directory,string target_directory){
    DIR *dir = opendir(directory.c_str());
    if(dir == NULL){
        cout << "No such Directory Exists!" << endl;
    }
    struct dirent *ptr = readdir(dir);
    while(ptr != NULL){
        struct stat file_meta;
        string entry = ptr->d_name;
        string src = directory + "/" + entry;
        stat(src.c_str(),&file_meta);
        if(entry != "." && entry != ".."){
            if(S_ISDIR(file_meta.st_mode) != 0){
                if(entry == target_directory){
                    return true;
                }
                bool result = searchDirectory(src,target_directory);
                if(result == true){
                    return true;
                }
            }
        }
        ptr = readdir(dir);
    }
    return false;
}

void deleteFile(string path){
    string file = path.substr(path.find_last_of('/')+1);
    if(file == "." || file == ".."){
        cout << "Cannot delete!" << endl;
        string str;
        getline(cin,str);
        return;
    }
    else{
        int status = remove(path.c_str());
        if(status == -1){
            cout << "Cannot delete!" << endl;
            string str;
            getline(cin,str);
        }    
    }
}

void deleteDirectory(string directory_path){
    DIR *dir = opendir(directory_path.c_str());
    if(dir == NULL){
        cout << "No such directory exists!" << endl;
        string str;
        getline(cin,str);
        return;
    }
    else{
        struct dirent *ptr = readdir(dir);
        string path = directory_path + "/";
        while(ptr != NULL){
            string temp = ptr->d_name;
            path = directory_path + "/" + temp;
            struct stat file_meta;
            stat(path.c_str(),&file_meta);
            if(temp == "." || temp == ".."){ 
                // Do nothing!
            }
            else if(S_ISDIR(file_meta.st_mode) != 0){
                deleteDirectory(path);
            }
            else{
                deleteFile(path);
            }
            ptr = readdir(dir);
        }
        closedir(dir);
        remove(directory_path.c_str());
    }
}

void moveFile(string src,string dtn){
    DIR *dir = opendir(dtn.c_str());
    if(dir == NULL){
        cout << "No such directory exists!" << endl;
        string str;
        getline(cin,str);
        return;
    }
    copyFile(src,dtn);
    deleteFile(src);
}

void moveDirectory(string src,string dtn){
    DIR *src_dir = opendir(src.c_str());
    DIR *dtn_dir = opendir(dtn.c_str());
    if(src_dir == NULL || dtn_dir == NULL){
        cout << "No such directory(s) exist!" << endl;
        string str;
        getline(cin,str);
        return;
    }
    copyDirectory(src,dtn);
    deleteDirectory(src);
}


void commandMode(){
    accumlateFiles();
    while(true){
        printFiles();
        char input_char;
        if(input_processed == false){
            read(STDIN_FILENO,&input_char,1);
            if(input_char == '\n'){
                input_processed = true;
            }
            else if(input_char == 127){  // Handling BACKSPACE
                if(user_input.length() > 0){
                    user_input.pop_back();
                }
            }
            else if(input_char == 27){   // Handling ESC to go back to normal mode
                mode_bit = 'N';
                mode = "NORMAL MODE";
                user_input = "";
                input_processed = false;
                break;
            }
            else{
                user_input.push_back(input_char);
            }
        }
        else{
            if(user_input == "quit"){
                mode_bit = 'E';
                break;
            }
            vector<string> user_input_splits = split(user_input,' ');
            string operation = user_input_splits[0];
            if(operation == "copy"){
                vector<string> source_files;
                int n = user_input_splits.size();
                string destination = user_input_splits[n-1];
                destination = getAbsolutePath(destination);
                for(int i=1;i<n-1;i++){
                    source_files.push_back(user_input_splits[i]);
                }
                
                for(auto file_path : source_files){
                    string path = getAbsolutePath(file_path);
                    copyFile(path,destination);
                }
                
                accumlateFiles();
            }
            else if(operation == "goto"){
                // Absolute path always starts with / and gives complete path
                // Relative path can start with anything ~ . .. <directory_name>
                string given_path = user_input_splits[1];
                if(given_path == "/"){
                    pwd = "/home/";
                    pwd.append(getlogin());
                }
                else{
                    pwd = getAbsolutePath(given_path);
                }
                files.clear();
                accumlateFiles();
            }
            else if(operation == "create_file"){
                string file_name = user_input_splits[1];
                string path = getAbsolutePath(user_input_splits[2]);
                path += '/';
                path += file_name;
                createFile(path);
                accumlateFiles();
            }
            else if(operation == "create_dir"){
                string dir_name = user_input_splits[1];
                string path = getAbsolutePath(user_input_splits[2]);
                if(searchDirNameInDir(path,dir_name)){
                    cout << endl << "Cannot create directory with this name!" << endl;
                    string str;
                    getline(cin,str);
                }
                else{
                    path += '/';
                    path += dir_name;
                    createDirectory(path);
                    accumlateFiles();
                }
            }
            else if(operation == "copy_dir"){
                vector<string> source_directories;
                int n = user_input_splits.size();
                string destination_dir = user_input_splits[n-1];
                destination_dir = getAbsolutePath(destination_dir);
                for(int i=1;i<n-1;i++){
                    source_directories.push_back(user_input_splits[i]);
                }
                for(auto dir_name : source_directories){
                    string abs_path = getAbsolutePath(dir_name);
                    int pos = abs_path.find_last_of('/');
                    if(searchDirNameInDir(destination_dir,abs_path.substr(pos+1))){
                        cout << endl << "Cannot copy directory!" << endl;
                        string str;
                        getline(cin,str);
                    }
                    else{
                        copyDirectory(getAbsolutePath(dir_name),destination_dir);
                    }
                }
            }
            else if(operation == "search_file"){
                bool result = searchFile(pwd,user_input_splits[1]);
                if(result == true){
                    cout << endl << "True" << endl;
                }
                else{
                    cout << endl << "False" << endl;
                }
                string str = "";
                getline(cin,str);
            }
            else if(operation == "search_dir"){
                bool result = searchDirectory(pwd,user_input_splits[1]);
                if(result == true){
                    cout << "True" << endl;
                }
                else{
                    cout << "False" << endl;
                }
                string str = "";
                getline(cin,str);
            }
            else if(operation == "delete_file"){
                string path = getAbsolutePath(user_input_splits[1]);
                deleteFile(path);
            }
            else if(operation == "delete_dir"){
                string path = getAbsolutePath(user_input_splits[1]);
                deleteDirectory(path);
            }
            else if(operation == "move_file"){
                vector<string> source_files;
                int n = user_input_splits.size();
                string destination = user_input_splits[n-1];
                destination = getAbsolutePath(destination);
                for(int i=1;i<n-1;i++){
                    source_files.push_back(user_input_splits[i]);
                }
                for(auto file_path : source_files){
                    string path = getAbsolutePath(file_path);
                    string temp = pwd;
                    int pos = path.find_last_of('/');
                    pwd = path.substr(0,pos);
                    bool status = searchInPWD(path.substr(pos+1));
                    pwd = temp;
                    if(status){
                        cout << endl << "Cannot move!" << endl;
                        string str;
                        getline(cin,str);
                    }
                    else{
                        moveFile(path,destination);
                    }
                }
            }
            else if(operation == "move_dir"){
                vector<string> source_directories;
                int n = user_input_splits.size();
                string destination_dir = user_input_splits[n-1];
                destination_dir = getAbsolutePath(destination_dir);
                for(int i=1;i<n-1;i++){
                    source_directories.push_back(user_input_splits[i]);
                }
                for(auto dir_name : source_directories){
                    string path = getAbsolutePath(dir_name);
                    int pos = path.find_last_of('/');
                    if(searchDirNameInDir(destination_dir,path.substr(pos+1))){
                        cout << endl << "Cannot move directory!" << endl;
                        string str;
                        getline(cin,str);
                    }
                    else{
                        moveDirectory(path,destination_dir);
                    }
                }
            }
            else if(operation == "rename"){
                string old_filename = user_input_splits[1];
                string new_filename = user_input_splits[2];
                bool status1 = searchInPWD(old_filename);
                bool status2 = searchInPWD(new_filename);
                cout << status1 << endl;
                cout << status2 << endl;
                if(status1 == true && status2 == false){
                    to_rename = true;
                    new_file_name = new_filename;
                    string src_path = pwd + "/" + old_filename;
                    copyFile(src_path,pwd);
                    to_rename = false;
                    new_file_name = "";
                    deleteFile(src_path);
                }
                else{
                    cout << "Cannot rename!" << endl;
                    string str;
                    getline(cin,str);
                }
            }
            files.clear();
            accumlateFiles();
            user_input = "";
            input_processed = false;
        }
    }
}

int main(){
    enableRawMode();
    while(true){
        switch(mode_bit){
            case 'N':
                pwd = get_current_dir_name();
                normalMode();
                files.clear();
                clearStack();
                cursor_idx = 0;
                break;
            case 'C':
                pwd = get_current_dir_name();
                commandMode();
                files.clear();
                break;
        }
        if(mode_bit == 'E'){
            break;
        }
    }
    return 0;
}