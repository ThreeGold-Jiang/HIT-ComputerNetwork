#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <process.h>
#include<iostream>
#include<regex>
#include <fstream>
#include <io.h>
#include<map>
#include <iostream>
#include <vector>
#include <string.h>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口
#define IWeb_Num 20
#define DateSize 50
#define cache_dir "D:\\code\\Computer Network\\lab1\\lab1\\cache\\"//定义本地cache路径
#define Permit_IP "127.0.0.1"
//Http 重要头部数据 
struct HttpHeader {
	char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024]; // 请求的 url
	char host[1024]; // 目标主机
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
BOOL InitSocket();//初始化ProxyServer和ProxyServerAddr
void ParseHttpHead(char* buffer, HttpHeader* httpHeader);
BOOL ConnectToServer(SOCKET* serverSocket, char* host);
BOOL SearchCache(char* name,char*file);
BOOL IsVaildWeb(char* Web);
BOOL IsVaildFish(char* Web,char* Host,char* Url);
void GetDate(char* file,char* date);
void DateHttp(char* buffer,char*  Date);
void GetSatus(char* buffer,char*num);
void changeHttpHead(char* buffer, char* url, char* host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
const char* Invaild_Web[IWeb_Num] = { "http://www.7k7k.com/" };//禁止访问网站集
std::map<std::string, std::string> Fishlist_Src = { {"http://today.hit.edu.cn/" ,"http://jwts.hit.edu.cn/"},{"3","c++"},{"4","python"}};
std::map<std::string, std::string> Fishlist_Host = { {"http://jwts.hit.edu.cn/","jwts.hit.edu.cn"},{"3","c++"},{"4","python"} };
int Web_NUM = 1;
//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};//作为代理服务器，既需要一个面向客户端的套接字，也需要一个面向服务端的套接字
int _tmain(int argc, _TCHAR* argv[])
{
	WORD p1 = MAKEWORD(1, 1);
	SOCKADDR_IN acceptAddr;
	int sockaddr_in_size = sizeof(SOCKADDR_IN);
	printf("%x\n", p1);
	printf("代理服务器正在启动\n");
	printf("初始化...\n");
	if (!InitSocket()) {
		printf("socket 初始化失败\n");
		return -1;
	}
	printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;//创建一个空套接字等待客户端的请求
	ProxyParam* lpProxyParam;//创建一个新代理端口（双端口）
	HANDLE hThread;//创建一个新线程
	DWORD dwThreadID;//创建一个新线程ID（用无符号双字）
	//代理服务器不断监听
	while (true) {
		acceptSocket = accept(ProxyServer, (SOCKADDR*)&acceptAddr, &sockaddr_in_size);//阻塞函数，一直等待连接
		//accept将客户端的信息绑定到一个socket上，也就是
		//给客户端创建一个socket，通过返回值
		//返回给我们客户端的socket
		printf("IP:%s\n", inet_ntoa(acceptAddr.sin_addr));
		//若是被屏蔽的IP字段则跳过
		//if (IsVaildIp(inet_ntoa(acceptAddr.sin_addr))==FALSE) {
		//	//printf("True IP");
		//	continue;
		//}
		lpProxyParam = new ProxyParam;//如果收到连接则新建代理服务器
		if (lpProxyParam == NULL) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;//指定代理客户端端口
		hThread = (HANDLE)_beginthreadex(NULL, 0,
			&ProxyThread, (LPVOID)lpProxyParam, 0, 0);//新建线程，参数解释见博客返回句柄
		CloseHandle(hThread);//ProxyThread表示线程处理函数
		Sleep(200);//CloseHandle关闭线程
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public 
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket() {
	//加载套接字库（必须）
	WORD wVersionRequested;//WORD的意思为字，是2byte的无符号整数，表示范围0~65535.相当于c语言中2个char
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);//makeword是将两个byte型合并成一个word型，一个在高8位(b)，一个在低8位(a)
	//这里的WSAStartup就是为了向操作系统说明，我们要用哪个库文件，让该库文件与当前的应用程序绑定，从而就可以调用该版本的socket的各种函数了。
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);//TCP Socket类型//AF_INET表示TCP/IP协议族
	if (INVALID_SOCKET == ProxyServer) {
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;//使用IPv4地址
	ProxyServerAddr.sin_port = htons(ProxyPort);//转网络字节顺序
	//ProxyServerAddr.sin_addr.S_un.S_addr
	//用户过滤
	//ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;//客户端初始化IP地址为地址通配符//S_un.S_addr可以缩写为s_addr
	ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr(Permit_IP);//客户端初始化IP地址为地址通配符//S_un.S_addr可以缩写为s_addr
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("绑定套接字失败\n");//绑定套接字和端点地址
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("监听端口%d 失败", ProxyPort);//服务器进入监听状态
		return FALSE;//SOMAXCONN为一个端口监听队列的最大长度
	}
	return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public 
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
	char Buffer[MAXSIZE];//接受端缓存区大小
	char file[200];
	char RecvBuffer[MAXSIZE];
	char *date ;
	char* CacheBuffer;//Cache大小
	char num[10];
	char fish_host[50];
	char fish_url[50];
	ZeroMemory(num, 10);
	ZeroMemory(fish_host, 50);
	ZeroMemory(fish_url, 50);
	ZeroMemory(Buffer, MAXSIZE);//置0
	ZeroMemory(file, 200);
	ZeroMemory(RecvBuffer, MAXSIZE);
	date = new char[DateSize];
	ZeroMemory(date, DateSize);
	SOCKADDR_IN clientAddr;//代理服务器的端点地址（包括IP和端口号，都是网络字节形式）
	int length = sizeof(SOCKADDR_IN);//端点地址大小
	int recvSize;//接受大小
	int ret;
	recvSize = recv(((ProxyParam	*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	//接受来自客户端的请求，为阻塞函数
	HttpHeader* httpHeader = new HttpHeader();//暂时移到这里
	if (recvSize <= 0) {
		goto error;
	}
	//接受缓存区
	printf("\nrecvbuffer_from_browser:\n%s\n", Buffer);
	CacheBuffer = new char[recvSize + 1];//CacheBuffer
	ZeroMemory(CacheBuffer, recvSize + 1);//置0
	memcpy(CacheBuffer, Buffer, recvSize);//内存拷贝函数
	ParseHttpHead(CacheBuffer, httpHeader);//解析HTTP头
	delete CacheBuffer;//删除CacheBuffer(用作暂存)a
	//查看是否为被禁止网站
	if (IsVaildWeb(httpHeader->url) == FALSE) {
		printf("\nThe web is not permitted to visit");
		goto error;
	}
	if (IsVaildFish(httpHeader->url,fish_url,fish_host)) {
		printf("\nFishing!\n");
		printf("From %s to %s\n",httpHeader->url,fish_url);
		memcpy(httpHeader->url, fish_url, strlen(fish_url) + 1);
		memcpy(httpHeader->host, fish_host, strlen(fish_host) + 1);
	}
	




	//查看是否有本地缓存
	if (SearchCache(httpHeader->url, file)) {
		FILE* fp ;
		errno_t error = fopen_s(&fp, file, "rb");
		if (error != 0) {
			printf("\nthe file path is--%s\n", file);
			exit(0);
		}
		printf("\nthe file path is--%s\n", file);
		GetDate(file,date);
		printf("date:%s\n",date);
		DateHttp(Buffer, date);
		if (fp != 0) {
			fclose(fp);
		}
		printf("newBuffer:\n%s\n", Buffer);
	}
	
	/*if (date[0]==0) {
		printf("hello ,JJY!\n");
	}*/


	if (!ConnectToServer(&((ProxyParam
		*)lpParameter)->serverSocket, httpHeader->host)) {
		goto error;
	}//解析HTTP头之后进行新建serverSocket连接url并发送请求，
	printf("代理连接主机 %s 成功\n", httpHeader->host);
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer)
		+ 1, 0);
	//等待目标服务器返回数据（进入阻塞）
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, RecvBuffer, MAXSIZE, 0);
	
	if (recvSize <= 0) {
		goto error;
	}
	//得到响应码
	GetSatus(RecvBuffer, num);
	printf("\nstatusnum:%s\n", num);
	printf("recvSize:%d\n",recvSize);
	printf("recvBuffer_from_server\n:%s\n", RecvBuffer);
	//如果为304，则把自身的发送给浏览器
	if (strcmp(num, "304")==0) {
		FILE* fp=NULL;
		errno_t error = fopen_s(&fp, file, "rb");
		printf("send local cache");
		if (error == 0) {
			fread(RecvBuffer,sizeof(char),MAXSIZE,fp);
			fclose(fp);
		}
	}
	//如果是200，则更新本地缓存或新建缓存
	//其他响应码则说明报错，统统转发给浏览器
	if(strcmp(num, "200")==0) {
		FILE* fp = NULL;
		errno_t error = fopen_s(&fp, file, "wb");
		printf("store new cache!\n");
		if (error == 0) {
			printf("new_cache:\n%s\n",RecvBuffer);
			fwrite(RecvBuffer, sizeof(char), MAXSIZE, fp);
			fclose(fp);
		}

	}
	//将目标服务器返回的数据直接转发给客户端
	printf("send_to_browser:\n%s\n", RecvBuffer);
	ret = send(((ProxyParam*)lpParameter)->clientSocket, RecvBuffer, sizeof(RecvBuffer), 0);

	//错误处理
error:
	printf("关闭套接字\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;//删除线程句柄
	_endthreadex(0);
	return 0;
}
//************************************
// Method: SearchCache
// FullName: SearchCache
// Access: public 
// Returns: char*
// Qualifier: 在本地缓存中查找对象
// Parameter: char * url
// Parameter: HttpHeader * httpHeader
//************************************
BOOL SearchCache(char* name,char*file) {
	int len = strlen(cache_dir) + strlen(name) + 1;//1是在字符串末尾加了一个\0
	int count = 0;
	int count_1 = 0;
	char buffer[MAXSIZE];
	std::string path = (char*)"D:\\code\\Computer Network\\lab1\\lab1\\cache\\";
	FILE* in;
	while(*name!='\0') {
		if (*name != ':' && *name != '/' && *name != '?') {
			file[count] = *name;
			count++;
		}
		name++;
	}
	file[count] = '\0';
	count = count + 5;
	strcat_s(file, strlen(file)+5, ".txt");
	path.append((const char*)file);
	strcpy_s(file, strlen((const char*)path.c_str()) + 1, (const char*)path.c_str());
	/*errno_t erro = strcat_s(path, strlen(path), (const char *)file);
	if (erro != 0) {
		printf("%d", erro);
		exit(1);
	}*/
	//strcpy_s(file,strlen(path)+1, path);
	//strcat_s(file, count+strlen((const char*)path.c_str()), (const char*)path.c_str());
	errno_t error = fopen_s(&in, file, "rb");
	//如果无错误，成功打开
	if (error== 0) {
		printf("");
		fread(buffer, sizeof(char), MAXSIZE, in);
		printf("\nlocal_cache:\n%s\n", buffer);
		//printf("\nthe file path is--%s\n", file);
		printf("Successfully!\n");
		fclose(in);
		return TRUE;
	}
	else {
		printf("Not Found!\n");
		return FALSE;
	}
	//strcat_s(path,len, name);
	//strcat_s(path, strlen(path)+strlen(name)+1 ,name);
	//printf("\nSearch%s  %d\n", path,len);
	//查找文件
	//free(path);
	
}
//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public 
// Returns: void
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char* buffer, HttpHeader* httpHeader) {
	char* p;
	char* ptr;
	const char* delim = "\r\n";//HTTP报文的分隔符
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	if (p[0] == 'G') {//GET 方式
		memcpy(httpHeader->method, "GET", 3);//更新HTTP头结构体的值
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);//减去（方法+版本的字长）
	}
	else if (p[0] == 'P') {//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);//同理
	}
	p = strtok_s(NULL, delim, &ptr);//指向开始那个指针的指针，即读下一行
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);//(HOST: www....)
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {//也有可能是Connection
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public 
// Returns: BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET* serverSocket, char* host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);//创建目标服务器套接字，用于转发
	HOSTENT* hostent = gethostbyname(host);//由域名解析得到网络字节顺序表示的IP地址
	//printf("host-hname:%s\n",&hostent->h_name);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);//结构体，表示IPV4地址
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	//把IP地址存入Server的sockaddr_in中
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	//初始化serverSocket
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	//利用connect向host的特定端口进行连接
	if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}
//************************************
// Method: GetDate
// FullName: GetDate
// Access: public 
// Returns: void
// Qualifier: 解析本地cache文件，得到修改日期
// Parameter: char* file
// Parameter: char* date
//************************************
void GetDate(char* file,char* date) {
	FILE* fp;
	char* ptr;
	char* p;
	char Buffer[MAXSIZE];
	const char* delim = "\r\n";
	fopen_s(&fp,file,"rb");
	fread(Buffer, sizeof(char), MAXSIZE, fp);
	p = strtok_s(Buffer, delim, &ptr);//提取第一行
	p = strtok_s(NULL, delim, &ptr);//指向开始那个指针的指针，即读下一行
	while (p) {
		if (p[0] == 'L'&&p[1]=='a') {//报文里面有Last-modified段
			memcpy(date, &p[6], strlen(p) - 6);//(HOST: www....)
			printf("date:%s",date);
			break;
		}
		if (p[0] == 'D') {//报文里面有Date段
			memcpy(date, &p[6], strlen(p) - 6);//(HOST: www....)
			printf("date:%s", date);
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	fclose(fp);	
}
//************************************
// Method: DateHttp
// FullName: DateHttp
// Access: public 
// Returns: void
// Qualifier: 修改请求报文头，加上"If-modified-since"字段
// Parameter: char* buffer
// Parameter: char* date
//************************************
void DateHttp(char* buffer, char* date) {
	char* new_header =(char*) "If-modified-since: ";
	char newbuffer[MAXSIZE];
	char* ptr;
	char* p;
	char* date_1 = date;
	date_1[strlen(date_1)] = '\0';
	const char* delim = "\r\n";
	int count = 0;
	ZeroMemory(newbuffer, MAXSIZE);
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	memcpy(newbuffer, &p[0], strlen(p));
	newbuffer[strlen(p)] = '\r';
	newbuffer[strlen(p)+1] = '\n';
	count = strlen(p) + 2;
	while (*new_header != '\0') {
		newbuffer[count] = *new_header;
		new_header++;
		count++;
	}
	int sum = count + strlen(date);
	while (*date != '\0' && count<=sum) {
		newbuffer[count] = *date;
		count++;
		date++;
	}
	newbuffer[count] = '\r';
	count++;
	newbuffer[count] = '\n';
	count++;
	int count_1=strlen(p)+2;
	while (buffer[count_1] != '\0') {
		newbuffer[count] = buffer[count_1];
		count++;
		count_1++;
		//printf("2");
	}
	newbuffer[count] = '\0';
	memcpy(buffer, newbuffer, MAXSIZE);
}
//************************************
// Method: GetSatus
// FullName: GetSatus
// Access: public 
// Returns: void
// Qualifier: 解析响应报文得到响应码
// Parameter: char* buffer
// Parameter: char* num
//************************************
void GetSatus(char* buffer, char* num) {
	char* ptr;
	char p[3];
	int count = 0;
	int count_1 = 0;
	const char* delim = "\r\n";
	while (buffer[count] != ' ') {
		count++;
	}
	count++;
	memcpy(num, &buffer[count], 3);
}
//************************************
// Method: IsVaildWeb
// FullName: IsVaildWeb
// Access: public 
// Returns: BOOL
// Qualifier: 检查是否为被禁止网站
// Parameter: char* url
//************************************
BOOL IsVaildWeb(char* url) {
	for (int i = 0; i < Web_NUM; i++) {
		if (strcmp(url,Invaild_Web[i])==0){
			return FALSE;
		}
	}
	return TRUE;
}
//************************************
// Method: IsVaildFish
// FullName: Is it a VaildFish
// Access: public 
// Returns: BOOL
// Qualifier: 检查是否为被钓鱼的url
// Parameter: char* Web
// Parameter: char* Host
// Parameter: char* Url
//************************************
BOOL IsVaildFish(char* Web,char* Host,char* Url) {
	std::string src = Web;
	std::map<std::string, std::string>::iterator iter;
	iter = Fishlist_Src.find(src);
	if (iter != Fishlist_Src.end()) {
		Host = (char *)iter->second.c_str();
		memcpy(Url, iter->second.c_str(), strlen(iter->second.c_str())+1);
		std::string url = Url;
		std::map<std::string, std::string>::iterator iter1 = Fishlist_Host.find(Url);
		if (iter1!= Fishlist_Host.end()) {
			memcpy(Host, iter1->second.c_str(), strlen(iter1->second.c_str()) + 1);
		}
		printf("Host:%s\n", Host);
		return TRUE;
	}
	return FALSE;
}