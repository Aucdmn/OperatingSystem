#include<iostream>
#include<fstream>
#include<string.h>
#include<stdlib.h>
#include<tchar.h>
#include<Windows.h>
#include<iomanip>
using namespace std;

#define DISK_SIZE 1024*1024   //���̴�С 
#define BLOCK_SIZE 1024       //ÿ�����̿�1024b 
#define BLOCK_NUM 1024        //����1024�����̿� 
#define INODE_NUM 1024        //�����ڵ�����1024
#define ERROR 0
#define OK 1

/*********************����һ��*********************/
void Format();  	//��ʽ�� 
void Cd();			//����Ŀ¼ 
void Dir();			//�г���ǰĿ¼���ļ� 
void Mkdir();		//����Ŀ¼ 
void Rmdir();		//ɾ��Ŀ¼ 
void XCopy();		//����Ŀ¼ 
void Create();		//�����ļ�
void Edit();		//�༭�ļ�
void View();		//��ʾ�ļ� 
void Attrib();		//�����ļ����� a:��д  w:ֻд  r:ֻ��  h:����  
void Find();		//�����ļ�λ�� 
void Del();    		//ɾ���ļ� 
void Copy();		//�����ļ� 
void Import();		//�����ļ�
void Export();		//�����ļ�
void Help();		//�����˵� 
void Exit();		//�˳� 

/*********************�ṹ�嶨��*********************/
struct SuperBlock {
	bool Block[BLOCK_NUM];   //��¼ÿ�����̿��ʹ����� trueδ�� flase���� 
	bool Inode[INODE_NUM];   //��¼ÿ���ڵ�ʹ����� trueδ�� flase���� 
}super_block;

struct Inode {
	char name[20];           //�ļ��� 
	char type;               //����  0-Ŀ¼ 1-�ļ� 
	char right;				 //�ļ�Ȩ��  a:��д  w:ֻд  r:ֻ��  h:����
	int length;				 //�ļ����� 
	short parent;			 //��Ŀ¼�ڵ�� 
	short BlockNum;			 //ռ�еĴ��̿����� 
	short BlockFirst;        //ռ�еĵ�һ����̺� 
	short BlockTable[16];	 //�ļ������� 
}inode[INODE_NUM];

typedef void(*CMDFUN)();
struct CMD {
	char cmd_name[20];   //������ 
	CMDFUN fun; 		 //������ 
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

/*****************������е���ʽ�洢�ṹ*******************/
/***********************BFS�ĸ�������********************/
struct Data {
	int InodeNum;  //ԭ�ڵ���� 
	int num;       //�½ڵ���� 
};

//������ 
typedef struct node {
	Data data;
	struct node* next;
}node, * queue;

//����
typedef struct LinkQueue {
	queue front;   //ͷ��㣬front->nextΪ��ͷ 
	queue rear;    //��β
}LinkQueue;

/*******************���л�������**************************/
//��ʼ������ 
int InitQueue(LinkQueue& Q) {
	Q.front = Q.rear = new node;
	Q.front->next = NULL;
	return OK;
}

//�ж϶����Ƿ�Ϊ�� ��Ϊ0 �ǿ�Ϊ1
int QueueEmpty(LinkQueue& Q) {
	if (Q.front == Q.rear)
		return ERROR;
	else
		return OK;
}

//���
int EnQueue(LinkQueue& Q, Data e) {//�β�Ϊԭ���к��½������� 
	//����һ���½�� 
	queue p = new node;
	p->data = e;  //�����ݴ�����½�� 
	p->next = NULL;
	//�����β 
	Q.rear->next = p;
	Q.rear = p;
	return OK;
}

//���� 
int DeQueue(LinkQueue& Q) {
	queue p = new node;
	if (Q.front == Q.rear)
		return ERROR;
	p = Q.front->next; //p�����ͷ 
	//e = p->data;
	Q.front->next = p->next;
	if (Q.rear == p) //�ؼ� 
		Q.rear = Q.front;
	delete p;
	return OK;
}

/*********************���ȫ�ֱ���*********************/
HANDLE File;		     //������� ������ָ��  
int cur_inode;           //��¼��ǰ�ڵ��
char Path[50];           //��¼��ǰ·�� 
char file_source[50];    //data.bin�ļ�ģ����������� 
int cmd_size = sizeof(cmd) / sizeof(CMD); //������� 

/**********************���̸߳��´���**********************/
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

/**********************��ʼ��**********************/
void Init() {
	cur_inode = 0;
	strcat(Path, "Root:");
	//��ȡ������Ϣ 
	ifstream file;
	file.open(file_source);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}
	file.read((char*)&super_block, sizeof(SuperBlock));
	file.read((char*)&inode, sizeof(Inode) * INODE_NUM);
	file.close();
}

/**********************��ʽ��**********************/
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

	//��ʼ������������Ϣ ���̿�ͽڵ㶼δ��false 
	for (int i = 0; i < BLOCK_NUM; i++) {
		super_block.Block[i] = false;
	}
	for (int i = 0; i < INODE_NUM; i++) {
		super_block.Inode[i] = false;
	}
	//0-1�����̿�洢��������Ϣ��2-69�����̿�洢�ڵ���Ϣ
	//ǰ70�������� 
	for (int i = 0; i < 70; i++) {
		super_block.Block[i] = true;
	}

	//��ʼ��ÿ���ڵ�Ľڵ�������͸��ڵ� 
	for (int i = 0; i < INODE_NUM; i++) {
		memset(inode[i].name, '\0', 20);
		//��BlockTable����ĵ�ǰλ������16*..���ֽ���-1�滻 
		memset(inode[i].BlockTable, -1, 16 * sizeof(short));
		inode[i].type = '\0';
		inode[i].parent = -1;
	}
	//����һ���ڵ㶨��Ϊ���ڵ� 
	super_block.Inode[0] = true;
	strcpy(inode[0].name, "Root");
	inode[0].type = '0';       //����ΪĿ¼ 
	inode[0].parent = -1;
	inode[0].BlockNum = 0;
	cout << "Successfully format!" << endl << endl;
	//���̸߳��´��� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,          	// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/***************Ѱ���ļ�/Ŀ¼�ڵ�*******************/
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

/**********************����Ŀ¼**********************/
void Cd() {
	char temp[50];
	cin >> temp;
	//������һ��Ŀ¼ 
	if (strcmp(temp, "..") == 0) {
		if (cur_inode == 0)
			cout << "Error:�Ѿ��Ǹ�Ŀ¼���޷�������һ����" << endl;
		else {
			//�޸�·�� 
			int num = strlen(inode[cur_inode].name);
			for (int k = 0; k < num + 2; k++) {
				Path[strlen(Path) - 1] = '\0';
			}
			//�޸ĵ�ǰ�ڵ� 
			cur_inode = inode[cur_inode].parent;
		}
	}
	//������һ��Ŀ¼
	else {
		//�ҵ���tempĿ¼�ڵ� 
		int index = FindInode(temp, '0', cur_inode);
		if (index == -1)
			cout << "Error:�Ҳ�����Ŀ¼�����������룡" << endl;
		else {
			//���¶�λ��ǰ�ڵ� 
			cur_inode = index;
			//����·�� 
			strcat(Path, "\\");
			strcat(Path, "\\");
			strcat(Path, inode[cur_inode].name);
		}
	}
}

/**********************��ʾĿ¼**********************/
void Dir() {
	cout << left << setw(25) << "Name" << left << setw(25) << "Type" << left << setw(25) << "Size" << endl;
	for (int i = 0; i < INODE_NUM; i++) {
		//�����ļ�������ʾ 
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

/**********************����Ŀ¼**********************/
void Mkdir() {
	char name[50];
	cin >> name;        //����Ŀ¼��
	int i;            //��¼�ҵ��Ŀսڵ�ռ�
	for (i = 0; i < INODE_NUM; i++) {
		//��ǰĿ¼����Ŀ¼����ͬ��Ŀ¼ 
		if (inode[i].parent == cur_inode && strcmp(inode[i].name, name) == 0 && inode[i].type == '0') {
			cout << "��Ŀ¼�Ѵ��ڣ�����ʧ�ܣ�" << endl;
			return;
		}
	}
	//�ҵ�δ�ýڵ� 
	for (i = 0; i < INODE_NUM; i++) {
		if (super_block.Inode[i] == false)
			break;
	}
	//��ʼ���ڵ� 
	super_block.Inode[i] = true;
	strcpy(inode[i].name, name);
	inode[i].type = '0';
	inode[i].parent = cur_inode;
	inode[i].BlockNum = 0;
	cout << "Successfully make a direct!" << endl << endl;
	//���߳�
	HANDLE hThread = CreateThread(
		NULL,              		// default security attributes
		0,                 		// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,          // �߳���ں��� 
		NULL,             		// �����߳���ں����Ĳ��� 
		0,                 		// ʹ��ȱʡ������״̬ 
		NULL);   				// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
	return;
}

/********************ɾ������Ŀ¼********************/
void DelDir(int i) {
	super_block.Inode[i] = false;
	strcpy(inode[i].name, "");
	inode[i].type = '\0';
	inode[i].parent = -1;
	inode[i].BlockNum = 0;
	//���߳� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,      		// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/******************���βε�ɾ���ļ�*******************/
void DelFile(int i) {
	//��մ�����Ϣ 
	fstream file;
	file.open(file_source, ios::binary | ios::out | ios::in);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}
	for (int j = 0; j < inode[i].BlockNum; j++) {
		super_block.Block[inode[i].BlockTable[j]] = false;
		byte b = 0x00;
		//��ָ�붨λ����ǰ���̿� 
		file.seekp(BLOCK_SIZE * inode[i].BlockTable[j]);
		for (int k = 0; k < BLOCK_SIZE; k++)
			file.write((char*)&b, sizeof(b));
		inode[i].BlockTable[j] = -1;
	}
	//����ؽڵ��ʼ��
	memset(inode[i].name, '\0', 20);
	inode[i].parent = -1;
	inode[i].length = 0;
	inode[i].right = '\0';
	inode[i].type = '\0';
	inode[i].BlockFirst = 0;
	inode[i].BlockNum = 0;
	super_block.Inode[i] = false;
	//���߳�д�ļ� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,          	// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/*******************��������㷨*********************/
void BFS_Rmdir(LinkQueue& Aux, int i) {
	InitQueue(Aux);
	//��Ԫ������ 
	Data data;
	data.InodeNum = i;
	data.num = -1;
	EnQueue(Aux, data);
	//���зǿ�ʱ 
	while (QueueEmpty(Aux) != 0) {
		for (int r = 0; r < INODE_NUM; r++) { //Ѱ����Ԫ�������к���
			if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '0') {
				//Ŀ¼��ӵȴ������Ƿ�����Ŀ¼ 
				data.InodeNum = r;
				data.num = -1;
				EnQueue(Aux, data);
			}
			else if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '1') {
				DelFile(r);//�ļ�û����Ŀ¼ֱ��ɾ�� 
			}
		}
		DelDir(Aux.front->next->data.InodeNum);//ɾ����Ԫ�������Ŀ¼ 
		DeQueue(Aux);//��Ԫ������		
	}
}

/**********************ɾ��Ŀ¼**********************/
void Rmdir() {
	char name[50];
	cin >> name;
	int i = FindInode(name, '0', cur_inode);    //����Ҫɾ��Ŀ¼�Ľڵ�� 
	if (i == -1) {
		cout << "Error:Ŀ¼�������������룡" << endl;
		return;
	}
	LinkQueue Aux_Rmdir;
	BFS_Rmdir(Aux_Rmdir, i);
	cout << "Successfully remove a direct" << endl << endl;
	//���߳� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,      		// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/*******************���βεĸ��Ƶ���Ŀ¼********************/
int CopyDir(int i, int j) {//ԭĿ¼�Ľڵ�ź���Ŀ¼�ĸ��ڵ�� 
	//�ҵ�δ�ýڵ� 
	int k;
	for (k = 0; k < INODE_NUM; k++) {
		if (super_block.Inode[k] == false)
			break;
	}
	if (k >= INODE_NUM)
		return -1;
	else {
		//��ʼ���ڵ� 
		super_block.Inode[k] = true;
		strcpy(inode[k].name, inode[i].name);
		inode[k].type = '0';
		inode[k].parent = j;
		inode[k].BlockNum = inode[i].BlockNum;
		//���߳�
		HANDLE hThread = CreateThread(
			NULL,              		// default security attributes
			0,                 		// ʹ��ȱʡ�Ķ�ջ��С  
			MyWriteToFile,          // �߳���ں��� 
			NULL,             		// �����߳���ں����Ĳ��� 
			0,                 		// ʹ��ȱʡ������״̬ 
			NULL);   				// ���߳�id����dwThread[i]�� 
		CloseHandle(hThread);
		return k;
	}
}

/*******************���βεĸ����ļ�********************/
int CopyFile(int i, int j) {//Դ�ļ��Ľڵ�ź͸��ڵ�� 
	//������̿�� ��Դ�ļ�������ͬ�������ļ����� 
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
		cout << "Error:�ռ䲻�㣡" << endl;
	//xΪ���ļ��ĵ�һ����̺� 
	for (y = x; y < x + inode[i].BlockNum; y++)
		super_block.Block[y] = true;
	//Ѱ�ҿյĽڵ�r
	int r;
	for (r = 0; r < INODE_NUM; r++) {
		if (super_block.Inode[r] == false) {
			super_block.Inode[r] = true;
			break;
		}
	}
	//�Խڵ�r����Դ�ļ����� 
	strcpy(inode[r].name, inode[i].name);
	inode[r].type = '1';
	inode[r].parent = j; //����Ŀ��Ŀ¼�� 
	inode[r].length = inode[i].length;
	inode[r].right = inode[i].right;
	inode[r].BlockNum = inode[i].BlockNum;
	inode[r].BlockFirst = x;
	for (y = 0; y < inode[r].BlockNum; y++) {
		inode[r].BlockTable[y] = x;
		x++;
	}
	//�����ļ����� �Դ��̽��в��� 
	fstream file_read, file_write;
	file_read.open(file_source, ios::binary | ios::out | ios::in);
	file_write.open(file_source, ios::binary | ios::out | ios::in);
	for (y = 0; y < inode[r].BlockNum; y++) {
		//��λҪ��ȡ���ݵ�λ��
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * inode[i].BlockTable[y]);
		//��ʼ��ȡ����
		char temp[1024];
		file_read.read((char*)&temp, BLOCK_SIZE);
		//��λҪд�����ݵ�λ��
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * inode[r].BlockTable[y]);
		//��������
		file_write.write((char*)&temp, BLOCK_SIZE);
	}
	file_read.close();
	file_write.close();
	//���̸߳��´��� 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,         	 		// �߳���ں��� 
		NULL,             				// �����߳���ں����Ĳ��� 
		0,                 				// ʹ��ȱʡ������״̬ 
		NULL);   						// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
	return r;
}

/*******************��������㷨*********************/
void BFS(LinkQueue& Aux, int i, int j) {
	InitQueue(Aux);
	//��Ԫ������ 
	Data data;
	data.InodeNum = i;
	data.num = CopyDir(i, j);
	EnQueue(Aux, data);
	//���зǿ�ʱ 
	while (QueueEmpty(Aux) != 0) {
		for (int r = 0; r < INODE_NUM; r++) { //Ѱ����Ԫ�������к���
			if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '0') {
				data.InodeNum = r;
				data.num = CopyDir(r, Aux.front->next->data.num);
				EnQueue(Aux, data);//���ʱ�Ѿ������ø��ӹ�ϵ 
			}
			else if (inode[r].parent == Aux.front->next->data.InodeNum && inode[r].type == '1') {
				data.InodeNum = r;
				data.num = CopyFile(r, Aux.front->next->data.num);
				EnQueue(Aux, data);//���ʱ�Ѿ������ø��ӹ�ϵ 
			}
		}
		DeQueue(Aux);//��Ԫ������ 
	}
}

/**********************����Ŀ¼**********************/
void XCopy() {
	char a[50];    //ԭĿ¼ 
	char b[50];    //Ŀ��Ŀ¼
	scanf("%s", a);
	scanf("%s", b);
	int i = FindInode(a, '0', cur_inode);
	if (i == -1) {
		cout << "Error:ԴĿ¼�����ڣ����������룡" << endl;
		return;
	}
	int j = FindInode(b, '0', cur_inode);
	if (j == -1) {
		cout << "Error:Ŀ��Ŀ¼�����ڣ����������룡" << endl;
		return;
	}
	LinkQueue Aux; //��������
	BFS(Aux, i, j);
	cout << "Successfully copy a direct!" << endl << endl;
	//���̸߳��´��� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,      		// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************�����ļ�**********************/
void Create() {
	char name[50];
	cin >> name;
	//�ж��ļ��Ƿ���� 
	int i = FindInode(name, '1', cur_inode);
	if (i != -1) {
		cout << "Error:���ļ��Ѵ��ڣ������������ļ�����" << endl;
		return;
	}
	//Ѱ�ҿյĴ��̿��i 
	for (i = 70; i < BLOCK_NUM; i++) {
		if (super_block.Block[i] == false) {
			super_block.Block[i] = true;
			break;
		}
	}
	//Ѱ�ҿյĽڵ�j
	int j;
	for (j = 0; j < INODE_NUM; j++) {
		if (super_block.Inode[j] == false) {
			super_block.Inode[j] = true;
			break;
		}
	}
	//�Խڵ�j���г�ʼ��
	strcpy(inode[j].name, name);
	inode[j].type = '1';
	inode[j].parent = cur_inode;
	inode[j].length = 0;
	inode[j].BlockNum = 1;
	inode[j].BlockFirst = i;
	inode[j].BlockTable[0] = i;
	cout << "Successfully create a file!" << endl << endl;
	//���߳� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,          	// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************ɾ���ļ�**********************/
void Del() {
	char name[50];
	cin >> name;
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:�ļ������ڣ����������룡" << endl;
		return;
	}
	//��մ�����Ϣ 
	fstream file;
	file.open(file_source, ios::binary | ios::out | ios::in);
	if (!file) {
		cout << "Error:Open File Failed!" << endl;
	}
	for (int j = 0; j < inode[i].BlockNum; j++) {
		super_block.Block[inode[i].BlockTable[j]] = false;
		byte b = 0x00;
		//��ָ�붨λ����ǰ���̿� 
		file.seekp(BLOCK_SIZE * inode[i].BlockTable[j]);
		for (int k = 0; k < BLOCK_SIZE; k++)
			file.write((char*)&b, sizeof(b));
		inode[i].BlockTable[j] = -1;
	}
	//����ؽڵ��ʼ��
	memset(inode[i].name, '\0', 20);
	inode[i].parent = -1;
	inode[i].length = 0;
	inode[i].right = '\0';
	inode[i].type = '\0';
	inode[i].BlockFirst = 0;
	inode[i].BlockNum = 0;
	super_block.Inode[i] = false;
	cout << "Successfully delete a file!" << endl << endl;
	//���߳�д�ļ� 
	HANDLE hThread = CreateThread(
		NULL,              			// default security attributes
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,          	// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************�༭�ļ�**********************/
void Edit() {
	char name[50];
	cin >> name;
	//iΪ���ļ��ڵ� 
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:�ļ������ڣ����������룡" << endl;
		return;
	}
	//ֻ���ļ����ɱ༭ 
	if (inode[i].right == 'r') {
		cout << "Error:ֻ���ļ����ɱ༭" << endl;
		return;
	}
	//��ʾ�� 
	cout << "��ѡ��1.��һ��д 2.��д 3.��д" << endl;
	int flag;
	cin >> flag;
	cout << "����������($:�ո� #:����)��" << endl;
	char temp[1024];
	cin >> temp;
	//$��Ϊ�ո� #��Ϊ���� 
	char Space = 32;
	char Enter = '\n';
	for (int k = 0; k < strlen(temp); k++) {
		if (temp[k] == '$')
			temp[k] = Space;
		else if (temp[k] == '#')
			temp[k] = Enter;
	}
	//��ʼ�༭ 
	switch (flag) {
	case 1:
	case 2: {
		fstream file;
		file.open(file_source, ios::binary | ios::out | ios::in);
		if (!file) {
			cout << "Error:Open File Failed!" << endl;
		}
		//��մ����������Ϣ 	
		for (int j = 1; j < inode[i].BlockNum; j++) {
			super_block.Block[inode[i].BlockTable[j]] = false;
			byte b = 0x00;
			//��ָ�붨λ����ǰ���̿� 
			file.seekp(BLOCK_SIZE * inode[i].BlockTable[j]);
			for (int k = 0; k < BLOCK_SIZE; k++)
				file.write((char*)&b, sizeof(b));
			inode[i].BlockTable[j] = -1;
		}
		inode[i].BlockNum = 0;
		inode[i].length = 0;
		//��һ�鵥������ֻ�޸����ݣ��������Բ��� 
		byte b = 0x00;
		file.seekp(BLOCK_SIZE * inode[i].BlockTable[0]);
		for (int k = 0; k < BLOCK_SIZE; k++)
			file.write((char*)&b, sizeof(b));
		//д��������
		file.seekp(BLOCK_SIZE * inode[i].BlockFirst);
		file.write((char*)&temp, 1024);
		file.close();
		inode[i].BlockNum++;
		inode[i].length = strlen(temp);
		cout << "Successfully write!" << endl << endl;
		break;
	}
	case 3: {
		//��д�����ݴ����µĴ��̿�
		int n;
		for (n = 70; n < BLOCK_NUM; n++) {
			if (super_block.Block[n] == false)
				break;
		}
		super_block.Block[n] = true;
		//Ѱ�Ҹýڵ���ļ��������еĿ�Ŀ¼ 
		int p;
		for (p = 0; p < 16; p++) {
			if (inode[i].BlockTable[p] == -1)
				break;
		}
		if (p >= 16) {
			cout << "Error:�ļ������޷���д��" << endl;
			return;
		}
		else
			inode[i].BlockTable[p] = n;
		//д��������
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
		0,                 			// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,          	// �߳���ں��� 
		NULL,             			// �����߳���ں����Ĳ��� 
		0,                 			// ʹ��ȱʡ������״̬ 
		NULL);   					// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************�鿴�ļ�**********************/
void View() {
	char name[50];
	cin >> name;
	//iΪ���ļ��ڵ� 
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:�ļ������ڣ����������룡" << endl;
		return;
	}
	//ֻд�ļ����ɲ鿴 
	if (inode[i].right == 'w') {
		cout << "Error:ֻд�ļ����ɲ鿴 " << endl;
		return;
	}
	//�����ļ��������е���Ϣ��ʾ
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

/**********************�ļ�����**********************/
void Attrib() {
	char name[50];
	cin >> name;
	//iΪ���ļ��ڵ� 
	int i = FindInode(name, '1', cur_inode);
	if (i == -1) {
		cout << "Error:�ļ������ڣ����������룡" << endl;
		return;
	}
	cout << "������Ҫ���õ����ԣ�a:��д w:ֻд r:ֻ�� h:����" << endl;
	char flag;
	cin >> flag;
	inode[i].right = flag;
	cout << "Successfully modify the right!" << endl << endl;
	//���̸߳��´��� 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,         	 		// �߳���ں��� 
		NULL,             				// �����߳���ں����Ĳ��� 
		0,                 				// ʹ��ȱʡ������״̬ 
		NULL);   						// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************�����ļ�**********************/
void Copy() {
	char a[50];    //�ļ� 
	char b[50];    //Ŀ��Ŀ¼
	scanf("%s", a);
	scanf("%s", b);
	int i = FindInode(a, '1', cur_inode);
	if (i == -1) {
		cout << "Error:Դ�ļ������ڣ����������룡" << endl;
		return;
	}
	int j = FindInode(b, '0', cur_inode);
	if (j == -1) {
		cout << "Error:Ŀ��Ŀ¼�����ڣ����������룡" << endl;
		return;
	}
	//������̿�� ��Դ�ļ�������ͬ�������ļ����� 
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
		cout << "Error:�ռ䲻�㣡" << endl;
	//xΪ���ļ��ĵ�һ����̺� 
	for (y = x; y < x + inode[i].BlockNum; y++)
		super_block.Block[y] = true;
	//Ѱ�ҿյĽڵ�r
	int r;
	for (r = 0; r < INODE_NUM; r++) {
		if (super_block.Inode[r] == false) {
			super_block.Inode[r] = true;
			break;
		}
	}
	//�Խڵ�r����Դ�ļ����� 
	strcpy(inode[r].name, inode[i].name);
	inode[r].type = '1';
	inode[r].parent = j; //����Ŀ��Ŀ¼�� 
	inode[r].length = inode[i].length;
	inode[r].right = inode[i].right;
	inode[r].BlockNum = inode[i].BlockNum;
	inode[r].BlockFirst = x;
	for (y = 0; y < inode[r].BlockNum; y++) {
		inode[r].BlockTable[y] = x;
		x++;
	}
	//�����ļ����� �Դ��̽��в��� 
	fstream file_read, file_write;
	file_read.open(file_source, ios::binary | ios::out | ios::in);
	file_write.open(file_source, ios::binary | ios::out | ios::in);
	for (y = 0; y < inode[r].BlockNum; y++) {
		//��λҪ��ȡ���ݵ�λ��
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * inode[i].BlockTable[y]);
		//��ʼ��ȡ����
		char temp[1024];
		file_read.read((char*)&temp, BLOCK_SIZE);
		//��λҪд�����ݵ�λ��
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * inode[r].BlockTable[y]);
		//��������
		file_write.write((char*)&temp, BLOCK_SIZE);
	}
	file_read.close();
	file_write.close();
	cout << "Successfully copy the file!" << endl << endl;
	//���̸߳��´��� 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,         	 		// �߳���ں��� 
		NULL,             				// �����߳���ں����Ĳ��� 
		0,                 				// ʹ��ȱʡ������״̬ 
		NULL);   						// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************�����ļ�**********************/
void Import() {
	char a[50];       //Դ�ļ�·�� 
	char b[50];		  //Ŀ���ļ��� 
	scanf("%s", a);
	scanf("%s", b);
	//�������ļ� 
	fstream file_read;
	file_read.open(a, ios::binary | ios::out | ios::in);
	if (!file_read) {
		cout << "Error:·����������������!" << endl;
		return;
	}
	//��ȡ�ļ�����
	file_read.seekg(0, ios::end);
	int size = file_read.tellg();
	//��ȡ�ļ���Ҫ�Ĵ��̿���
	int num;
	if (size % BLOCK_SIZE == 0)
		num = size / BLOCK_SIZE;
	else
		num = size / BLOCK_SIZE + 1;
	//�ж����� 
	int i = FindInode(b, '1', cur_inode);
	if (i != -1) {
		cout << "Error:���������ļ�������������Ŀ���ļ���!" << endl;
		return;
	}
	//������̿�� ��Դ�ļ�������ͬ�������ļ����� 
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
		cout << "Error:�ռ䲻�㣡" << endl;
		return;
	}
	//xΪ���ļ��ĵ�һ����̺� 
	for (y = x; y < x + num; y++)
		super_block.Block[y] = true;
	//Ѱ�ҿյĽڵ�r
	int r;
	for (r = 0; r < INODE_NUM; r++) {
		if (super_block.Inode[r] == false) {
			super_block.Inode[r] = true;
			break;
		}
	}
	//�Խڵ�r����Դ�ļ����� 
	strcpy(inode[r].name, b); //��ΪĿ���ļ��� 
	inode[r].type = '1';
	inode[r].parent = cur_inode;
	inode[r].length = size;
	inode[r].BlockNum = num;
	inode[r].BlockFirst = x;
	for (y = 0; y < inode[r].BlockNum; y++) {
		inode[r].BlockTable[y] = x;
		x++;
	}
	//���ļ�����д�����
	fstream file_write;
	file_write.open(file_source, ios::binary | ios::out | ios::in);
	for (y = 0; y < num; y++) {
		//��λҪ��ȡ���ݵ�λ��
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * y);
		//��ʼ��ȡ����
		char temp[1024];
		file_read.read((char*)&temp, BLOCK_SIZE);
		//��λҪд�����ݵ�λ��
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * inode[r].BlockTable[y]);
		//��������
		file_write.write((char*)&temp, BLOCK_SIZE);
	}
	file_read.close();
	file_write.close();
	cout << "Successfully import the file!" << endl << endl;
	//���̸߳��´��� 
	HANDLE hThread = CreateThread(
		NULL,              				// default security attributes
		0,                 				// ʹ��ȱʡ�Ķ�ջ��С  
		MyWriteToFile,         	 		// �߳���ں��� 
		NULL,             				// �����߳���ں����Ĳ��� 
		0,                 				// ʹ��ȱʡ������״̬ 
		NULL);   						// ���߳�id����dwThread[i]�� 
	CloseHandle(hThread);
}

/**********************�����ļ�**********************/
void Export() {
	char a[50];       //��ǰĿ¼���ļ��� 
	char b[50];		  //Ŀ��·�� 
	scanf("%s", a);
	scanf("%s", b);
	//�����ļ��ڵ�i 
	int i = FindInode(a, '1', cur_inode);
	if (i == -1) {
		cout << "Error:�ļ������ڣ����������룡" << endl;
		return;
	}
	fstream file_read, file_write;
	//��������̺͵������ļ� 
	file_read.open(file_source, ios::binary | ios::out | ios::in);
	file_write.open(b, ios::out);
	for (int j = 0; j < inode[i].BlockNum; j++) {
		//��λҪ��ȡ���ݵ�λ��
		file_read.seekg(0, ios::beg);
		file_read.seekg(BLOCK_SIZE * inode[i].BlockTable[j]);
		//��ȡ����
		char temp[1024];
		file_read.read((char*)&temp, 1024);
		//��λд��λ��
		file_write.seekp(0, ios::beg);
		file_write.seekp(BLOCK_SIZE * j);
		//��������
		file_write.write((char*)&temp, strlen(temp));
	}
	file_read.close();
	file_write.close();
	cout << "Successfully export the file!" << endl << endl;
}

/**********************�����ļ�**********************/
//��ʼ��insertΪ'\\'
void InitInsert(char* insert) {
	insert[0] = '\\';
	insert[1] = '\\';
	for (int k = 2; k < 50; k++) {
		insert[k] = '\0';
	}
}
//��ʼ��DirNameΪ��
void InitDirName(char* DirName) {
	for (int k = 0; k < 50; k++) {
		DirName[k] = '\0';
	}
}
//���� 
void Find() {
	char name[50];			//�ļ���
	cin >> name;
	char path[50] = "";		//·����
	char insert[50];		//�洢Ҫ������ַ�     
	char DirName[50];       //�洢Ҫ�����Ŀ¼����   
	int flag;               //��¼�����ļ��Ľڵ�� 
	int parent;
	for (int i = 0; i < INODE_NUM; i++) {//���� ��Ϊ����ͬ���ļ��ڲ�ͬĿ¼ 
		if (inode[i].type == '1' && strcmp(inode[i].name, name) == 0 && inode[i].right != 'h') {
			flag = i;
			strcat(path, name); //path����ļ��� 
			while (inode[flag].parent != -1) {//���ҵ���Ŀ¼ʱѭ������ 
				parent = inode[flag].parent;
				if (parent == 0) { //��Ŀ¼ 
					//��'\\'����path��ͷ
					InitInsert(insert);     //��ʼ��insertΪ'\\'
					strcat(insert, path);
					strcpy(path, insert);
					//��Ŀ¼������ӵ�path��ͷ
					InitDirName(DirName);   //��ʼ��DirNameΪ�� 
					strcpy(DirName, inode[parent].name);
					strcat(DirName, ":");
					strcat(DirName, path);
					strcpy(path, DirName);
				}
				else {//�Ǹ�Ŀ¼ 
					//��'\\'����path��ͷ
					InitInsert(insert);     //��ʼ��insertΪ'\\'
					strcat(insert, path);
					strcpy(path, insert);
					//��Ŀ¼������ӵ�path��ͷ
					InitDirName(DirName);   //��ʼ��DirNameΪ�� 
					strcpy(DirName, inode[parent].name);
					strcat(DirName, path);
					strcpy(path, DirName);
				}
				flag = parent;
			}
			//���·�� 
			cout << "Path: " << path << endl;
			for (int k = 0; k < 50; k++) {//��ʼ��path 
				path[k] = '\0';
			}
		}
	}
}

/**********************�����˵�**********************/
void Help() {
	cout << "***************************************************************************" << endl;
	cout << "* 1.cd + <string>                         ���ĵ�ǰĿ¼                    *" << endl;
	cout << "* 2.dir                                   ��ʾĿ¼����������              *" << endl;
	cout << "* 3.mkdir + <string>                      �½�Ŀ¼,Ŀ¼��Ϊ<string>       *" << endl;
	cout << "* 4.rmdir + <string>                      ɾ��Ŀ¼,Ŀ¼��Ϊ<string>       *" << endl;
	cout << "* 5.xcopy + <string> + <string>           ����Ŀ¼,ԭĿ¼ + Ŀ��Ŀ¼      *" << endl;
	cout << "* 6.create + <string>                     �����ļ�,�ļ���Ϊ<string>       *" << endl;
	cout << "* 7.edit + <string>                       �༭�ļ�,�ļ���Ϊ<string>       *" << endl;
	cout << "* 8.view + <string>                       ��ʾ�ļ�,�ļ���Ϊ<string>       *" << endl;
	cout << "* 9.attrib + <string>                     �����ļ�����,�ļ���Ϊ<string>   *" << endl;
	cout << "* 10.find + <string>                      �����ļ�λ��,�ļ���Ϊ<string>   *" << endl;
	cout << "* 11.del + <string>                       ɾ���ļ�,�ļ���Ϊ<string>       *" << endl;
	cout << "* 12.copy + <string> + <string>           �����ļ�,<string>ΪĿ¼���ļ��� *" << endl;
	cout << "* 13.import + <string> + <string>         �����ļ�,Ŀ¼���ļ��� + �ļ���  *" << endl;
	cout << "* 14.export + <string> + <string>         �����ļ�,�ļ��� + Ŀ¼���ļ���  *" << endl;
	cout << "* 15.help                                 ��ʾ�����˵�                    *" << endl;
	cout << "* 16.exit                                 �˳�                            *" << endl;
	cout << "***************************************************************************" << endl << endl;
}

/**********************�˳�����**********************/
void Exit() {
	exit(0);
}

/*******************���¶������ļ�*******************/
void ReadNew()
{
	fstream file;
	file.open(file_source, ios::out | ios::in | ios::binary);
	file.read((char*)&super_block, sizeof(SuperBlock));
	file.read((char*)&inode, sizeof(Inode) * INODE_NUM);
	file.close();
}

/**********************ִ������**********************/
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
			cout << "Error:���������������������help��ѯ��" << endl;
		}
		printf("%s", Path);
		cout << ">";
		cin >> temp;
	}
}

/**********************������**********************/
int main() {
	cout << " ==============================================================================" << endl;
	cout << " ================================= File System ================================" << endl;
	cout << " ==============================================================================" << endl;
	cout << " ����ʹ��������format�����ʽ��ϵͳ" << endl;
	strcpy(file_source, "data.bin");
	File = CreateSemaphore(
		NULL,           // ʹ��ȱʡ�İ�ȫ���ԡ�default security attributes
		1,  			// ��ʼ�ź���������Դֵ��initial count
		1,  			// �����ź���������Դ���ֵ��maximum count
		NULL);          // �����ź���������unnamed semaphore
						//�������ֵΪ�գ����ź�������ʧ��
	if (File == NULL)
	{
		printf("CreateSemaphore error: %d\n", GetLastError());
		return 0;
	}
	Cmd();
	return 0;
}
