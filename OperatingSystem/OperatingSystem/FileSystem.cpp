#include<iostream>
#include<fstream>
#include<string.h>
#include<stdlib.h>
#include<tchar.h>
#include<Windows.h>
#include<iomanip>
using namespace std;

#define DISK_SIZE 1024*1024   //磁盘大小 
#define BLOCK_SIZE 1024       //每个磁盘块1024b 
#define BLOCK_NUM 1024        //共有1024个磁盘块 
#define INODE_NUM 1024        //索引节点数量1024
#define ERROR 0
#define OK 1

/*********************功能一览*********************/
void Format();  	//格式化 
void Cd();			//更改目录 
void Dir();			//列出当前目录及文件 
void Mkdir();		//创建目录 
void Rmdir();		//删除目录 
void XCopy();		//复制目录 
void Create();		//创建文件
void Edit();		//编辑文件
void View();		//显示文件 
void Attrib();		//设置文件属性 a:读写  w:只写  r:只读  h:隐藏  
void Find();		//查找文件位置 
void Del();    		//删除文件 
void Copy();		//复制文件 
void Import();		//导入文件
void Export();		//导出文件
void Help();		//帮助菜单 
void Exit();		//退出 

/*********************结构体定义*********************/
struct SuperBlock {
	bool Block[BLOCK_NUM];   //记录每个磁盘块的使用情况 true未用 flase已用 
	bool Inode[INODE_NUM];   //记录每个节点使用情况 true未用 flase已用 
}super_block;

struct Inode {
	char name[20];           //文件名 
	char type;               //类型  0-目录 1-文件 
	char right;				 //文件权限  a:读写  w:只写  r:只读  h:隐藏
	int length;				 //文件长度 
	short parent;			 //父目录节点号 
	short BlockNum;			 //占有的磁盘块数量 
	short BlockFirst;        //占有的第一块磁盘号 
	short BlockTable[16];	 //文件索引表 
}inode[INODE_NUM];

typedef void(*CMDFUN)();
struct CMD {
	char cmd_name[20];   //命令名 
	CMDFUN fun; 		 //函数名 
};

CMD cmd[] = {
	"format",	Format,
	"cd",    	Cd,
	"dir",		Dir,
	"mkdir",	Mkdir,
	"rmdir",	Rmdir,
	"xcopy",	XCopy,
	"create",	Create,
	"edit",		Edit,
	"view",		View,
	"attrib",	Attrib,
	"find",		Find,
	"del",		Del,
	"copy",		Copy,
	"import",	Import,
	"export",	Export,
	"help",		Help,
	"exit",		Exit,
};

/*****************定义队列的链式存储结构*******************/
/***********************BFS的辅助队列********************/
struct Data {
	int InodeNum;  //原节点序号 
	int num;       //新节点序号 
};

//定义结点 
typedef struct node {
	Data data;
	struct node* next;
}node, * queue;

//队列
typedef struct LinkQueue {
	queue front;   //头结点，front->next为队头 
	queue rear;    //队尾
}LinkQueue;

/*******************队列基本操作**************************/
//初始化队列 
int InitQueue(LinkQueue& Q) {
	Q.front = Q.rear = new node;
	Q.front->next = NULL;
	return OK;
}

//判断队列是否为空 空为0 非空为1
int QueueEmpty(LinkQueue& Q) {
	if (Q.front == Q.rear)
		return ERROR;
	else
		return OK;
}

//入队
int EnQueue(LinkQueue& Q, Data e) {//形参为原队列和新结点的数据 
	//定义一个新结点 
	queue p = new node;
	p->data = e;  //将数据传输给新结点 
	p->next = NULL;
	//插入队尾 
	Q.rear->next = p;
	Q.rear = p;
	return OK;
}

//出队 
int DeQueue(LinkQueue& Q) {
	queue p = new node;
	if (Q.front == Q.rear)
		return ERROR;
	p = Q.front->next; //p代表队头 
	//e = p->data;
	Q.front->next = p->next;
	if (Q.rear == p) //关键 
		Q.rear = Q.front;
	delete p;
	return OK;
}

/*********************相关全局变量*********************/
HANDLE File;		     //句柄类型 无类型指针  
int cur_inode;           //记录当前节点号
char Path[50];           //记录当前路径 
char file_source[50];    //data.bin文件模拟磁盘驱动器 
int cmd_size = sizeof(cmd) / sizeof(CMD); //命令个数 

/**********************多线程更新磁盘**********************/
DWORD WINAPI MyWriteToFile(LPVOID lpParam) {
	WaitForSingleObject(File, INFINITE);
	fstream file;
	file.open(file_source, ios::out | ios::in | ios::binary);
	file.write((char*)&super_block, sizeof(SuperBlock));
	file.write((char*)&inode, sizeof(Inode) * INODE_NUM);
	file.close();
	if (!ReleaseSemaphore(
		File,  						// handle to semaphore
		1,          				// increase count by one
		NULL))      				// not interested in previous count
	{
		printf("ReleaseSemaphore error: %d\n", GetLastError());
	}
	return 0;
}

/**********************初始化**********************/
void Init() {
	cur_inode = 0;
	strcat(Path, "Root:");
	//读取磁盘信息 
	ifstream file;
	file.open(file_source);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}
	file.read((char*)&super_block, sizeof(SuperBlock));
	file.read((char*)&inode, sizeof(Inode) * INODE_NUM);
	file.close();
}

/**********************格式化**********************/
void Format() {
	ofstream file;
	file.open(file_source, ios::out | ios::binary);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}

	byte b = 0x00;
	for (int i = 0; i < DISK_SIZE; i++) {
		file.write((char*)&b, sizeof(b));
	}
	file.close();

	//初始化超级块中信息 磁盘块和节点都未用false 
	for (int i = 0; i < BLOCK_NUM; i++) {
		super_block.Block[i] = false;
	}
	for (int i = 0; i < INODE_NUM; i++) {
		super_block.Inode[i] = false;
	}
	//0-1个磁盘块存储超级块信息，2-69个磁盘块存储节点信息
	//前70个块已用 
	for (int i = 0; i < 70; i++) {
		super_block.Block[i] = true;
	}

	//初始化每个节点的节点索引表和父节点 
	for (int i = 0; i < INODE_NUM; i++) {
		memset(inode[i].name, '\0', 20);
		//在BlockTable数组的当前位置向后的16*..个字节用-1替换 
		memset(inode[i].BlockTable, -1, 16 * sizeof(short));
		inode[i].type = '\0';
		inode[i].parent = -1;
	}
	//将第一个节点定义为根节点 
	super_block.Inode[0] = true;
	strcpy(inode[0].name, "Root");
	inode[0].type = '0';       //属性为目录 
	inode[0].parent = -1;
	inode[0].BlockNum = 0;
	cout << "Successfully format!" << endl << endl;
	//多线程更新磁盘 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                			// 使用缺省的堆栈大小  
		MyWriteToFile,          	// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/***************寻找文件/目录节点*******************/
int FindInode(char* name, int flag, int cur_inode) {
	int i;
	for (i = 0; i < INODE_NUM; i++) {
		if (inode[i].parent == cur_inode && inode[i].type == flag && strcmp(inode[i].name, name) == 0) {
			return i;
			break;
		}
	}
	if (i >= INODE_NUM) {
		return -1;
	}
}

/**********************更改目录**********************/
void Cd() {
	char temp[50];
	cin >> temp;
	//返回上一级目录 
	if (strcmp(temp, "..") == 0) {
		if (cur_inode == 0)
			cout << "Error:已经是根目录，无法返回上一级！" << endl;
		else {
			//修改路径 
			int num = strlen(inode[cur_inode].name);
			for (int k = 0; k < num + 2; k++) {
				Path[strlen(Path) - 1] = '\0';
			}
			//修改当前节点 
			cur_inode = inode[cur_inode].parent;
		}
	}
	//进入下一级目录
	else {
		//找到该temp目录节点 
		int index = FindInode(temp, '0', cur_inode);
		if (index == -1)
			cout << "Error:找不到此目录，请重新输入！" << endl;
		else {
			//重新定位当前节点 
			cur_inode = index;
			//更新路径 
			strcat(Path, "\\");
			strcat(Path, "\\");
			strcat(Path, inode[cur_inode].name);
		}
	}
}

/**********************显示目录**********************/
void Dir() {
	cout << left << setw(25) << "Name" << left << setw(25) << "Type" << left << setw(25) << "Size" << endl;
	for (int i = 0; i < INODE_NUM; i++) {
		//隐藏文件不可显示 
		if (inode[i].right != 'h') {
			if (inode[i].parent == cur_inode) {
				cout << left << setw(25) << inode[i].name;
				if (inode[i].type == '0')
					cout << left << setw(25) << "<Direct>";
				else
					cout << left << setw(25) << "<File>" << left << setw(25) << inode[i].length;
				cout << endl;
			}
		}
	}
}

/**********************创建目录**********************/
void Mkdir() {
	char name[50];
	cin >> name;        //输入目录名
	int i;            //记录找到的空节点空间
	for (i = 0; i < INODE_NUM; i++) {
		//当前目录的子目录中有同名目录 
		if (inode[i].parent == cur_inode && strcmp(inode[i].name, name) == 0 && inode[i].type == '0') {
			cout << "该目录已存在！创建失败！" << endl;
			return;
		}
	}
	//找到未用节点 
	for (i = 0; i < INODE_NUM; i++) {
		if (super_block.Inode[i] == false)
			break;
	}
	//初始化节点 
	super_block.Inode[i] = true;
	strcpy(inode[i].name, name);
	inode[i].type = '0';
	inode[i].parent = cur_inode;
	inode[i].BlockNum = 0;
	cout << "Successfully make a direct!" << endl << endl;
	//多线程
	HANDLE hThread = CreateThread(
		NULL,              		// default security attributes
		0,                 		// 使用缺省的堆栈大小  
		MyWriteToFile,          // 线程入口函数 
		NULL,             		// 传给线程入口函数的参数 
		0,                 		// 使用缺省的生成状态 
		NULL);   				// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
	return;
}

/********************删除单个目录********************/
void DelDir(int i) {
	super_block.Inode[i] = false;
	strcpy(inode[i].name, "");
	inode[i].type = '\0';
	inode[i].parent = -1;
	inode[i].BlockNum = 0;
	//多线程 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,      		// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/******************带形参的删除文件*******************/
void DelFile(int i) {
	//清空磁盘信息 
	fstream file;
	file.open(file_source, ios::binary | ios::out | ios::in);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}
	for (int j = 0; j < inode[i].BlockNum; j++) {
		super_block.Block[inode[i].BlockTable[j]] = false;
		byte b = 0x00;
		//将指针定位到当前磁盘块 
		file.seekp(BLOCK_SIZE * inode[i].BlockTable[j]);
		for (int k = 0; k < BLOCK_SIZE; k++)
			file.write((char*)&b, sizeof(b));
		inode[i].BlockTable[j] = -1;
	}
	//将相关节点初始化
	memset(inode[i].name, '\0', 20);
	inode[i].parent = -1;
	inode[i].length = 0;
	inode[i].right = '\0';
	inode[i].type = '\0';
	inode[i].BlockFirst = 0;
	inode[i].BlockNum = 0;
	super_block.Inode[i] = false;
	//多线程写文件 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,          	// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/*******************广度搜索算法*********************/
void BFS_Rmdir(LinkQueue& Aux, int i) {
	InitQueue(Aux);
	//首元结点入队 
	Data data;
	data.InodeNum = i;
	data.num = -1;
	EnQueue(Aux, data);
	//队列非空时 
	while (QueueEmpty(Aux) != 0) {
		for (int r = 0; r < INODE_NUM; r++) { //寻找首元结点的所有孩子
			if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '0') {
				//目录入队等待搜索是否有子目录 
				data.InodeNum = r;
				data.num = -1;
				EnQueue(Aux, data);
			}
			else if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '1') {
				DelFile(r);//文件没有子目录直接删除 
			}
		}
		DelDir(Aux.front->next->data.InodeNum);//删除首元结点代表的目录 
		DeQueue(Aux);//首元结点出队		
	}
}

/**********************删除目录**********************/
void Rmdir() {
	char name[50];
	cin >> name;
	int i = FindInode(name, '0', cur_inode);    //保存要删除目录的节点号 
	if (i == -1) {
		cout << "Error:目录错误，请重新输入！" << endl;
		return;
	}
	LinkQueue Aux_Rmdir;
	BFS_Rmdir(Aux_Rmdir, i);
	cout << "Successfully remove a direct" << endl << endl;
	//多线程 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,      		// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/*******************有形参的复制单个目录********************/
int CopyDir(int i, int j) {//原目录的节点号和新目录的父节点号 
	//找到未用节点 
	int k;
	for (k = 0; k < INODE_NUM; k++) {
		if (super_block.Inode[k] == false)
			break;
	}
	if (k >= INODE_NUM)
		return -1;
	else {
		//初始化节点 
		super_block.Inode[k] = true;
		strcpy(inode[k].name, inode[i].name);
		inode[k].type = '0';
		inode[k].parent = j;
		inode[k].BlockNum = inode[i].BlockNum;
		//多线程
		HANDLE hThread = CreateThread(
			NULL,              		// default security attributes
			0,                 		// 使用缺省的堆栈大小  
			MyWriteToFile,          // 线程入口函数 
			NULL,             		// 传给线程入口函数的参数 
			0,                 		// 使用缺省的生成状态 
			NULL);   				// 将线程id存入dwThread[i]中 
		CloseHandle(hThread);
		return k;
	}
}

/*******************有形参的复制文件********************/
int CopyFile(int i, int j) {//源文件的节点号和父节点号 
	//分配磁盘块号 与源文件块数相同的连续的几个块 
	int x, y;
	for (x = 70; x < BLOCK_NUM; x++) {
		for (y = 0; y < inode[i].BlockNum; y++) {
			if (super_block.Block[x + y] == true)
				break;
			else
				continue;
		}
		if (y == inode[i].BlockNum)
			break;
	}
	if (x >= BLOCK_NUM)
		cout << "Error:空间不足！" << endl;
	//x为新文件的第一块磁盘号 
	for (y = x; y < x + inode[i].BlockNum; y++)
		super_block.Block[y] = true;
	//寻找空的节点r
	int r;
	for (r = 0; r < INODE_NUM; r++) {
		if (super_block.Inode[r] == false) {
			super_block.Inode[r] = true;
			break;
		}
	}
	//对节点r设置源文件属性 
	strcpy(inode[r].name, inode[i].name);
	inode[r].type = '1';
	inode[r].parent = j; //放入目标目录下 
	inode[r].length = inode[i].length;
	inode[r].right = inode[i].right;
	inode[r].BlockNum = inode[i].BlockNum;
	inode[r].BlockFirst = x;
	for (y = 0; y < inode[r].BlockNum; y++) {
		inode[r].BlockTable[y] = x;
		x++;
	}
	//复制文件内容 对磁盘进行操作 
	fstream file_read, file_write;
	file_read.open(file_source, ios::binary | ios::out | ios::in);
	file_write.open(file_source, ios::binary | ios::out | ios::in);
	for (y = 0; y < inode[r].BlockNum; y++) {
		//定位要获取内容的位置
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * inode[i].BlockTable[y]);
		//开始获取内容
		char temp[1024];
		file_read.read((char*)&temp, BLOCK_SIZE);
		//定位要写入内容的位置
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * inode[r].BlockTable[y]);
		//复制内容
		file_write.write((char*)&temp, BLOCK_SIZE);
	}
	file_read.close();
	file_write.close();
	//多线程更新磁盘 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// 使用缺省的堆栈大小  
		MyWriteToFile,         	 		// 线程入口函数 
		NULL,             				// 传给线程入口函数的参数 
		0,                 				// 使用缺省的生成状态 
		NULL);   						// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
	return r;
}

/*******************广度搜索算法*********************/
void BFS(LinkQueue& Aux, int i, int j) {
	InitQueue(Aux);
	//首元结点入队 
	Data data;
	data.InodeNum = i;
	data.num = CopyDir(i, j);
	EnQueue(Aux, data);
	//队列非空时 
	while (QueueEmpty(Aux) != 0) {
		for (int r = 0; r < INODE_NUM; r++) { //寻找首元结点的所有孩子
			if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '0') {
				data.InodeNum = r;
				data.num = CopyDir(r, Aux.front->next->data.num);
				EnQueue(Aux, data);//入队时已经建立好父子关系 
			}
			else if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '1') {
				data.InodeNum = r;
				data.num = CopyFile(r, Aux.front->next->data.num);
				EnQueue(Aux, data);//入队时已经建立好父子关系 
			}
		}
		DeQueue(Aux);//首元结点出队 
	}
}

/**********************复制目录**********************/
void XCopy() {
	char a[50];    //原目录 
	char b[50];    //目标目录
	scanf("%s", a);
	scanf("%s", b);
	int i = FindInode(a, '0', cur_inode);
	if (i == -1) {
		cout << "Error:源目录不存在，请重新输入！" << endl;
		return;
	}
	int j = FindInode(b, '0', cur_inode);
	if (j == -1) {
		cout << "Error:目标目录不存在，请重新输入！" << endl;
		return;
	}
	LinkQueue Aux; //辅助队列
	BFS(Aux, i, j);
	cout << "Successfully copy a direct!" << endl << endl;
	//多线程更新磁盘 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,      		// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************创建文件**********************/
void Create() {
	char name[50];
	cin >> name;
	//判断文件是否存在 
	int i = FindInode(name, '1', cur_inode);
	if (i != -1) {
		cout << "Error:该文件已存在，请重新输入文件名！" << endl;
		return;
	}
	//寻找空的磁盘块号i 
	for (i = 70; i < BLOCK_NUM; i++) {
		if (super_block.Block[i] == false) {
			super_block.Block[i] = true;
			break;
		}
	}
	//寻找空的节点j
	int j;
	for (j = 0; j < INODE_NUM; j++) {
		if (super_block.Inode[j] == false) {
			super_block.Inode[j] = true;
			break;
		}
	}
	//对节点j进行初始化
	strcpy(inode[j].name, name);
	inode[j].type = '1';
	inode[j].parent = cur_inode;
	inode[j].length = 0;
	inode[j].BlockNum = 1;
	inode[j].BlockFirst = i;
	inode[j].BlockTable[0] = i;
	cout << "Successfully create a file!" << endl << endl;
	//多线程 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,          	// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************删除文件**********************/
void Del() {
	char name[50];
	cin >> name;
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:文件不存在，请重新输入！" << endl;
		return;
	}
	//清空磁盘信息 
	fstream file;
	file.open(file_source, ios::binary | ios::out | ios::in);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}
	for (int j = 0; j < inode[i].BlockNum; j++) {
		super_block.Block[inode[i].BlockTable[j]] = false;
		byte b = 0x00;
		//将指针定位到当前磁盘块 
		file.seekp(BLOCK_SIZE * inode[i].BlockTable[j]);
		for (int k = 0; k < BLOCK_SIZE; k++)
			file.write((char*)&b, sizeof(b));
		inode[i].BlockTable[j] = -1;
	}
	//将相关节点初始化
	memset(inode[i].name, '\0', 20);
	inode[i].parent = -1;
	inode[i].length = 0;
	inode[i].right = '\0';
	inode[i].type = '\0';
	inode[i].BlockFirst = 0;
	inode[i].BlockNum = 0;
	super_block.Inode[i] = false;
	cout << "Successfully delete a file!" << endl << endl;
	//多线程写文件 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,          	// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************编辑文件**********************/
void Edit() {
	char name[50];
	cin >> name;
	//i为该文件节点 
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:文件不存在，请重新输入！" << endl;
		return;
	}
	//只读文件不可编辑 
	if (inode[i].right == 'r') {
		cout << "Error:只读文件不可编辑" << endl;
		return;
	}
	//提示语 
	cout << "请选择：1.第一次写 2.重写 3.续写" << endl;
	int flag;
	cin >> flag;
	cout << "请输入内容($:空格 #:换行)：" << endl;
	char temp[1024];
	cin >> temp;
	//$作为空格 #作为换行 
	char Space = 32;
	char Enter = '\n';
	for (int k = 0; k < strlen(temp); k++) {
		if (temp[k] == '$')
			temp[k] = Space;
		else if (temp[k] == '#')
			temp[k] = Enter;
	}
	//开始编辑 
	switch (flag) {
	case 1:
	case 2: {
		fstream file;
		file.open(file_source, ios::binary | ios::out | ios::in);
		if (!file) {
			cout << "Error:Open File Failed!" << endl;
		}
		//清空磁盘中相关信息 	
		for (int j = 1; j < inode[i].BlockNum; j++) {
			super_block.Block[inode[i].BlockTable[j]] = false;
			byte b = 0x00;
			//将指针定位到当前磁盘块 
			file.seekp(BLOCK_SIZE * inode[i].BlockTable[j]);
			for (int k = 0; k < BLOCK_SIZE; k++)
				file.write((char*)&b, sizeof(b));
			inode[i].BlockTable[j] = -1;
		}
		inode[i].BlockNum = 0;
		inode[i].length = 0;
		//第一块单独处理，只修改内容，其他属性不变 
		byte b = 0x00;
		file.seekp(BLOCK_SIZE * inode[i].BlockTable[0]);
		for (int k = 0; k < BLOCK_SIZE; k++)
			file.write((char*)&b, sizeof(b));
		//写入新内容
		file.seekp(BLOCK_SIZE * inode[i].BlockFirst);
		file.write((char*)&temp, 1024);
		file.close();
		inode[i].BlockNum++;
		inode[i].length = strlen(temp);
		cout << "Successfully write!" << endl << endl;
		break;
	}
	case 3: {
		//续写的内容存入新的磁盘块
		int n;
		for (n = 70; n < BLOCK_NUM; n++) {
			if (super_block.Block[n] == false)
				break;
		}
		super_block.Block[n] = true;
		//寻找该节点的文件索引表中的空目录 
		int p;
		for (p = 0; p < 16; p++) {
			if (inode[i].BlockTable[p] == -1)
				break;
		}
		if (p >= 16) {
			cout << "Error:文件过大，无法续写！" << endl;
			return;
		}
		else
			inode[i].BlockTable[p] = n;
		//写入新内容
		fstream file;
		file.open(file_source, ios::binary | ios::out | ios::in);
		if (!file) {
			cout << "Error:Open File Failed!" << endl;
		}
		file.seekp(BLOCK_SIZE * n);
		file.write((char*)&temp, 1024);
		file.close();
		inode[i].BlockNum++;
		inode[i].length += strlen(temp);
		cout << "Successfully write!" << endl << endl;
		break;
	}
	}
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// 使用缺省的堆栈大小  
		MyWriteToFile,          	// 线程入口函数 
		NULL,             			// 传给线程入口函数的参数 
		0,                 			// 使用缺省的生成状态 
		NULL);   					// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************查看文件**********************/
void View() {
	char name[50];
	cin >> name;
	//i为该文件节点 
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:文件不存在，请重新输入！" << endl;
		return;
	}
	//只写文件不可查看 
	if (inode[i].right == 'w') {
		cout << "Error:只写文件不可查看 " << endl;
		return;
	}
	//根据文件索引表中的信息显示
	fstream file;
	file.open(file_source, ios::binary | ios::out | ios::in);
	if (!file) {
		cout << "Error:Open File Failed!" << endl << endl;
	}
	char temp[1024];
	int j = 0;
	while (j < inode[i].BlockNum) {
		file.seekg(ios::beg);
		file.seekg(BLOCK_SIZE * inode[i].BlockTable[j]);
		file.read((char*)&temp, 1024);
		cout << temp << endl;
		j++;
	}
	file.close();
	cout << endl;
}

/**********************文件属性**********************/
void Attrib() {
	char name[50];
	cin >> name;
	//i为该文件节点 
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:文件不存在，请重新输入！" << endl;
		return;
	}
	cout << "请输入要设置的属性：a:读写 w:只写 r:只读 h:隐藏" << endl;
	char flag;
	cin >> flag;
	inode[i].right = flag;
	cout << "Successfully modify the right!" << endl << endl;
	//多线程更新磁盘 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// 使用缺省的堆栈大小  
		MyWriteToFile,         	 		// 线程入口函数 
		NULL,             				// 传给线程入口函数的参数 
		0,                 				// 使用缺省的生成状态 
		NULL);   						// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************复制文件**********************/
void Copy() {
	char a[50];    //文件 
	char b[50];    //目标目录
	scanf("%s", a);
	scanf("%s", b);
	int i = FindInode(a, '1', cur_inode);
	if (i == -1) {
		cout << "Error:源文件不存在，请重新输入！" << endl;
		return;
	}
	int j = FindInode(b, '0', cur_inode);
	if (j == -1) {
		cout << "Error:目标目录不存在，请重新输入！" << endl;
		return;
	}
	//分配磁盘块号 与源文件块数相同的连续的几个块 
	int x, y;
	for (x = 70; x < BLOCK_NUM; x++) {
		for (y = 0; y < inode[i].BlockNum; y++) {
			if (super_block.Block[x + y] == true)
				break;
			else
				continue;
		}
		if (y == inode[i].BlockNum)
			break;
	}
	if (x >= BLOCK_NUM)
		cout << "Error:空间不足！" << endl;
	//x为新文件的第一块磁盘号 
	for (y = x; y < x + inode[i].BlockNum; y++)
		super_block.Block[y] = true;
	//寻找空的节点r
	int r;
	for (r = 0; r < INODE_NUM; r++) {
		if (super_block.Inode[r] == false) {
			super_block.Inode[r] = true;
			break;
		}
	}
	//对节点r设置源文件属性 
	strcpy(inode[r].name, inode[i].name);
	inode[r].type = '1';
	inode[r].parent = j; //放入目标目录下 
	inode[r].length = inode[i].length;
	inode[r].right = inode[i].right;
	inode[r].BlockNum = inode[i].BlockNum;
	inode[r].BlockFirst = x;
	for (y = 0; y < inode[r].BlockNum; y++) {
		inode[r].BlockTable[y] = x;
		x++;
	}
	//复制文件内容 对磁盘进行操作 
	fstream file_read, file_write;
	file_read.open(file_source, ios::binary | ios::out | ios::in);
	file_write.open(file_source, ios::binary | ios::out | ios::in);
	for (y = 0; y < inode[r].BlockNum; y++) {
		//定位要获取内容的位置
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * inode[i].BlockTable[y]);
		//开始获取内容
		char temp[1024];
		file_read.read((char*)&temp, BLOCK_SIZE);
		//定位要写入内容的位置
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * inode[r].BlockTable[y]);
		//复制内容
		file_write.write((char*)&temp, BLOCK_SIZE);
	}
	file_read.close();
	file_write.close();
	cout << "Successfully copy the file!" << endl << endl;
	//多线程更新磁盘 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// 使用缺省的堆栈大小  
		MyWriteToFile,         	 		// 线程入口函数 
		NULL,             				// 传给线程入口函数的参数 
		0,                 				// 使用缺省的生成状态 
		NULL);   						// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************导入文件**********************/
void Import() {
	char a[50];       //源文件路径 
	char b[50];		  //目标文件名 
	scanf("%s", a);
	scanf("%s", b);
	//读本地文件 
	fstream file_read;
	file_read.open(a, ios::binary | ios::out | ios::in);
	if (!file_read) {
		cout << "Error:路径错误，请重新输入!" << endl;
		return;
	}
	//获取文件长度
	file_read.seekg(0, ios::end);
	int size = file_read.tellg();
	//获取文件需要的磁盘块数
	int num;
	if (size % BLOCK_SIZE == 0)
		num = size / BLOCK_SIZE;
	else
		num = size / BLOCK_SIZE + 1;
	//判断重名 
	int i = FindInode(b, '1', cur_inode);
	if (i != -1) {
		cout << "Error:已有重名文件，请重新设置目标文件名!" << endl;
		return;
	}
	//分配磁盘块号 与源文件块数相同的连续的几个块 
	int x, y;
	for (x = 70; x < BLOCK_NUM; x++) {
		for (y = 0; y < num; y++) {
			if (super_block.Block[x + y] == true)
				break;
			else
				continue;
		}
		if (y == num)
			break;
	}
	if (x >= BLOCK_NUM) {
		cout << "Error:空间不足！" << endl;
		return;
	}
	//x为新文件的第一块磁盘号 
	for (y = x; y < x + num; y++)
		super_block.Block[y] = true;
	//寻找空的节点r
	int r;
	for (r = 0; r < INODE_NUM; r++) {
		if (super_block.Inode[r] == false) {
			super_block.Inode[r] = true;
			break;
		}
	}
	//对节点r设置源文件属性 
	strcpy(inode[r].name, b); //改为目标文件名 
	inode[r].type = '1';
	inode[r].parent = cur_inode;
	inode[r].length = size;
	inode[r].BlockNum = num;
	inode[r].BlockFirst = x;
	for (y = 0; y < inode[r].BlockNum; y++) {
		inode[r].BlockTable[y] = x;
		x++;
	}
	//将文件内容写入磁盘
	fstream file_write;
	file_write.open(file_source, ios::binary | ios::out | ios::in);
	for (y = 0; y < num; y++) {
		//定位要获取内容的位置
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * y);
		//开始获取内容
		char temp[1024];
		file_read.read((char*)&temp, BLOCK_SIZE);
		//定位要写入内容的位置
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * inode[r].BlockTable[y]);
		//复制内容
		file_write.write((char*)&temp, BLOCK_SIZE);
	}
	file_read.close();
	file_write.close();
	cout << "Successfully import the file!" << endl << endl;
	//多线程更新磁盘 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// 使用缺省的堆栈大小  
		MyWriteToFile,         	 		// 线程入口函数 
		NULL,             				// 传给线程入口函数的参数 
		0,                 				// 使用缺省的生成状态 
		NULL);   						// 将线程id存入dwThread[i]中 
	CloseHandle(hThread);
}

/**********************导出文件**********************/
void Export() {
	char a[50];       //当前目录下文件名 
	char b[50];		  //目标路径 
	scanf("%s", a);
	scanf("%s", b);
	//查找文件节点i 
	int i = FindInode(a, '1', cur_inode);
	if (i == -1) {
		cout << "Error:文件不存在，请重新输入！" << endl;
		return;
	}
	fstream file_read, file_write;
	//打开虚拟磁盘和导出的文件 
	file_read.open(file_source, ios::binary | ios::out | ios::in);
	file_write.open(b, ios::out);
	for (int j = 0; j < inode[i].BlockNum; j++) {
		//定位要获取内容的位置
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * inode[i].BlockTable[j]);
		//读取内容
		char temp[1024];
		file_read.read((char*)&temp, 1024);
		//定位写入位置
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * j);
		//复制内容
		file_write.write((char*)&temp, strlen(temp));
	}
	file_read.close();
	file_write.close();
	cout << "Successfully export the file!" << endl << endl;
}

/**********************查找文件**********************/
//初始化insert为'\\'
void InitInsert(char* insert) {
	insert[0] = '\\';
	insert[1] = '\\';
	for (int k = 2; k < 50; k++) {
		insert[k] = '\0';
	}
}
//初始化DirName为空
void InitDirName(char* DirName) {
	for (int k = 0; k < 50; k++) {
		DirName[k] = '\0';
	}
}
//查找 
void Find() {
	char name[50];			//文件名
	cin >> name;
	char path[50] = "";		//路径名
	char insert[50];		//存储要插入的字符     
	char DirName[50];       //存储要插入的目录名称   
	int flag;               //记录查找文件的节点号 
	int parent;
	for (int i = 0; i < INODE_NUM; i++) {//遍历 因为会有同名文件在不同目录 
		if (inode[i].type == '1' && strcmp(inode[i].name, name) == 0 && inode[i].right != 'h') {
			flag = i;
			strcat(path, name); //path添加文件名 
			while (inode[flag].parent != -1) {//当找到根目录时循环结束 
				parent = inode[flag].parent;
				if (parent == 0) { //根目录 
					//将'\\'插入path开头
					InitInsert(insert);     //初始化insert为'\\'
					strcat(insert, path);
					strcpy(path, insert);
					//将目录名称添加到path开头
					InitDirName(DirName);   //初始化DirName为空 
					strcpy(DirName, inode[parent].name);
					strcat(DirName, ":");
					strcat(DirName, path);
					strcpy(path, DirName);
				}
				else {//非根目录 
					//将'\\'插入path开头
					InitInsert(insert);     //初始化insert为'\\'
					strcat(insert, path);
					strcpy(path, insert);
					//将目录名称添加到path开头
					InitDirName(DirName);   //初始化DirName为空 
					strcpy(DirName, inode[parent].name);
					strcat(DirName, path);
					strcpy(path, DirName);
				}
				flag = parent;
			}
			//输出路径 
			cout << "Path: " << path << endl;
			for (int k = 0; k < 50; k++) {//初始化path 
				path[k] = '\0';
			}
		}
	}
}

/**********************帮助菜单**********************/
void Help() {
	cout << "***************************************************************************" << endl;
	cout << "* 1.cd + <string>                         更改当前目录                    *" << endl;
	cout << "* 2.dir                                   显示目录下所有内容              *" << endl;
	cout << "* 3.mkdir + <string>                      新建目录,目录名为<string>       *" << endl;
	cout << "* 4.rmdir + <string>                      删除目录,目录名为<string>       *" << endl;
	cout << "* 5.xcopy + <string> + <string>           复制目录,原目录 + 目标目录      *" << endl;
	cout << "* 6.create + <string>                     创建文件,文件名为<string>       *" << endl;
	cout << "* 7.edit + <string>                       编辑文件,文件名为<string>       *" << endl;
	cout << "* 8.view + <string>                       显示文件,文件名为<string>       *" << endl;
	cout << "* 9.attrib + <string>                     设置文件属性,文件名为<string>   *" << endl;
	cout << "* 10.find + <string>                      查找文件位置,文件名为<string>   *" << endl;
	cout << "* 11.del + <string>                       删除文件,文件名为<string>       *" << endl;
	cout << "* 12.copy + <string> + <string>           复制文件,<string>为目录及文件名 *" << endl;
	cout << "* 13.import + <string> + <string>         导入文件,目录及文件名 + 文件名  *" << endl;
	cout << "* 14.export + <string> + <string>         导出文件,文件名 + 目录及文件名  *" << endl;
	cout << "* 15.help                                 显示帮助菜单                    *" << endl;
	cout << "* 16.exit                                 退出                            *" << endl;
	cout << "***************************************************************************" << endl << endl;
}

/**********************退出程序**********************/
void Exit() {
	exit(0);
}

/*******************重新读磁盘文件*******************/
void ReadNew()
{
	fstream file;
	file.open(file_source, ios::out | ios::in | ios::binary);
	file.read((char*)&super_block, sizeof(SuperBlock));
	file.read((char*)&inode, sizeof(Inode) * INODE_NUM);
	file.close();
}

/**********************执行命令**********************/
void Cmd() {
	Init();
	printf("%s", Path);
	cout << ">";
	char temp[50];
	cin >> temp;
	while (1) {
		int i;
		ReadNew();
		for (i = 0; i < cmd_size; i++) {
			if (strcmp(temp, cmd[i].cmd_name) == 0) {
				cmd[i].fun();
				break;
			}
		}
		if (i >= cmd_size) {
			cout << "Error:您输入的命令有误，请输入help查询！" << endl;
		}
		printf("%s", Path);
		cout << ">";
		cin >> temp;
	}
}

/**********************主函数**********************/
int main() {
	cout << " ==============================================================================" << endl;
	cout << " ================================= File System ================================" << endl;
	cout << " ==============================================================================" << endl;
	cout << " 初次使用请输入format命令格式化系统" << endl;
	strcpy(file_source, "data.bin");
	File = CreateSemaphore(
		NULL,           // 使用缺省的安全属性。default security attributes
		1,  			// 初始信号量空闲资源值。initial count
		1,  			// 设置信号量空闲资源最大值。maximum count
		NULL);          // 不给信号量命名。unnamed semaphore
						//如果返回值为空，则信号量生成失败
	if (File == NULL)
	{
		printf("CreateSemaphore error: %d\n", GetLastError());
		return 0;
	}
	Cmd();
	return 0;
}
